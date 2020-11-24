/*
 * eForce Libs - Tx2
 * 2016, 2018 Martin Cejp
 */

#ifndef TX2_TX_H
#define TX2_TX_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STD_ID(sid_) (sid_)
#define EXT_ID(eid_) (0x80000000L | (eid_))

#define IS_EXT_ID(id_) ((id_) & 0x80000000L)
#define IS_STD_ID(id_) (!IS_EXT_ID(id_))

typedef uint32_t CAN_ID_t;

enum {
	TX_RECV_BUFFER_OVERFLOW = (1<<0),
	TX_SEND_BUFFER_OVERFLOW = (1<<1),
	TX_UNHANDLED_MESSAGE = (1<<2),
	TX_IO_ERROR = (1<<3),
	TX_NOT_IMPLEMENTED = (1<<4),
	TX_BAD_IRQ = (1<<5),
};

int txReceiveCANMessage(int bus, CAN_ID_t id, const void* data, size_t length);
void txProcess(void);
bool txBufferGettingFull();
bool txBufferGettingEmpty();


/* implemented by library user */
uint32_t txGetTimeMillis(void);
int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length);
int txSendCANMessage(int bus, CAN_ID_t id, const void* data, size_t length);

#ifdef __cplusplus
}
#endif

#endif
