#include "ABDataAdapter.h"
#include "SpectrumDataSet.h"
#include <arpa/inet.h>
#include <iomanip>
//#include <sys/time.h>
#include <omp.h>
#include <hiredis/hiredis.h>

using namespace pelican;
using namespace pelican::ampp;

TimerData ABDataAdapter::_adapterTime;

// Construct the signal data adapter.
ABDataAdapter::ABDataAdapter(const ConfigNode& config)
    : AbstractStreamAdapter(config)
{
    // Read the configuration using configuration node utility methods.
    _pktsPerSpec = config.getOption("spectrum", "packets").toUInt();
    _channelsPerPacket = config.getOption("packet", "channels").toUInt();
    _samplesPerPacket = config.getOption("packet", "samples").toUInt();
    _channelsPerBlob = config.getOption("blob", "channels").toUInt();
    _tSamp = config.getOption("samplingTime", "seconds").toFloat();

    // Set up the packet data.
    _packetSize = _headerSize + _channelsPerPacket * 8;// + _footerSize;

    // Calculate the total number of channels.
    //_nChannels = _pktsPerSpec * _channelsPerPacket;
    //    _nChannels = _channelsPerPacket;
    _nChannels = _channelsPerBlob;

    // Set missing packet stats.
    _numMissInst = 0;
    _numMissPkts = 0;

    // Set the previous counts.
    _prevSpecQuart = 0;
    _prevIntegCount = 0;

    _integCountStart = 0;
    _first = 1;
    _timestampFirst = 1;

    _x = 0;

    // Get start time (MJD) from the S6 redis server
    _mcount0UnixTime = 0.0;
    getResetTimeFromRedis();
}

// Get ROACH2 reset time (MJD) from the S6 redis database
void ABDataAdapter::getResetTimeFromRedis()
{
    long int mcount0time = 0;
    double mcount0MJD = 0.0;

    redisContext *c = redisConnect("serendip6", 6379);
    redisReply *reply = (redisReply *) redisCommand(c, "GET s6_mcount_0");
    if (REDIS_REPLY_ERROR == reply->type)
    {
        std::cerr << "ERROR: Getting MJD from redis failed! Start MJD will be 0." << std::endl;
        freeReplyObject(reply);
        redisFree(c);
        return;
    }

    // mcount0time is the number of seconds elapsed since the Epoch,
    // 1970-01-01 00:00:00 +0000 (UTC), when the mcount on the ROACH2s were
    // reset to 0
    mcount0time = atoi(reply->str);
    // Convert this to MJD; 2440587.5 is the MJD of the Unix Epoch
    mcount0MJD = ((double) mcount0time / 86400) + 40587.0;
    _mcount0UnixTime = mcount0time;

    std::cout << "ROACH2 reset MJD: " << mcount0MJD << std::endl;

    freeReplyObject(reply);
    redisFree(c);

    return;
}

