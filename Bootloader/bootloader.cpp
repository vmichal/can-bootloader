/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */
#include <algorithm>

#include "bootloader.hpp"
#include "flash.hpp"

#include <ufsel/assert.hpp>
#include <library/timer.hpp>
namespace boot {

	namespace {

		// Checks given register and value and returns HandshakeResponse accordingly.
		// Returns Ok iff reg is TransactionMagic and value has the expected value
		HandshakeResponse checkMagic(Register const reg, std::uint32_t const value) {
			if (reg != Register::TransactionMagic)
				return HandshakeResponse::HandshakeSequenceError;

			if (value != Bootloader::transactionMagic)
				return HandshakeResponse::InvalidTransactionMagic;

			return HandshakeResponse::Ok;
		}

	}

	WriteStatus FirmwareDownloader::checkAddressBeforeWrite(std::uint32_t const address) {
		//TODO consider checking the alignment as well

		AddressSpace const origin = Flash::addressOrigin(address);
		switch (origin) {

		case AddressSpace::ApplicationFlash:
		case AddressSpace::BootloaderFlash:
			if (origin == bootloader_.expectedAddressSpace())
				break;

			[[fallthrough]];
		case AddressSpace::JumpTable:
			return WriteStatus::MemoryProtected;

		case AddressSpace::RAM: case AddressSpace::Unknown:
			return WriteStatus::NotInFlash;
		}

		assert(Flash::isBootloaderAddress(address) == bootloader_.updatingBootloader());

		if (expectedWriteLocation() != address)
			return WriteStatus::DiscontinuousWriteAccess;

		//see whether this is not the same page as the last one written.
		//if it is, we can let the write continue.
		if (std::ranges::find(erasedBlocks_, Flash::getEnclosingBlock(address)) == end(erasedBlocks_))
			return WriteStatus::NotInErasedMemory;

		return WriteStatus::Ok; //Everything seems ok, try to write
	}

	HandshakeResponse PhysicalMemoryBlockEraser::tryErasePage(std::uint32_t const address) {

		if (!Flash::isPageAligned(address))
			return HandshakeResponse::PageAddressNotAligned;

		AddressSpace const space = Flash::addressOrigin(address);
		switch (space) {
		case AddressSpace::ApplicationFlash:
		case AddressSpace::BootloaderFlash:
			// If the address lies in memory we want to update, continue with other checks.
			// Otherwise fail right away
			if (space == bootloader_.expectedAddressSpace())
				break;

			[[fallthrough]];
		case AddressSpace::JumpTable:
			return HandshakeResponse::PageProtected;
		case AddressSpace::RAM:
		case AddressSpace::Unknown:
			return HandshakeResponse::AddressNotInFlash;
		}

		MemoryBlock const enclosingBlock = Flash::getEnclosingBlock(address);

		std::span const already_erased = erased_pages();
		if (std::ranges::find(already_erased, enclosingBlock) != end(already_erased))
			return HandshakeResponse::PageAlreadyErased;

		Flash::ErasePage(address);
		erased_pages_[erased_pages_count_++] = enclosingBlock;

		return HandshakeResponse::Ok;
	}

	Bootloader::FirmwareData Bootloader::summarizeFirmwareData() const {
		FirmwareData firmware;

		firmware.expectedBytes_ = firmwareDownloader_.expectedSize();
		firmware.writtenBytes_ = firmwareDownloader_.actualSize();
		firmware.entryPoint_ = metadataReceiver_.entry_point();
		firmware.interruptVector_ = metadataReceiver_.isr_vector();
		firmware.logical_memory_blocks_ = logicalMemoryMapReceiver_.logicalMemoryBlocks();

		return firmware;
	}


