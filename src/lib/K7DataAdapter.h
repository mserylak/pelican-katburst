#ifndef K7DATAADAPTER_H
#define K7DATAADAPTER_H

#include "pelican/core/AbstractStreamAdapter.h"
#include "timer.h"
#include "K7Packet.h"

namespace pelican {

class ConfigNode;
class SpectrumDataSetStokes;

namespace ampp {

// Adapter to convert chunks of signal stream data into a K7Data data-blob.
class K7DataAdapter : public AbstractStreamAdapter
{
    public:
        // Constructs the adapter.
        K7DataAdapter(const ConfigNode& config);

        // Destructor
        ~K7DataAdapter() {}

        // Method to deserialise chunks of memory provided by the I/O device.
        void deserialise(QIODevice* in);

    private:

        /// Read the UDP packet header from a buffer read from the IO device.
        void _readHeader(char* buffer, K7Packet::Header& header);

        // Prints the header to standard out (for debugging).
        void _printHeader(const K7Packet::Header& header);

        static TimerData _adapterTime;

    private:

        // Returns an error message suitable for throwing.
        QString _err(QString message)
        {
            return QString("K7DataAdapter::") + message;
        }

    private:
        size_t _headerSize;
        size_t _packetSize;
        size_t _payloadSize;
        std::vector<char> _headerBuffer;
        std::vector<char> _dataBuffer;

        unsigned int _channelsPerPacket;
        unsigned int _channelsPerBlob;
        unsigned int _startChannel;
        unsigned int _nChannels;
        unsigned int _nPolarisations;
        unsigned long int _packetsPerSecond;
        unsigned long int _counter;
        double _samplingTime;
        double _lastTimestamp;
        double _expectedLastTimestamp;

};

PELICAN_DECLARE_ADAPTER(K7DataAdapter)

} // namespace ampp
} // namespace pelican

#endif // K7DATAADAPTER_H

