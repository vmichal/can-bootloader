
#include <array>

#include "canmanager.hpp"
#include "options.hpp"
#include "main.hpp"
#include "bootloader.hpp"

#include <BSP/timer.hpp>
#include <library/utility.hpp>
#include <library/assert.hpp>
#include <ufsel/sw_build.hpp>
#include <ufsel/bit.hpp>

#include <CANdb/can_Bootloader.h>

namespace {
	Bootloader_WriteResult toCan(boot::WriteStatus s) {
		switch (s) {
		case boot::WriteStatus::AlreadyWritten:
			return Bootloader_WriteResult_AlreadyWritten;
		
		case boot::WriteStatus::MemoryProtected:
		case boot::WriteStatus::NotInErasedMemory:
		case boot::WriteStatus::NotInFlash:
			return Bootloader_WriteResult_InvalidMemory;

		case boot::WriteStatus::Ok:
			return Bootloader_WriteResult_Ok;

		case boot::WriteStatus::Timeout:
		case boot::WriteStatus::NotReady:
			return Bootloader_WriteResult_Timeout;
		}
		assert_unreachable();
	}

}

namespace boot {

	bool CanManager::hasEmptyMailbox(int const periph) {
		assert(periph == bus_CAN1 || periph == bus_CAN2);
		if (periph == bus_CAN1)
			return ufsel::bit::get(CAN1->TSR, CAN_TSR_TME0, CAN_TSR_TME1, CAN_TSR_TME2);
		else // CAN == 2
			return ufsel::bit::get(CAN2->TSR, CAN_TSR_TME0, CAN_TSR_TME1, CAN_TSR_TME2);
	}

	void CanManager::SendSoftwareBuild() const {

		Bootloader_SoftwareBuild_t msg;
		msg.DirtyRepo = ufsel::git::has_dirty_working_tree();
		msg.CommitSHA = ufsel::git::commit_hash();
		msg.Target = Bootloader::thisUnit;

		send(msg);
	}

	void CanManager::SendExitAck(bool ok) const {
		Bootloader_ExitAck_t message;
		message.Target = Bootloader::thisUnit;
		message.Confirmed = ok;

		for (; !CanManager::hasEmptyMailbox(Bootloader_ExitReq_status.bus););
		send(message);
	}


	void CanManager::SendDataAck(std::uint32_t const address, WriteStatus const status) const {
		Bootloader_DataAck_t message;

		message.Address = address >> 2;
		message.Result = toCan(status);

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox(Bootloader_Data_status.bus););
		send(message);
	}

	void CanManager::SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const {
		Bootloader_HandshakeAck_t message;
		message.Register = static_cast<Bootloader_Register>(reg);
		message.Response = static_cast<Bootloader_HandshakeResponse>(response);
		message.Value = val;

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox(Bootloader_Handshake_status.bus););
		send(message);
	}

	void CanManager::SendTransactionMagic() const {
		Bootloader_Handshake_t const msg = handshake::get(Register::TransactionMagic, Command::None, Bootloader::transactionMagic);

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox(Bootloader_Handshake_status.bus););
		send(msg);
	}

	void CanManager::yieldCommunication() const {
		Bootloader_CommunicationYield_t msg;
		msg.Target = Bootloader::thisUnit;

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox(Bootloader_CommunicationYield_status.bus););
		send(msg);
	}

	void CanManager::SendHandshake(Bootloader_Handshake_t const& msg) {

		for (; !CanManager::hasEmptyMailbox(Bootloader_Handshake_status.bus);); //TODO check correct bus
		if (send(msg) == 0) //if message sent successfully, copy new message to storage
			lastSentHandshake_ = msg;
	}

	void CanManager::SendHandshake(Register reg, Command command, std::uint32_t value) {
		return SendHandshake(handshake::get(reg, command, value));
	}


	void CanManager::SendBeacon(Status const BLstate, EntryReason const entryReason) const {
		Bootloader_Beacon_t message;
		message.State = static_cast<Bootloader_State>(BLstate);
		message.Target = Bootloader::thisUnit;
		message.FlashSize = Flash::availableMemory / 1024;
		message.EntryReason = static_cast<Bootloader_EntryReason>(entryReason);

		send(message);
	}

}
