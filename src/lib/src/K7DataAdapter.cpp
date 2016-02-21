#include "K7DataAdapter.h"
#include "SpectrumDataSet.h"
#include "pelican/utility/ConfigNode.h"
#include <iomanip>
#include <cmath>
#include <omp.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <QtCore/QString>

namespace pelican {
namespace ampp {

// Construct the signal data adapter.
K7DataAdapter::K7DataAdapter(const ConfigNode& config) : AbstractStreamAdapter(config)
{
    // Check the configuration type matches the class name.
    if (config.type() != "K7DataAdapter")
    {
        throw _err("K7DataAdapter(): Invalid or missing XML configuration.");
    }

    // Get the total number of channels adapter should take.
    _nChannels = config.getOption("channelsPerBlob", "value", "1024").toUInt();
    std::cout << "K7DataAdapter::K7DataAdapter(): _nChannels " << _nChannels << std::endl;

    _headerSize = sizeof(K7Packet::Header);
    _packetSize = _nChannels * sizeof(uint64_t) + _headerSize;
    _payloadSize = _packetSize - _headerSize;
    _packetsPerSecond = 390625; // Number of Nyquist-sampled values leaving F-engines per second.
    std::cout << "K7DataAdapter::K7DataAdapter(): _packetSize " << _packetSize << std::endl;
    std::cout << "K7DataAdapter::K7DataAdapter(): _headerSize " << _headerSize << std::endl;
    std::cout << "K7DataAdapter::K7DataAdapter(): _payloadSize " << _payloadSize << std::endl;

    // Setting timestamp for first iteration of the pipeline.
    _lastTimestamp = 0.0;

    // Total intensity will be used only.
    _nPolarisations = 1;

    // Temporary arrays for buffering data from the IO Device.
    _headerBuffer.resize(_headerSize);
    _dataBuffer.resize(_payloadSize);
}

// De-serialise a chunk of data from the input device.
void K7DataAdapter::deserialise(QIODevice* in)
{
    // Sanity check on data blob dimensions and chunk size.
    // Check that there is something to adapt to.
    if (_chunkSize == 0)
    {
        throw _err("deserialise(): Chunk size zero!");
    }

    // Check the data blob passed to the adapter is allocated.
    if (!_data)
    {
        throw _err("deserialise(): Cannot deserialise into an unallocated blob!.");
    }

    // UDP packet header definition.
    K7Packet::Header header;

    // Declare variables;
    unsigned int i, j, k;
    unsigned int nPacketsPerChunk = 0; // Number of UDP packets in chunk/blob.
    unsigned int block = 0;
    unsigned int bytesRead = 0;
    int temporaryBytesRead = 0;
    double currentTimestamp = 0.0;
    float *data = NULL;

    // Pointers to temporary arrays for data buffering;
    char* headerBuffer = &_headerBuffer[0];
    char* dataBuffer = &_dataBuffer[0];

    i = 0;
    j = 0;

    // A pointer to the data blob to fill should be obtained by calling the
    // dataBlob() inherited method. This returns a pointer to an
    // abstract dataBlob, which should be cast to the appropriate type.
    SpectrumDataSetStokes* blob = (SpectrumDataSetStokes*) dataBlob();

    // Set the size of the data blob to fill. The chunk size is obtained by calling the chunkSize() inherited method.
    nPacketsPerChunk = chunkSize() / _packetSize;
    //std::cout << "K7DataAdapter::deserialise(): nPacketsPerChunk " << nPacketsPerChunk << std::endl;

    // Resize the blob of data.
    blob->resize(nPacketsPerChunk, 1, _nPolarisations, _nChannels); // Where 1 is number of subbands (LOFAR-wise).

    // Loop over the UDP packets in the chunk.
    for (i = 0; i < nPacketsPerChunk; i++)
    {
        // Read the header from the IO device.
        bytesRead = 0;
        while (1)
        {
            temporaryBytesRead = in->read(headerBuffer + bytesRead, _headerSize - bytesRead);
            //std::cout << "K7DataAdapter::deserialise(): temporaryBytesRead " << temporaryBytesRead << std::endl;
            if (temporaryBytesRead <= 0) in->waitForReadyRead(-1);
            else bytesRead += temporaryBytesRead;
            if (bytesRead == _headerSize) break;
        }

        // Read the packet header from the input device.
        _readHeader(headerBuffer, header);

        // First packet in the chunk.
        if (i == 0)
        {
            // Set the sampling time for precise timestamp calculation.
            _samplingTime = 1.0 * header.accumulationRate / _packetsPerSecond;
            //std::cout << "K7DataAdapter::deserialise(): _samplingTime " << std::fixed << std::setprecision(8) << _samplingTime << std::endl;

            // Get the timestamp from UDP packet header.
            //std::cout << "K7DataAdapter::deserialise(): header.UTCtimestamp " << std::fixed << std::setprecision(10) << header.UTCtimestamp << std::endl;
            //std::cout << "K7DataAdapter::deserialise(): header.accumulationNumber " << std::fixed << std::setprecision(10) << header.accumulationNumber << std::endl;
            //std::cout << "K7DataAdapter::deserialise(): header.accumulationRate " << std::fixed << std::setprecision(10) << header.accumulationRate << std::endl;

            // Calculate timestamp of first packet in the chunk.
            currentTimestamp = 1.0 * header.UTCtimestamp + (1.0 * header.accumulationNumber / _packetsPerSecond);
            //std::cout << "K7DataAdapter::deserialise(): currentTimestamp " << std::fixed << std::setprecision(10) << currentTimestamp << std::endl;

            _expectedLastTimestamp = currentTimestamp + ((nPacketsPerChunk + 1) * _samplingTime);
            //std::cout << "K7DataAdapter::deserialise(): _expectedLastTimestamp " << std::fixed << std::setprecision(10) << _expectedLastTimestamp << std::endl;

            // Check if the data is out of sync. During first iteration it will always complain.
            if (currentTimestamp - _lastTimestamp > _samplingTime)
            {
                std::cout << "K7DataAdapter::deserialise(): data out of sequence -- " << std::fixed << std::setprecision(10) << currentTimestamp - _lastTimestamp << std::endl;
            }
            _lastTimestamp = currentTimestamp + ((nPacketsPerChunk + 1) * _samplingTime);
            //std::cout << "K7DataAdapter::deserialise(): _lastTimestamp " << std::fixed << std::setprecision(10) << _lastTimestamp << std::endl;
            //std::cout << "K7DataAdapter::deserialise(): difference " << std::fixed << std::setprecision(10) << (_lastTimestamp-currentTimestamp)/_samplingTime << std::endl;
            //std::cout << "K7DataAdapter::deserialise(): expected difference " << std::fixed << std::setprecision(10) << (_expectedLastTimestamp-currentTimestamp)/_samplingTime << std::endl;

            // Set the timestamp and sampling rate in the blob.
            blob->setLofarTimestamp(currentTimestamp);
            blob->setBlockRate(_samplingTime);
        }

        // Read the useful data.
        bytesRead = 0;
        while (1)
        {
            temporaryBytesRead = in->read(dataBuffer + bytesRead, _payloadSize - bytesRead);
            if (temporaryBytesRead <= 0) in->waitForReadyRead(-1);
            else bytesRead += temporaryBytesRead;
            if (bytesRead == _payloadSize) break;
        }

        // Read the packet payload from the input device.
        //in->read(dataBuffer, _payloadSize);

        // Write out spectrum to blob.
        unsigned short int* dd = (unsigned short int*) dataBuffer;
        // Assign data (pointer) to a pointer to the spectrum data for the specified time block, sub-band and polarisation.
        data = (float*) blob->spectrumData(i, 0, 0);
        for (j = 0; j <= _nChannels; j++)
        {
            data[j] = (float) (dd[j * 4] + dd[(j * 4) + 1]); // XX* + YY*
//            data[j] = (float) (dd[j * 4]); // XX*
//            data[j] = (float) (dd[(j * 4) + 1]); // YY*
        }
    }
}

// Reads the UDP packet header from the IO device.
void K7DataAdapter::_readHeader(char* buffer, K7Packet::Header& header)
{
    header = *reinterpret_cast<K7Packet::Header*>(buffer);
    //_printHeader(header);
}

// Prints UDP packet header.
void K7DataAdapter::_printHeader(const K7Packet::Header& header)
{
    std::cout << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    std::cout << "K7DataAdapter::_printHeader()" << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    std::cout << "* UTCtimestamp       = " << (size_t) header.UTCtimestamp << std::endl;
    std::cout << "* accumulationNumber = " << (uint32_t) header.accumulationNumber << std::endl;
    std::cout << "* accumulationRate   = " << (uint32_t) header.accumulationRate << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    std::cout << std::endl;
}

} // namespace ampp
} // namespace pelican

