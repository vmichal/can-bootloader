
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

	Bootloader_State toCan(boot::Bootloader::Status s) {
		using namespace boot;
		switch (s) {

		case Bootloader::Status::Ready: return Bootloader_State_Ready;
		case Bootloader::Status::TransactionStarted: return Bootloader_State_TransactionStarted;
		case Bootloader::Status::ReceivedFirmwareSize: return Bootloader_State_ReceivedFirmwareSize;
		case Bootloader::Status::ReceivedNumPagestoErase: return Bootloader_State_ReceivedNumPagestoErase;
		case Bootloader::Status::ErasingPages: return Bootloader_State_ErasingPages;
		case Bootloader::Status::ReceivedEntryPoint: return Bootloader_State_ReceivedEntryPoint;
		case Bootloader::Status::ReceivedInterruptVector: return Bootloader_State_ReceivedInterruptVector;
		case Bootloader::Status::ReceivingData: return Bootloader_State_ReceivingData; 
		case Bootloader::Status::ReceivedChecksum: return Bootloader_State_ReceivedChecksum;
		case Bootloader::Status::Error: return Bootloader_State_Error;
		}
		assert_unreachable();
	}

	Bootloader_HandshakeResponse toCan(boot::HandshakeResponse response) {
		using namespace boot;

		switch (response) {
		case HandshakeResponse::Ok:
			return Bootloader_HandshakeResponse_OK;

		case HandshakeResponse::PageAddressNotAligned:
			return Bootloader_HandshakeResponse_PageAddressNotAligned;
		case HandshakeResponse::AddressNotInFlash:
			return Bootloader_HandshakeResponse_AddressNotInFlash;
		case HandshakeResponse::PageProtected:
			return Bootloader_HandshakeResponse_PageProtected;
		case HandshakeResponse::ErasedPageCountMismatch:
			return Bootloader_HandshakeResponse_ErasedPageCountMismatch;
		case HandshakeResponse::InvalidTransactionMagic:
			return Bootloader_HandshakeResponse_InvalidTransactionMagic;
		case HandshakeResponse::HandshakeSequenceError:
			return Bootloader_HandshakeResponse_HandshakeSequenceError;
		case HandshakeResponse::InterruptVectorNotAligned:
			return Bootloader_HandshakeResponse_InterruptVectorNotAligned;
		case HandshakeResponse::BinaryTooBig:
			return Bootloader_HandshakeResponse_BinaryTooBig;
		case HandshakeResponse::PageAlreadyErased:
			return Bootloader_HandshakeResponse_PageAlreadyErased;
		case HandshakeResponse::NotEnoughPages:
			return Bootloader_HandshakeResponse_NotEnoughPages;
		case HandshakeResponse::NumWrittenBytesMismatch:
			return Bootloader_HandshakeResponse_NumWrittenBytesMismatch;
		case HandshakeResponse::EntryPointAddressMismatch:
			return Bootloader_HandshakeResponse_EntryPointAddressMismatch;
		case HandshakeResponse::ChecksumMismatch:
			return Bootloader_HandshakeResponse_ChecksumMismatch;
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

	void CanManager::SendSoftwareBuild() const {

		Bootloader_SoftwareBuild_t msg;
		msg.DirtyRepo = ufsel::git::has_dirty_working_tree();
		msg.CommitSHA = ufsel::git::commit_hash();

		send(msg);
	}

	void CanManager::SendExitAck(bool ok) const {
		Bootloader_ExitAck_t message;
		message.Target = Bootloader::thisUnit;
		message.Confirmed = ok;

		for (; !CanManager::hasEmptyMailbox<1>(););
		send(message);
	}


	void CanManager::SendDataAck(std::uint32_t const address, WriteStatus const status) const {
		Bootloader_DataAck_t message;

		message.Address = address >> 2;
		message.Result = toCan(status);

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox<1>(););
		send(message);
	}

	void CanManager::SendHandshakeAck(Register reg, HandshakeResponse response, std::uint32_t val) const {
		Bootloader_HandshakeAck_t message;
		message.Register = regToReg(reg);
		message.Response = static_cast<Bootloader_HandshakeResponse>(response);
		message.Value = val;

		//prevent the data form getting lost if all mailboxes are full
		for (; !CanManager::hasEmptyMailbox<1>(););
		send(message);
	}

	void CanManager::SendBeacon() const {
		Bootloader_Beacon_t message;
		message.State = toCan(bootloader_.status());
		message.Target = Bootloader::thisUnit;
		message.FlashSize = Flash::availableMemory / 1024;
		message.EntryReason = static_cast<Bootloader_EntryReason>(bootloader_.entryReason()); //TODO should probably be replaced by a conversion functions

		send(message);
	}

	void CanManager::Update() {

		if (need_to_send<Bootloader_SoftwareBuild_t>())
			SendSoftwareBuild();

		if (need_to_send<Bootloader_Beacon_t>())
			SendBeacon();

	}


}
