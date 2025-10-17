
#include <tx2/can.h>
#include <tx2/ringbuf.h>

#include <stdbool.h>

#ifndef TX_RECV_BUFFER_SIZE
enum { TX_RECV_BUFFER_SIZE = 1024 };
#endif
#ifndef TX_MAX_MSGS_PROCESSED_IN_A_ROW
enum { TX_MAX_MSGS_PROCESSED_IN_A_ROW = 16 };
#endif

#ifndef TX_RECV_BUFFER_FULL_THRESHOLD
enum { TX_RECV_BUFFER_FULL_THRESHOLD = TX_RECV_BUFFER_SIZE / 4 };
#endif

#ifndef TX_RECV_BUFFER_EMPTY_THRESHOLD
enum { TX_RECV_BUFFER_EMPTY_THRESHOLD = 3 * TX_RECV_BUFFER_SIZE / 4 };
#endif

static uint8_t recv_buf[TX_RECV_BUFFER_SIZE];
static ringbuf_t recv_rb = {.data = recv_buf, .size = TX_RECV_BUFFER_SIZE, .readpos = 0, .writepos = 0};

typedef struct  {
	txError error_flags;
	int bus;
	CAN_ID_t id;
	size_t length;
} tx_irq_error_t;
tx_irq_error_t volatile tx_irq_error = {TX_OK, 0, 0, 0};

extern void candbHandleMessage(int bus, uint32_t timestamp, CAN_ID_t id, const uint8_t* payload, size_t payload_length);

int txReceiveCANMessage(int bus, CAN_ID_t id, const void* data, size_t length) {
	const size_t required_size = sizeof(struct CAN_msg_header) + length;

	if (!ringbufCanWrite(&recv_rb, required_size)) {
		tx_irq_error.error_flags = TX_RECV_BUFFER_OVERFLOW;
		tx_irq_error.bus = bus;
		tx_irq_error.id = id;
		tx_irq_error.length = length;
		return -TX_RECV_BUFFER_OVERFLOW;
	}

	struct CAN_msg_header hdr = {txGetTimeMillis(), bus, id, length};
	ringbufWriteUnchecked(&recv_rb, (const uint8_t*) &hdr, sizeof(hdr));
	ringbufWriteUnchecked(&recv_rb, (const uint8_t*) data, length);

	return required_size;
}

bool txBufferGettingFull() {
	return !ringbufCanWrite(&recv_rb, TX_RECV_BUFFER_FULL_THRESHOLD);
}
bool txBufferGettingEmpty() {
	return ringbufCanWrite(&recv_rb, TX_RECV_BUFFER_EMPTY_THRESHOLD);
}

void txProcess(void) {
	struct CAN_msg_header hdr;
	uint8_t msg_data[CAN_MESSAGE_SIZE];


	// As long as there is pending data in the rx buffer, but do not allow more than some specified number
	for (int i = 0;i < TX_MAX_MSGS_PROCESSED_IN_A_ROW;++i) {
		if (tx_irq_error.error_flags != TX_OK) {
			txHandleError(tx_irq_error.error_flags, tx_irq_error.bus, tx_irq_error.id, NULL, tx_irq_error.length);
			tx_irq_error.error_flags = TX_OK;
		}

		if (recv_rb.readpos == recv_rb.writepos)
			return;
		size_t read_pos = recv_rb.readpos;

		// Read message header & data. The message in the rx buffer may not yet be complete,
		// in that case we abort and try again next time.
		bool const header_ok = ringbufTryRead(&recv_rb, (uint8_t*) &hdr, sizeof(hdr), &read_pos) == sizeof(hdr);

		if (!header_ok)
			txHandleError(TX_RECV_BUFFER_CORRUPTED, 0, 0, NULL, 0); //well, what else do we have..

		bool const message_ok = ringbufTryRead(&recv_rb, msg_data, hdr.length, &read_pos) == hdr.length;

		if (!message_ok)
			txHandleError(TX_RECV_BUFFER_CORRUPTED, hdr.bus, hdr.id, NULL, hdr.length); //well, what else do we have..

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
	status->timestamp = 0;
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