	void Bootloader::finishFlashingTransaction() const {
		//Make sure every subtransaction was carried out successfully
		assert(physicalMemoryMapTransmitter_.done());
		assert(logicalMemoryMapReceiver_.done());
		assert(physicalMemoryBlockEraser_.done());
		assert(firmwareDownloader_.done());
		assert(metadataReceiver_.done());

		FirmwareData const firmware = summarizeFirmwareData();

		//Sanity checks of internal state
		assert(firmware.expectedBytes_ == firmware.writtenBytes_);
		assert(Flash::addressOrigin(firmware.interruptVector_) == AddressSpace::ApplicationFlash);
		assert(Flash::addressOrigin(firmware.entryPoint_) == AddressSpace::ApplicationFlash);
		assert(ufsel::bit::all_cleared(firmware.interruptVector_, isrVectorAlignmentMask));

		//The page must have been cleared before
		constexpr auto empty = std::numeric_limits<decltype(jumpTable.magic1_)>::max();
		assert(jumpTable.magic1_ == empty && jumpTable.magic2_ == empty && jumpTable.magic3_ == empty && jumpTable.magic4_ == empty && jumpTable.magic5_ == empty);

		//entry point is not stored as it can be derived from the isr vector
		ApplicationJumpTable table;
		table.set_metadata(firmware.writtenBytes_, firmware.logical_memory_blocks_);
		table.set_interrupt_vector(firmware.interruptVector_);
		table.set_magics();

		Flash::RAII_unlock const _;
		table.writeToFlash();
	}

	Bootloader_Handshake_t PhysicalMemoryMapTransmitter::update() {
		//the first "available" block. Bootloader is located in preceding memory pages.
		std::uint32_t const firstBlockIndex = bootloader_.updatingBootloader() ? customization::firstBlockAvailableToBootloader : customization::firstBlockAvailableToApplication;
		std::uint32_t const pagesToSend = bootloader_.updatingBootloader() ? PhysicalMemoryMap::bootloaderPages() : PhysicalMemoryMap::applicationPages();

		switch (status_) {
		case Status::uninitialized:
		case Status::pending: //Update function shall not be reached with these states
			status_ = Status::error;
			return handshake::abort;

		case Status::masterYielded:
			status_ = Status::sentInitialMagic;
			return handshake::transactionMagic;

		case Status::sentInitialMagic:
			status_ = Status::sendingBlockAddress;
			return handshake::get(Register::NumPhysicalMemoryBlocks, Command::None, pagesToSend);

		case Status::sendingBlockAddress: {
			if (pagesToSend == blocks_sent_) { //we have sent all available blocks
				status_ = Status::shouldYield;
				return handshake::transactionMagic;
			}
			status_ = Status::sendingBlockLength;
			auto const currentBlock = PhysicalMemoryMap::block(firstBlockIndex + blocks_sent_);
			return handshake::get(Register::PhysicalBlockStart, Command::None, currentBlock.address);

		}
		case Status::sendingBlockLength: {
			status_ = Status::sendingBlockAddress;
			auto const currentBlock = PhysicalMemoryMap::block(firstBlockIndex + blocks_sent_);
			++blocks_sent_;
			return handshake::get(Register::PhysicalBlockLength, Command::None, currentBlock.length);
		}
		case Status::shouldYield:
		case Status::done:
			status_ = Status::error;
			return handshake::abort;
		case Status::error:
			return handshake::abort;
		}
		assert_unreachable();
	}

	void LogicalMemoryMapReceiver::startSubtransaction() {
		status_ = Status::pending;
		remaining_bytes_ = bootloader_.updatingBootloader() ? Flash::bootloaderMemorySize : Flash::applicationMemorySize;
	}

