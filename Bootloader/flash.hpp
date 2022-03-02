/*
 * eForce CAN Bootloader
 *
 *
 * Written by Vojtech Michal
 *
 * Copyright (c) 2020, 2021 eforce FEE Prague Formula
 */


#pragma once

#include <ufsel/bit.hpp>
#include <ufsel/assert.hpp>
#include "options.hpp"

#include <span>
#include <bit>
#include <cstddef>
#include <cstdint>

#include <CANdb/tx2/ringbuf.h>

namespace boot {

	constexpr std::uint32_t end(MemoryBlock const & block) { return block.address + block.length; }

	template<std::size_t bits>
	auto deduce_int_type_of_size() {
		static_assert(bits == 8 || bits == 16 || bits == 32 || bits == 64, "Can't deduce any integer type of this width!");

		if constexpr (bits == 8)
			return std::uint8_t{0};
		else if constexpr (bits == 16)
			return std::uint16_t{0};
		else if constexpr (bits == 32)
			return std::uint32_t{0};
		else if constexpr (bits == 64)
			return std::uint64_t{0};
	}

	enum class AddressSpace {
		BootloaderFlash,
		JumpTable,
		ApplicationFlash,
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
		DiscontinuousWriteAccess,
		InsufficientData
	};

	template<typename T>
	concept WriteableIntegral = std::is_same_v<T, std::uint16_t> || std::is_same_v<T, std::uint32_t> || std::is_same_v<T, std::uint64_t>;

	template<std::size_t CAPACITY>
	struct FlashWriteBuffer {
		struct record {
			std::uint32_t address_;
			std::uint8_t size_;
			std::uint64_t data_;
		};

		constexpr static std::size_t capacity = CAPACITY;
		record buffer[capacity];
		ringbuf_t ringbuf {.data = reinterpret_cast<std::uint8_t*>(buffer), .size = CAPACITY, .readpos = 0, .writepos = 0};


		void push(std::uint32_t address, WriteableIntegral auto data, std::size_t const length) {
			assert(ringbufCanWrite(&ringbuf, sizeof(record)));
			record const new_record {.address_ = address, .size_ = static_cast<std::uint8_t>(length), .data_ = data};
			ringbufWrite(&ringbuf, reinterpret_cast<std::uint8_t const*>(&new_record), sizeof(record));
		}

		[[nodiscard]]
		record peek(int offset) const {
			//cannot read anything, since there is not that many elements
			assert(ringbufSize(&ringbuf) >= (offset + 1) * sizeof(record));

			size_t readpos = (ringbuf.readpos + offset * sizeof(record)) % capacity;
			record result;
			ringbufTryRead(&ringbuf, reinterpret_cast<std::uint8_t *>(&result), sizeof(record), &readpos);
			return result;
		}

		void pop(int count) {
			//cannot read anything, since there is not that many elements
			assert(ringbufSize(&ringbuf) >= count * sizeof(record));

			//advance the read index.
			ringbuf.readpos = (ringbuf.readpos + count * sizeof(record)) % capacity;
		}

		[[nodiscard]] std::size_t size() const { return ringbufSize(&ringbuf) / sizeof(record); }
		[[nodiscard]] bool is_full() const { return !ringbufCanWrite(&ringbuf, sizeof(record)); }
		[[nodiscard]] bool empty() const { return size() == 0; }
	};

	struct Flash {
		friend struct ApplicationJumpTable;
		static inline FlashWriteBuffer<flash_write_buffer_size> writeBuffer_;

		using nativeType = decltype(deduce_int_type_of_size<customization::flashProgrammingParallelism>());
		static_assert(std::is_unsigned_v<nativeType>, "Flash native type shall be unsigned to prevent problems with signed overflow.");

		constexpr static bool pagesHaveSameSize() { return customization::physicalBlockSizePolicy == PhysicalBlockSizes::same; };

		static std::size_t const applicationMemorySize;
		static std::size_t const bootloaderMemorySize;
		static std::uint32_t const jumpTableAddress;
		static std::uint32_t const applicationAddress;
		static std::uint32_t const bootloaderAddress;

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
		static void AwaitEndOfOperation();
		static bool ErasePage(std::uint32_t pageAddress);
		static void ClearProgrammingErrors();

		static WriteStatus Write(std::uint32_t address, nativeType data);

	public:
		template<WriteableIntegral T>
		static bool ScheduleBufferedWrite(std::uint32_t address, T data, std::size_t length = sizeof(T)) {
			if (writeBuffer_.is_full())
				return false;
			writeBuffer_.push(address, data, length);
			return true;
		}

