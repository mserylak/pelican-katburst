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


//TimerData K7DataAdapter::_adapterTime;


// Construct the signal data adapter.
K7DataAdapter::K7DataAdapter(const ConfigNode& config) : AbstractStreamAdapter(config)
{
    // Read the configuration using configuration node utility methods.
    _channelsPerPacket = config.getOption("packet", "channels", "1024").toUInt(); // Number of channels in UDP packet arrving at adapter.
    _channelsPerBlob = config.getOption("blob", "channels", "1024").toUInt(); // Number of channels per UDP packet leaving adapter.
    _startChannel = config.getOption("start", "channel", "0").toUInt(); // Select from which channel a section of band is used.

    // Packet size variables.
    _packetSize = sizeof(K7Packet);
    _headerSize = sizeof(K7Packet::Header);
    _payloadSize = sizeof(K7Packet().data);
    _packetsPerSecond = 390625; // Number of Nyquist-sampled values leaving F-engines per second.
    //std::cout << "_packetSize " << _packetSize << std::endl;
    //std::cout << "_headerSize " << _headerSize << std::endl;
    //std::cout << "_payloadSize " << _payloadSize << std::endl;

    // Setting timestamp for first iteration of the pipeline.
    _lastTimestamp = 0.0;

    // Get the total number of channels adapter should take.
    _nChannels = _channelsPerBlob;

    // Total intensity will be used only.
    _nPolarisations = 1;

    // Check if _startChannel, _channelsPerPacket and _channelsPerBlob selection is correct.
    if (_startChannel > _channelsPerPacket - _channelsPerBlob - 1)
    {
        throw _err("K7DataAdapter(): Invalid startChannel and/or blob selection.");
    }

    // If whole spectrum is selected then reset _startChannel to 0.
    if (1024 == _channelsPerBlob)
    {
        _startChannel = 0;
    }

    // Temporary arrays for buffering data from the IO Device.
    _headerBuffer.resize(_headerSize);
    _dataBuffer.resize(_payloadSize);

}


// De-serialise a chunk of data from the input device.
void K7DataAdapter::deserialise(QIODevice* in)
{
    // Used for timing the pipeline.
    timerStart(&adapterTime);

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

    // UDP packet header.
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
    //std::cout << "nPacketsPerChunk " << nPacketsPerChunk << std::endl;

    // Resize the blob of data.
    blob->resize(nPacketsPerChunk, 1, _nPolarisations, _nChannels);

    // Loop over the UDP packets in the chunk.
    for (i = 0; i < nPacketsPerChunk; i++)
    {
        // Read the header from the IO device.
        bytesRead = 0;
        while (1)
        {
            temporaryBytesRead = in->read(headerBuffer + bytesRead, _headerSize - bytesRead);
            //std::cout << "temporaryBytesRead " << temporaryBytesRead << std::endl;
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
            //std::cout << "_samplingTime " << _samplingTime << std::endl;

            // Get the timestamp from UDP packet header.
            //std::cout << "header.UTCtimestamp " << std::fixed << std::setprecision(10) << header.UTCtimestamp << std::endl;
            //std::cout << "header.accumulationNumber " << std::fixed << std::setprecision(10) << header.accumulationNumber << std::endl;
            //std::cout << "header.accumulationRate " << std::fixed << std::setprecision(10) << header.accumulationRate << std::endl;

            // Calculate timestamp of first packet in the chunk.
            currentTimestamp = 1.0 * header.UTCtimestamp + (1.0 * header.accumulationNumber / _packetsPerSecond);
            //std::cout << "currentTimestamp " << std::fixed << std::setprecision(10) << currentTimestamp << std::endl;

            _expectedLastTimestamp = currentTimestamp + ((nPacketsPerChunk + 1) * _samplingTime);
            //std::cout << "_expectedLastTimestamp " << std::fixed << std::setprecision(10) << _expectedLastTimestamp << std::endl;

            // Check if the data is out of sync. During first iteration it will always complain.
            if (currentTimestamp - _lastTimestamp > _samplingTime)
            {
                std::cout << "K7DataAdapter::deserialise(): data out of sequence -- " << std::fixed << std::setprecision(10) << currentTimestamp - _lastTimestamp << std::endl;
            }
            _lastTimestamp = currentTimestamp + ((nPacketsPerChunk + 1) * _samplingTime);
            //std::cout << "_lastTimestamp " << std::fixed << std::setprecision(10) << _lastTimestamp << std::endl;
            //std::cout << "difference " << std::fixed << std::setprecision(10) << (_lastTimestamp-currentTimestamp)/_samplingTime << std::endl;
            //std::cout << "expected difference " << std::fixed << std::setprecision(10) << (_expectedLastTimestamp-currentTimestamp)/_samplingTime << std::endl;

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

#if 0
        // Read the packet payload from the input device.
        in->read(dataBuffer, _payloadSize);

        // Write out spectrum to blob.

        unsigned short int* dd = (unsigned short int*) dataBuffer;

        // Assign data (pointer) to a pointer to the spectrum data for the specified time block, sub-band and polarisation.
        data = (float*) blob->spectrumData(i, 0, 0);

        for (j = 0; j < _nChannels; j++)
        {
            data[j] = (float) (dd[j] + dd[j + 1]);
        }
#endif
    }
    // Set the timestamp and sampling rate in the blob.
    blob->setLofarTimestamp(currentTimestamp);
    blob->setBlockRate(_samplingTime);
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
    std::cout << "K7Packet::Header" << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    std::cout << "* UTCtimestamp       = " << (size_t) header.UTCtimestamp << std::endl;
    std::cout << "* accumulationNumber = " << (uint32_t) header.accumulationNumber << std::endl;
    std::cout << "* accumulationRate   = " << (uint32_t) header.accumulationRate << std::endl;
    std::cout << QString(80, '-').toStdString() << std::endl;
    std::cout << std::endl;
}

} // namespace ampp
} // namespace pelican

