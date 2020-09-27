#include "ringstream.hpp"
#include <cstdint>

RingStream::RingStream() {
    static_assert((sizeof(buffer) & (sizeof(buffer) - 1)) == 0, "Buffer size must be power of 2!");

    ringbuf.data = buffer;
    ringbuf.size = sizeof(buffer);
    ringbuf.readpos = 0;
    ringbuf.writepos = 0;
}

size_t RingStream::Read(uint8_t* data_out, size_t length, bool* empty_out, bool* truncated_out) {
    size_t readpos = ringbuf.readpos;
    size_t read = ringbufTryRead(&ringbuf, data_out, length, &readpos);
    ringbuf.readpos = readpos;

    *empty_out = (ringbufCanRead(&ringbuf, 1) == 0);
    *truncated_out = truncated;
    truncated = false;

    return read;
}



void RingStream::Write(std::uint8_t const* data, size_t length) {
    // TODO: allow at least partial write!
    if (!ringbufWrite(&ringbuf, data, length)) {
        truncated = true;
    }
}