	HandshakeResponse LogicalMemoryMapReceiver::receive(Register reg, Command com, std::uint32_t value) {
		switch (status_) {
		case Status::uninitialized:
			status_ = Status::error;
			return HandshakeResponse::InternalStateMachineError;

		case Status::pending:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::waitingForBlockCount;
			return HandshakeResponse::Ok;

		case Status::waitingForBlockCount:
			if (reg != Register::NumLogicalMemoryBlocks)
				return HandshakeResponse::HandshakeSequenceError;

			if (value == 0)
				return HandshakeResponse::MustBeNonZero;

			if (!bootloader_.updatingBootloader()) {
				//since we don't need to save logical blocks of the bootloader, we don't care about the number of blocks in this case
				if (value > blocks_.size()) //That many memory blocks cant be stored
					return HandshakeResponse::TooManyLogicalMemoryBlocks;
			}


			blocks_expected_ = value;
			status_ = Status::waitingForBlockAddress;
			return HandshakeResponse::Ok;

		case Status::waitingForBlockAddress: {

			if (reg != Register::LogicalBlockStart) {
				// At the the end of (address, length) pairs there should be a single transaction magic. Check it and conditionally
				// continue to the next subtransaction
				HandshakeResponse const response = checkMagic(reg, value);
				if (response == HandshakeResponse::Ok)
					status_ = Status::done;
				return response;
			}

			if (blocks_received_ == blocks_expected_) //no more memory blocks may be received
				return HandshakeResponse::LogicalBlockCountMismatch;

			if (Flash::addressOrigin(value) != bootloader_.expectedAddressSpace())
				return bootloader_.updatingBootloader() ? HandshakeResponse::AddressNotInBootloader : HandshakeResponse::AddressNotInFlash;

			if (blocks_received_) { //We have already received at least one block
				MemoryBlock const& previous = blocks_[blocks_received_ - 1];

				if (value < previous.address)
					return HandshakeResponse::LogicalBlockAddressesNotIncreasing;

				if (value < end(previous))
					return HandshakeResponse::LogicalBlocksOverlapping;
			}

			blocks_[blocks_received_].address = value; //Add the address to list of received.
			status_ = Status::waitingForBlockLength;

			return HandshakeResponse::Ok;

		}
		case Status::waitingForBlockLength:
			if (reg != Register::LogicalBlockLength)
				return HandshakeResponse::HandshakeSequenceError;

			if (value == 0)
				return HandshakeResponse::MustBeNonZero;

			if (value > remaining_bytes_) //the block is too long
				return HandshakeResponse::LogicalBlockTooLong;

			if (!PhysicalMemoryMap::canCover(bootloader_.expectedAddressSpace(), blocks_[blocks_received_]))
				return HandshakeResponse::LogicalBlockNotCoverable;

			remaining_bytes_ -= value;
			blocks_[blocks_received_].length = value;
			++blocks_received_;
			status_ = Status::waitingForBlockAddress;
			return HandshakeResponse::Ok;

		case Status::done:
			return HandshakeResponse::InternalStateMachineError;
		case Status::error:
			return HandshakeResponse::BootloaderInError;
		}
		assert_unreachable();
	}

	HandshakeResponse PhysicalMemoryBlockEraser::receive(Register reg, Command com, std::uint32_t value) {
		switch (status_) {
		case Status::uninitialized:
			status_ = Status::error;
			return HandshakeResponse::InternalStateMachineError;
		case Status::pending:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::waitingForMemoryBlockCount;
			return HandshakeResponse::Ok;

		case Status::waitingForMemoryBlockCount: {

			if (reg != Register::NumPhysicalBlocksToErase)
				return HandshakeResponse::HandshakeSequenceError;

			if (value == 0)
				return HandshakeResponse::MustBeNonZero;

			std::uint32_t const max_pages = bootloader_.updatingBootloader()
					? PhysicalMemoryMap::bootloaderPages() : PhysicalMemoryMap::applicationPages();

			if (value > max_pages)
				return HandshakeResponse::NotEnoughPages;

			expectedPageCount_ = value;
			status_ = Status::waitingForMemoryBlocks;
			return HandshakeResponse::Ok;

		}
		case Status::waitingForMemoryBlocks:
			if (reg != Register::PhysicalBlockToErase) //We have to erase at least one page.
				return HandshakeResponse::HandshakeSequenceError;

			//We are about to erase one of the pages of flash. It meas that we are really commited
			Flash::Unlock();

			//Invalidate the jump table only when the application is updated. When updating bootloader, preserve all information.
			if (!bootloader_.updatingBootloader())
				jumpTable.invalidate();

			if (HandshakeResponse const result = tryErasePage(value); result != HandshakeResponse::Ok)
				return result;

			status_ = Status::receivingMemoryBlocks;
			return HandshakeResponse::Ok;

		case Status::receivingMemoryBlocks:

			if (reg != Register::PhysicalBlockToErase) {
				auto const response = checkMagic(reg, value);
				if (response == HandshakeResponse::Ok) {
					//wait for all operations to finish and lock flash
					Flash::AwaitEndOfErasure();

					Flash::Lock();
					status_ = Status::done;
				}
				return response;
			}

			if (erased_pages_count_ == expectedPageCount_)
				return HandshakeResponse::ErasedPageCountMismatch;

			if (HandshakeResponse const result = tryErasePage(value); result != HandshakeResponse::Ok)
				return result;

			return HandshakeResponse::Ok;

		case Status::done:
			status_ = Status::error;
			return HandshakeResponse::InternalStateMachineError;

		case Status::error:
			return HandshakeResponse::BootloaderInError;
		}
		assert_unreachable();
	}

