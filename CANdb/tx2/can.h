/*
 * eForce Libs - Tx2
 * 2016, 2018 Martin Cejp
 */

#ifndef TX2_CAN_H
#define TX2_CAN_H

#include "tx.h"

#ifdef __cplusplus
extern "C" {
#endif

enum { CAN_MESSAGE_SIZE = 8 };

struct CAN_msg_header {
	uint32_t timestamp;
	int bus;
	CAN_ID_t id;
	uint16_t length;
};

typedef struct {
	uint32_t flags;
	int32_t timeout;
	int32_t timestamp;
	int bus;

	void (*on_receive)(void);
} CAN_msg_status_t;

enum {
	CAN_MSG_PENDING = 1,
	CAN_MSG_RECEIVED = 2,
	CAN_MSG_MISSED = 4,
};

void canInitMsgStatus(CAN_msg_status_t*, int timeout);
void canUpdateMsgStatusOnReceive(CAN_msg_status_t*, uint32_t timestamp, int bus);

#ifdef __cplusplus
}
#endif

#endif
