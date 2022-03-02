#include "flash.hpp"
#include <ufsel/bit.hpp>

#include <ufsel/assert.hpp>

namespace boot {



	extern "C" {

		extern char application_flash_start[], application_flash_end[];
		extern char bootloader_flash_start[], bootloader_flash_end[];
		extern char ram_start[], ram_end[];
		extern char jumpTable_start[], jumpTable_end[];
	}

	std::size_t const Flash::applicationMemorySize = application_flash_end - application_flash_start;
	std::size_t const Flash::bootloaderMemorySize = bootloader_flash_end - bootloader_flash_start;
	std::uint32_t const Flash::jumpTableAddress = reinterpret_cast<std::uint32_t>(jumpTable_start);
	std::uint32_t const Flash::applicationAddress = reinterpret_cast<std::uint32_t>(application_flash_start);
	std::uint32_t const Flash::bootloaderAddress = reinterpret_cast<std::uint32_t>(bootloader_flash_start);

	void Flash::AwaitEndOfErasure() {
		//wait for all operations to finish and lock flash
		AwaitEndOfOperation();
#if defined BOOT_STM32F1 || defined BOOT_STM32G4
		ufsel::bit::clear(std::ref(FLASH->CR), FLASH_CR_PER);
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2
		ufsel::bit::clear(std::ref(FLASH->CR), FLASH_CR_SER);
#endif
	}

	void Flash::AwaitEndOfOperation() {
		//wait for all operations to finish
		ufsel::bit::wait_until_cleared(FLASH->SR, FLASH_SR_BSY);
	}

	void Flash::ClearProgrammingErrors() {
#if defined BOOT_STM32G4
		ufsel::bit::set(std::ref(FLASH->SR),
				FLASH_SR_FASTERR, // fast programming error
				FLASH_SR_MISERR, // fast programming data miss error
				FLASH_SR_PGSERR, // programming sequence error
				FLASH_SR_SIZERR, // Size error
				FLASH_SR_PGAERR, // Programming alignment error
				FLASH_SR_WRPERR, // Write protection error
				FLASH_SR_PROGERR // Programming error
		);
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2
		ufsel::bit::set(std::ref(FLASH->SR), , FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR);
#elif defined BOOT_STM32F1
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_EOP, FLASH_SR_WRPRTERR, FLASH_SR_PGERR);
#endif
	}

	bool Flash::ErasePage(std::uint32_t pageAddress) {
		using namespace ufsel;
		#if defined BOOT_STM32F1

		AwaitEndOfOperation();
		ClearProgrammingErrors();

		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PER);
		FLASH->AR = pageAddress;
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_STRT);
		return true;
#elif defined BOOT_STM32G4

		AwaitEndOfOperation();
		ClearProgrammingErrors();

		auto const page_id = getEnclosingBlockIndex(pageAddress);
		bit::sliceable_reference CR{FLASH->CR};
		CR[FLASH_CR_PER_Pos] = true;
		CR[bit::slice::for_mask(FLASH_CR_PNB)] = page_id.block_index;
		CR[FLASH_CR_BKER_Pos] = page_id.bank_num;
		CR[FLASH_CR_STRT_Pos] = true; // start page erase
		AwaitEndOfOperation();
		return true;
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2

		AwaitEndOfOperation();
		ClearProgrammingErrors();

		//configure write paralelism based on voltage range, use 32bit paralellism (0b10)
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(2), 0b10, POS_FROM_MASK(FLASH_CR_PSIZE));

		unsigned const sectorIndex = Flash::getEnclosingBlockIndex(pageAddress);

		assert(ufsel::bit::all_cleared(FLASH->CR, FLASH_CR_MER, FLASH_CR_PG));
		FLASH->CR |= FLASH_CR_SER; //Choose sector erase.
		//FLASH_CR_SNB is 5 bits wide on f767, but the highest bit is always 0, so it is enough to write only lower four bits.
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(4), sectorIndex, POS_FROM_MASK(FLASH_CR_SNB));
		FLASH->CR |= FLASH_CR_STRT; //Start the operation

		AwaitEndOfOperation();
		FLASH->CR &= ~FLASH_CR_SER; //disable sector erase flag

		return true;
