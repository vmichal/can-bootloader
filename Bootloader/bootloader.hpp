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

#include <library/assert.hpp>

#include "flash.hpp"
#include "can_Bootloader.h"

namespace boot {

	enum class Register {
		EntryPoint,
		InterruptVector,
		FlashSize,
		NumPagesToErase,
		PageToErase
	};

	constexpr Bootloader_Register regToReg(Register r) {
		switch (r) {
		case Register::EntryPoint:
			return Bootloader_Register_EntryPoint;
		case Register::InterruptVector:
			return Bootloader_Register_InterruptVector;
		case Register::FlashSize:
			return Bootloader_Register_FlashSize;
		case Register::NumPagesToErase:
			return Bootloader_Register_NumPagesToErase;
		case Register::PageToErase:
			return Bootloader_Register_PageToErase;
		}
		assert_unreachable();
	}
	constexpr Register regToReg(Bootloader_Register r) {
		switch (r) {
		case Bootloader_Register_EntryPoint:
			return Register::EntryPoint;
		case Bootloader_Register_InterruptVector:
			return Register::InterruptVector;
		case Bootloader_Register_FlashSize:
			return Register::FlashSize;
		case Bootloader_Register_NumPagesToErase:
			return Register::NumPagesToErase;
		case Bootloader_Register_PageToErase:
			return Register::PageToErase;
		}
		assert_unreachable();
	}

	constexpr char const* to_string(Bootloader_BootTarget target) {
		switch (target) {
		case Bootloader_BootTarget_AMS: return "AMS";
		}
		return "unknown";
	}



	enum class HandshakeResponse {
		Ok,
		PageAddressNotAligned,
		AddressNotInFlash,
		PageProtected,
		ErasedPageCountMismatch
	};


	class Bootloader {

		std::array<std::uint32_t, Flash::pageCount> erased_pages_;
		std::size_t erased_pages_count_;


		WriteStatus checkBeforeWrite(std::uint32_t address);


	public:
		constexpr static Bootloader_BootTarget thisUnit = Bootloader_BootTarget_AMS;

		static void resetToApplication();

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


		HandshakeResponse processHandshake(Register reg, std::uint32_t value);

	};

}

