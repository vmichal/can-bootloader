
#include <array>

#include "canmanager.hpp"
#include "options.hpp"
#include "bootloader.hpp"

#include <ufsel/assert.hpp>
#include <ufsel/time.hpp>
#include <ufsel/sw_build.hpp>
#include <ufsel/bit.hpp>

#include <CANdb/can_Bootloader.h>

#include <BSP/can.hpp>
#include <BSP/fdcan.hpp>

namespace boot {
	namespace {
		Bootloader_WriteResult toCan(boot::WriteStatus s) {
			switch (s) {
			case boot::WriteStatus::AlreadyWritten:
				return Bootloader_WriteResult_AlreadyWritten;

			case boot::WriteStatus::MemoryProtected:
			case boot::WriteStatus::NotInErasedMemory:
			case boot::WriteStatus::NotInFlash:
			case boot::WriteStatus::NotAligned:
				return Bootloader_WriteResult_InvalidMemory;

			case boot::WriteStatus::Ok:
			case boot::WriteStatus::InsufficientData:
				return Bootloader_WriteResult_Ok;

			case boot::WriteStatus::DiscontinuousWriteAccess:
			case boot::WriteStatus::NotReady:
			case boot::WriteStatus::OtherError:
				return Bootloader_WriteResult_Timeout;
			}
			assert_unreachable();
		}

		inline std::array<uint8_t, 1024 * 4> tx_buf[bsp::can::num_used_buses];

		consteval auto initialize_tx_ringbuffers() {
			std::array<ringbuf_t, bsp::can::num_used_buses> result;

			for (int i = 0; i < bsp::can::num_used_buses; ++i)
				result[i] = ringbuf_t{.data = tx_buf[i].data(), .size = tx_buf[i].size(), .readpos = 0, .writepos = 0};

			return result;
		}

		constinit inline std::array<ringbuf_t, bsp::can::num_used_buses> tx_rb = initialize_tx_ringbuffers();

		inline void process_tx_fifo(bsp::can::bus_info_t const& bus_info) {
			ringbuf_t & rb = tx_rb[bus_info.bus_index];
			auto * const peripheral = bus_info.get_peripheral();

			// As long as there is pending data in the rx buffer, but do not allow more than some specified number
			while (rb.readpos != rb.writepos && bsp::can::has_empty_mailbox(peripheral)) {
				size_t read_pos = rb.readpos;

				// Read message header & data. The message in the rx buffer may not yet be complete,
				// in that case we abort and try again next time.
				CAN_msg_header hdr;
				bool const header_ok = ringbufTryRead(&rb, (uint8_t*) &hdr, sizeof(hdr), &read_pos) == sizeof(hdr);
				assert(header_ok); //well, what else do we have..

				bsp::can::MessageData message {.id = hdr.id, .length = hdr.length};

				bool const message_ok = ringbufTryRead(&rb, (std::uint8_t*)message.data.data(), hdr.length, &read_pos) == hdr.length;
				assert(message_ok); //well, what else do we have..

				rb.readpos = read_pos;

				bsp::can::write_message_for_transmission(bus_info, message);
			}
		}
	} // end anonymous namespace

	void process_all_tx_fifos() {
		for (auto const& bus : bsp::can::bus_info)
			process_tx_fifo(bus);
	}

	int CanManager::get_tx_buffer_size() {
		bsp::can::bus_info_t const& bus = bsp::can::find_bus_info_by_bus(Bootloader_Handshake_get_rx_bus());
		return ringbufSize(&tx_rb[bus.bus_index]);
	}

	void CanManager::SendSoftwareBuild() {

		Bootloader_SoftwareBuild_t msg;
		msg.DirtyRepo = ufsel::git::has_dirty_working_tree();
		msg.CommitSHA = ufsel::git::commit_hash();
		msg.Target = customization::thisUnit;

		send(msg);
	}

