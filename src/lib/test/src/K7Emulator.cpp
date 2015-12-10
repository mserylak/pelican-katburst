#include "K7Emulator.h"
#include "pelican/utility/ConfigNode.h"
#include <sys/time.h>
#include <cmath>
#include <iostream>

namespace pelican {
namespace ampp {

// Constructs the K7Emulator. This obtains the relevant configuration parameters.
K7Emulator::K7Emulator(const ConfigNode& configNode) : AbstractUdpEmulator(configNode)
{
    // Initialise defaults.
    _channels = configNode.getOption("packet", "channels", "1024").toULong(); // Number of spectral channels per packet.
    _interval = configNode.getOption("packet", "interval", "82").toULong(); // Interval in microseconds.
    _accumulationRate = 32;
    _accumulationNumber = 0;

    // Set the packet size in bytes (each sample is 8 bytes + 16 for header).
    _packet.resize(_channels * 8 + 16);

    // Set constant parts of packet header data.
    char* ptr = _packet.data();

    // Timestamp in Unix time.
    struct timeval tv;
    gettimeofday(&tv, NULL);
    _UTCtimestamp = tv.tv_sec;

    // _UTCtimestamp has to be changed into bits and put into first 0->7 bytes of packet.
    *(ptr)     = (unsigned char)  (_UTCtimestamp & 0x00000000000000FF);
    *(ptr + 1) = (unsigned char) ((_UTCtimestamp & 0x000000000000FF00) >> 8);
    *(ptr + 2) = (unsigned char) ((_UTCtimestamp & 0x0000000000FF0000) >> 16);
    *(ptr + 3) = (unsigned char) ((_UTCtimestamp & 0x00000000FF000000) >> 24);
    *(ptr + 4) = (unsigned char) ((_UTCtimestamp & 0x000000FF00000000) >> 32);
    *(ptr + 5) = (unsigned char) ((_UTCtimestamp & 0x0000FF0000000000) >> 40);
    *(ptr + 6) = (unsigned char) ((_UTCtimestamp & 0x00FF000000000000) >> 48);
    *(ptr + 7) = (unsigned char) ((_UTCtimestamp & 0xFF00000000000000) >> 56);

    // _accumulationNumber has to be changed into bits and put into 8->11 bytes of packet.
    *(ptr + 8) = (unsigned char)   (_accumulationNumber & 0x00000000000000FF);
    *(ptr + 9) = (unsigned char)  ((_accumulationNumber & 0x000000000000FF00) >> 8);
    *(ptr + 10) = (unsigned char) ((_accumulationNumber & 0x0000000000FF0000) >> 16);
    *(ptr + 11) = (unsigned char) ((_accumulationNumber & 0x00000000FF000000) >> 24);

    // _accumulationRate has to be changed into bits and put into 12->16 bytes of packet.
    *(ptr + 12) = (unsigned char)  (_accumulationRate & 0x00000000000000FF);
    *(ptr + 13) = (unsigned char) ((_accumulationRate & 0x000000000000FF00) >> 8);
    *(ptr + 14) = (unsigned char) ((_accumulationRate & 0x0000000000FF0000) >> 16);
    *(ptr + 15) = (unsigned char) ((_accumulationRate & 0x00000000FF000000) >> 24);

    // Print initial values.
    std::cout << "# _UTCtimestamp " << "_accumulationNumber " << "_accumulationRate" << std::endl;
    std::cout << _UTCtimestamp << " " << _accumulationNumber << " " << _accumulationRate << std::endl;
}

// Creates a packet of UDP signal data containing the psuedo-Stokes values
// setting the pointer to the start of the packet and the size of the packet.
void K7Emulator::getPacketData(char*& ptr, unsigned long& size)
{
    // Initialise defaults.
    uint32_t packetsPerSecond = 390625; // Maximal number of packets per second.

    // Set pointer to the output data.
    ptr = _packet.data();
    size = _packet.size();

    // Set the packet header.
    // Set timestamp
    *(ptr + 0) = (unsigned char)  (_UTCtimestamp & 0x00000000000000FF);
    *(ptr + 1) = (unsigned char) ((_UTCtimestamp & 0x000000000000FF00) >> 8);
    *(ptr + 2) = (unsigned char) ((_UTCtimestamp & 0x0000000000FF0000) >> 16);
    *(ptr + 3) = (unsigned char) ((_UTCtimestamp & 0x00000000FF000000) >> 24);
    *(ptr + 4) = (unsigned char) ((_UTCtimestamp & 0x000000FF00000000) >> 32);
    *(ptr + 5) = (unsigned char) ((_UTCtimestamp & 0x0000FF0000000000) >> 40);
    *(ptr + 6) = (unsigned char) ((_UTCtimestamp & 0x00FF000000000000) >> 48);
    *(ptr + 7) = (unsigned char) ((_UTCtimestamp & 0xFF00000000000000) >> 56);

    // Set accumulation number
    *(ptr + 8)  = (unsigned char)  (_accumulationNumber & 0x00000000000000FF);
    *(ptr + 9)  = (unsigned char) ((_accumulationNumber & 0x000000000000FF00) >> 8);
    *(ptr + 10)= (unsigned char)  ((_accumulationNumber & 0x0000000000FF0000) >> 16);
    *(ptr + 11)= (unsigned char)  ((_accumulationNumber & 0x00000000FF000000) >> 24);

    // Set accumulation rate
    *(ptr + 12) = (unsigned char)  (_accumulationRate & 0x00000000000000FF);
    *(ptr + 13) = (unsigned char) ((_accumulationRate & 0x000000000000FF00) >> 8);
    *(ptr + 14) = (unsigned char) ((_accumulationRate & 0x0000000000FF0000) >> 16);
    *(ptr + 15) = (unsigned char) ((_accumulationRate & 0x00000000FF000000) >> 24);

    // Fill the packet payload with data.
    for (unsigned i = 0; i < _channels; ++i)
    {
        int8_t XXre = int(i/8);
        int8_t YYre = int(i/8);
        int8_t XYre = int(i/8);
        int8_t XYim = int(i/8);
        // Set XXre
        *(ptr + 16 + (i * 8)) = (unsigned char)  (XXre & 0x00000000000000FF);
        *(ptr + 17 + (i * 8)) = (unsigned char) ((XXre & 0x000000000000FF00) >> 8);
        // Set YYre
        *(ptr + 18 + (i * 8)) = (unsigned char)  (YYre & 0x00000000000000FF);
        *(ptr + 19 + (i * 8)) = (unsigned char) ((YYre & 0x000000000000FF00) >> 8);
        // Set XYre
        *(ptr + 20 + (i * 8)) = (unsigned char)  (XYre & 0x00000000000000FF);
        *(ptr + 21 + (i * 8)) = (unsigned char) ((XYre & 0x000000000000FF00) >> 8);
        // Set XYim
        *(ptr + 22 + (i * 8)) = (unsigned char)  (XYim & 0x00000000000000FF);
        *(ptr + 23 + (i * 8)) = (unsigned char) ((XYim & 0x000000000000FF00) >> 8);
    }
    _accumulationNumber += _accumulationRate;
    if (_accumulationNumber >= packetsPerSecond)
    {
        _accumulationNumber = _accumulationNumber % packetsPerSecond;
        _UTCtimestamp += 1;
    }
    //std::cout << _UTCtimestamp << " " << _accumulationNumber << " " << _accumulationRate << std::endl;
}

} // namespace ampp
} // namespace pelican

