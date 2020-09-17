/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#pragma once

#include <cstdint>

namespace boot {

	enum class Register {
		EntryPoint,
		InterruptVector,
		FlashSize,
		NumPagesToErase,
		PageToErase
	};

	enum class WriteStatus {
		Ok,
		NotInErasableMemory,
		NotInFlash,
		NotWriteable
	};

	enum class HandshakeResponse {
		Ok,
		PageAddressNotAligned,
		AddressNotInFlash,
		PageProtected,
		ErasedPageCountMismatch
	};


	class Bootloader {

	public:

		WriteStatus write(std::uint32_t address, std::uint16_t half_word);
		WriteStatus write(std::uint32_t address, std::uint32_t word);


		HandshakeResponse handshake(Register reg, std::uint32_t value);

	};

}

