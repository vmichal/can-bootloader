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
		__disable_irq();
		FLASH_Unlock();
		FLASH->SR = FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR | FLASH_FLAG_PGERR;

		FLASH_ErasePage(pageAddress);
		__enable_irq();

		FLASH_Lock();
		return true;
	}

	WriteStatus Flash::Write(std::uint32_t address, std::uint16_t halfWord) {

		__disable_irq();
		FLASH_Unlock();
		FLASH->SR = FLASH_SR_EOP | FLASH_FLAG_PGERR;

		FLASH_Status const ret = FLASH_ProgramHalfWord(address, halfWord);

		__enable_irq();
		FLASH_Lock();

		switch (ret) {
		case FLASH_BUSY:  return WriteStatus::Timeout;
		case FLASH_ERROR_PG: return WriteStatus::AlreadyWritten;
		case FLASH_COMPLETE: return WriteStatus::Ok;
		case FLASH_TIMEOUT: return WriteStatus::Timeout;
		case FLASH_ERROR_WRP: break;
		}

		assert_unreachable();
	}

	WriteStatus Flash::Write(std::uint32_t address, std::uint32_t word) {

		__disable_irq();
		FLASH_Unlock();
		FLASH->SR = FLASH_SR_EOP | FLASH_FLAG_PGERR;

		FLASH_Status const ret = FLASH_ProgramWord(address, word); //TODO probably return this result

		__enable_irq();
		FLASH_Lock();

		switch (ret) {
		case FLASH_BUSY: case FLASH_TIMEOUT: return WriteStatus::Timeout;
		case FLASH_ERROR_PG: return WriteStatus::AlreadyWritten;
		case FLASH_COMPLETE: return WriteStatus::Ok;
		case FLASH_ERROR_WRP: break;
		}

		assert_unreachable();
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
