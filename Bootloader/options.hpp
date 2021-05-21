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
#include <bit>
#include <array>

#define POS_FROM_MASK(x) std::countr_zero(x)
#define BIT_MASK(name) name ## _Msk
#ifdef BOOT_STM32F1
#include "../Drivers/stm32f10x.h"
#include "../Drivers/core_cm3.h"
#elif defined(BOOT_STM32F4)
#include "../Drivers/stm32f4xx.h"
#include "../Drivers/core_cm4.h"
#elif defined(BOOT_STM32F7)
#include "../Drivers/stm32f767xx.h"
#include "../Drivers/core_cm7.h"
#else
#error "This MCU is not supported!"
#endif

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

	template<std::size_t N, std::size_t BlockSize, std::size_t BaseAddress>
	constexpr std::size_t size(EquidistantMemoryGenerator<N, BlockSize, BaseAddress> const&gen) { return gen.size(); }

	namespace customization {

		//The ISR vector is required to be 512B aligned, this holds for all Cortex M3s I have seen, but you are free to adjust based on your needs.
		constexpr int isrVectorAlignmentBits = 9;
		//Start of flash memory. Must be kept in sync with the linker script
		constexpr std::uint32_t flashMemoryBaseAddress = 0x0800'0000;

#ifdef BOOT_STM32F4 
		//Governs the width of programming acesses to the flash memory
		constexpr unsigned flashProgrammingParallelism = 32;

		//The number of physical blocks available on the target chip 
		constexpr std::uint32_t physicalBlockCount = 12;

		//Used only iff the flash memory consists of blocks of the same size
		constexpr InformationSize physicalBlockSize = 0_B;

		//Controls, whether the flash memory consists of blocks of the same size (f1xx flash pages) or different sizes (f4xx sectors)
		constexpr PhysicalBlockSizes physicalBlockSizePolicy = PhysicalBlockSizes::different;

		//In case of AMS/DSH, the bootloader occupies 10K of flash and the application jump table occupies two more.
		//Hence the application can not start at lower address as the start of sixth block
		//TODO this must be checked once in a while, whether it is correct...
		constexpr std::uint32_t firstBlockAvailableToApplication = 2;

		//Fill this array with memory blocks iff the memory blocks have unequal sizes
		constexpr std::array<MemoryBlock, physicalBlockCount> blocksWhenSizesAreUnequal {
			MemoryBlock{0x0800'0000, ( 16_KiB).toBytes()},
			MemoryBlock{0x0800'4000, ( 16_KiB).toBytes()},
			MemoryBlock{0x0800'8000, ( 16_KiB).toBytes()},
			MemoryBlock{0x0800'C000, ( 16_KiB).toBytes()},
			MemoryBlock{0x0801'0000, ( 64_KiB).toBytes()},
			MemoryBlock{0x0802'0000, (128_KiB).toBytes()},
			MemoryBlock{0x0804'0000, (128_KiB).toBytes()},
			MemoryBlock{0x0806'0000, (128_KiB).toBytes()},
			MemoryBlock{0x0808'0000, (128_KiB).toBytes()},
			MemoryBlock{0x080A'0000, (128_KiB).toBytes()},
			MemoryBlock{0x080C'0000, (128_KiB).toBytes()},
			MemoryBlock{0x080E'0000, (128_KiB).toBytes()}
		};
#elif defined BOOT_STM32F7
		//Governs the width of programming acesses to the flash memory
		constexpr unsigned flashProgrammingParallelism = 32;

		//The number of physical blocks available on the target chip 
		constexpr std::uint32_t physicalBlockCount = 8;

		//Used only iff the flash memory consists of blocks of the same size
		constexpr InformationSize physicalBlockSize = 0_B;

		//Controls, whether the flash memory consists of blocks of the same size (f1xx flash pages) or different sizes (f4xx sectors)
		constexpr PhysicalBlockSizes physicalBlockSizePolicy = PhysicalBlockSizes::different;

		//In case of AMS/DSH, the bootloader occupies 10K of flash and the application jump table occupies two more.
		//Hence the application can not start at lower address as the start of sixth block
		//TODO this must be checked once in a while, whether it is correct...
		constexpr std::uint32_t firstBlockAvailableToApplication = 4;

		//Fill this array with memory blocks iff the memory blocks have unequal sizes
		constexpr std::array<MemoryBlock, physicalBlockCount> blocksWhenSizesAreUnequal{
			MemoryBlock{0x0800'0000, (32_KiB).toBytes()},
			MemoryBlock{0x0800'8000, (32_KiB).toBytes()},
			MemoryBlock{0x0801'0000, (32_KiB).toBytes()},
			MemoryBlock{0x0801'8000, (32_KiB).toBytes()},
			MemoryBlock{0x0802'0000, (128_KiB).toBytes()},
			MemoryBlock{0x0804'0000, (256_KiB).toBytes()},
			MemoryBlock{0x0808'0000, (256_KiB).toBytes()},
			MemoryBlock{0x080C'0000, (256_KiB).toBytes()},
	};
#elif defined BOOT_STM32F1
		//Governs the width of programming acesses to the flash memory
		constexpr unsigned flashProgrammingParallelism = 16;

		//The number of physical blocks available on the target chip 
		constexpr std::uint32_t physicalBlockCount = 128;

		//Used only iff the flash memory consists of blocks of the same size
		constexpr InformationSize physicalBlockSize = 2048_B;

		//Controls, whether the flash memory consists of blocks of the same size (f1xx flash pages) or different sizes (f4xx sectors)
		constexpr PhysicalBlockSizes physicalBlockSizePolicy = PhysicalBlockSizes::same;

		//In case of AMS/DSH, the bootloader occupies 10K of flash and the application jump table occupies two more.
		//Hence the application can not start at lower address as the start of sixth block
		//TODO this must be checked once in a while, whether it is correct...
		constexpr std::uint32_t firstBlockAvailableToApplication = 6;

		//Fill this array with memory blocks iff the memory blocks have unequal sizes
		constexpr std::array<MemoryBlock, physicalBlockCount> blocksWhenSizesAreUnequal{ };
#else
#error "This MCU is not supported"
#endif


		//Bootloader target identification
		constexpr Bootloader_BootTarget thisUnit = Bootloader_BootTarget_STW;
		//Frequency of used external oscillator
		constexpr Frequency HSE = 12_MHz;
#ifdef BOOT_STM32F1 //currently supprted only in STM32F1 mode
		constexpr bool remapCAN2 = false; //Govenrs whether the CAN2 is remapped
#endif
	}

	auto constexpr getMemoryBlocks() {
		if constexpr (customization::physicalBlockSizePolicy == PhysicalBlockSizes::same)
			return EquidistantMemoryGenerator<customization::physicalBlockCount, customization::physicalBlockSize.toBytes(), customization::flashMemoryBaseAddress>{};
		else
			return customization::blocksWhenSizesAreUnequal;
	}
	constexpr auto physicalMemoryBlocks = getMemoryBlocks();
#ifdef BOOT_STM32F1
	static_assert(physicalMemoryBlocks.size() == 128); //sanity check that this template magic works
#elif defined BOOT_STM32F4
	static_assert(physicalMemoryBlocks.size() == 12); //sanity check that this template magic works
#elif defined BOOT_STM32F7
	static_assert(physicalMemoryBlocks.size() == 8); //sanity check that this template magic works
#else
#error "This MCU is not supported"
#endif

	static_assert(customization::flashProgrammingParallelism == 16 || customization::flashProgrammingParallelism == 32, "Unsupported flash programming parallelism!");

	constexpr bool enableAssert = true;

	constexpr std::uint32_t isrVectorAlignmentMask = ufsel::bit::bitmask_of_width(customization::isrVectorAlignmentBits);

	constexpr std::uint32_t smallestPageSize = (*std::min_element(physicalMemoryBlocks.begin(), physicalMemoryBlocks.end(),[](auto const &a, auto const &b) {return a.length < b.length;} )).length;


}



