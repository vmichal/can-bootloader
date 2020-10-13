
#include <tx2/can.h>
#include <tx2/ringbuf.h>
#include "can_Bootloader.h"

#ifndef TX_RECV_BUFFER_SIZE
enum { TX_RECV_BUFFER_SIZE = 256 };
#endif

static uint8_t recv_buf[TX_RECV_BUFFER_SIZE];
static ringbuf_t recv_rb;

static volatile int flags;

extern void candbHandleMessage(int bus, uint32_t timestamp, CAN_ID_t id, const uint8_t* payload, size_t payload_length);

static void set_flag(int flag) {
	// TODO: must be atomic
	flags |= flag;
}

void txInit(void) {
	flags = 0;

	recv_rb.data = recv_buf;
	recv_rb.size = TX_RECV_BUFFER_SIZE;
	recv_rb.readpos = 0;
	recv_rb.writepos = 0;
}

int txReceiveCANMessage(int bus, CAN_ID_t id, const void* data, size_t length) {
	const size_t required_size = sizeof(struct CAN_msg_header) + length;

	if (!ringbufCanWrite(&recv_rb, required_size)) {
		set_flag(TX_RECV_BUFFER_OVERFLOW);
		return -TX_RECV_BUFFER_OVERFLOW;
	}

	struct CAN_msg_header hdr = {txGetTimeMillis(), bus, id, length};
	ringbufWriteUnchecked(&recv_rb, (const uint8_t*) &hdr, sizeof(hdr));
	ringbufWriteUnchecked(&recv_rb, (const uint8_t*) data, length);

	return required_size;
}

void txProcess(void) {
	struct CAN_msg_header hdr;
	uint8_t msg_data[CAN_MESSAGE_SIZE];

	// As long as there is pending data in the rx buffer
	while (recv_rb.readpos != recv_rb.writepos) {
		size_t read_pos = recv_rb.readpos;

		// Read message header & data. The message in the rx buffer may not yet be complete,
		// in that case we abort and try again next time.
		if (ringbufTryRead(&recv_rb, (uint8_t*) &hdr, sizeof(hdr), &read_pos) == sizeof(hdr)
				&& ringbufTryRead(&recv_rb, msg_data, hdr.length, &read_pos) == hdr.length) {
			recv_rb.readpos = read_pos;

			// Call the user filter
			if (txHandleCANMessage(hdr.timestamp, hdr.bus, hdr.id, msg_data, hdr.length) < 0)
				continue;

#if TX_WITH_CANDB
			// Call the CANdb dispatcher, if enabled.
			candbHandleMessage(hdr.timestamp, hdr.bus, hdr.id, msg_data, hdr.length);
#endif
		}
	}
}

void canInitMsgStatus(CAN_msg_status_t* status, int timeout) {
	status->flags = 0;
	status->timeout = timeout;
	status->timestamp = -1;
	status->on_receive = NULL;
	status->bus = bus_UNDEFINED;
}

void canUpdateMsgStatusOnReceive(CAN_msg_status_t* status, uint32_t timestamp, int bus) {
	if (status->flags & CAN_MSG_PENDING)
		status->flags |= CAN_MSG_MISSED;

	status->flags |= CAN_MSG_RECEIVED | CAN_MSG_PENDING;
	status->timestamp = timestamp;
	status->bus = bus;
}
