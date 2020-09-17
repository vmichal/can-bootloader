
#include <array>

#include "canmanager.hpp"
#include "options.hpp"
#include "main.hpp"

#include <BSP/timer.hpp>
#include <library/utility.hpp>
#include <library/assert.hpp>
#include <ufsel/sw_build.hpp>
#include <ufsel/bit.hpp>

#include <CANdb/can_Bootloader.h>

namespace {
	Bootloader_WriteResult toCan(boot::WriteStatus s) {
		switch (s) {
		case boot::WriteStatus::AlreadyWritten: return Bootloader_WriteResult_AlreadyWritten;
		case boot::WriteStatus::MemoryProtected:
		case boot::WriteStatus::NotInErasedMemory:
		case boot::WriteStatus::NotInFlash:
			return Bootloader_WriteResult_InvalidMemory;

		case boot::WriteStatus::Ok: return Bootloader_WriteResult_Ok;

		case boot::WriteStatus::Timeout: return Bootloader_WriteResult_Timeout;
		}
		assert_unreachable();
	}

	Bootloader_State toCan(boot::Bootloader::Status s) {
		using namespace boot;
		switch (s) {
		case Bootloader::Status::Ready: return Bootloader_State_Ready;

		}
		assert_unreachable();
	}
}

namespace boot {

	template<int CAN>
	bool CanManager::hasEmptyMailbox() {
		static_assert(CAN == 1 || CAN == 2, "There are not that many CAN buses in the car.");
		if constexpr (CAN == 1)
			return ufsel::bit::get(CAN1->TSR, CAN_TSR_TME0, CAN_TSR_TME1, CAN_TSR_TME2);
		else // CAN == 2
			return ufsel::bit::get(CAN2->TSR, CAN_TSR_TME0, CAN_TSR_TME1, CAN_TSR_TME2);
	}

	void CanManager::SendSerialOutput() const {
		if (!CanManager::hasEmptyMailbox<2>())
			return; //If there is no empty mailbox, delay sending in order not to discard data

		static int seq = 0;

		Bootloader_SerialOutput_t packet;
		packet.SEQ = ++seq;
		std::fill(std::begin(packet.Payload), std::end(packet.Payload), '\0');

		bool empty, truncated;

		packet.CouldNotRead = ser0.Read(packet.Payload, sizeof(packet.Payload), &empty, &truncated) == 0;
		packet.Completed = empty;
		packet.Channel = 0;
		packet.Truncated = truncated;
		send(packet);
	}

	void CanManager::SendSoftwareBuild() const {

		Bootloader_SoftwareBuild_t msg;
		msg.DirtyRepo = ufsel::git::has_dirty_working_tree();
		msg.CommitSHA = ufsel::git::commit_hash();

		send(msg);
	}

	void CanManager::SendDataAck(std::uint32_t const address, WriteStatus const status) const {
		Bootloader_DataAck_t message;

		message.Address = address >> 2;
		message.Result = toCan(status);

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox<2>(););
		send(message);
	}

	void CanManager::SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const {
		Bootloader_HandshakeAck_t message;
		message.Register = regToReg(reg);
		message.Response = static_cast<Bootloader_HandshakeResponse>(response);
		message.Value = val;

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox<2>(););
		send(message);
	}

	void CanManager::SendBootloaderBeacon() const {
		Bootloader_BootloaderBeacon_t message;
		message.State = toCan(bootloader_.status());
		message.Unit = Bootloader::thisUnit;
		message.FlashSize = Flash::availableMemory / 1024;

		send(message);
	}

	void CanManager::FlushSerialOutput() const {

		for (; ser0.ringbuf.readpos != ser0.ringbuf.writepos;)
			SendSerialOutput();
	}



	void CanManager::Update() {

		if (need_to_send<Bootloader_SoftwareBuild_t>()) {
			SendSoftwareBuild();
		}

		if (need_to_send<Bootloader_SerialOutput_t>())
			SendSerialOutput();

		if (need_to_send<Bootloader_BootloaderBeacon_t>())
			SendBootloaderBeacon();

	}


}
