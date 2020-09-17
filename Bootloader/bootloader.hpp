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

#include "flash.hpp"

namespace boot {

	enum class Register {
		EntryPoint,
		InterruptVector,
		FlashSize,
		NumPagesToErase,
		PageToErase
	};



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

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


		HandshakeResponse handshake(Register reg, std::uint32_t value);

	};

}

