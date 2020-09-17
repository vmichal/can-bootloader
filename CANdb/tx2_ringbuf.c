
#include <tx2/ringbuf.h>

int ringbufCanRead(volatile ringbuf_t* rb, size_t length) {
	const size_t readpos = rb->readpos;
	const size_t writepos = rb->writepos;

	if (readpos <= writepos)
		return readpos + length <= writepos;
	else
		return readpos + length <= rb->size + writepos;
}

int ringbufCanWrite(volatile ringbuf_t* rb, size_t length) {
	const size_t readpos = rb->readpos;
	const size_t writepos = rb->writepos;

	if (writepos < readpos)
		return writepos + length < readpos;
	else
		return writepos + length < rb->size + readpos;
}

size_t ringbufTryRead(volatile ringbuf_t* rb, uint8_t* data, size_t length, size_t* readpos_inout) {
	const uint32_t mask = rb->size - 1;
	size_t readpos = *readpos_inout;
	size_t read = 0;

	while (readpos != rb->writepos && read < length) {
		*data++ = rb->data[readpos];
		readpos = (readpos + 1) & mask;
		read++;
	}

	*readpos_inout = readpos;
	return read;
}

int ringbufWrite(volatile ringbuf_t* rb, const uint8_t* data, size_t length) {
	if (!ringbufCanWrite(rb, length))
		return 0;

	ringbufWriteUnchecked(rb, data, length);
	return 1;
}

void ringbufWriteUnchecked(volatile ringbuf_t* rb, const uint8_t* data, size_t length) {
	const uint32_t mask = rb->size - 1;
	size_t writepos = rb->writepos;

	while (length--) {
		rb->data[writepos] = *data++;
		writepos = (writepos + 1) & mask;
	}

	rb->writepos = writepos;
}
