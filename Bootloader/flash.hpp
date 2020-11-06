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

		constexpr std::uint32_t end() const { return address + length; }
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

		//Customization points:
		static constexpr std::size_t pageCountTotal = 128; //taken from the reference manual
		static constexpr std::size_t pageAlignmentClearedBits = 11; //taken from the reference manual


		static constexpr std::uint32_t pageSize = 1 << pageAlignmentClearedBits;
		static constexpr std::uint32_t pageAlignmentMask = ufsel::bit::bitmask_of_width(pageAlignmentClearedBits);

		static std::size_t const availableMemory;
		static std::uint32_t const jumpTableAddress;
		static std::uint32_t const applicationAddress;

		static void Lock() { FLASH_Lock(); }
		static void Unlock() { FLASH_Unlock(); }

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
		static AddressSpace addressOrigin_located_in_flash(std::uint32_t address) __attribute__((section(".executed_from_flash")));
	};

	struct PhysicalMemoryMap {
		constexpr static PhysicalBlockSizes physicalBlockSizes = PhysicalBlockSizes::same; //TODO customization point
		static std::uint32_t availablePages() { return Flash::availableMemory / Flash::pageSize; }

		static MemoryBlock block(std::uint32_t const index) {

			if constexpr (physicalBlockSizes == PhysicalBlockSizes::same) {
				assert(index < availablePages());

				std::uint32_t const address = Flash::applicationAddress + index * Flash::pageSize;
				std::uint32_t const length = Flash::pageSize;

				return {address, length};
			}
			else {
				constexpr std::array<MemoryBlock, Flash::pageCountTotal> blocks_ {{0,0}};
				constexpr std::uint32_t firstApplicationBlock = 2; //TODO customization point
				assert(index + firstApplicationBlock < availablePages());
				
				return blocks_[firstApplicationBlock];
			}
		}

		struct iterator {
			struct end_sentinel_t {};

			std::uint32_t index_;

			MemoryBlock operator*() const {
				return block(index_);
			}

			auto& operator++() { ++index_; return *this; }

			bool operator!= (end_sentinel_t) const {
				return index_ < availablePages();
			}

			end_sentinel_t end() { return {}; }
			iterator& begin() { return *this; }
		};

		static iterator iterate() {
			return {};
		}

		static bool canCover(MemoryBlock logical) {

			for (MemoryBlock const& physical : iterate()) {

				if (physical.end() <= logical.address)
					continue; //Ignore all physical blocks that end before the logical block even starts


				//There are pretty much three options as far as starting address is concerned:
				//1) The logical block starts at lower address -> error as we cannot cover its beginning
				//2,3) They start at the same address or the physical blocks starts on lower addres -> OK 

				if (logical.address < physical.address)
					return false; //option 1

				//option 2 - 3 will certainly cover the memory range from start of logical block to physical block end
				//assume there is shared address range:
				//the physical block starts at lower address and spans at least part of the logical block
				std::uint32_t const covered_bytes = physical.end() - logical.address;
				if (covered_bytes >= logical.length)
					return true; //Remaining uncovered ranges of logical block have been depleted. We can cover it

				//The logical block spans further than the current physical. Shrink it and continue to next physical block.
				logical.address += covered_bytes; //remove the covered address range from remaining logical block
				logical.length -= covered_bytes;
			}

			return false; //We have run out of physical memory blocks
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
		bool magicValid() const __attribute__((section(".executed_from_flash"))) {
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
