
#include <array>

#include "canmanager.hpp"
#include "options.hpp"
#include "bootloader.hpp"

#include <BSP/timer.hpp>
#include <ufsel/assert.hpp>
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
		case boot::WriteStatus::InsufficientData:
			return Bootloader_WriteResult_Ok;

		case boot::WriteStatus::DiscontinuousWriteAccess:
		case boot::WriteStatus::NotReady:
			return Bootloader_WriteResult_Timeout;
		}
		assert_unreachable();
	}

}

namespace boot {

	void CanManager::SendSoftwareBuild() const {

		Bootloader_SoftwareBuild_t msg;
		msg.DirtyRepo = ufsel::git::has_dirty_working_tree();
		msg.CommitSHA = ufsel::git::commit_hash();
		msg.Target = customization::thisUnit;

		send(msg);
	}

	void CanManager::SendExitAck(bool ok) const {
		Bootloader_ExitAck_t message;
		message.Target = customization::thisUnit;
		message.Confirmed = ok;

		for (;send(message););
	}


	void CanManager::SendDataAck(std::uint32_t const address, WriteStatus const status) const {
		Bootloader_DataAck_t message;

		message.Address = address >> 2;
		message.Result = toCan(status);

		for (;send(message););
	}

	void CanManager::SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const {
		Bootloader_HandshakeAck_t message;
		message.Register = static_cast<Bootloader_Register>(reg);
		message.Target = customization::thisUnit;
		message.Response = static_cast<Bootloader_HandshakeResponse>(response);
		message.Value = val;

		for (;send(message););
	}

	void CanManager::SendTransactionMagic() const {
		Bootloader_Handshake_t const msg = handshake::get(Register::TransactionMagic, Command::None, Bootloader::transactionMagic);

		for (;send(msg););
	}

	void CanManager::yieldCommunication() const {
		Bootloader_CommunicationYield_t msg;
		msg.Target = customization::thisUnit;

		for (;send(msg););
	}

	void CanManager::sendPingResponse(bool entering_bl) const {
		Bootloader_PingResponse_t msg;

		msg.Target = customization::thisUnit;
		msg.Confirmed = entering_bl;

		for (;send(msg););
	}

	void CanManager::SendHandshake(Bootloader_Handshake_t const& msg) {

		for (;send(msg););
		lastSentHandshake_ = msg;
	}

	void CanManager::SendHandshake(Register reg, Command command, std::uint32_t value) {
		return SendHandshake(handshake::get(reg, command, value));
	}


	void CanManager::SendBeacon(Status const BLstate, EntryReason const entryReason) const {
		Bootloader_Beacon_t message;
		message.State = static_cast<Bootloader_State>(BLstate);
		message.Target = customization::thisUnit;
		message.FlashSize = Flash::applicationMemorySize / 1024;
		message.EntryReason = static_cast<Bootloader_EntryReason>(entryReason);

		send(message);
	}

	void CanManager::RestartDataFrom(std::uint32_t address) {
		return SendHandshake(handshake::get(Register::Command, Command::RestartFromAddress, address));
	}

}
