/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */
#include <algorithm>

#include "bootloader.hpp"
#include "flash.hpp"
#include "stm32f10x.h"

#include <library/assert.hpp>
namespace boot {

	WriteStatus Bootloader::checkAddressBeforeWrite(std::uint32_t const address) {
		if (status_ != Status::ReceivingData)
			return WriteStatus::NotReady;

		switch (Flash::addressOrigin(address)) {

		case AddressSpace::AvailableFlash:
			break;

		case AddressSpace::BootloaderFlash: case AddressSpace::JumpTable:
			return WriteStatus::MemoryProtected;

		case AddressSpace::RAM: case AddressSpace::Unknown:
			return WriteStatus::NotInFlash;
		}

		assert(Flash::isAvailableAddress(address));

		std::uint32_t const pageAligned = Flash::makePageAligned(address);

		if (!std::find(std::begin(erased_pages_), std::end(erased_pages_), pageAligned))
			return WriteStatus::NotInErasedMemory;

		return WriteStatus::Ok; //Everything seems ok, try to write
	}

	WriteStatus Bootloader::write(std::uint32_t address, std::uint16_t half_word) {
		if (WriteStatus const ret = checkAddressBeforeWrite(address); ret != WriteStatus::Ok)
			return ret;

		firmware_.checksum_ += half_word;
		firmware_.writtenBytes_ += InformationSize::fromBytes(sizeof(half_word));
		return Flash::Write(address, half_word);
	}

	WriteStatus Bootloader::write(std::uint32_t address, std::uint32_t word) {
		std::uint16_t const lower_half = word, upper_half = word >> 16;
		if (WriteStatus const ret = write(address, lower_half); ret != WriteStatus::Ok)
			return ret;

		return write(address + 2, upper_half);
	}

	HandshakeResponse Bootloader::tryErasePage(std::uint32_t address) {

		if (!ufsel::bit::all_cleared(address, ufsel::bit::bitmask_of_width(11))) //pages are 2KiB wide
			return HandshakeResponse::PageAddressNotAligned;

		AddressSpace const space = Flash::addressOrigin(address);
		switch (space) {
		case AddressSpace::AvailableFlash:
			break;
		case AddressSpace::BootloaderFlash:
		case AddressSpace::JumpTable:
			return HandshakeResponse::PageProtected;
		case AddressSpace::RAM:
		case AddressSpace::Unknown:
			return HandshakeResponse::AddressNotInFlash;
		}

		
		auto const beg = begin(erased_pages_);
		auto const end = std::next(beg, erased_pages_count_);
		if (std::find(beg, end, address) != end)
			return HandshakeResponse::PageAlreadyErased;

		erased_pages_[erased_pages_count_++] = address;
		debug_printf(("Erasing page at %0lx\r\n", address));
		Flash::ErasePage(address);

		return HandshakeResponse::Ok;
	}

	void Bootloader::finishTransaction() const {
		assert(firmware_.expectedBytes_ == firmware_.writtenBytes_);
		assert(Flash::addressOrigin(firmware_.interruptVector_) == AddressSpace::AvailableFlash);
		assert(Flash::addressOrigin(firmware_.entryPoint_) == AddressSpace::AvailableFlash);
		assert(ufsel::bit::all_cleared(firmware_.interruptVector_, ufsel::bit::bitmask_of_width(9))); //TODO replace by named constant

		//The page must have been cleared before
		auto const empty = std::numeric_limits<decltype(jumpTable.magic1_)>::max();
		assert(jumpTable.magic1_ == empty && jumpTable.magic2_ == empty && jumpTable.magic3_ == empty);

		Flash::Write(reinterpret_cast<std::uint32_t>(&jumpTable.magic1_), ApplicationJumpTable::expected_magic1_value);
		Flash::Write(reinterpret_cast<std::uint32_t>(&jumpTable.magic2_), ApplicationJumpTable::expected_magic2_value);
		Flash::Write(reinterpret_cast<std::uint32_t>(&jumpTable.magic3_), ApplicationJumpTable::expected_magic3_value);

		Flash::Write(reinterpret_cast<std::uint32_t>(&jumpTable.entryPoint_), firmware_.entryPoint_);
		Flash::Write(reinterpret_cast<std::uint32_t>(&jumpTable.interruptVector_), firmware_.interruptVector_);
	}

