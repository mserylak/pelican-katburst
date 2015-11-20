#ifndef __K7PACKET_H__
#define __K7PACKET_H__

#include <boost/cstdint.hpp>
#include <time.h>

namespace pelican {
namespace ampp {

// Size of payload data. All data is in Little Endian format!
#define PAYLOAD_SIZE 8192 // In bytes, of course :)

struct K7Packet {
    struct Header {
        time_t UTCtimestamp;         // 64 bit timestamp (Unix time)
        uint32_t accumulationNumber; // 32 bit
        uint32_t accumulationRate;   // 32 bit
    } header;

    char data[PAYLOAD_SIZE];
};

}   // namespace ampp
}   // namespace pelican

#endif // __K7PACKET_H__

