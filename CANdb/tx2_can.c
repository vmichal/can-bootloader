
#include <tx2/can.h>
#include <tx2/ringbuf.h>

#include <stdbool.h>

#ifndef TX_RECV_BUFFER_SIZE
enum { TX_RECV_BUFFER_SIZE = 1024 };
#endif
#ifndef MAX_MESSAGES_IN_A_ROW
enum { MAX_MESSAGES_IN_A_ROW = 64 };
#endif

static uint8_t recv_buf[TX_RECV_BUFFER_SIZE];
static ringbuf_t recv_rb = {.data = recv_buf, .size = TX_RECV_BUFFER_SIZE, .readpos = 0, .writepos = 0};

static volatile int flags = 0;

extern void candbHandleMessage(int bus, uint32_t timestamp, CAN_ID_t id, const uint8_t* payload, size_t payload_length);
extern void HardFault_Handler();
static void set_flag(int flag) {
	// TODO: must be atomic
	flags |= flag;
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

bool txBufferGettingFull() {
	//TODO probably too big margin. Decrease to six messages? BL filters all non-BL messages in HW
	//therefore all data in the recv_rb should be expected messages.
	return !ringbufCanWrite(&recv_rb, TX_RECV_BUFFER_SIZE / 2);
}
bool txBufferGettingEmpty() {
	return ringbufCanWrite(&recv_rb, 3*TX_RECV_BUFFER_SIZE / 4);
}

void txProcess(void) {
	struct CAN_msg_header hdr;
	uint8_t msg_data[CAN_MESSAGE_SIZE];

	// As long as there is pending data in the rx buffer, but do not allow more than some specified number
	for (int i = 0;i < MAX_MESSAGES_IN_A_ROW;++i) {
		if (recv_rb.readpos == recv_rb.writepos)
			return;
		size_t read_pos = recv_rb.readpos;

		// Read message header & data. The message in the rx buffer may not yet be complete,
		// in that case we abort and try again next time.
		bool const header_ok = ringbufTryRead(&recv_rb, (uint8_t*) &hdr, sizeof(hdr), &read_pos) == sizeof(hdr);
		bool const message_ok = ringbufTryRead(&recv_rb, msg_data, hdr.length, &read_pos) == hdr.length;


		if (!header_ok || !message_ok)
			HardFault_Handler(); //well, what else do we have..

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

void canInitMsgStatus(CAN_msg_status_t* status, int default_bus, int timeout) {
	status->flags = 0;
	status->timeout = timeout;
	status->timestamp = -1;
	status->on_receive = NULL;
	status->rx_bus = default_bus;
}

void canUpdateMsgStatusOnReceive(CAN_msg_status_t* status, int bus_origin, uint32_t timestamp) {
	if (status->flags & CAN_MSG_PENDING)
		status->flags |= CAN_MSG_MISSED;

	status->flags |= CAN_MSG_RECEIVED | CAN_MSG_PENDING;
	status->timestamp = timestamp;
	status->rx_bus = bus_origin;
}
