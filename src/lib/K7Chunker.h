#ifndef K7CHUNKER_H
#define K7CHUNKER_H

#include "K7Packet.h"
#include "pelican/server/AbstractChunker.h"
#include <QtCore/QString>
#include <QtCore/QObject>
#include <QtCore/QMutex>

namespace pelican {
namespace ampp {

class DataManager;

// KATBURST data chunker class.
class K7Chunker : public AbstractChunker
{
    public:
        // Constructor
        K7Chunker(const ConfigNode& config);

        // Destructor
        ~K7Chunker() {}

        // Creates the socket to use for the incoming data stream.
        virtual QIODevice* newDevice();

        // Called whenever there is data ready to be processed.
        virtual void next(QIODevice*);

        // Sets the number of packets to read.
        void setPackets(int packets)
        {
            _nPackets = packets;
        }

    private:
        // Generates an empty UDP packet.
        void updateEmptyPacket(K7Packet& packet, unsigned long int timestamp, unsigned int accumulation, unsigned int rate);

        // Write K7Packet to writeableData object
        int writePacket(WritableData* writer, K7Packet& packet, unsigned int packetSize, unsigned int offset);

        // Returns an error message suitable for throwing.
        QString _err(QString message)
        {
            return QString("K7Chunker::") + message;
        }

    private:
        unsigned int _packetCount;
        WritableData _writable;
        unsigned int _nPackets;
        unsigned int _packetsRejected;
        unsigned int _packetsAccepted;
        unsigned int _byte1OfStream;
        unsigned int _packetSize;
        unsigned int _headerSize;
        unsigned int _bytesStream;
        unsigned int _packetSizeStream;
        unsigned long int _startTimestamp;
        unsigned long int _startAccumulation;
        unsigned int _clock;
        unsigned long int _packetsPerSecond;
        unsigned int _chunkerCounter;
        unsigned long int _chunksProcessed;
        unsigned int _channelStart;
        unsigned int _channelEnd;
        unsigned int _nChannels;
        unsigned int _streamChannels;
        K7Packet _emptyPacket;
};

PELICAN_DECLARE_CHUNKER(K7Chunker)

} // namespace ampp
} // namespace pelican
#endif // K7CHUNKER_H_
