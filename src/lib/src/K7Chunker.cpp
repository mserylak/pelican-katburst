#include "K7Chunker.h"
#include "K7Packet.h"
#include <QtNetwork/QUdpSocket>
#include <QtCore/QTextStream>
#include <QtCore/QFile>
#include <QtCore/QString>
#include <QtCore/QIODevice>
#include <QtCore/QObject>
#include <QtCore/QMutex>
#include <QtCore/QMutexLocker>
#include <cstdio>
#include <iostream>

namespace pelican {
namespace ampp {

// Construct the chunker.
K7Chunker::K7Chunker(const ConfigNode& config) : AbstractChunker(config)
{
    // Check the configuration type matches the class name.
    if (config.type() != "K7Chunker")
    {
        throw _err("K7Chunker::K7Chunker(): Invalid or missing XML configuration.");
    }

    // Packet dimensions.
    _packetSize = sizeof(K7Packet);
    _headerSize = sizeof(K7Packet::Header);
    // Number of packets per chunk (pipeline iteration).
    _nPackets = config.getOption("udpPacketsPerIteration", "value", "128").toUInt();
    // Number of Nyquist-sampled values leaving F-engines per second.
    _packetsPerSecond = 390625;

    // Total number of channels per incoming packet.
    _nChannels = config.getOption("channelsPerPacket", "value", "1024").toUInt();

    // Channel range for the output stream (counted from 0).
    _channelStart = config.getOption("stream", "channelStart", "0").toUInt();
    _channelEnd = config.getOption("stream", "channelEnd", "1023").toUInt();
    if ( (_channelEnd >= _nChannels) || (_channelStart >= _channelEnd) || (_channelStart < 0) || (_channelEnd < 0) )
    {
        throw _err("K7Chunker::K7Chunker(): Invalid channel ranges.");
    }
    std::cout << "K7Chunker::K7Chunker(): _channelStart " << _channelStart << std::endl;
    std::cout << "K7Chunker::K7Chunker(): _channelEnd " << _channelEnd << std::endl;

    // Calculate the size of the packet for the output stream...
    _streamChannels = _channelEnd - _channelStart + 1;
    std::cout << "K7Chunker::K7Chunker(): _streamChannels " << _streamChannels << std::endl;

    // Since there is only one sample per packet and no raw polarizations but pseudo-Stokes both values are 1.
    _packetSizeStream = _streamChannels * sizeof(uint64_t) + _headerSize;
    std::cout << "K7Chunker::K7Chunker(): _packetSizeStream " << _packetSizeStream << std::endl;

    // ...and the output streams.
    _bytesStream = _packetSizeStream - _headerSize;
    std::cout << "K7Chunker::K7Chunker(): _bytesStream " << _bytesStream << std::endl;
    _byte1OfStream = _channelStart * sizeof(uint64_t);
    std::cout << "K7Chunker::K7Chunker(): _byte1OfStream " << _byte1OfStream << std::endl;

    // Initialise class variables.
    _startTimestamp = 0;
    _startAccumulation = 0;
    _packetsAccepted = 0;
    _packetsRejected = 0;

    // Set the empty packet data for stream.
    memset((void*)_emptyPacket.data, 0, _bytesStream);
    _emptyPacket.header.UTCtimestamp = 0;
    _emptyPacket.header.accumulationNumber = 0;
    _emptyPacket.header.accumulationRate = 0;

    // Set the chunk processed counter.
    _chunksProcessed = 0;
    _chunkerCounter = 0;
}

// Constructs a new QIODevice (in this case a QUdpSocket) and returns it
// after binding the socket to the port specified in the XML node and read by
// the constructor of the abstract chunker.
QIODevice* K7Chunker::newDevice()
{
    QUdpSocket* udpSocket = new QUdpSocket;

    if (!udpSocket->bind(port(), QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint ))
    {
        std::cerr << "K7Chunker::newDevice(): Unable to bind to UDP port!" << udpSocket->errorString().toStdString() << std::endl;
    }

    return udpSocket;
}

// Gets the next chunk of data from the UDP socket (if it exists).
void K7Chunker::next(QIODevice* device)
{
    unsigned int i;
    unsigned int lostPackets, difference;
    unsigned int offsetStream;
    unsigned int lostPacketCounter;
    unsigned long int previousTimestamp;
    unsigned int previousAccumulation;
    unsigned long int timestamp;
    unsigned int accumulation;
    unsigned int rate;
    QUdpSocket* udpSocket = static_cast<QUdpSocket*>(device);
    K7Packet currentPacket;
    K7Packet outputPacket;
    K7Packet _emptyPacket;

    difference = 0;
    offsetStream = 0;
    timestamp = 0;
    accumulation = 0;
    rate = 0;
    previousTimestamp = _startTimestamp;
    previousAccumulation = _startAccumulation;

    // Get writable buffer space for the chunk.
    WritableData writableData;
    if ( ! _writable.isValid() )
    {
        writableData = getDataStorage(_nPackets * _packetSizeStream, chunkTypes().at(0));
    }
    else
    {
        writableData = _writable;
    }

    if (writableData.isValid())
    {
        // Loop over the number of UDP packets to put in a chunk.
        for (i = 0; i < _nPackets; ++i)
        {
            // Chunker sanity check.
            if (!isActive())
                return;

            // Wait for datagram to be available.
            while ( !udpSocket->hasPendingDatagrams() )
            {
                _writable = writableData;
                continue;
            }

            // Read the current UDP packet from the socket.
            if (udpSocket->readDatagram(reinterpret_cast<char*>(&currentPacket), _packetSize) <= 0)
            {
                std::cerr << "K7Chunker::next(): Error while receiving UDP Packet!" << std::endl;
                i--;
                continue;
            }

            // Get the UDP packet header.
            timestamp    = currentPacket.header.UTCtimestamp;
            accumulation = currentPacket.header.accumulationNumber;
            rate         = currentPacket.header.accumulationRate;
            //std::cout << "K7Chunker::next(): timestamp " << timestamp << " accumulation " << accumulation << " rate " << rate << std::endl;
            // First time next() has been run, initialise _startTimestamp and _startAccumulation.
            if (i == 0 && _startTimestamp == 0)
            {
                previousTimestamp = _startTimestamp = _startTimestamp == 0 ? timestamp : _startTimestamp;
                previousAccumulation = _startAccumulation = _startAccumulation == 0 ? accumulation : _startAccumulation;
                //std::cout << "K7Chunker::next(): timestamp " << timestamp << " accumulation " << accumulation << " rate " << rate << std::endl;
            }

            // Sanity check in UTCtimestamp. If the seconds counter is 0xFFFFFFFFFFFFFFFF, the data cannot be trusted (ignore).
            if (timestamp == ~0UL || previousTimestamp + 10 < timestamp)
            {
                std::cerr << "K7Chunker::next(): Data cannot be trusted! Timestamp is " << timestamp << " or previousTimestamp is " << previousTimestamp << std::endl;
                exit(-1);
            }

            // Check that the packets are contiguous. accumulationNumber increments by
            // accumulationRate which is defined in the header of UDP packet.
            // accumulationNumber is reset every interval (although it might not start from 0
            // as the previous frame might contain data from this one).
            lostPackets = 0;
            difference = (accumulation >= previousAccumulation) ? (accumulation - previousAccumulation) : (accumulation + _packetsPerSecond - previousAccumulation);
#if 0
            // Duplicated packets. Need to address this. ICBF does not duplicate packets though.
            if (difference < rate)
            {
                std::cout << "Duplicated packets, difference " << difference << std::endl;
                ++_packetsRejected;
                i -= 1;
                continue;
            }
            else
#endif
            // Missing packets.
            if (difference > rate)
            {
                // -1 since it includes the received packet as well.
                lostPackets = (difference / rate) - 1;
            }

            if (lostPackets > 0)
            {
                printf("K7Chunker::next(): generate %u empty packets, previousTimestamp: %lu, new timestamp: %lu, prevAccumulation: %u, newAccumulation: %u\n",
                        lostPackets, previousTimestamp, timestamp, previousAccumulation, accumulation);
            }

            // Generate lostPackets (empty packets) if needed.
            lostPacketCounter = 0;
            for (lostPacketCounter = 0; lostPacketCounter < lostPackets && i + lostPacketCounter < _nPackets; ++lostPacketCounter)
            {
                // Generate empty packet with correct timestamp and accumulation number.
                previousTimestamp = (previousAccumulation < _packetsPerSecond) ? previousTimestamp : previousTimestamp + 1;
                previousAccumulation = previousAccumulation % _packetsPerSecond;
                updateEmptyPacket(_emptyPacket, previousTimestamp, previousAccumulation, rate);
                offsetStream = writePacket(&writableData, _emptyPacket, _packetSizeStream, offsetStream);
            }
            i += lostPacketCounter;

            // Write received packet to stream after updating header and data.
            if (i != _nPackets)
            {
                ++_packetsAccepted;
                // Generate stream packet.
                outputPacket.header = currentPacket.header;
                memcpy((void*)outputPacket.data, &currentPacket.data[_byte1OfStream], _bytesStream);
                offsetStream = writePacket(&writableData, outputPacket, _packetSizeStream, offsetStream);
                previousTimestamp = timestamp;
                previousAccumulation = accumulation;
            }
        }
        _chunksProcessed++;
        _chunkerCounter++;
        // Clear any data locks.
        _writable = WritableData();
        if (_chunkerCounter % 5 == 0)
        {
            std::cout << "K7Chunker::next(): " << _chunksProcessed << " chunks processed. " << "UTC timestamp " << timestamp << " accumulationNumber " << accumulation << " accumulationRate " << rate << std::endl;
        }
    }
    else
    {
        // Must discard the datagram if there is no available space.
        if (!isActive()) return;
        udpSocket->readDatagram(0, 0);
        std::cout << "K7Chunker::next(): Writable data not valid, discarding packets." << std::endl;
    }

    // Update _startTime.
    _startTimestamp = previousTimestamp;
    _startAccumulation = previousAccumulation;
}

// Generates an empty UDP packet with no time stamp.
void K7Chunker::updateEmptyPacket(K7Packet& packet, unsigned long int timestamp, unsigned int accumulation, unsigned int rate)
{
    packet.header.UTCtimestamp = timestamp;
    packet.header.accumulationNumber = accumulation;
    packet.header.accumulationRate = rate;
}

// Write packet to WritableData object.
int K7Chunker::writePacket(WritableData *writer, K7Packet& packet, unsigned int packetSize, unsigned int offset)
{
    if (writer->isValid()) {
        writer->write(reinterpret_cast<void*>(&packet), packetSize, offset);
        return offset + packetSize;
    }
    else {
        throw _err("writePacket(): WritableData is not valid!");
        return -1;
    }
}

} // namespace ampp
} // namespace pelican