// Called to de-serialise a chunk of data from the input device.
void ABDataAdapter::deserialise(QIODevice* device)
{
    //timerStart(&_adapterTime);
    /*struct timeval stTime = {0};
    (void) gettimeofday(&stTime, NULL);
    double t = (stTime.tv_sec - 1425601680) + (stTime.tv_usec * 0.000001);
    std::cout << std::fixed << std::setprecision(6) << t << std::endl;*/
    // A pointer to the data blob to fill should be obtained by calling the
    // dataBlob() inherited method. This returns a pointer to an
    // abstract DataBlob, which should be cast to the appropriate type.
    SpectrumDataSetStokes* blob = (SpectrumDataSetStokes*) dataBlob();

    // Set the size of the data blob to fill.
    // The chunk size is obtained by calling the chunkSize() inherited method.
    unsigned packets = chunkSize() / _packetSize;
    // Number of time samples; Each channel contains 4 pseudo-Stokes values,
    // each of size sizeof(short int)
    unsigned nBlocks = (packets / _pktsPerSpec) * _samplesPerPacket;
    _nPolarisations = 1;
    blob->resize(nBlocks, 1, _nPolarisations, _nChannels);

    // Create a temporary buffer to read out the packet headers, and
    // get the pointer to the data array in the data blob being filled.
    char headerData[_headerSize];
    //char d[_pktsPerSpec * (_packetSize - _headerSize - _footerSize)];
    char d[_packetSize - _headerSize];// - _footerSize];
    char footerData[_footerSize];

    // Loop over the UDP packets in the chunk.
    float *data = NULL;
    unsigned bytesRead = 0;
    unsigned block = 0;
    signed int specQuart = 0;
    unsigned long int integCount = 0;
    unsigned int icDiff = 0;
    unsigned int sqDiff = 0;
    double timestamp = 0.0;
    unsigned long int firstIntegCountThisBlock = 0;

    for (unsigned p = 0; p < packets; p++)
    {
        // Ensure there is enough data to read from the device.
        while (device->bytesAvailable() < _packetSize)
        {
            device->waitForReadyRead(-1);
        }

        // Read the packet header from the input device.
        device->read(headerData, _headerSize);

#if 1
        // Get the packet integration count
        unsigned long int counter = (*((unsigned long int *) headerData)) & 0x0000FFFFFFFFFFFF;
        integCount = (unsigned long int)        // Casting required.
                      (((counter & 0x0000FF0000000000) >> 40)
                     + ((counter & 0x000000FF00000000) >> 24)
                     + ((counter & 0x00000000FF000000) >> 8)
                     + ((counter & 0x0000000000FF0000) << 8)
                     + ((counter & 0x000000000000FF00) << 24)
                     + ((counter & 0x00000000000000FF) << 40));

        if (0 == p)
        {
            firstIntegCountThisBlock = integCount;
        }
        //std::cout << integCount << std::endl;
        timestamp = ((double) (integCount - _integCountStart) * _tSamp);
        if (!_first)
        {
            if (timestamp - _lastTimestamp > _tSamp)
            {
                std::cerr << "ERROR! " << integCount << ", " << _prevIntegCount << ", " << _integCountStart
                    << std::fixed << std::setprecision(10)
                    << ", " << timestamp << ", " << _lastTimestamp
                    << std::endl;
            }
        }
        else
        {
            _integCountStart = integCount;
            _first = 0;
        }

        // Get the spectral quarter number
        specQuart = (unsigned char) headerData[6];
        //std::cout << specQuart << ", " << integCount << std::endl;

        //bytesRead += device->read(d + bytesRead,
        //                          _packetSize - _headerSize - _footerSize);
        device->read(d, _packetSize - _headerSize);// - _footerSize);
        // Write out spectrum to blob if this is the last spectral quarter.
        unsigned short int* dd = (unsigned short int*) d;
        if (_pktsPerSpec - 3 == specQuart)
        {
#if 0
            for (unsigned pol = 0; pol < _nPolarisations; pol++)
            {
                data = (float*) blob->spectrumData(block, 0, pol);
                for (unsigned chan = 0; chan < _nChannels; chan++)
                {
                    data[chan] = (float) dd[chan * 4 + pol];
                }
            }
#else

            // Compute Stokes I and ignore the rest.
            data = (float*) blob->spectrumData(block, 0, 0);
            for (unsigned chan = 0; chan < _nChannels; chan++)
            {
                data[chan] = (float) (ntohs(dd[chan * 4 + 0])          // XX*
                                      + ntohs(dd[chan * 4 + 1]));      // YY*
            }
#endif
     
            //memset(d, '\0', _pktsPerSpec * (_packetSize - _headerSize - _footerSize));
            //memset(d, '\0', _packetSize - _headerSize - _footerSize);
            //bytesRead = 0;
            block++;
        }
        // Read the packet footer from the input device and discard it.
        //device->read(footerData, _footerSize);

        _lastTimestamp = timestamp;
        _prevIntegCount = integCount;
#endif
    }

    blob->setLofarTimestamp((firstIntegCountThisBlock * _tSamp) + _mcount0UnixTime);
    blob->setBlockRate(_tSamp);
    //timerUpdate(&_adapterTime);
}

