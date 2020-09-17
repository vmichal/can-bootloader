/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */


#pragma once

#include <ufsel/bit.hpp>

#include <cstddef>
#include <cstdint>

namespace boot {

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
		Timeout
	};

	struct Flash {

		static constexpr std::size_t pageCount = 128; //taken from the reference manual
		static constexpr std::uint32_t pageSize = 1 << 11; //taken from the reference manual
		static constexpr std::uint32_t pageAlignmentMask = pageSize - 1;

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

	struct ApplicationJumpTable {
		constexpr static std::uint32_t magic1_value = 0xb16'b00b5;
		constexpr static std::uint32_t magic2_value = 0xcafe'babe;
		constexpr static std::uint32_t magic3_value = 0xdead'beef;

		//Writing this value to the Backup register 1 requests entering the bootloader after reset
		constexpr static std::uint16_t backup_reg_bootloader_request = 0x4B1D;

		std::uint32_t magic1_;
		std::uint32_t entryPoint_;
		std::uint32_t magic2_;
		std::uint32_t interruptVector_;
		std::uint32_t magic3_;
	};

	inline ApplicationJumpTable jumpTable __attribute__((section("jumpTableSection")));
}