	HandshakeResponse Bootloader::processHandshake(Register reg, std::uint32_t value) {

		switch (status_) {
		case Status::Ready: {//We are waiting for write to the magic register
			if (reg != Register::TransactionMagic)
				return HandshakeResponse::HandshakeSequenceError;

			if (value != transactionMagic)
				return HandshakeResponse::InvalidTransactionMagic;

			status_ = Status::TransactionStarted;
			return HandshakeResponse::Ok;
		}

		case Status::TransactionStarted: {//We are waiting for the size of binary
			if (reg != Register::FirmwareSize)
				return HandshakeResponse::HandshakeSequenceError;

			//value holds the size of firmware in bytes.
			debug_printf(("Firmware has size %ld bytes.\r\n", value));

			if (value > Flash::availableMemory)
				return HandshakeResponse::BinaryTooBig;

			firmware_.expectedBytes_ = InformationSize::fromBytes(value);
			status_ = Status::ReceivedFirmwareSize;
			return HandshakeResponse::Ok;
		}

		case Status::ReceivedFirmwareSize: {//Waiting for the number of pages
			if (reg != Register::NumPagesToErase)
				return HandshakeResponse::HandshakeSequenceError;

			debug_printf(("Will erase %ld flash pages\r\n", value));
			if (value >= Flash::pageCount || value == 0)
				return HandshakeResponse::NotEnoughPages;

			firmware_.numPagesToErase_ = value;
			status_ = Status::ReceivedNumPagestoErase;
			return HandshakeResponse::Ok;
		}

		case Status::ReceivedNumPagestoErase: { //We are expectiong the first page to erase

			if (reg != Register::PageToErase) //We have to erase at least one page.
				return HandshakeResponse::HandshakeSequenceError;

			if (HandshakeResponse const result = tryErasePage(value); result != HandshakeResponse::Ok)
				return result;

			//We have erased one of the pages of flash. It meas that we are really commited
			jumpTable.invalidate();
			status_ = Status::ErasingPages;
			return HandshakeResponse::Ok;
		}
		case Status::ErasingPages: { //We are expecting either more pages to erase 
			switch (reg) {
			case Register::PageToErase: {
				if (erased_pages_count_ >= firmware_.numPagesToErase_)
					return HandshakeResponse::ErasedPageCountMismatch;

				if (HandshakeResponse const result = tryErasePage(value); result != HandshakeResponse::Ok)
					return result;

				return HandshakeResponse::Ok;
			}
			case Register::EntryPoint: {
				if (erased_pages_count_ != firmware_.numPagesToErase_)
					return HandshakeResponse::ErasedPageCountMismatch;

				debug_printf(("Received entry point at %0lx\r\n", value));
				switch (Flash::addressOrigin(value)) {
				case AddressSpace::AvailableFlash:
					break; //Entry point in available application flash is expected

				case AddressSpace::BootloaderFlash:
				case AddressSpace::JumpTable:
					return HandshakeResponse::PageProtected;

				case AddressSpace::RAM:
				case AddressSpace::Unknown:
					return HandshakeResponse::AddressNotInFlash;
				}

				firmware_.entryPoint_ = value;
				status_ = Status::ReceivedEntryPoint;
				return HandshakeResponse::Ok;
			}
			default: //Any other register is an error tight now
				return HandshakeResponse::HandshakeSequenceError;
			}
		}
		case Status::ReceivedEntryPoint: //We are expecting the interrupt vector
			if (reg != Register::InterruptVector)
				return HandshakeResponse::HandshakeSequenceError;

			debug_printf(("Received isr vector at %0lx\r\n", value));
			//TODO replace nine by some named constant
			if (!ufsel::bit::all_cleared(value, ufsel::bit::bitmask_of_width(9))) //The interrupt vector is not aligned to 512B boundary.
				return HandshakeResponse::InterruptVectorNotAligned;

			firmware_.interruptVector_ = value;
			status_ = Status::ReceivedInterruptVector;
			return HandshakeResponse::Ok;

		case Status::ReceivedInterruptVector: //Expecting one more transaction magic
			if (reg != Register::TransactionMagic)
				return HandshakeResponse::HandshakeSequenceError;

			if (value != transactionMagic)
				return HandshakeResponse::InvalidTransactionMagic;

			status_ = Status::ReceivingData;
			return HandshakeResponse::Ok;

		case Status::ReceivingData: { //Expecting the checksum
			if (reg != Register::Checksum)
				return HandshakeResponse::HandshakeSequenceError;

			if (firmware_.writtenBytes_ != firmware_.expectedBytes_) //A different number of bytes has been written
				return HandshakeResponse::NumWrittenBytesMismatch;

			if (value != firmware_.checksum_)
				return HandshakeResponse::ChecksumMismatch;

			std::uint32_t const* isr_vector = reinterpret_cast<std::uint32_t const*>(firmware_.interruptVector_);
			if (firmware_.entryPoint_ != isr_vector[1]) //The second word of isr vector shall be the address of application's entry point
				return HandshakeResponse::EntryPointAddressMismatch;

			status_ = Status::ReceivedChecksum;
			return HandshakeResponse::Ok;
		}
		case Status::ReceivedChecksum:
			if (reg != Register::TransactionMagic)
				return HandshakeResponse::HandshakeSequenceError;

			if (value != transactionMagic) //Invalid magic was given
				return HandshakeResponse::InvalidTransactionMagic;

			finishTransaction();
			status_ = Status::Ready; //Transaction magic received. Let's call this transaction finished
			return HandshakeResponse::Ok;

		case Status::Error:
			return HandshakeResponse::HandshakeSequenceError;
		}
		assert_unreachable();
	}


	void Bootloader::resetToApplication() {
		ufsel::bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN); //Enable clock to backup domain
		ufsel::bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable write protection of Backup domain

		BackupDomain::bootControlRegister = BackupDomain::application_magic;

		ufsel::bit::set(std::ref(SCB->AIRCR),
			0x5fA << SCB_AIRCR_VECTKEYSTAT_Pos, //magic value required for write to succeed
			SCB_AIRCR_SYSRESETREQ); //Start the system reset

		for (;;); //wait for reset
	}

	void Bootloader::setEntryReason(EntryReason reason, int appErrorCode) {
		assert(entryReason_ == EntryReason::DontEnter); //Make sure this is called only once
		assert(reason != EntryReason::DontEnter); //Sanity check

		entryReason_ = reason;
		appErrorCode_ = appErrorCode;
	}


}
