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
		Unknown,
		Flashing,
		BootloaderUpdate
		//TODO implement FirmwareDump
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
		std::array<MemoryBlock, customization::physicalBlockCount> erased_pages_;
		std::uint32_t erased_pages_count_ = 0, expectedPageCount_ = 0;

	public:
		bool done() const { return status_ == Status::done; }
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);

		std::span<MemoryBlock const> erased_pages() const { return std::span{erased_pages_.begin(), erased_pages_count_}; }
		HandshakeResponse tryErasePage(std::uint32_t address);

		using BootloaderSubtransactionBase::BootloaderSubtransactionBase;
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
		std::uint32_t checksum_ = 0;

		std::span<MemoryBlock const> erasedBlocks_;
		std::span<MemoryBlock const> firmwareBlocks_;
		std::size_t current_block_index_ = 0;
		std::uint32_t blockOffset_ = 0;

		std::uint32_t calculate_checksum(WriteableIntegral auto data) {
			std::uint32_t result = 0;
			int const half_words = sizeof(data) / sizeof(std::uint16_t);
			for (int half = 0; half < half_words; ++half) {
				int const shift = std::numeric_limits<std::uint16_t>::digits * half;
				result += (data >> shift) & std::numeric_limits<std::uint16_t>::max();
			}
			return result;
		}

		static bool do_schedule_write(std::uint32_t const address, WriteableIntegral auto data) {
			static_assert(sizeof(data) >= sizeof(Flash::nativeType));
			if constexpr (sizeof(data) == sizeof(Flash::nativeType)) {
				// No need to go through the for loop since only one element is written.
				return Flash::ScheduleBufferedWrite(address, data);
			}
			else {
				for (std::size_t offset = 0; offset < sizeof(data); offset += sizeof(Flash::nativeType)) {
					if (bool const ret = Flash::ScheduleBufferedWrite<Flash::nativeType>(address + offset, data); !ret)
						return false;
					data >>= sizeof(Flash::nativeType) * 8;
				}
				return true;
			}
		}

		static constexpr int calculate_padding_width(std::uint32_t address, WriteableIntegral auto const data, MemoryBlock const * next_block) {
			std::uint32_t const data_end_address = address + sizeof(data); //this is the address one past last byte written
			// The address one past the containing flash native type
			std::uint32_t const native_type_end = (data_end_address + sizeof(Flash::nativeType) - 1) & ~(sizeof(Flash::nativeType) - 1);
			// We either need to fill bytes from end of data to end of native type or less if the next block starts earlier
			std::uint32_t const padding_end = next_block ? std::min(native_type_end, end(*next_block)) : native_type_end;

			return padding_end - data_end_address;
		}

		constexpr void schedule_data_write(std::uint32_t const address, WriteableIntegral auto const data, bool is_last_write_in_logical_block) {
			if constexpr (sizeof(data) >= sizeof(Flash::nativeType)) {
				// there is no need to add padding
				bool const success = do_schedule_write(address, data);
				assert(success);
			}
			else {
				// sizeof(data) < sizeof(Flash::nativeType)
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

		WriteStatus write(std::uint32_t const address, WriteableIntegral auto const data) {
			assert(address % sizeof(data) == 0 && "Address not aligned.");

			MemoryBlock const & current_block = firmwareBlocks_[current_block_index_];
			bool const is_last_write_in_logical_block = blockOffset_ + sizeof(data)  == current_block.length;

			schedule_data_write(address, data, is_last_write_in_logical_block);

			int times_written = 0;
			WriteStatus writeStatus;
			for (; ;++times_written) {
				writeStatus = Flash::tryPerformingBufferedWrite();
				if (writeStatus != WriteStatus::Ok)
					break;
			}

			if (is_last_write_in_logical_block) {
				assert(times_written > 0);
				assert(Flash::writeBufferIsEmpty());
			}

			// Update internal counters etc
			checksum_ += calculate_checksum(data);
			written_bytes_ += InformationSize::fromBytes(sizeof(data));
			blockOffset_ += sizeof(data);
			if (is_last_write_in_logical_block) {
				blockOffset_ = 0;
				if (++current_block_index_ == size(firmwareBlocks_))
					status_ = Status::noMoreDataExpected;
			}

			return writeStatus;
		}

	public:
		WriteStatus checkAddressBeforeWrite(std::uint32_t address);
		WriteStatus check_and_write(std::uint32_t address, WriteableIntegral auto data) {
			if (!data_expected())
				return WriteStatus::NotReady;

			if (WriteStatus const ret = checkAddressBeforeWrite(address); ret != WriteStatus::Ok)
				return ret;

			return write(address, data);
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
	};


	class Bootloader {

		struct FirmwareData {
			InformationSize expectedBytes_ = 0_B; //Set during the handshake. Number of bytes to be written
			InformationSize writtenBytes_ = 0_B; //The current number of bytes written. It is expected to equal the value expectedBytes_.
			std::uint32_t entryPoint_ = 0; //Address of the entry point of flashed firmware
			std::uint32_t interruptVector_ = 0; //Address of the interrupt table of flashed firmware
			std::span<MemoryBlock const> logical_memory_blocks_;
		};

		PhysicalMemoryMapTransmitter physicalMemoryMapTransmitter_;
		LogicalMemoryMapReceiver logicalMemoryMapReceiver_;
		PhysicalMemoryBlockEraser physicalMemoryBlockEraser_;
		FirmwareDownloader firmwareDownloader_;
		MetadataReceiver metadataReceiver_;

		Status status_ = Status::Ready;
		bool stall_ = false;
		TransactionType transactionType_ = TransactionType::Unknown;
		CanManager& can_;
		static inline EntryReason entryReason_ = EntryReason::Unknown;

	public:
		[[nodiscard]] bool updatingBootloader() const { return transactionType_ == TransactionType::BootloaderUpdate; }
		[[nodiscard]] AddressSpace expectedAddressSpace() const { return updatingBootloader() ? AddressSpace::BootloaderFlash : AddressSpace::ApplicationFlash; }

	private:
		//Sets the jumpTable
		void finishFlashingTransaction() const;
		FirmwareData summarizeFirmwareData() const;

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

		WriteStatus write(std::uint32_t address, WriteableIntegral auto data) {
			assert(firmwareDownloader_.data_expected());
			auto const ret = firmwareDownloader_.check_and_write(address, data);
			if (firmwareDownloader_.expectedSize() == firmwareDownloader_.actualSize())
				can_.SendDataAck(address, boot::WriteStatus::Ok);
			return ret;
		}

		HandshakeResponse setNewVectorTable(std::uint32_t isr_vector);
		HandshakeResponse processHandshake(Register reg, Command command, std::uint32_t value);
		void processHandshakeAck(HandshakeResponse response);

		Bootloader_Handshake_t processYield();
		static HandshakeResponse validateVectorTable(AddressSpace expected_space, std::uint32_t address);

		static void setEntryReason(EntryReason);
		[[nodiscard]]
		static EntryReason entryReason() { return entryReason_; }

		[[nodiscard]]
		static bool startupCheckInProgress() {
			return entryReason_ == EntryReason::StartupCanBusCheck;
		}

		explicit Bootloader(CanManager& can) :
			physicalMemoryMapTransmitter_{*this},
			logicalMemoryMapReceiver_{*this},
			physicalMemoryBlockEraser_{*this},
			firmwareDownloader_{*this},
			metadataReceiver_{*this},
				can_{can} {}
	};

	static_assert(Bootloader::transactionMagic == 0x696c6548); //This value is stated in the protocol description

	namespace handshake {
		constexpr Bootloader_Handshake_t get(Register reg, Command com, std::uint32_t value) {
			Bootloader_Handshake_t const msg{
				.Register = static_cast<Bootloader_Register>(reg),
				.Command = static_cast<Bootloader_Command>(com),
				.Target = customization::thisUnit,
				.Value = value
			};
			return msg;
		}

		constexpr Bootloader_Handshake_t get(Command com, Register reg, std::uint32_t value) {
			return get(reg, com, value);
		}

		constexpr Bootloader_Handshake_t transactionMagic = get(Register::TransactionMagic, Command::None, Bootloader::transactionMagic);
		constexpr Bootloader_Handshake_t abort = get(Register::Command, Command::AbortTransaction, 0);
		constexpr Bootloader_Handshake_t stall = get(Register::Command, Command::StallSubtransaction, 0);
		constexpr Bootloader_Handshake_t resume = get(Register::Command, Command::ResumeSubtransaction, 0);
	}
}