	HandshakeResponse FirmwareDownloader::receive(Register reg, Command com, std::uint32_t value) {
		switch (status_) {
		case Status::unitialized:
			return HandshakeResponse::InternalStateMachineError;
		case Status::pending:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::waitingForFirmwareSize;
			return HandshakeResponse::Ok;

		case Status::waitingForFirmwareSize:
			if (reg != Register::FirmwareSize)
				return HandshakeResponse::HandshakeSequenceError;

			if (value == 0)
				return HandshakeResponse::MustBeNonZero;

			if (value > Flash::applicationMemorySize)
				return HandshakeResponse::BinaryTooBig;

			firmware_size_ = InformationSize::fromBytes(value);
			Flash::Unlock();
			status_ = Status::receivingData;
			return HandshakeResponse::Ok;

		case Status::receivingData:
			assert_unreachable();
		case Status::noMoreDataExpected:
			if (reg != Register::Checksum)
				return HandshakeResponse::HandshakeSequenceError;

			if (value != checksum_)
				return HandshakeResponse::ChecksumMismatch;

			Flash::Lock();
			status_ = Status::receivedChecksum;
			return HandshakeResponse::Ok;

		case Status::receivedChecksum:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::done;
			return HandshakeResponse::Ok;
		case Status::done:
			status_ = Status::error;
			return HandshakeResponse::InternalStateMachineError;
		case Status::error:
			return HandshakeResponse::BootloaderInError;
		}
		assert_unreachable();
	}

	HandshakeResponse Bootloader::validateVectorTable(AddressSpace const expected_space, std::uint32_t const address) {

		if (Flash::addressOrigin(address) != expected_space)
			return expected_space == AddressSpace::BootloaderFlash
				? HandshakeResponse::AddressNotInBootloader : HandshakeResponse::AddressNotInFlash;

		if (!ufsel::bit::all_cleared(address, isrVectorAlignmentMask))
			return HandshakeResponse::InterruptVectorNotAligned;

		return HandshakeResponse::Ok;
	}

	HandshakeResponse MetadataReceiver::receive(Register reg, Command com, std::uint32_t value) {
		switch (status_) {
		case Status::unitialized:
			return HandshakeResponse::InternalStateMachineError;
		case Status::pending:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::awaitingInterruptVector;
			return HandshakeResponse::Ok;

		case Status::awaitingInterruptVector:
			if (reg != Register::InterruptVector)
				return HandshakeResponse::HandshakeSequenceError;

			if (auto const response = Bootloader::validateVectorTable(bootloader_.expectedAddressSpace(), value); response != HandshakeResponse::Ok)
				return response;

			isr_vector_ = value;
			status_ = Status::awaitingEntryPoint;
			return HandshakeResponse::Ok;

		case Status::awaitingEntryPoint: {

			if (reg != Register::EntryPoint)
				return HandshakeResponse::HandshakeSequenceError;

			if (Flash::addressOrigin(value) != bootloader_.expectedAddressSpace())
				return bootloader_.updatingBootloader() ? HandshakeResponse::AddressNotInBootloader : HandshakeResponse::AddressNotInFlash;

			std::uint32_t const* const isr_vector = reinterpret_cast<std::uint32_t const*>(isr_vector_);
			if (isr_vector[1] != value)
				return HandshakeResponse::EntryPointAddressMismatch;

			entry_point_ = value;
			status_ = Status::awaitingTerminatingMagic;
			return HandshakeResponse::Ok;
		}

		case Status::awaitingTerminatingMagic:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::done;
			return HandshakeResponse::Ok;

		case Status::done:
			status_ = Status::error;
			return HandshakeResponse::InternalStateMachineError;
		case Status::error:
			return HandshakeResponse::BootloaderInError;
		}
		assert_unreachable();
	}


	Bootloader_Handshake_t Bootloader::processYield() {
		switch (status_) {
		case Status::TransmittingPhysicalMemoryBlocks:
			physicalMemoryMapTransmitter_.processYield();
			return physicalMemoryMapTransmitter_.update(); //send the initial transaction magic straight away
		default:
			status_ = Status::Error; //TODO make more concrete
			return handshake::abort;
		}
		assert_unreachable();
	}

