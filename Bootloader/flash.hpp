/*
 * eForce CAN Bootloader
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */


#pragma once

#include <ufsel/bit.hpp>
#include <library/assert.hpp>

#include <cstddef>
#include <cstdint>

#include "stm32f10x.h"

namespace boot {

	enum class PhysicalBlockSizes {
		same,
		different
	};

	struct MemoryBlock {
		std::uint32_t address, length;
	};


	enum class AddressSpace {
		BootloaderFlash,
		JumpTable,
		AvailableFlash,
		RAM,
		Unknown
	};

	enum class WriteStatus {
		Ok,
		NotInErasedMemory,
		MemoryProtected,
		NotInFlash,
		AlreadyWritten,
		Timeout,
		NotReady
	};

	struct Flash {

		static constexpr std::size_t pageCountTotal = 128; //taken from the reference manual
		static constexpr std::uint32_t pageSize = 1 << 11; //taken from the reference manual
		static constexpr std::uint32_t pageAlignmentMask = pageSize - 1;

		static std::size_t const availableMemory;
		static std::uint32_t const jumpTableAddress;
		static std::uint32_t const applicationAddress;


		static bool ErasePage(std::uint32_t pageAddress);
		static WriteStatus Write(std::uint32_t flashAddress, std::uint32_t word);
		static WriteStatus Write(std::uint32_t flashAddress, std::uint16_t halfWord);

		static bool isAvailableAddress(std::uint32_t address) {
			return addressOrigin(address) == AddressSpace::AvailableFlash;
		}

		static bool isPageAligned(std::uint32_t address) {
			return ufsel::bit::all_cleared(address, pageAlignmentMask);
		}

		static std::uint32_t makePageAligned(std::uint32_t address) {
			return ufsel::bit::clear(address, pageAlignmentMask);
		}

		static AddressSpace addressOrigin(std::uint32_t address);
	};

	struct PhysicalMemoryMap {
		constexpr static PhysicalBlockSizes physicalBlockSizes = PhysicalBlockSizes::same; //TODO customization point
		static inline std::uint32_t const availablePages = Flash::availableMemory / Flash::pageSize;

		static inline MemoryBlock block(std::uint32_t const index) {

			if constexpr (physicalBlockSizes == PhysicalBlockSizes::same) {
				assert(index < availablePages);

				std::uint32_t const address = Flash::applicationAddress + index * Flash::pageSize;
				std::uint32_t const length = Flash::pageSize;

				return {address, length};
			}
			else {
				constexpr std::array<MemoryBlock, Flash::pageCountTotal> blocks_ {{0,0}};
				constexpr std::uint32_t firstApplicationBlock = 2; //TODO customization point
				assert(index+ firstApplicationBlock < availablePages);
				
				return blocks_[firstApplicationBlock];
			}
		}

	};

	struct BackupDomain {
		constexpr static std::uint16_t reset_value = 0x00'00; //value after power reset. Enter the application
		//Writing this value to the Backup register 1 requests entering the bootloader after reset
		constexpr static std::uint16_t bootloader_magic = 0xB007;
		constexpr static std::uint16_t application_magic = 0xC0DE; //enter the application

		inline static std::uint16_t volatile& bootControlRegister = BKP->DR1;

	};

	struct ApplicationJumpTable {
		constexpr static std::uint32_t expected_magic1_value = 0xb16'b00b5;
		constexpr static std::uint32_t expected_magic2_value = 0xcafe'babe;
		constexpr static std::uint32_t expected_magic3_value = 0xdead'beef;
		constexpr static std::uint32_t expected_magic4_value = 0xfeed'd06e;
		constexpr static std::uint32_t expected_magic5_value = 0xface'b00c;

	private:
		constexpr static int members_before_segment_array = 8;
	public:

		std::uint32_t magic1_;
		std::uint32_t magic2_;
		std::uint32_t interruptVector_;
		std::uint32_t magic3_;
		std::uint32_t firmwareSize_;
		std::uint32_t magic4_;
		std::uint32_t logical_memory_block_count_;
		std::uint32_t magic5_;
		std::array<MemoryBlock, (Flash::pageSize - sizeof(std::uint32_t)*members_before_segment_array) / sizeof(MemoryBlock)> logical_memory_blocks_;

		//Returns true iff all magics are valid
		bool magicValid() const {
			return magic1_ == expected_magic1_value
				&& magic2_ == expected_magic2_value
				&& magic3_ == expected_magic3_value
				&& magic4_ == expected_magic4_value
				&& magic5_ == expected_magic5_value;
		}

		//Clear the memory location with jump table
		void invalidate();
	};

	static_assert(sizeof(ApplicationJumpTable) <= Flash::pageSize, "The application jump table must fit within one page of flash.");

	inline ApplicationJumpTable jumpTable __attribute__((section("jumpTableSection")));
}
