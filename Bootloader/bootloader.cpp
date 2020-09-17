/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */
#include "bootloader.hpp"

namespace boot {

	WriteStatus Bootloader::write(std::uint32_t address, std::uint16_t half_word) {
		return WriteStatus::NotInErasableMemory; //TODO implement
	}
	WriteStatus Bootloader::write(std::uint32_t address, std::uint32_t word) {
		return WriteStatus::NotInErasableMemory; //TODO implement

	}


	HandshakeResponse Bootloader::handshake(Register reg, std::uint32_t value) {
		return HandshakeResponse::PageProtected; //TODO implement

	}



}