	void CanManager::SendExitAck(bool ok) {
		Bootloader_ExitAck_t message;
		message.Target = customization::thisUnit;
		message.Confirmed = ok;

		if (send(message))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedExitAck));
	}

	void CanManager::SendData(std::uint32_t address, std::uint32_t word) {
		Bootloader_Data_t message;

		message.Address = address >> 2;
		message.Word = word;

		if (send(message))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedData));
	}

	void CanManager::SendDataAck(std::uint32_t const address, WriteStatus const status) {
		Bootloader_DataAck_t message;

		message.Address = address >> 2;
		message.Result = toCan(status);

		if (send(message))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedDataAck));
	}

	void CanManager::SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) {
		Bootloader_HandshakeAck_t message;
		message.Register = static_cast<Bootloader_Register>(reg);
		message.Target = customization::thisUnit;
		message.Response = static_cast<Bootloader_HandshakeResponse>(response);
		message.Value = val;

		if (send(message))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedHandshakeAck));
	}

	void CanManager::SendTransactionMagic() {
		SendHandshake(handshake::create(Register::TransactionMagic, Command::None, Bootloader::transactionMagic));
	}

	void CanManager::yieldCommunication() {
		Bootloader_CommunicationYield_t msg;
		msg.Target = customization::thisUnit;

		if (send(msg))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedYieldComm));
	}

	void CanManager::SendPingResponse(bool entering_bl) {
		Bootloader_PingResponse_t msg;

		msg.Target = customization::thisUnit;
		msg.BootloaderPending = entering_bl;
		msg.BootloaderMetadataValid = true;
		msg.BL_SoftwareBuild = ufsel::git::commit_hash();
		msg.BL_DirtyRepo = ufsel::git::has_dirty_working_tree();

		if (send(msg))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedPingResponse));
	}

	void CanManager::SendHandshake(Bootloader_Handshake_t const& msg) {

		if (send(msg))
			set_pending_abort_request(handshake::abort(AbortCode::CanSendFailedHandshake));
		else
			lastSentHandshake_ = msg;
	}

	void CanManager::SendBeacon(Status const BLstate, EntryReason const entryReason) {
		Bootloader_Beacon_t message;
		message.State = static_cast<Bootloader_State>(BLstate);
		message.Target = customization::thisUnit;
		message.FlashSize = Flash::applicationMemorySize / 1024;
		message.EntryReason = static_cast<Bootloader_EntryReason>(entryReason);

		send(message);
	}

	void CanManager::RestartDataFrom(std::uint32_t address) {
		SendHandshake(handshake::create(Register::Command, Command::RestartFromAddress, address));
	}

	void CanManager::update() {
		if (pending_abort_request_.has_value())
			if (send(*pending_abort_request_) == 0)
				pending_abort_request_.reset();
	}

} // end namespace boot

/* Implementation of function required by tx library.
   These functions must use C linkage, because they are used by the code generated from CANdb (which is pure C). */

extern "C" {
uint32_t txGetTimeMillis() {
	return systemStartupTime.TimeElapsed().toMilliseconds();
}

int txHandleCANMessage(uint32_t timestamp, int bus, CAN_ID_t id, const void* data, size_t length) {
	return 0; //all messages are filtered by hardware
}

int txSendCANMessage(int const bus, CAN_ID_t const id, const void* const data, size_t const length) {
	if (bus == bus_ALL) {
		int rc1 = CAN1_used ? txSendCANMessage(bus_CAN1, id, data, length) : 1;
		int rc2 = CAN2_used ? txSendCANMessage(bus_CAN2, id, data, length) : 1;
		return rc1 && rc2;
	}

	using namespace ufsel;
	auto const& bus_info = bsp::can::find_bus_info_by_bus((candb_bus_t)bus);

	const size_t required_size = sizeof(struct CAN_msg_header) + length;

	ringbuf_t & rb = boot::tx_rb[bus_info.bus_index];
	//TODO handle case when peripherals are not receiving acknowledges!
	if (!ringbufCanWrite(&rb, required_size))
		return 1;

	struct CAN_msg_header hdr = {0, bus, id, (uint16_t)length};
	ringbufWriteUnchecked(&rb, (const uint8_t*) &hdr, sizeof(hdr));
	ringbufWriteUnchecked(&rb, (const uint8_t*) data, length);

	return 0;
}
}
