#include "flash.hpp"
#include <ufsel/bit.hpp>
#include <stm32f10x_flash.h>

#include <library/assert.hpp>

namespace boot {



	extern "C" {

		extern char available_flash_start[], available_flash_end[];
		extern char bootloader_flash_start[], bootloader_flash_end[];
		extern char ram_start[], ram_end[];
		extern char jumpTable_start[], jumpTable_end[];
	}

	std::size_t const Flash::availableMemory = available_flash_end - available_flash_start;
	std::uint32_t const Flash::jumpTableAddress = reinterpret_cast<std::uint32_t>(&jumpTable_start);
	std::uint32_t const Flash::applicationAddress = reinterpret_cast<std::uint32_t>(&available_flash_start);

	bool Flash::ErasePage(std::uint32_t pageAddress) {

		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_FLAG_EOP, FLASH_FLAG_WRPRTERR, FLASH_FLAG_PGERR);

		auto const res = FLASH_ErasePage(pageAddress);

		return res == FLASH_COMPLETE;
	}

	WriteStatus Flash::Write(std::uint32_t address, std::uint16_t halfWord) {

		auto const cachedResult = FLASH->SR;
		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_FLAG_EOP, FLASH_FLAG_PGERR);

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for previous operation to end
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //enable flash programming
		ufsel::bit::access_register<std::uint16_t>(address) = halfWord; //initiate programming

		//assert(ufsel::bit::all_cleared(cachedResult, FLASH_SR_WRPRTERR));

		return ufsel::bit::all_set(cachedResult, FLASH_SR_PGERR) ? WriteStatus::AlreadyWritten : WriteStatus::Ok;
	}

	WriteStatus Flash::Write(std::uint32_t address, std::uint32_t word) {

		auto const cachedResult = FLASH->SR;
		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_FLAG_EOP, FLASH_FLAG_PGERR);

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for previous operation to end
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //enable flash programming
		ufsel::bit::access_register<std::uint16_t>(address) = word; //program lower half

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for first operation to end
		ufsel::bit::access_register<std::uint16_t>(address + 2) = word >> 16; //program upper half

		assert(ufsel::bit::all_cleared(cachedResult, FLASH_SR_WRPRTERR));

		return ufsel::bit::all_set(cachedResult, FLASH_SR_PGERR) ? WriteStatus::AlreadyWritten : WriteStatus::Ok;
	}

	AddressSpace Flash::addressOrigin_located_in_flash(std::uint32_t const address) {
		std::uint32_t const available_start = reinterpret_cast<std::uint32_t>(available_flash_start);
		std::uint32_t const available_end = reinterpret_cast<std::uint32_t>(available_flash_end);

		std::uint32_t const jump_table_start = reinterpret_cast<std::uint32_t>(jumpTable_start);
		std::uint32_t const jump_table_end = reinterpret_cast<std::uint32_t>(jumpTable_end);

		std::uint32_t const bootloader_start = reinterpret_cast<std::uint32_t>(bootloader_flash_start);
		std::uint32_t const bootloader_end = reinterpret_cast<std::uint32_t>(bootloader_flash_end);


		std::uint32_t const RAM_start = reinterpret_cast<std::uint32_t>(ram_start);
		std::uint32_t const RAM_end = reinterpret_cast<std::uint32_t>(ram_end);

		if (available_start <= address && address < available_end)
			return AddressSpace::AvailableFlash;

		if (jump_table_start <= address && address < jump_table_end)
			return AddressSpace::JumpTable;

		if (bootloader_start <= address && address < bootloader_end)
			return AddressSpace::BootloaderFlash;

		if (RAM_start <= address && address < RAM_end)
			return AddressSpace::RAM;

		return AddressSpace::Unknown;

	}

	AddressSpace Flash::addressOrigin(std::uint32_t const address) {
		std::uint32_t const available_start = reinterpret_cast<std::uint32_t>(available_flash_start);
		std::uint32_t const available_end = reinterpret_cast<std::uint32_t>(available_flash_end);

		std::uint32_t const jump_table_start = reinterpret_cast<std::uint32_t>(jumpTable_start);
		std::uint32_t const jump_table_end = reinterpret_cast<std::uint32_t>(jumpTable_end);

		std::uint32_t const bootloader_start = reinterpret_cast<std::uint32_t>(bootloader_flash_start);
		std::uint32_t const bootloader_end = reinterpret_cast<std::uint32_t>(bootloader_flash_end);


		std::uint32_t const RAM_start = reinterpret_cast<std::uint32_t>(ram_start);
		std::uint32_t const RAM_end = reinterpret_cast<std::uint32_t>(ram_end);

		if (available_start <= address && address < available_end)
			return AddressSpace::AvailableFlash;

		if (jump_table_start <= address && address < jump_table_end)
			return AddressSpace::JumpTable;

		if (bootloader_start <= address && address < bootloader_end)
			return AddressSpace::BootloaderFlash;

		if (RAM_start <= address && address < RAM_end)
			return AddressSpace::RAM;

		return AddressSpace::Unknown;

	}

	void ApplicationJumpTable::invalidate() {
		//This vv better hold if we want to preserve data integrity
		assert(Flash::jumpTableAddress == reinterpret_cast<std::uint32_t>(&jumpTable));
		Flash::ErasePage(Flash::jumpTableAddress);
	}

}
