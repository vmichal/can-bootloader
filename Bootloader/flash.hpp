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
#include "options.hpp"

#include <span>
#include <cstddef>
#include <cstdint>

namespace boot {

	constexpr std::uint32_t end(MemoryBlock const & block) { return block.address + block.length; }


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
		NotReady,
		DiscontinuousWriteAccess
	};

	struct Flash {

		constexpr static bool pagesHaveSameSize() { return customization::physicalBlockSizePolicy == PhysicalBlockSizes::same; };

		static std::size_t const availableMemory;
		static std::uint32_t const jumpTableAddress;
		static std::uint32_t const applicationAddress;

		static void Lock() { ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_LOCK); }
		static void Unlock() {
			FLASH->KEYR = 0x45670123;
			FLASH->KEYR = 0xcdef89ab;
		}

		struct RAII_unlock {
			RAII_unlock() { Unlock(); }
			~RAII_unlock() { Lock(); }
		};

		static void AwaitEndOfErasure();
		static bool ErasePage(std::uint32_t pageAddress);
		static WriteStatus Write(std::uint32_t flashAddress, std::uint32_t word);
		static WriteStatus Write(std::uint32_t flashAddress, std::uint16_t halfWord);

		static bool isAvailableAddress(std::uint32_t address) {
			return addressOrigin(address) == AddressSpace::AvailableFlash;
		}

		constexpr static int getEnclosingBlockIndex(std::uint32_t address) {
			if constexpr (pagesHaveSameSize()) {
				constexpr std::uint32_t block_size = customization::physicalBlockSize.toBytes();
				constexpr std::uint32_t base_address = customization::flashMemoryBaseAddress;
				return (address - base_address) / block_size;
			}
			else {
				for (std::size_t i = 0; i < size(physicalMemoryBlocks); ++i) {
					MemoryBlock const& block = physicalMemoryBlocks[i];
					if (block.address <= address && address < end(block))
						return i;
				}
				return -1;
			}
		}
		
		constexpr static MemoryBlock getEnclosingBlock(std::uint32_t address) {
			if constexpr (pagesHaveSameSize()) {
				return physicalMemoryBlocks[getEnclosingBlockIndex(address)];
			}
			else {
				return physicalMemoryBlocks[getEnclosingBlockIndex(address)];
			}
		}

		constexpr static std::uint32_t makePageAligned(std::uint32_t address) {
			return getEnclosingBlock(address).address;
		}

		constexpr static bool isPageAligned(std::uint32_t address) {
			return makePageAligned(address) == address;
		}

		static AddressSpace addressOrigin(std::uint32_t address);
		static AddressSpace addressOrigin_located_in_flash(std::uint32_t address) __attribute__((section(".executed_from_flash")));
	};

	struct PhysicalMemoryMap {

		constexpr static unsigned availablePages() {return customization::physicalBlockCount - customization::firstBlockAvailableToApplication;}

		static MemoryBlock block(std::uint32_t const index) {
			assert(index < customization::physicalBlockCount);
			return physicalMemoryBlocks[index];
		}

		static bool canCover(MemoryBlock logical) {

			for (std::uint32_t i = customization::firstBlockAvailableToApplication; i <customization::physicalBlockCount; ++i) {
				MemoryBlock const physical = physicalMemoryBlocks[i];

				if (end(physical) <= logical.address)
					continue; //Ignore all physical blocks that end before the logical block even starts


				//There are pretty much three options as far as starting address is concerned:
				//1) The logical block starts at lower address -> error as we cannot cover its beginning
				//2,3) They start at the same address or the physical blocks starts on lower addres -> OK 

				if (logical.address < physical.address)
					return false; //option 1

				//option 2 - 3 will certainly cover the memory range from start of logical block to physical block end
				//assume there is shared address range:
				//the physical block starts at lower address and spans at least part of the logical block
				std::uint32_t const covered_bytes = end(physical) - logical.address;
				if (covered_bytes >= logical.length)
					return true; //Remaining uncovered ranges of logical block have been depleted. We can cover it

				//The logical block spans further than the current physical. Shrink it and continue to next physical block.
				logical.address += covered_bytes; //remove the covered address range from remaining logical block
				logical.length -= covered_bytes;
			}

			return false; //We have run out of physical memory blocks
		}


	};

	struct ApplicationJumpTable {
		constexpr static std::uint32_t expected_magic1_value = 0xb16'b00b5;
		constexpr static std::uint32_t expected_magic2_value = 0xcafe'babe;
		constexpr static std::uint32_t expected_magic3_value = 0xdead'beef;
		constexpr static std::uint32_t expected_magic4_value = 0xfeed'd06e;
		constexpr static std::uint32_t expected_magic5_value = 0xface'b00c;

		constexpr static std::uint32_t metadata_valid_magic_value = 0x0f0c'd150;

	private:
		constexpr static int members_before_segment_array = 9;
	public:

		std::uint32_t magic1_;
		//If this contains the magic value, the rest of firmware metadata is valid. It is not necessary for the bootloader,
		//for example the firmware size is only useful for firmware readout.
		std::uint32_t metadata_valid_magic_;
		std::uint32_t magic2_;
		//The interruptVector must be valid at all times when magic[1-5] are valid.
		std::uint32_t interruptVector_;
		std::uint32_t magic3_;
		std::uint32_t firmwareSize_;
		std::uint32_t magic4_;
		std::uint32_t logical_memory_block_count_;
		std::uint32_t magic5_;
		std::array<MemoryBlock, (smallestPageSize - sizeof(std::uint32_t)*members_before_segment_array) / sizeof(MemoryBlock)> logical_memory_blocks_;

		//Returns true iff all magics are valid
		bool magicValid() const __attribute__((section(".executed_from_flash")));
		bool has_valid_metadata() const;

		//Clear the memory location with jump table
		void invalidate();

		void write_magics();
		void write_metadata(InformationSize firmware_size, std::span<MemoryBlock const> logical_memory_blocks);

		void write_interrupt_vector(std::uint32_t isr_vector);
	};

	static_assert(sizeof(ApplicationJumpTable) <= smallestPageSize, "The application jump table must fit within one page of flash.");

	inline ApplicationJumpTable jumpTable __attribute__((section("jumpTableSection")));
}
