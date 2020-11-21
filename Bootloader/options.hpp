/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 *
 * Contains compile-time configuration options controlling the program's behaviour
 */

#pragma once

#include <ufsel/bit_operations.hpp>
#include <library/units.hpp>
#include <algorithm>
#include <CANdb/can_Bootloader.h>

namespace boot {

	struct MemoryBlock {
		std::uint32_t address, length;

		constexpr bool operator==(MemoryBlock const& rhs) const {
			return rhs.address == address && rhs.length == length;
		}
	};

	enum class PhysicalBlockSizes {
		same,
		different
	};

	template<std::size_t N, std::size_t BlockSize, std::size_t BaseAddress>
	struct EquidistantMemoryGenerator {

		static constexpr std::size_t size() {
			return N;
		}

		static constexpr MemoryBlock block(int index) {
			return {BaseAddress + index * BlockSize, BlockSize};
		}

		constexpr MemoryBlock operator[](int index) const {
			return block(index);
		}

		struct iterator {
			std::uint32_t index_ = 0;

			constexpr MemoryBlock operator*() const {
				return block(index_);
			}

			constexpr auto& operator++() { ++index_; return *this; }

			constexpr bool operator!= (iterator) const {
				return index_ < size();
			}

			constexpr bool operator== (iterator) const {
				return index_ < size();
			}

			constexpr iterator end() { return {}; }
			constexpr iterator& begin() { return *this; }
		};

		constexpr auto begin() const {
			return iterator();
		}
		constexpr auto end() const {
			return iterator{};
		}
	};


	namespace customization {

		//The ISR vector is required to be 512B aligned, this holds for all Cortex M3s I have seen, but you are free to adjust based on your needs.
		constexpr int isrVectorAlignmentBits = 9;

		//The number of physical blocks available on the target chip 
		constexpr std::uint32_t physicalBlockCount = 128;
		//Start of flash memory. Must be kept in sync with the linker script
		constexpr std::uint32_t flashMemoryBaseAddress = 0x0800'0000;

		//Used only iff the flash memory consists of blocks of the same size
		constexpr InformationSize physicalBlockSize = 2048_B;

		//Controls, whether the flash memory consists of blocks of the same size (f1xx flash pages) or different sizes (f4xx sectors)
		constexpr PhysicalBlockSizes physicalBlockSizePolicy = PhysicalBlockSizes::same;

		//Fill this array with memory blocks iff the memory blocks have unequal sizes
		constexpr std::array<MemoryBlock, physicalBlockCount> blocksWhenSizesAreUnequal {};

		//In case of AMS/DSH, the bootloader occupies 10K of flash and the application jump table occupies two more.
		//Hence the application can not start at lower address as the start of sixth block
		//TODO this must be checked once in a while, whether it is correct...
		constexpr std::uint32_t firstBlockAvailableToApplication = 6;

		//Bootloader target identification
		constexpr Bootloader_BootTarget thisUnit = Bootloader_BootTarget_AMS;
	}



	auto constexpr getMemoryBlocks() {
		if constexpr (customization::physicalBlockSizePolicy == PhysicalBlockSizes::same)
			return EquidistantMemoryGenerator<customization::physicalBlockCount, customization::physicalBlockSize.toBytes(), customization::flashMemoryBaseAddress>{};
		else
			return customization::blocksWhenSizesAreUnequal;
	}
	constexpr auto physicalMemoryBlocks = getMemoryBlocks();
	static_assert(physicalMemoryBlocks.size() == 128); //sanity check that this template magic works

	constexpr bool enableAssert = true;

	constexpr auto otherBLdetectionTime = 1_s;

	constexpr std::uint32_t isrVectorAlignmentMask = ufsel::bit::bitmask_of_width(customization::isrVectorAlignmentBits);

	constexpr std::uint32_t smallestPageSize = (*std::min_element(physicalMemoryBlocks.begin(), physicalMemoryBlocks.end(),[](auto const &a, auto const &b) {return a.length < b.length;} )).length;

}
