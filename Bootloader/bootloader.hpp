/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include <cstdint>
#include <array>
#include <type_traits>
#include <optional>
#include <span>

#include <ufsel/assert.hpp>
#include <ufsel/units.hpp>

#include "flash.hpp"
#include "can_Bootloader.h"
#include "enums.hpp"
#include "canmanager.hpp"

namespace boot {

	enum class TransactionType {
		Unknown = 0,
		Flashing = 1,
		BootloaderUpdate = 2,
		FirmwareReadout = 3,
		BootloaderReadout = 4,
	};

	class Bootloader;

	struct  BootloaderSubtransactionBase {
		Bootloader const& bootloader_;

		BootloaderSubtransactionBase(Bootloader const & bl) : bootloader_{bl} {}
	};

	class PhysicalMemoryMapTransmitter : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,
			masterYielded,
			sentInitialMagic,

			sendingBlockAddress,
			sendingBlockLength,
			shouldYield,
			done,
			error,
		};

		Status status_ = Status::uninitialized;
		std::uint32_t blocks_sent_ = 0;

	public:
		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool shouldYield() const { return status_ == Status::shouldYield; }
		[[nodiscard]] bool error() const { return status_ == Status::error; }
		void startSubtransaction() { status_ = Status::pending; }
		void endSubtransaction() { status_ = Status::done; }
		void processYield() { status_ = Status::masterYielded; }
		Bootloader_Handshake_t update();

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::uninitialized;
			blocks_sent_ = 0;
		}
	};

	class LogicalMemoryMapReceiver : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,
			waitingForBlockCount,
			waitingForBlockAddress,
			waitingForBlockLength,

			done,
			error
		};

		decltype(ApplicationJumpTable::logical_memory_blocks_) blocks_;
		std::uint32_t remaining_bytes_ = 0;
		std::uint32_t blocks_received_ = 0;

		std::uint32_t blocks_expected_ = 0;

		Status status_ = Status::uninitialized;

	public:
		void startSubtransaction();

		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool error() const { return status_ == Status::error; }

		[[nodiscard]] std::span<MemoryBlock const> logicalMemoryBlocks() const { return std::span{blocks_.begin(), blocks_received_}; }
		HandshakeResponse receive(Register reg, Command com, std::uint32_t value);

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			remaining_bytes_ = 0;
			blocks_received_ = 0;
			blocks_expected_ = 0;
			status_ = Status::uninitialized;
		}
	};

	// TODO Does not check handshake acknowledges!
	class LogicalMemoryMapTransmitter : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,
			masterYielded,
			sentInitialMagic,

			sendingBlockAddress,
			sendingBlockLength,
			done,
			error,
		};

		Status status_ = Status::uninitialized;
		decltype(ApplicationJumpTable::logical_memory_blocks_) blocks_;
		std::uint32_t block_count_ = 0;
		std::uint32_t blocks_sent_ = 0;

	public:
		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool error() const { return status_ == Status::error; }
		void startSubtransaction();
		void endSubtransaction() { status_ = Status::done; }
		void processYield() { status_ = Status::masterYielded; }
		Bootloader_Handshake_t update();

		[[nodiscard]]
		std::span<MemoryBlock const> logical_memory_map() const { return std::span{blocks_.begin(), block_count_ }; }

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::uninitialized;
			blocks_sent_ = 0;
			block_count_ = 0;
		}
	};

	class PhysicalMemoryBlockEraser : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,
			waitingForMemoryBlockCount,
			waitingForMemoryBlocks,
			receivingMemoryBlocks,
			done,
			error
		};

		Status status_ = Status::uninitialized;
		std::array<MemoryBlock, customization::NumPhysicalBlocksPerBank> erased_pages_;
		std::uint32_t erased_pages_count_ = 0, expectedPageCount_ = 0;

	public:
		bool done() const { return status_ == Status::done; }
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);

		std::span<MemoryBlock const> erased_pages() const { return std::span{erased_pages_.begin(), erased_pages_count_}; }
		HandshakeResponse tryErasePage(std::uint32_t address);

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::uninitialized;
			erased_pages_count_ = 0;
			expectedPageCount_ = 0;
		}
	};

	class FirmwareDownloader : public BootloaderSubtransactionBase {
		enum class Status {
			unitialized,
			pending,
			waitingForFirmwareSize,
			receivingData,
			noMoreDataExpected,
			receivedChecksum,
			done,
			error
		};

		Status status_ = Status::unitialized;
		InformationSize firmware_size_ = 0_B, written_bytes_ = 0_B;
		std::span<MemoryBlock const> erasedBlocks_;
		std::span<MemoryBlock const> firmwareBlocks_;
		std::size_t current_block_index_ = 0;
		std::uint32_t blockOffset_ = 0;

		static int calculate_padding_width(std::uint32_t address, std::uint32_t const data, MemoryBlock const * next_block);

		void schedule_data_write(std::uint32_t const address, std::uint32_t const data, bool is_last_write_in_logical_block);

		WriteStatus write(std::uint32_t const address, std::uint32_t const data);

		WriteStatus transfer_bl_update_buffer();

		static WriteStatus update_flash_write_buffer();

	public:
		WriteStatus checkAddressBeforeWrite(std::uint32_t address, std::uint32_t data) const;

		WriteStatus check_and_write(std::uint32_t address, std::uint32_t const data) {
			if (!data_expected())
				return WriteStatus::NotReady;

			if (WriteStatus const ret = checkAddressBeforeWrite(address, data); ret != WriteStatus::Ok)
				return ret;

			auto const write_status = write(address, data);

			// Update internal counters etc
			written_bytes_ += InformationSize::fromBytes(sizeof(data));
			blockOffset_ += sizeof(data);
			if (blockOffset_ == firmwareBlocks_[current_block_index_].length) {
				blockOffset_ = 0;
				if (++current_block_index_ == size(firmwareBlocks_))
					status_ = Status::noMoreDataExpected;
			}

			return write_status;
		}

		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool data_expected() const { return status_ == Status::receivingData; }
		void startSubtransaction(std::span<MemoryBlock const> erasedBlocks, std::span<MemoryBlock const> firmwareBlocks) {
			erasedBlocks_ = erasedBlocks;
			firmwareBlocks_ = firmwareBlocks;
			status_ = Status::pending;
		}
		HandshakeResponse receive(Register, Command, std::uint32_t);

		[[nodiscard]] InformationSize expectedSize() const { return firmware_size_; }
		[[nodiscard]] InformationSize actualSize() const { return written_bytes_; }

		[[nodiscard]] std::uint32_t expectedWriteLocation() const {
			return firmwareBlocks_[current_block_index_].address + blockOffset_;
		}

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset();
	};

	class FirmwareUploader : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,

			sendFirmwareSize,
			waitingForFirmwareSizeAck,
			sendData,
			waitForDataAck,
			sentChecksum,

			done,
			error,
		};

		Status status_ = Status::uninitialized;
		std::uint32_t firmware_size_ = 0;
		std::span<MemoryBlock const> logical_memory_map_;
		MemoryBlock const* current_block_ = nullptr;
		std::uint32_t offset_in_block_ = 0;

	public:
		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool error() const { return status_ == Status::error; }
		[[nodiscard]] bool ack_expected() const { return status_ == Status::waitForDataAck; }
		[[nodiscard]] bool sending_data() const { return status_ == Status::sendData; }
		void startSubtransaction(std::span<MemoryBlock const> logical_memory_map) {
			status_ = Status::pending;
			logical_memory_map_ = logical_memory_map;
			current_block_ = std::to_address(logical_memory_map.begin());
		}
		void endSubtransaction() { status_ = Status::done; }
		void update();
		void handle_data_ack();
		void restart_from_address(std::uint32_t address) {
			auto const containing_block = std::ranges::find_if(logical_memory_map_, [address](MemoryBlock const& block) { return block.contains_address(address);});
			current_block_ = std::to_address(containing_block);
			offset_in_block_ = address - containing_block->address;
		}

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::uninitialized;
			firmware_size_ = 0;
			current_block_ = nullptr;
			offset_in_block_ = 0;
		}
	};

	class MetadataReceiver : public BootloaderSubtransactionBase {
		enum class Status {
			unitialized,
			pending,
			awaitingInterruptVector,
			awaitingEntryPoint,
			awaitingTerminatingMagic,
			done,
			error
		};

		Status status_ = Status::unitialized;
		std::uint32_t entry_point_ = 0, isr_vector_ = 0;

	public:
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);

		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] std::uint32_t entry_point() const { return entry_point_; }
		[[nodiscard]] std::uint32_t isr_vector() const { return isr_vector_; }

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::unitialized;
			entry_point_ = 0;
			isr_vector_ = 0;
		}
	};

	class MetadataTransmitter : public BootloaderSubtransactionBase {
		enum class Status {
			uninitialized,
			pending,

			sendInterruptVector,
			sendEntryPoint,
			sendEndMagic,

			done,
			error,
		};

		Status status_ = Status::uninitialized;

	public:
		[[nodiscard]] bool done() const { return status_ == Status::done; }
		[[nodiscard]] bool error() const { return status_ == Status::error; }
		void startSubtransaction() { status_ = Status::pending; }
		void endSubtransaction() { status_ = Status::done; }
		Bootloader_Handshake_t update();

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;

		void reset() {
			status_ = Status::uninitialized;
		}
	};

	class Bootloader {

		struct FirmwareData {
			InformationSize expectedBytes_ = 0_B; //Set during the handshake. Number of bytes to be written
			InformationSize writtenBytes_ = 0_B; //The current number of bytes written. It is expected to equal the value expectedBytes_.
			std::uint32_t entryPoint_ = 0; //Address of the entry point of flashed firmware
			std::uint32_t interruptVector_ = 0; //Address of the interrupt table of flashed firmware
			std::span<MemoryBlock const> logical_memory_blocks_;
		};

		// Subtransactions for flashing or bootloader update
		PhysicalMemoryMapTransmitter physicalMemoryMapTransmitter_;
		LogicalMemoryMapReceiver logicalMemoryMapReceiver_;
		PhysicalMemoryBlockEraser physicalMemoryBlockEraser_;
		FirmwareDownloader firmwareDownloader_;
		MetadataReceiver metadataReceiver_;

		// Subtransactions for firmware or bootloader readout
		LogicalMemoryMapTransmitter logicalMemoryMapTransmitter_;
		FirmwareUploader firmwareUploader_;
		MetadataTransmitter metadataTransmitter_;

		Status status_ = Status::Ready;
		bool stall_ = false;
		TransactionType transactionType_ = TransactionType::Unknown;
		static inline EntryReason entryReason_ = EntryReason::Unknown;

	public:
		[[nodiscard]] TransactionType transaction_type() const { return transactionType_; }
		[[nodiscard]] bool updatingBootloader() const { return transactionType_ == TransactionType::BootloaderUpdate; }
		[[nodiscard]] AddressSpace expectedAddressSpace() const { return updatingBootloader() ? AddressSpace::BootloaderFlash : AddressSpace::ApplicationFlash; }

	private:
		//Sets the jumpTable
		void finishFlashingTransaction() const;
		[[nodiscard]] FirmwareData summarizeFirmwareData() const;

		constexpr static auto magic_ = "Heli";
	public:
		constexpr static std::uint32_t transactionMagic = magic_[0] | magic_[1] << 8 | magic_[2] << 16 | magic_[3] << 24;

		[[nodiscard]]
		std::optional<std::uint32_t> expectedWriteLocation() const {
			if (!firmwareDownloader_.data_expected())
				return std::nullopt;
			return firmwareDownloader_.expectedWriteLocation();
		}

		bool & stalled() {return stall_;}

		[[nodiscard]]
		Status status() const { return status_; }
		[[nodiscard]]
		bool transactionInProgress() const { return transactionType_ != TransactionType::Unknown && status_ != Status::Error && status_ != Status::Ready; }

		WriteStatus write(std::uint32_t address, std::uint32_t const data) {
			assert(firmwareDownloader_.data_expected());
			auto const ret = firmwareDownloader_.check_and_write(address, data);
			if (firmwareDownloader_.expectedSize() == firmwareDownloader_.actualSize())
				canManager.SendDataAck(address, boot::WriteStatus::Ok);
			return ret;
		}

		HandshakeResponse setNewVectorTable(std::uint32_t isr_vector);
		HandshakeResponse processHandshake(Register reg, Command command, std::uint32_t value);
		void processHandshakeAck(HandshakeResponse response);
		void processDataAck(Bootloader_WriteResult result);

		Bootloader_Handshake_t processYield();
		static HandshakeResponse validateVectorTable(AddressSpace expected_space, std::uint32_t address);

		static void setEntryReason(EntryReason);
		[[nodiscard]]
		static EntryReason entryReason() { return entryReason_; }

		[[nodiscard]]
		static bool startupCheckInProgress() {
			return entryReason_ == EntryReason::StartupCanBusCheck;
		}

		explicit Bootloader() :
			physicalMemoryMapTransmitter_{*this},
			logicalMemoryMapReceiver_{*this},
			physicalMemoryBlockEraser_{*this},
			firmwareDownloader_{*this},
			metadataReceiver_{*this},
			logicalMemoryMapTransmitter_{*this},
			firmwareUploader_{*this},
			metadataTransmitter_{*this} {}

		void update();
	};

	inline Bootloader bootloader;

	static_assert(Bootloader::transactionMagic == 0x696c6548); //This value is stated in the protocol description

	namespace handshake {
		constexpr Bootloader_Handshake_t create(Register reg, Command com, std::uint32_t value) {
			Bootloader_Handshake_t const msg{
				.Register = static_cast<Bootloader_Register>(reg),
				.Command = static_cast<Bootloader_Command>(com),
				.Target = customization::thisUnit,
				.Value = value
			};
			return msg;
		}

		constexpr Bootloader_Handshake_t transactionMagic = create(Register::TransactionMagic, Command::None, Bootloader::transactionMagic);
		constexpr Bootloader_Handshake_t stall = create(Register::Command, Command::StallSubtransaction, 0);
		constexpr Bootloader_Handshake_t resume = create(Register::Command, Command::ResumeSubtransaction, 0);
		constexpr auto abort = [](AbortCode abort_code, std::uint32_t aux_code = 0) {
			return create(Register::Command, Command::AbortTransaction, aux_code << 8 | static_cast<std::uint32_t>(abort_code));
		};
	}
}

