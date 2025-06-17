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
#include "canmanager.hpp"

#include <ufsel/assert.hpp>
#include <ufsel/units.hpp>
#include <ufsel/time.hpp>

extern "C" {
	// Defined in linker script
	extern std::uint32_t bootloader_update_buffer_begin[], bootloader_update_buffer_end[];

	void Reset_Handler();
}

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

		constexpr std::uint32_t calculate_checksum(std::span<MemoryBlock const> logical_memory_map, bool is_bootloader) {
			std::uint32_t result = 0;
			for (auto const& block : logical_memory_map) {
				for (std::uint32_t address = block.address; address < end(block); address += sizeof(std::uint32_t)) {
					std::uint32_t data = is_bootloader
							? bootloader_update_buffer_begin[(address - Flash::bootloaderAddress) / sizeof(std::uint32_t)]
							: ufsel::bit::access_register(address);
					while (data) {
						result += data & std::numeric_limits<std::uint16_t>::max();
						data >>= std::numeric_limits<std::uint16_t>::digits;
					}
				}
			}
			return result;
		}
	}

	WriteStatus FirmwareDownloader::checkAddressBeforeWrite(std::uint32_t const address, std::uint32_t const data) const {

		AddressSpace const origin = Flash::addressOrigin(address);
		switch (origin) {

		case AddressSpace::ApplicationFlash:
		case AddressSpace::BootloaderFlash:
			if (origin == bootloader_.expectedAddressSpace())
				break;

			[[fallthrough]];
		case AddressSpace::JumpTable:
			return WriteStatus::MemoryProtected;

		case AddressSpace::Unknown:
			return WriteStatus::NotInFlash;
		}

		if (address % sizeof(data) != 0)
			return WriteStatus::NotAligned;

		assert(Flash::isBootloaderAddress(address) == bootloader_.updatingBootloader());

		std::uint32_t const expected_address = expectedWriteLocation();
		if (address != expected_address)
			return address < expected_address ? WriteStatus::AlreadyWritten : WriteStatus::DiscontinuousWriteAccess;

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
		case AddressSpace::Unknown:
			return HandshakeResponse::AddressNotInFlash;
		}

		MemoryBlock const enclosingBlock = Flash::getEnclosingBlock(address);

		std::span const already_erased = erased_pages();
		if (std::ranges::find(already_erased, enclosingBlock) != end(already_erased))
			return HandshakeResponse::PageAlreadyErased;

		if (!bootloader_.updatingBootloader()) {
			std::uint32_t const code = Flash::ErasePage(address);
			if (!Flash::is_SR_ok(code)) {
				canManager.SendHandshake(handshake::abort(AbortCode::FlashErase, code));
				return HandshakeResponse::PageEraseFailed;
			}
		}
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
		assert(jumpTable.isErased());

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
			return handshake::abort(AbortCode::PhysicalMemoryMapTransmit_Update_UninitPending, static_cast<int>(status_));

		case Status::masterYielded:
			status_ = Status::sentInitialMagic;
			return handshake::transactionMagic;

		case Status::sentInitialMagic:
			status_ = Status::sendingBlockAddress;
			return handshake::create(Register::NumPhysicalMemoryBlocks, Command::None, pagesToSend);

		case Status::sendingBlockAddress: {
			if (pagesToSend == blocks_sent_) { //we have sent all available blocks
				status_ = Status::shouldYield;
				return handshake::transactionMagic;
			}
			status_ = Status::sendingBlockLength;
			auto const currentBlock = PhysicalMemoryMap::block(firstBlockIndex + blocks_sent_);
			return handshake::create(Register::PhysicalBlockStart, Command::None, currentBlock.address);

		}
		case Status::sendingBlockLength: {
			status_ = Status::sendingBlockAddress;
			auto const currentBlock = PhysicalMemoryMap::block(firstBlockIndex + blocks_sent_);
			++blocks_sent_;
			return handshake::create(Register::PhysicalBlockLength, Command::None, currentBlock.length);
		}
		case Status::shouldYield:
		case Status::done:
			status_ = Status::error;
			return handshake::abort(AbortCode::PhysicalMemoryMapTransmit_Update_DoneYield, static_cast<int>(status_));
		case Status::error:
			return handshake::abort(AbortCode::PhysicalMemoryMapTransmit_Update_Error, static_cast<int>(status_));
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

	void LogicalMemoryMapTransmitter::startSubtransaction() {
		status_ = Status::pending;

		switch (bootloader_.transaction_type()) {
			case TransactionType::BootloaderReadout:
				// There is no way of telling, how many logical memory logical memory blocks the bootloader consists of (there is no JumpTable for it)
				blocks_[0] = MemoryBlock{.address = Flash::bootloaderAddress, .length = Flash::bootloaderMemorySize};
				block_count_ = 1;
				break;

			case TransactionType::FirmwareReadout:
				if (jumpTable.has_valid_metadata()) {
					block_count_ = jumpTable.logical_memory_block_count_;
					std::copy_n(jumpTable.logical_memory_blocks_.begin(), block_count_, blocks_.begin());
				}
				else {
					blocks_[0] = MemoryBlock{.address = Flash::applicationAddress, .length = Flash::applicationMemorySize};
					block_count_ = 1;
				}
				break;

			default:
				canManager.SendHandshake(handshake::abort(AbortCode::LogicalMemoryMapTransmit_incorrect_transaction_type, static_cast<int>(bootloader_.transaction_type())));
		}
	}

	Bootloader_Handshake_t LogicalMemoryMapTransmitter::update() {
		//the first "available" block. Bootloader is located in preceding memory pages.
		std::uint32_t const firstBlockIndex = bootloader_.updatingBootloader() ? customization::firstBlockAvailableToBootloader : customization::firstBlockAvailableToApplication;
		std::uint32_t const pagesToSend = bootloader_.updatingBootloader() ? PhysicalMemoryMap::bootloaderPages() : PhysicalMemoryMap::applicationPages();

		switch (status_) {
			case Status::uninitialized:
			case Status::pending: //Update function shall not be reached with these states
				status_ = Status::error;
				return handshake::abort(AbortCode::LogicalMemoryMapTransmit_Update_UninitPending, static_cast<int>(status_));

			case Status::masterYielded:
				status_ = Status::sentInitialMagic;
				return handshake::transactionMagic;

			case Status::sentInitialMagic:
				status_ = Status::sendingBlockAddress;
				return handshake::create(Register::NumLogicalMemoryBlocks, Command::None, block_count_);

			case Status::sendingBlockAddress: {
				if (blocks_sent_ == block_count_) { //we have sent all available blocks
					status_ = Status::done;
					return handshake::transactionMagic;
				}
				status_ = Status::sendingBlockLength;
				auto const currentBlock = blocks_[blocks_sent_];
				return handshake::create(Register::LogicalBlockStart, Command::None, currentBlock.address);

			}
			case Status::sendingBlockLength: {
				status_ = Status::sendingBlockAddress;
				auto const currentBlock = blocks_[blocks_sent_];
				++blocks_sent_;
				return handshake::create(Register::LogicalBlockLength, Command::None, currentBlock.length);
			}
			case Status::done:
				status_ = Status::error;
				return handshake::abort(AbortCode::LogicalMemoryMapTransmit_Update_DoneYield, static_cast<int>(status_));
			case Status::error:
				return handshake::abort(AbortCode::LogicalMemoryMapTransmit_Update_Error, static_cast<int>(status_));
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
			if (ufsel::bit::all_set(FLASH->CR, FLASH_CR_LOCK))
				Flash::Unlock();

			//Invalidate the jump table only when the application is updated. When updating bootloader, preserve all information.
			if (!bootloader_.updatingBootloader()) {
				bool const ok = jumpTable.invalidate();
				if (!ok)
					return HandshakeResponse::PageEraseFailed;

			}

			if (HandshakeResponse const result = tryErasePage(value); result != HandshakeResponse::Ok)
				return result;

			status_ = Status::receivingMemoryBlocks;
			return HandshakeResponse::Ok;

		case Status::receivingMemoryBlocks:

			if (reg != Register::PhysicalBlockToErase) {
				auto const response = checkMagic(reg, value);
				if (response != HandshakeResponse::Ok)
					return response;

				//wait for all operations to finish and lock flash
				Flash::AwaitEndOfErasure();

				Flash::Lock();
				status_ = Status::done;
				if (erased_pages_count_ != expectedPageCount_)
					return HandshakeResponse::ErasedPageCountMismatch;
				return HandshakeResponse::Ok;
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
			if (ufsel::bit::all_set(FLASH->CR, FLASH_CR_LOCK))
				Flash::Unlock();
			status_ = Status::receivingData;
			return HandshakeResponse::Ok;

		case Status::receivingData:
			return HandshakeResponse::HandshakeNotExpected;
		case Status::noMoreDataExpected: {

			if (reg != Register::Checksum)
				return HandshakeResponse::HandshakeSequenceError;

			std::uint32_t const volatile checksum = calculate_checksum(firmwareBlocks_, bootloader_.updatingBootloader());
			if (value != checksum)
				return HandshakeResponse::ChecksumMismatch;

			status_ = Status::receivedChecksum;

			if (bootloader_.updatingBootloader()) {
				// Checksum is valid, actually update the flash memory

				if (ufsel::bit::all_set(FLASH->CR, FLASH_CR_LOCK))
					Flash::Unlock();

				for (MemoryBlock const& page : erasedBlocks_) {
					std::uint32_t const code = Flash::ErasePage(page.address);
					if (!Flash::is_SR_ok(code)) {
						canManager.SendHandshake(handshake::abort(AbortCode::FlashErase, code));
						return HandshakeResponse::PageEraseFailed;
					}
				}
				Flash::AwaitEndOfErasure();

				if (transfer_bl_update_buffer() != WriteStatus::Ok) {
					status_ = Status::error;
					return HandshakeResponse::BufferTransferFailed;
				}
			}
			return HandshakeResponse::Ok;

		}
		case Status::receivedChecksum:
			if (auto const res = checkMagic(reg, value); res != HandshakeResponse::Ok)
				return res;
			Flash::Lock();

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

	int FirmwareDownloader::calculate_padding_width(std::uint32_t address, std::uint32_t const data, MemoryBlock const * next_block) {
		std::uint32_t const data_end_address = address + sizeof(data); //this is the address one past last byte written
		// The address one past the containing flash native type
		std::uint32_t const native_type_end = (data_end_address + sizeof(Flash::nativeType) - 1) & ~(sizeof(Flash::nativeType) - 1);
		// We either need to fill bytes from end of data to end of native type or less if the next block starts earlier
		std::uint32_t const padding_end = next_block ? std::min(native_type_end, next_block->address) : native_type_end;

		return padding_end - data_end_address;
	}

	void FirmwareDownloader::schedule_data_write(std::uint32_t const address, std::uint32_t const data, bool is_last_write_in_logical_block) {
		if constexpr (sizeof(data) == sizeof(Flash::nativeType)) {
			// No need to go through the for loop since only one element is written.
			bool const success = Flash::ScheduleBufferedWrite(address, data);
			assert(success);
		}
		else if constexpr (sizeof(data) > sizeof(Flash::nativeType)) {
			auto data_copy = data;
			for (std::size_t offset = 0; offset < sizeof(data_copy); offset += sizeof(Flash::nativeType)) {
				if (bool const ret = Flash::ScheduleBufferedWrite<Flash::nativeType>(address + offset, data_copy); !ret)
					assert(false);
				data_copy >>= sizeof(Flash::nativeType) * 8;
			}
		}
		else {
			// sizeof(data) < sizeof(Flash::nativeType)
			// need to add padding
			if (!is_last_write_in_logical_block) {// more data to go, don't bother calculating padding yet
				bool const scheduled = Flash::ScheduleBufferedWrite(address, data);
				assert(scheduled);
			}
				// We are writing the last data of current logical memory block -> extend it to native flash type
			else {
				// We are writing a smaller integral type and no more data is comming... Padding should be added to data_to_write
				MemoryBlock const * next_block = current_block_index_ + 1 < size(firmwareBlocks_) ? &firmwareBlocks_[current_block_index_ + 1] : nullptr;

				int const padding_width = calculate_padding_width(address, data, next_block);
				int const padding_offset = sizeof(data) * 8;

				Flash::nativeType const padding = ufsel::bit::bitmask_of_width<Flash::nativeType>(padding_width * 8) << padding_offset;

				bool const scheduled = Flash::ScheduleBufferedWrite(address, data | padding, sizeof(data) + padding_width);
				assert(scheduled);
			}
		}
	}

	WriteStatus FirmwareDownloader::transfer_bl_update_buffer() {
		using data_t = std::uint32_t;

		for (MemoryBlock const& current_block : firmwareBlocks_) {
			for (std::uint32_t address = current_block.address; address < end(current_block); address += sizeof(data_t)) {

				bool const is_last_write_in_logical_block = address + sizeof(data_t) >= end(current_block);
				int const word_index = (address - Flash::bootloaderAddress) / sizeof(data_t);
				assert(word_index < bootloader_update_buffer_end - bootloader_update_buffer_begin);
				data_t const data = bootloader_update_buffer_begin[word_index];

				schedule_data_write(address, data, is_last_write_in_logical_block);
				WriteStatus const writeStatus = update_flash_write_buffer();
				if (is_last_write_in_logical_block)
					assert(Flash::writeBufferIsEmpty());
				if (writeStatus != WriteStatus::Ok && writeStatus != WriteStatus::InsufficientData) {
					__BKPT();
					return writeStatus;
				}
			}
		}
		return WriteStatus::Ok;
	}

	WriteStatus FirmwareDownloader::update_flash_write_buffer() {
		int times_written = 0;
		WriteStatus writeStatus = WriteStatus::Ok;
		for (; ;++times_written) {
			writeStatus = Flash::tryPerformingBufferedWrite();
			if (writeStatus != WriteStatus::Ok)
				break;
		}
		return writeStatus;
	}

	WriteStatus FirmwareDownloader::write(std::uint32_t const address, std::uint32_t const data) {
		assert(address % sizeof(data) == 0 && "Address not aligned.");


		if (bootloader_.updatingBootloader()) {
			// Store the incoming data in RAM instead of flash directly. Wait for reception of the whole bootloader
			assert(Flash::addressOrigin(address) == AddressSpace::BootloaderFlash);
			int const word_index = (address - Flash::bootloaderAddress) / sizeof(data);
			assert(word_index < bootloader_update_buffer_end - bootloader_update_buffer_begin);
			bootloader_update_buffer_begin[word_index] = data;
			return WriteStatus::Ok;
		}
		else {
			MemoryBlock const & current_block = firmwareBlocks_[current_block_index_];
			bool const is_last_write_in_logical_block = blockOffset_ + sizeof(data)  == current_block.length;

			schedule_data_write(address, data, is_last_write_in_logical_block);
			WriteStatus const writeStatus = update_flash_write_buffer();

			if (is_last_write_in_logical_block)
				assert(Flash::writeBufferIsEmpty());
			return writeStatus;
		}
	}

	void FirmwareDownloader::reset() {
		status_ = Status::unitialized;
		firmware_size_ = 0_B;
		written_bytes_ = 0_B;

		current_block_index_ = 0;
		blockOffset_ = 0;

		std::ranges::fill(bootloader_update_buffer_begin, bootloader_update_buffer_end, 0xcc'cc'cc'cc);
	}

	void FirmwareUploader::update() {

		switch (status_) {
			case Status::uninitialized: //Update function shall not be reached with these states
				status_ = Status::error;
				canManager.set_pending_abort_request(handshake::abort(AbortCode::FirmwareUpload_Update_Uninit, static_cast<int>(status_)));
				return;

			case Status::pending:
				status_ = Status::sendFirmwareSize;
				canManager.SendTransactionMagic();
				return;

			case Status::sendFirmwareSize:
				switch (bootloader_.transaction_type()) {
					case TransactionType::BootloaderReadout:
						// There is no way of telling, how big the bootloader actually is (there is no JumpTable for it)
						firmware_size_ = Flash::bootloaderMemorySize;
						break;

					case TransactionType::FirmwareReadout:
						firmware_size_ = jumpTable.has_valid_metadata() ? jumpTable.firmwareSize_ : 0;
						break;

					default:
						canManager.set_pending_abort_request(handshake::abort(AbortCode::FirmwareUpload_incorrect_transaction_type, static_cast<int>(bootloader_.transaction_type())));
						break;
				}
				status_ = Status::waitingForFirmwareSizeAck;
				canManager.SendHandshake(handshake::create(Register::FirmwareSize, Command::None, firmware_size_));
				return;

			case Status::waitingForFirmwareSizeAck:
				status_ = Status::sendData;
				[[fallthrough]];

			case Status::sendData: {

				if (canManager.get_tx_buffer_size() > max_tx_buffer_fill_by_data)
					return; // Can't proceed now, buffer is almost full!

				std::uint32_t const absolute_address = current_block_->address + offset_in_block_;
				std::uint32_t const word = ufsel::bit::access_register(absolute_address);
				canManager.SendData(absolute_address, word);

				offset_in_block_ += sizeof(std::uint32_t);

				if (offset_in_block_ > current_block_->length) {
					canManager.set_pending_abort_request(handshake::abort(AbortCode::LogicalMemoryMapBlockLengthNotMultipleOf4, current_block_->length));
					__BKPT();
				}

				if (offset_in_block_ == current_block_->length) {
					offset_in_block_ = 0;
					if (++current_block_ == std::to_address(logical_memory_map_.end()))
						status_ = Status::waitForDataAck;
				}

				return;
			}
			case Status::waitForDataAck:
				// No handshake expected now
				status_ = Status::error;
				canManager.set_pending_abort_request(handshake::abort(AbortCode::UnexpectedDataAck));
				return;

			case Status::sentChecksum:
				status_ = Status::done;
				canManager.SendTransactionMagic();
				return;

			case Status::done:
				status_ = Status::error;
				canManager.set_pending_abort_request(handshake::abort(AbortCode::FirmwareUpload_Update_Done, static_cast<int>(status_)));
				return;
			case Status::error:
				canManager.set_pending_abort_request(handshake::abort(AbortCode::FirmwareUpload_Update_Error, static_cast<int>(status_)));
				return;
		}
		assert_unreachable();
	}

	void FirmwareUploader::handle_data_ack() {
		if (ack_expected()) {
			canManager.SendHandshake(handshake::create(Register::Checksum, Command::None, calculate_checksum(logical_memory_map_, false)));
			status_ = Status::sentChecksum;
		}
		//TODO handle unexpected DataAck?
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

	Bootloader_Handshake_t MetadataTransmitter::update() {
		switch (status_) {
			case Status::uninitialized: //Update function shall not be reached with these states
				status_ = Status::error;
				return handshake::abort(AbortCode::MetadataTransmitter_Update_Uninit, static_cast<int>(status_));

			case Status::pending:
				status_ = Status::sendInterruptVector;
				return handshake::transactionMagic;

			case Status::sendInterruptVector: {

				std::uint32_t isr_vector_to_send = 0;
				switch (bootloader_.transaction_type()) {
					case TransactionType::BootloaderReadout:
						// TODO this is the best estimate of BL isr vector we currently have. Can't use pointer to interrupt_routines since that resides in CCM_RAM
						isr_vector_to_send = Flash::bootloaderAddress;
						break;

					case TransactionType::FirmwareReadout:
						isr_vector_to_send = jumpTable.has_valid_metadata() ? jumpTable.interruptVector_ : 0;
						break;

					default:
						return handshake::abort(AbortCode::LogicalMemoryMapTransmit_incorrect_transaction_type, static_cast<int>(bootloader_.transaction_type()));
				}
				status_ = Status::sendEntryPoint;
				return handshake::create(Register::InterruptVector, Command::None, isr_vector_to_send);
			}

			case Status::sendEntryPoint: {

				std::uint32_t entry_point_to_send = 0;
				switch (bootloader_.transaction_type()) {
					case TransactionType::BootloaderReadout:
						entry_point_to_send = reinterpret_cast<std::uint32_t>(&Reset_Handler);
						break;

					case TransactionType::FirmwareReadout:
						entry_point_to_send = jumpTable.has_valid_metadata() ? ufsel::bit::access_register(jumpTable.interruptVector_ + 4) : 0;
						break;

					default:
						return handshake::abort(AbortCode::LogicalMemoryMapTransmit_incorrect_transaction_type, static_cast<int>(bootloader_.transaction_type()));
				}
				status_ = Status::sendEndMagic;
				return handshake::create(Register::EntryPoint, Command::None, entry_point_to_send);
			}

			case Status::sendEndMagic:
				status_ = Status::done;
				return handshake::transactionMagic;

			case Status::done:
				status_ = Status::error;
				return handshake::abort(AbortCode::MetadataTransmitter_Update_Done, static_cast<int>(status_));
			case Status::error:
				return handshake::abort(AbortCode::MetadataTransmitter_Update_Error, static_cast<int>(status_));
		}
		assert_unreachable();
	}


	Bootloader_Handshake_t Bootloader::processYield() {
		switch (status_) {
		case Status::TransmittingPhysicalMemoryBlocks:
			physicalMemoryMapTransmitter_.processYield();
			return physicalMemoryMapTransmitter_.update(); //send the initial transaction magic straight away
		case Status::TransmittingMemoryMap:
			logicalMemoryMapTransmitter_.processYield();
			return logicalMemoryMapTransmitter_.update(); //send the initial transaction magic straight away
		default:
			status_ = Status::Error; //TODO make more concrete
			return handshake::abort(AbortCode::ProcessYield, static_cast<int>(status_));
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

		Flash::RAII_unlock const _;

		jumpTable.invalidate();
		Flash::AwaitEndOfErasure(); //Make sure the page erasure has finished and exit erase mode.

		//entry point is not stored as it can be derived from the isr vector
		table_copy.set_interrupt_vector(isr_vector);
		table_copy.set_magics();

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
			case Command::StartFirmwareReadout:
			case Command::StartBootloaderReadout:
				status_ = Status::TransmittingMemoryMap;
				transactionType_ = command == Command::StartFirmwareReadout ? TransactionType::FirmwareReadout : TransactionType::BootloaderReadout;
				logicalMemoryMapTransmitter_.startSubtransaction();
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
			break;

		case Status::TransmittingPhysicalMemoryBlocks:
			//During this stage the bootloader transmits data. The master therefore should not proceed
			return HandshakeResponse::HandshakeNotExpected;

		// Flash or bootloader update path:
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

		// Readout path:
		case Status::TransmittingMemoryMap:
			//During this stage the bootloader transmits data. The master therefore should not proceed
			return HandshakeResponse::HandshakeNotExpected;

		case Status::UploadingFirmware:
			if (reg == Register::Command && command == Command::RestartFromAddress) {
				firmwareUploader_.restart_from_address(value);
				return HandshakeResponse::Ok;
			}
			else
				return HandshakeResponse::HandshakeNotExpected;

		case Status::TransmittingMetadata:
			//During this stage the bootloader transmits data. The master therefore should not proceed
			return HandshakeResponse::HandshakeNotExpected;


		// Error path:
		case Status::EFU:
		case Status::Error:
		case Status::ComunicationStalled:
			return HandshakeResponse::BootloaderInError;
		}
		assert_unreachable();
	}

	void Bootloader::processHandshakeAck(HandshakeResponse const response) {
		// TODO this should check the response
		switch (status_) {
		case Status::TransmittingPhysicalMemoryBlocks:
			if (physicalMemoryMapTransmitter_.shouldYield()) {
				physicalMemoryMapTransmitter_.endSubtransaction();

				logicalMemoryMapReceiver_.startSubtransaction();
				status_ = Status::ReceivingFirmwareMemoryMap;

				canManager.yieldCommunication();
			}
			else
				canManager.SendHandshake(physicalMemoryMapTransmitter_.update());
			return;

		case Status::TransmittingMemoryMap:
			// TODO this actually never checks the handshake response...
			if (logicalMemoryMapTransmitter_.done()) {
				logicalMemoryMapTransmitter_.endSubtransaction();
				status_ = Status::TransmittingMetadata;

				metadataTransmitter_.startSubtransaction();
				canManager.SendHandshake(metadataTransmitter_.update());
			}
			else
				canManager.SendHandshake(logicalMemoryMapTransmitter_.update());
			return;

		case Status::TransmittingMetadata:
			if (metadataTransmitter_.done()) {
				metadataTransmitter_.endSubtransaction();
				status_ = Status::UploadingFirmware;

				firmwareUploader_.startSubtransaction(logicalMemoryMapTransmitter_.logical_memory_map());
				firmwareUploader_.update();
			}
			else
				canManager.SendHandshake(metadataTransmitter_.update());
			return;

		case Status::UploadingFirmware:
			if (firmwareUploader_.done()) {
				firmwareUploader_.endSubtransaction();
				status_ = Status::Ready;
			}
			else
				firmwareUploader_.update();

			return;
		default:
			status_ = Status::Error; //TODO make more concrete
			return;
		}
		assert_unreachable();
	}

	void Bootloader::processDataAck(Bootloader_WriteResult result) {
		switch (status_) {
			case Status::UploadingFirmware:
				firmwareUploader_.handle_data_ack();
				break;

			default:
				assert_unreachable();
		}
	}

	void Bootloader::update() {
		switch (status_) {
			case Status::UploadingFirmware:
				if (firmwareUploader_.sending_data())
					firmwareUploader_.update();
				break;

			default:
				// No operation, everything is handled from CAN message handlers such as processHandshake
				break;
		}
	}

	void Bootloader::setEntryReason(EntryReason reason) {
		bool const requested_during_startup_check = entryReason_ == EntryReason::StartupCanBusCheck && reason == EntryReason::Requested;
		//Make sure this is called only once or after startup check detects a BL request
		assert(entryReason_ == EntryReason::Unknown || requested_during_startup_check);
		assert(reason != EntryReason::Unknown); //Sanity check

		entryReason_ = reason;
	}

}
