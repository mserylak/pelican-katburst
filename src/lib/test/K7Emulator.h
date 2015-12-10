#ifndef K7EMULATOR_H
#define K7EMULATOR_H

#include "pelican/emulator/AbstractUdpEmulator.h"
#include <QtCore/QByteArray>
#include <time.h>
#include <stdint.h>

namespace pelican {

class ConfigNode;

namespace ampp {

class K7Emulator : public AbstractUdpEmulator
{
    public:
        // Constructs a new UDP packet emulator.
        K7Emulator(const ConfigNode& configNode);

        // Destroys the UDP packet emulator.
        ~K7Emulator() {}

        // Creates a UDP packet.
        void getPacketData(char*& ptr, unsigned long& size);

        // Returns the interval between packets in microseconds.
        unsigned long interval()
        {
            return _interval;
        }

    private:
        uint64_t _UTCtimestamp; // (8-Byte) UTC timestamp in Unix time (seconds from 00:00:00 UTC, Thursday, 1 January 1970).
        uint32_t _accumulationNumber; // (4-Byte) Number of first spectrum since beginning of second denoted by _UTCtimestamp.
        uint32_t _accumulationRate; // (4-Byte) Number of spectra added together in beamformer.
        unsigned long long _totalSamples; // The total number of samples sent.
        unsigned long _channels; // The number of (frequency) channels in the packet.
        unsigned long _interval; // The interval between packets in microsec.
        QByteArray _packet; // The data packet.
};

} // namespace ampp
} // namespace pelican

#endif // K7EMULATOR_H

