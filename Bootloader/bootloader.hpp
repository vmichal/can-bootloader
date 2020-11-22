/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include <cstdint>
#include <array>
#include <type_traits>
#include <optional>
#include <span>

#include <library/assert.hpp>
#include <library/units.hpp>

#include "flash.hpp"
#include "can_Bootloader.h"
#include "enums.hpp"
#include "canmanager.hpp"

namespace boot {


	enum class TransactionType {
		Unknown,
		Flashing,
		//TODO implement FirmwareDump
	};

	class PhysicalMemoryMapTransmitter {
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
		bool done() const { return status_ == Status::done; }
		bool shouldYield() const { return status_ == Status::shouldYield; }
		bool error() const { return status_ == Status::error; }
		void startSubtransaction() { status_ = Status::pending; }
		void endSubtransaction() { status_ = Status::done; }
		void processYield() { status_ = Status::masterYielded; }
		Bootloader_Handshake_t update();
	};

	class FirmwareMemoryMapReceiver {
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
		void startSubtransaction() {
			status_ = Status::pending;
			remaining_bytes_ = Flash::availableMemory;
		}

		bool done() const { return status_ == Status::done; }
		bool error() const { return status_ == Status::error; }

		std::span<MemoryBlock const> logicalMemoryBlocks() const { return std::span{blocks_.begin(), blocks_received_}; }
		HandshakeResponse receive(Register reg, Command com, std::uint32_t value);
	};

	class PhysicalMemoryBlockEraser {
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
	};

	class FirmwareDownloader {
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

		WriteStatus do_write(std::uint32_t address, std::uint16_t half_word);
	public:
		WriteStatus checkAddressBeforeWrite(std::uint32_t address);
		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);

		bool done() const { return status_ == Status::done; }
		bool data_expected() const { return status_ == Status::receivingData; }
		void startSubtransaction(std::span<MemoryBlock const> erasedBlocks, std::span<MemoryBlock const> firmwareBlocks) { 
			erasedBlocks_ = erasedBlocks;
			firmwareBlocks_ = firmwareBlocks;
			status_ = Status::pending; 
		}
		HandshakeResponse receive(Register, Command, std::uint32_t);

		InformationSize expectedSize() const { return firmware_size_; }
		InformationSize actualSize() const { return written_bytes_; }

		std::uint32_t expectedWriteLocation() const { 
			return firmwareBlocks_[current_block_index_].address + blockOffset_;
		}
	};

	class MetadataReceiver {
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
		bool done() const { return status_ == Status::done; }
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);

		std::uint32_t entry_point() const { return entry_point_; }
		std::uint32_t isr_vector() const { return isr_vector_; }

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
		FirmwareMemoryMapReceiver firmwareMemoryMapReceiver_;
		PhysicalMemoryBlockEraser physicalMemoryBlockEraser_;
		FirmwareDownloader firmwareDownloader_;
		MetadataReceiver metadataReceiver_;

		Status status_ = Status::Ready;
		TransactionType transactionType_ = TransactionType::Unknown;
		CanManager& can_;
		static inline EntryReason entryReason_ = EntryReason::DontEnter;


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

		[[nodiscard]]
		Status status() const { return status_; }
		[[nodiscard]]
		bool transactionInProgress() const { return !isPassive() && status_ != Status::Error && status_ != Status::Ready; }

		void enterPassiveMode() { status_ = Status::OtherBootloaderDetected; }
		void exitPassiveMode() { status_ = Status::Ready; }
		[[nodiscard]]
		bool isPassive() const { return status_ == Status::OtherBootloaderDetected; }

		[[noreturn]]
		static void resetTo(std::uint16_t code);

		WriteStatus write(std::uint32_t address, std::uint16_t half_word) {
			auto const ret = firmwareDownloader_.write(address, half_word);
			if (firmwareDownloader_.expectedSize() == firmwareDownloader_.actualSize())
				can_.SendDataAck(address, boot::WriteStatus::Ok);
			return ret;
		}
		WriteStatus write(std::uint32_t address, std::uint32_t word) {
			auto const ret = firmwareDownloader_.write(address, word);
			if (firmwareDownloader_.expectedSize() == firmwareDownloader_.actualSize())
				can_.SendDataAck(address, boot::WriteStatus::Ok);
			return ret;
		}


		HandshakeResponse processHandshake(Register reg, Command command, std::uint32_t value);
		void processHandshakeAck(HandshakeResponse response);

		Bootloader_Handshake_t processYield();

		static void setEntryReason(EntryReason);

		static EntryReason entryReason() { return entryReason_; }

		Bootloader(CanManager& can) : can_{can} {}
	};

	static_assert(Bootloader::transactionMagic == 0x696c6548); //This value is stated in the protocol description

	namespace handshake {
		constexpr Bootloader_Handshake_t get(Register reg, Command com, std::uint32_t value) {
			Bootloader_Handshake_t const msg{
				.Register = static_cast<Bootloader_Register>(reg),
				.Command = static_cast<Bootloader_Command>(com),
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