#else
#error "This MCU is not supported"
#endif
}

	WriteStatus Flash::Write(std::uint32_t address, nativeType data) {
		assert(address % sizeof(nativeType) == 0 && "Attempt to perform unaligned write!");
#if defined BOOT_STM32F1
		static_assert(std::is_same_v<nativeType, std::uint16_t>, "STM32F1 flash is unable to perform write access other than 16bits wide.");
		AwaitEndOfOperation();
		auto const cachedResult = FLASH->SR;
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_EOP, FLASH_SR_PGERR);

		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //enable flash programming
		ufsel::bit::access_register<nativeType>(address) = data; //initiate programming

		assert(ufsel::bit::all_cleared(cachedResult, FLASH_SR_WRPRTERR));

		return ufsel::bit::all_set(cachedResult, FLASH_SR_PGERR) ? WriteStatus::AlreadyWritten : WriteStatus::Ok;
#elif defined BOOT_STM32F4 || defined BOOT_STM32F2
		static_assert(std::is_same_v<nativeType, std::uint32_t>, "STM32F4 flash is currently hardcoded to use 32bit writes.");
		std::uint32_t const cachedResult = FLASH->SR;
		//select x32 programming paralelism
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(2), 0b10, POS_FROM_MASK(FLASH_CR_PSIZE));
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //Start flash programming
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_PGSERR, FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR);
		ufsel::bit::access_register<nativeType>(address) = data; //Write one word of data

		return ufsel::bit::all_cleared(cachedResult, FLASH_SR_PGSERR, FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR) ? WriteStatus::Ok : WriteStatus::MemoryProtected; //TODO make this more concrete
#elif defined BOOT_STM32G4
		static_assert(std::is_same_v<nativeType, std::uint64_t>, "STM32G4 flash must be written with 64 bit granularity.");
		AwaitEndOfOperation();
		std::uint32_t const cachedResult = FLASH->SR;
		ClearProgrammingErrors();

		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //Start flash programming

		//TODO maybe disable interrupt to write 8 bytes atomically?
		ufsel::bit::access_register<std::uint32_t>(address) = data; //Write lower word of data
		ufsel::bit::access_register<std::uint32_t>(address + 4) = data >> 32; //Write higher word of data

		AwaitEndOfOperation();
		ufsel::bit::wait_until_set(FLASH->SR, FLASH_SR_EOP); // Wait for end of programming
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_EOP); // clear the flag

		return ufsel::bit::all_cleared(cachedResult, FLASH_SR_SIZERR, FLASH_SR_PGSERR, FLASH_SR_PROGERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR) ? WriteStatus::Ok : WriteStatus::MemoryProtected; //TODO make this more concrete

#elif defined BOOT_STM32F7
		static_assert(std::is_same_v<nativeType, std::uint32_t>, "STM32F7 flash is currently hardcoded to use 32bit writes.");
		AwaitEndOfOperation();
		std::uint32_t const cachedResult = FLASH->SR;

		//select x32 programming paralelism
		ufsel::bit::modify(std::ref(FLASH->CR), ufsel::bit::bitmask_of_width(2), 0b10, POS_FROM_MASK(FLASH_CR_PSIZE));
		ufsel::bit::set(std::ref(FLASH->CR), FLASH_CR_PG); //Start flash programming
		ufsel::bit::set(std::ref(FLASH->SR), FLASH_SR_ERSERR, FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR);
		__DSB();
		ufsel::bit::access_register<nativeType>(address) = data; //Write one word of data
		AwaitEndOfOperation();

		return ufsel::bit::all_cleared(cachedResult, FLASH_SR_ERSERR, FLASH_SR_PGPERR, FLASH_SR_PGAERR, FLASH_SR_WRPERR) ? WriteStatus::Ok : WriteStatus::MemoryProtected; //TODO make this more concrete

