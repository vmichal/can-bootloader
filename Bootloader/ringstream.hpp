#ifndef AMS_RINGSTREAM_HPP
#define AMS_RINGSTREAM_HPP


#include <tx2/ringbuf.h>
#include <cstdint>

inline constexpr size_t kRingStreamBufferSize = 1024;

namespace boot {
    class CanManager;
}

class RingStream {
    friend class CanProtocol;
public:
    RingStream();
    size_t Read(uint8_t* data_out, size_t length, bool* empty_out, bool* truncated_out);
    void Write(std::uint8_t const* data, size_t length);

private:
    bool truncated = false;
    ringbuf_t ringbuf;

    uint8_t buffer[kRingStreamBufferSize];

    friend class boot::CanManager;
};

#endif