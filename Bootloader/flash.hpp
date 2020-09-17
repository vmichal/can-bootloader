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


}
