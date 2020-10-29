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
		auto const& logicalMemoryBlocks() const { return blocks_; }
		std::uint32_t logicalMemoryBlockCount() const { return blocks_received_; }

		void startSubtransaction() {
			status_ = Status::pending;
			remaining_bytes_ = Flash::availableMemory;
		}

		bool done() const { return status_ == Status::done; }
		bool error() const { return status_ == Status::error; }

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
		std::array<std::uint32_t, Flash::pageCountTotal> erased_pages_;
		std::uint32_t erased_pages_count_ = 0, expectedPageCount_ = 0;

	public:
		bool done() const { return status_ == Status::done; }
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);

		auto const& erased_pages() const { return erased_pages_; }
		std::uint32_t erased_page_count() const { return erased_pages_count_; }
		HandshakeResponse tryErasePage(std::uint32_t address);
	};

	class FirmwareDownloader {
		enum class Status {
			unitialized,
			pending,
			waitingForFirmwareSize,
			receivingData,
			receivedChecksum,
			done,
			error
		};

		Status status_ = Status::unitialized;
		InformationSize firmware_size_ = 0_B, written_bytes_ = 0_B;
		std::uint32_t checksum_ = 0;

	public:
		bool done() const { return status_ == Status::done; }
		bool data_expected() const { return status_ == Status::receivingData; }
		void startSubtransaction() { status_ = Status::pending; }
		HandshakeResponse receive(Register, Command, std::uint32_t);
		WriteStatus write(std::uint32_t address, std::uint16_t half_word);

		InformationSize expectedSize() const { return firmware_size_; }
		InformationSize actualSize() const { return written_bytes_; }
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
			std::uint32_t logical_memory_block_count_ = 0; //Set during the handshake. Number of firmware memory blocks
			decltype(ApplicationJumpTable::logical_memory_blocks_) logical_memory_blocks_;
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

		WriteStatus checkAddressBeforeWrite(std::uint32_t address);

		std::uint32_t predictedWriteDestination_ = 0;

		//Sets the jumpTable
		void finishFlashingTransaction() const;
		FirmwareData summarizeFirmwareData() const;

		constexpr static auto magic_ = "Heli";
	public:
		constexpr static std::uint32_t transactionMagic = magic_[0] | magic_[1] << 8 | magic_[2] << 16 | magic_[3] << 24;


		//TODO make this a customization point
		constexpr static Bootloader_BootTarget thisUnit = Bootloader_BootTarget_AMS;
		[[nodiscard]]
		Status status() const { return status_; }
		[[nodiscard]]
		bool transactionInProgress() const { return !isPassive() && status_ != Status::Error && status_ != Status::Ready; }

		void enterPassiveMode() { status_ = Status::OtherBootloaderDetected; }
		void exitPassiveMode() { status_ = Status::Ready; }
		[[nodiscard]]
		bool isPassive() const { return status_ == Status::OtherBootloaderDetected; }

		[[noreturn]]
		static void resetToApplication();

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


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
	}
}

