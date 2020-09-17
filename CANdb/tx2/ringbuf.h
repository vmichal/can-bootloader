/*
 * eForce Libs - Tx
 * 2016 Martin Cejp
 */

#ifndef EFORCE_LIBS_RINGBUF
#define EFORCE_LIBS_RINGBUF

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	uint8_t* data;
	size_t size;
	size_t readpos, writepos;
} ringbuf_t;

/* returns boolean */
int ringbufCanRead(volatile ringbuf_t* rb, size_t length);
int ringbufCanWrite(volatile ringbuf_t* rb, size_t length);

/* returns number of bytes read */
size_t ringbufTryRead(volatile ringbuf_t* rb, uint8_t* data, size_t length, size_t* readpos_inout);

/* returns boolean */
int ringbufWrite(volatile ringbuf_t* rb, const uint8_t* data, size_t length);

void ringbufWriteUnchecked(volatile ringbuf_t* rb, const uint8_t* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