		static WriteStatus tryPerformingBufferedWrite() {
			if (writeBuffer_.empty())
				return WriteStatus::InsufficientData;
			auto const shift_data = [](std::uint64_t data, std::size_t width, std::size_t shift) {
				return (data & ufsel::bit::bitmask_of_width(width * 8)) << (shift * 8);
			};

			auto prev_record = writeBuffer_.peek(0);
			std::uint32_t const address = prev_record.address_;
			std::size_t num_bytes_to_write = prev_record.size_;
			nativeType data_to_write = shift_data(prev_record.data_, prev_record.size_, 0);

			std::size_t records_consumed = 1;
			for (; num_bytes_to_write < sizeof(nativeType) && records_consumed < writeBuffer_.size(); ++records_consumed) {
				auto const record = writeBuffer_.peek(records_consumed);
				num_bytes_to_write += record.size_;
				data_to_write |= shift_data(record.data_, record.size_, num_bytes_to_write);

				assert(record.address_ == prev_record.address_ + prev_record.size_); // records must be contiguous
				prev_record = record;
			}
			assert(num_bytes_to_write <= sizeof(nativeType));
			if (num_bytes_to_write < sizeof(nativeType))
				return WriteStatus::InsufficientData;
			else { // we can perform the write!
				writeBuffer_.pop(records_consumed);
				return Write(address, data_to_write);
			}
		}

		[[nodiscard]]
		static bool writeBufferIsEmpty() { return writeBuffer_.empty(); }

		static bool isApplicationAddress(std::uint32_t address) {
			return addressOrigin(address) == AddressSpace::ApplicationFlash;
		}
		static bool isBootloaderAddress(std::uint32_t address) {
			return addressOrigin(address) == AddressSpace::BootloaderFlash;
		}

		struct block_bank_id {
			int block_index, bank_num;
		};

		constexpr static block_bank_id getEnclosingBlockId(std::uint32_t address) {
			if constexpr (pagesHaveSameSize()) {
				constexpr std::uint32_t block_size = customization::physicalBlockSize.toBytes();
				constexpr std::uint32_t base_address = customization::flashMemoryBaseAddress;
				constexpr int blocksPerBank = customization::NumPhysicalBlocksPerBank;
				int const block_offset_ignoring_banks = (address - base_address) / block_size;
				// Assume flash memory banks form a contiguous range
				return block_bank_id{
					.block_index = block_offset_ignoring_banks % blocksPerBank,
					.bank_num = block_offset_ignoring_banks / blocksPerBank
				};
			}
			else {
				static_assert(customization::flashBankCount == 1,
						"It is not currently supported to have more than one bank and unequal sector sizes.If you need this, you need to implement it.");
				for (std::size_t i = 0; i < size(physicalMemoryBlocks); ++i) {
					MemoryBlock const& block = physicalMemoryBlocks[i];
					if (block.address <= address && address < end(block))
						return {.block_index = static_cast<int>(i), .bank_num = 0};
				}
				return {.block_index = -1, .bank_num = -1};
			}
		}
		
		constexpr static MemoryBlock getEnclosingBlock(std::uint32_t address) {
			auto const id = getEnclosingBlockId(address);
			MemoryBlock result = physicalMemoryBlocks[id.block_index];
			result.address += id.bank_num * flashBankSize.toBytes(); // Correct the memory block start (correctly handle bank number)
			return result;
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

		constexpr static unsigned applicationPages() {return customization::NumPhysicalBlocksPerBank - customization::firstBlockAvailableToApplication;}
		constexpr static unsigned bootloaderPages() {return customization::firstBlockAvailableToApplication - customization::firstBlockAvailableToBootloader;}

		static MemoryBlock block(std::uint32_t const index) {
			assert(index < customization::NumPhysicalBlocksPerBank);
			return physicalMemoryBlocks[index];
		}

		static bool canCover(AddressSpace const space, MemoryBlock logical) {

			bool const is_bootloader = space == AddressSpace::BootloaderFlash;
			std::uint32_t const begin_index = is_bootloader ? customization::firstBlockAvailableToBootloader : customization::firstBlockAvailableToApplication;
			std::uint32_t const end_index = is_bootloader ? customization::firstBlockAvailableToApplication : customization::NumPhysicalBlocksPerBank;

			for (std::uint32_t i = begin_index; i < end_index; ++i) {
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

		constexpr static int members_before_segment_array = 10;
		constexpr static std::size_t bytes_before_segment_array = sizeof(std::uint32_t)*members_before_segment_array;

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
		std::uint32_t padding_dont_care_; // means of aligning the following array to 8 byte boundary
		std::array<MemoryBlock, (smallestPageSize - bytes_before_segment_array) / sizeof(MemoryBlock)> logical_memory_blocks_;

		//Returns true iff all magics are valid
		[[nodiscard]]
		bool magicValid() const __attribute__((section(".executed_from_flash")));
		// Returns true if the jump table is filled with value corresponding to erased flash page (all ones)
		[[nodiscard]]
		bool isErased() const __attribute__((section(".executed_from_flash")));;
		[[nodiscard]]
		bool has_valid_metadata() const;

		//Clear the memory location with jump table
		void invalidate();

		void set_magics();
		void set_metadata(InformationSize firmware_size, std::span<MemoryBlock const> logical_memory_blocks);

		void set_interrupt_vector(std::uint32_t isr_vector);

		void writeToFlash();
	};
	static_assert(offsetof(ApplicationJumpTable, logical_memory_blocks_) == ApplicationJumpTable::bytes_before_segment_array);

	static_assert(sizeof(ApplicationJumpTable) <= smallestPageSize, "The application jump table must fit within one page of flash.");

	inline ApplicationJumpTable jumpTable __attribute__((section("jumpTableSection")));
}
