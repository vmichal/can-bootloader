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
	int rx_bus;

	void (*on_receive)(void);
} CAN_msg_status_t;

enum {
	CAN_MSG_PENDING = 1,
	CAN_MSG_RECEIVED = 2,
	CAN_MSG_MISSED = 4,
};

void canInitMsgStatus(CAN_msg_status_t*, int default_bus, int timeout);
void canUpdateMsgStatusOnReceive(CAN_msg_status_t*, int bus_origin, uint32_t timestamp);

#define FIELD_IDENTIFIER(unit_msg_field, iden) unit_msg_field ## _ ## iden

#define CanConversionConstants(unit_msg_field) FIELD_IDENTIFIER(unit_msg_field, OFFSET), \
	FIELD_IDENTIFIER(unit_msg_field, FACTOR), FIELD_IDENTIFIER(unit_msg_field, MIN), FIELD_IDENTIFIER(unit_msg_field, MAX)

inline float helper_ConvertValueToCAN(float value, float offset, float factor, float min, float max) {

	auto const clamped = value < min ? min : value > max ? max : value;
	auto const normalized = clamped - offset;
	return normalized / factor;
}

inline float helper_ConvertValueFromCAN(float value, float offset, float factor) {
	return value * factor + offset;
}

#define ConvertValueToCAN(unit_msg_field, value) helper_ConvertValueToCAN(value, CanConversionConstants(unit_msg_field))
#define ConvertValueFromCAN(unit_msg_field, value) helper_ConvertValueFromCAN(value, FIELD_IDENTIFIER(unit_msg_field, OFFSET), FIELD_IDENTIFIER(unit_msg_field, FACTOR))


#ifdef __cplusplus
}
#endif

#endif
