/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */
#include <algorithm>

#include "bootloader.hpp"
#include "flash.hpp"
#include "stm32f10x.h"

#include <library/assert.hpp>
namespace boot {

	WriteStatus Bootloader::checkBeforeWrite(std::uint32_t const address) {
		switch (Flash::addressOrigin(address)) {

		case AddressSpace::AvailableFlash:
			break;

		case AddressSpace::BootloaderFlash: case AddressSpace::JumpTable:
			return WriteStatus::MemoryProtected;

		case AddressSpace::RAM: case AddressSpace::Unknown:
			return WriteStatus::NotInFlash;
		}

		assert(Flash::isAvailableAddress(address));

		std::uint32_t const pageAligned = Flash::makePageAligned(address);

		if (!std::find(std::begin(erased_pages_), std::end(erased_pages_), pageAligned))
			return WriteStatus::NotInErasedMemory;

		return WriteStatus::Ok; //Everything seems ok, try to write
	}

	WriteStatus Bootloader::write(std::uint32_t address, std::uint16_t half_word) {
		if (WriteStatus const ret = checkBeforeWrite(address); ret != WriteStatus::Ok)
			return ret;

		return Flash::Write(address, half_word);
	}

	WriteStatus Bootloader::write(std::uint32_t address, std::uint32_t word) {
		if (WriteStatus const ret = checkBeforeWrite(address); ret != WriteStatus::Ok)
			return ret;

		return Flash::Write(address, word);
	}


	HandshakeResponse Bootloader::processHandshake(Register reg, std::uint32_t value) {
		return HandshakeResponse::PageProtected; //TODO implement

	}


	void Bootloader::resetToApplication() {
		BKP->DR1 = 0x00'00; //Clear the bootloaderEntryRequest

		ufsel::bit::set(std::ref(SCB->AIRCR),
			0x5fA << SCB_AIRCR_VECTKEYSTAT_Pos, //magic value required for write to succeed
			SCB_AIRCR_SYSRESETREQ); //Start the system reset

		for (;;); //wait for reset
	}


}