#else
#error "This MCU is not supported"
#endif
	}

	AddressSpace Flash::addressOrigin_located_in_flash(std::uint32_t const address) {
		std::uint32_t const application_start = reinterpret_cast<std::uint32_t>(application_flash_start);
		std::uint32_t const application_end = reinterpret_cast<std::uint32_t>(application_flash_end);

		std::uint32_t const jump_table_start = reinterpret_cast<std::uint32_t>(jumpTable_start);
		std::uint32_t const jump_table_end = reinterpret_cast<std::uint32_t>(jumpTable_end);

		std::uint32_t const bootloader_start = reinterpret_cast<std::uint32_t>(bootloader_flash_start);
		std::uint32_t const bootloader_end = reinterpret_cast<std::uint32_t>(bootloader_flash_end);


		std::uint32_t const RAM_start = reinterpret_cast<std::uint32_t>(ram_start);
		std::uint32_t const RAM_end = reinterpret_cast<std::uint32_t>(ram_end);

		if (application_start <= address && address < application_end)
			return AddressSpace::ApplicationFlash;

		if (jump_table_start <= address && address < jump_table_end)
			return AddressSpace::JumpTable;

		if (bootloader_start <= address && address < bootloader_end)
			return AddressSpace::BootloaderFlash;

		if (RAM_start <= address && address < RAM_end)
			return AddressSpace::RAM;

		return AddressSpace::Unknown;

	}

	AddressSpace Flash::addressOrigin(std::uint32_t const address) {
		std::uint32_t const application_start = reinterpret_cast<std::uint32_t>(application_flash_start);
		std::uint32_t const application_end = reinterpret_cast<std::uint32_t>(application_flash_end);

		std::uint32_t const jump_table_start = reinterpret_cast<std::uint32_t>(jumpTable_start);
		std::uint32_t const jump_table_end = reinterpret_cast<std::uint32_t>(jumpTable_end);

		std::uint32_t const bootloader_start = reinterpret_cast<std::uint32_t>(bootloader_flash_start);
		std::uint32_t const bootloader_end = reinterpret_cast<std::uint32_t>(bootloader_flash_end);


		std::uint32_t const RAM_start = reinterpret_cast<std::uint32_t>(ram_start);
		std::uint32_t const RAM_end = reinterpret_cast<std::uint32_t>(ram_end);

		if (application_start <= address && address < application_end)
			return AddressSpace::ApplicationFlash;

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
		assert(this == &jumpTable);

		Flash::ErasePage(Flash::jumpTableAddress);
	}

	bool ApplicationJumpTable::magicValid() const {
		return magic1_ == expected_magic1_value
			&& magic2_ == expected_magic2_value
			&& magic3_ == expected_magic3_value
			&& magic4_ == expected_magic4_value
			&& magic5_ == expected_magic5_value;
	}

	bool ApplicationJumpTable::has_valid_metadata() const {
		return metadata_valid_magic_ == metadata_valid_magic_value;
	}

	void ApplicationJumpTable::set_magics() {
		magic1_ = expected_magic1_value;
		magic2_ = expected_magic2_value;
		magic3_ = expected_magic3_value;
		magic4_ = expected_magic4_value;
		magic5_ = expected_magic5_value;
	}

	void ApplicationJumpTable::set_metadata(InformationSize const firmware_size, std::span<MemoryBlock const> const logical_memory_blocks) {
		firmwareSize_ = static_cast<std::uint32_t>(firmware_size.toBytes());
		logical_memory_block_count_ = size(logical_memory_blocks);

		for (int block_index = 0; block_index < ssize(logical_memory_blocks); ++block_index) {
			logical_memory_blocks_[block_index].address = logical_memory_blocks[block_index].address;
			logical_memory_blocks_[block_index].length = logical_memory_blocks[block_index].length;
		}
		metadata_valid_magic_ = metadata_valid_magic_value;
	}

	void ApplicationJumpTable::set_interrupt_vector(std::uint32_t const isr_vector) {
		interruptVector_ = isr_vector;
	}

	void ApplicationJumpTable::writeToFlash() {
		assert(reinterpret_cast<std::uint32_t>(&jumpTable) == Flash::jumpTableAddress && "Sanity check of consistent addresses.");
		assert(this != &jumpTable && "Jump table in flash must not be initialized from itself.");

		// Go from the end, since magics are in the front of jump table and we want them written last
		// Metadata shall be written iff they are valid.
		std::size_t const valid_data_length = has_valid_metadata() ? sizeof(MemoryBlock) * logical_memory_block_count_ : 0;
		std::size_t const length_in_bytes = bytes_before_segment_array + valid_data_length;
		assert(length_in_bytes % sizeof(Flash::nativeType) == 0 && "The application jump table must span a whole number of flash native types.");
		Flash::nativeType const * data_array = reinterpret_cast<Flash::nativeType *>(this);

		for (int offset = length_in_bytes / sizeof(Flash::nativeType) - 1; offset >= 0; --offset)
			Flash::Write(Flash::jumpTableAddress + offset * sizeof(Flash::nativeType), data_array[offset]);
	}

}
