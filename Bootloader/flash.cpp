#include "flash.hpp"
#include <ufsel/bit.hpp>

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

#ifdef STM32F1
	bool Flash::ErasePage(std::uint32_t pageAddress) {

		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_SR_EOP, FLASH_SR_WRPRTERR, FLASH_SR_PGERR);

		while (ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY)); //wait for previous operation to end

		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PER);
		FLASH->AR = pageAddress;
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_STRT);
		return true;
	}
#else
#ifdef STM32F4
	bool Flash::ErasePage(std::uint32_t pageAddress) {

		while (ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY)); //wait for previous operation to end

		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_SR_EOP);

		//configure write paralelism based on voltage range, use 32bit paralellism (0b10)
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(2), 0b10, POS_FROM_MASK(FLASH_CR_PSIZE));

		unsigned const sectorIndex = Flash::makePageAligned(pageAddress);
		//TODO erase sector 11
		assert(ufsel::bit::all_cleared(FLASH->CR, FLASH_CR_MER, FLASH_CR_PG));
		FLASH->CR |= FLASH_CR_SER; //Choose sector erase.
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(4), sectorIndex, POS_FROM_MASK(FLASH_CR_SNB));
		FLASH->CR |= FLASH_CR_STRT; //Start the operation

		for (; FLASH->SR & FLASH_SR_BSY;); //wait for the erase to finish
		FLASH->CR &= ~FLASH_CR_SER; //disable sector erase flag

		return true;
}
#else
#error "This MCU is not supported"
#endif
#endif

#if 0
	WriteStatus Flash::Write(std::uint32_t address, std::uint16_t halfWord) {

		auto const cachedResult = FLASH->SR;
		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_SR_EOP, FLASH_SR_PGERR);

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for previous operation to end
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //enable flash programming
		ufsel::bit::access_register<std::uint16_t>(address) = halfWord; //initiate programming

		assert(ufsel::bit::all_cleared(cachedResult, FLASH_SR_WRPRTERR));

		return ufsel::bit::all_set(cachedResult, FLASH_SR_PGERR) ? WriteStatus::AlreadyWritten : WriteStatus::Ok;
	}
#endif

	WriteStatus Flash::Write(std::uint32_t address, std::uint32_t word) {

#ifdef STM32F4
		std::uint32_t const cachedResult = FLASH->SR;
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //Start flash programming
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_PGSERR , FLASH_SR_PGPERR , FLASH_SR_PGAERR , FLASH_SR_WRPERR);
		//Write one word of data
		ufsel::bit::access_register<std::uint32_t>(address) = word;




		return ufsel::bit::all_cleared(cachedResult, FLASH_SR_PGSERR, FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR) ? WriteStatus::Ok: WriteStatus::MemoryProtected; //TODO make this more concrete
#else
#ifdef STM32F1

		auto const cachedResult = FLASH->SR;
		ufsel::bit::clear(std::ref(FLASH->SR), FLASH_SR_EOP, FLASH_SR_PGERR);

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for previous operation to end
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //enable flash programming
		ufsel::bit::access_register<std::uint16_t>(address) = word; //program lower half

		for (; ufsel::bit::all_set(FLASH->SR, FLASH_SR_BSY);); //wait for first operation to end
		ufsel::bit::access_register<std::uint16_t>(address + 2) = word >> 16; //program upper half

		assert(ufsel::bit::all_cleared(cachedResult, FLASH_SR_WRPRTERR));

		return ufsel::bit::all_set(cachedResult, FLASH_SR_PGERR) ? WriteStatus::AlreadyWritten : WriteStatus::Ok;
#else
#error "This MCU is not supported"
#endif
#endif
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

	bool ApplicationJumpTable::magicValid() const {
		return magic1_ == expected_magic1_value
			&& magic2_ == expected_magic2_value
			&& magic3_ == expected_magic3_value
			&& magic4_ == expected_magic4_value
			&& magic5_ == expected_magic5_value;
	}

}