	HandshakeResponse Bootloader::setNewVectorTable(std::uint32_t const isr_vector) {
		//We want to update the interrupt vector address stored in the application jump table.
		//Depending on the state on the jump table, we either have to copy and store all data again
		//or fill it with empty metadata to make it valid. When this function returns, the jump table
		//must be in a valid state (and the application bootable)

		if (auto const response = validateVectorTable(AddressSpace::ApplicationFlash, isr_vector); response != HandshakeResponse::Ok)
			return response; //Ignore this write if the given address does not fulfill requirements on vector table

		//TODO make sure there is enough stack space (in linker scripts)
		ApplicationJumpTable table_copy = jumpTable; //copy the old jump table to RAM
		table_copy.interruptVector_ = isr_vector;

		Flash::RAII_unlock const _;

		jumpTable.invalidate();
		Flash::AwaitEndOfErasure(); //Make sure the page erasure has finished and exit erase mode.

		table_copy.writeToFlash();
		return HandshakeResponse::Ok;
	}

	HandshakeResponse Bootloader::processHandshake(Register const reg, Command const command, std::uint32_t const value) {

		if (reg != Register::Command && command != Command::None)
			return HandshakeResponse::CommandNotNone;

		switch (status_) {
		case Status::Ready:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;

			status_ = Status::Initialization;
			return HandshakeResponse::Ok;

		case Status::Initialization:
			if (reg != Register::Command)
				return HandshakeResponse::HandshakeSequenceError;

			switch (command) {
			case Command::StartTransactionFlashing:
			case Command::StartBootloaderUpdate:
				status_ = Status::TransmittingPhysicalMemoryBlocks;
				transactionType_ = command == Command::StartTransactionFlashing ? TransactionType::Flashing : TransactionType::BootloaderUpdate;
				physicalMemoryMapTransmitter_.startSubtransaction();
				return HandshakeResponse::Ok;
			case Command::SetNewVectorTable: {

				auto const response = setNewVectorTable(value);
				//Return to the ready state. Should the master want to send data again, it would start a new transaction
				status_ = Status::Ready;
				return response;
			}
			default:
				return HandshakeResponse::UnknownTransactionType;
			}
			assert_unreachable();

		case Status::TransmittingPhysicalMemoryBlocks:
			//During this stage the bootloader transmits data. The master therefore should not proceed
			return HandshakeResponse::HandshakeNotExpected;

		case Status::ReceivingFirmwareMemoryMap: {

			auto const result = logicalMemoryMapReceiver_.receive(reg, command, value);
			if (logicalMemoryMapReceiver_.done()) {
				status_ = Status::ErasingPhysicalBlocks;
				physicalMemoryBlockEraser_.startSubtransaction();
			}
			return result;
		}
		case Status::ErasingPhysicalBlocks: {

			auto const result = physicalMemoryBlockEraser_.receive(reg, command, value);
			if (physicalMemoryBlockEraser_.done()) {
				status_ = Status::DownloadingFirmware;
				firmwareDownloader_.startSubtransaction(physicalMemoryBlockEraser_.erased_pages(), logicalMemoryMapReceiver_.logicalMemoryBlocks());
			}
			return result;

		}
		case Status::DownloadingFirmware: {

			auto const result = firmwareDownloader_.receive(reg, command, value);
			if (firmwareDownloader_.done()) {
				status_ = Status::ReceivingFirmwareMetadata;
				metadataReceiver_.startSubtransaction();
			}
			return result;
		}
		case Status::ReceivingFirmwareMetadata: {

			auto const result = metadataReceiver_.receive(reg, command, value);
			if (metadataReceiver_.done()) {
				status_ = Status::Ready;
				//We are done here. If the bootloader was updated, we don't need to write anything more to flash.
				if (!updatingBootloader())
					finishFlashingTransaction();
			}
			return result;
		}
		case Status::Error:
		case Status::ComunicationStalled:
			return HandshakeResponse::BootloaderInError;
		}

		assert_unreachable();
	}

	void Bootloader::processHandshakeAck(HandshakeResponse const response) {
		switch (status_) {
		case Status::TransmittingPhysicalMemoryBlocks:
			if (physicalMemoryMapTransmitter_.shouldYield()) {
				physicalMemoryMapTransmitter_.endSubtransaction();

				logicalMemoryMapReceiver_.startSubtransaction();
				status_ = Status::ReceivingFirmwareMemoryMap;

				can_.yieldCommunication();
			}
			else
				can_.SendHandshake(physicalMemoryMapTransmitter_.update());
			return;
		default:
			status_ = Status::Error; //TODO make more concrete
			return;
		}
		assert_unreachable();
	}




	void Bootloader::setEntryReason(EntryReason reason) {
		assert(entryReason_ == EntryReason::Unknown); //Make sure this is called only once
		assert(reason != EntryReason::Unknown); //Sanity check

		entryReason_ = reason;
	}

}
