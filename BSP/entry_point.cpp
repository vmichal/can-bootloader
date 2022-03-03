/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <API/BLdriver.hpp>
#include <Bootloader/bootloader.hpp>
#include <Bootloader/options.hpp>
#include <ufsel/bit_operations.hpp>

#include "gpio.hpp"
#include "can.hpp"
#include "fdcan.hpp"
#include "timer.hpp"

#include <cstdint>
#include <bit>

extern "C" {
	//The following is taken from linker.ld:
	//the LOADADDR is the address FROM which the section shall be loaded.
	//It is set by the AT > declaration

#if 0
	  /* used by the startup to initialize data */
	_load_data = LOADADDR(.data);

	/* Initialized data sections goes into RAM, load LMA copy after code */
	.data :
	{
		. = ALIGN(4);
		_sdata = .;        /* create a global symbol at data start */
		*(.data)           /* .data sections */
			* (.data*)          /* .data* sections */

			. = ALIGN(4);
		_edata = .;        /* define a global symbol at data end */
	} > RAM AT > BootloaderFlash
#endif

	using section_elem_t = std::uint32_t;

	extern section_elem_t           _sdata[]; //Points to the start of data in RAM
	extern section_elem_t           _edata[]; //Points past the end of data in RAM
	extern section_elem_t const _load_data[]; //Points to the start of data in FLASH

	extern section_elem_t _sbss[]; //Points to the beginning of .bss in RAM
	extern section_elem_t _ebss[]; //Points past the end of .bss in RAM

	extern section_elem_t           _stext[]; //Points to the beginning of .text in RAM
	extern section_elem_t           _etext[]; //Points past the end of .text in RAM
	extern section_elem_t const _load_text[]; //Points to the start of .text in FLASH

	extern section_elem_t           _sisr_vector[]; //Points to the beginning of .isr_vector in RAM
	extern section_elem_t           _eisr_vector[]; //Points past the end of .isr_vector in RAM
	extern section_elem_t const _load_isr_vector[]; //Points to the start of .isr_vector in FLASH

	extern section_elem_t           _srodata[]; //Points to the beginning of .rodata in RAM
	extern section_elem_t           _erodata[]; //Points past the end of .rodata in RAM
	extern section_elem_t const _load_rodata[]; //Points to the start of .rodata in FLASH

	void __libc_init_array();
}

namespace boot {
	void main();
}

namespace {
	using namespace ufsel;

	/* Checks the backup domain, jump table in flash, firmware integrity etc.
	to decide whether the application or the bootloader shall be entered.*/
	boot::EntryReason determineApplicationAvailability()  __attribute__((section(".executed_from_flash")));
	boot::EntryReason determineApplicationAvailability() {

		auto const destination = static_cast<boot::BackupDomain::magic>(boot::BackupDomain::bootControlRegister);
		switch (destination) {
		case boot::BackupDomain::magic::bootloader:
			return boot::EntryReason::Requested; //return to enter the bootloader
		case boot::BackupDomain::magic::app_fatal_error:
			return boot::EntryReason::ApplicationFailure;

		case boot::BackupDomain::magic::reset_value:
		case boot::BackupDomain::magic::app_skip_can_check:
		case boot::BackupDomain::magic::app_perform_can_check:
			break; //these three options must still be validated
		default: //the backup domain contains unknown value
			return boot::EntryReason::BackupRegisterCorrupted;
		}

		if (boot::jumpTable.isErased()) // The jump table is completely empty
			return boot::EntryReason::ApplicationMissing;

		if (!boot::jumpTable.magicValid())
			return boot::EntryReason::JumpTableCorrupted; //Magics do not match. Enter the bootloader

		if (!bit::all_cleared(boot::jumpTable.interruptVector_, boot::isrVectorAlignmentMask))
			return boot::EntryReason::InterruptVectorNotAligned; //The interrupt table is not properly aligned to the 512 B boundary

		if (boot::Flash::addressOrigin_located_in_flash(boot::jumpTable.interruptVector_) != boot::AddressSpace::ApplicationFlash)
			return boot::EntryReason::InterruptVectorNotInFlash;

		//Application entry point is saved as the second word of the interrupt table. Initial stack pointer is the first word
		std::uint32_t const* const interruptVector = reinterpret_cast<std::uint32_t const*>(boot::jumpTable.interruptVector_);

		if (boot::Flash::addressOrigin_located_in_flash(interruptVector[1]) != boot::AddressSpace::ApplicationFlash)
			return boot::EntryReason::EntryPointNotInFlash;

		boot::AddressSpace const entry_space = boot::Flash::addressOrigin_located_in_flash(interruptVector[0]);
		using Space = boot::AddressSpace;
		//Application initial stack pointer is saved as the first word of the interrupt table
		if (entry_space == Space::ApplicationFlash || entry_space == Space::JumpTable || entry_space == Space::BootloaderFlash)
			return boot::EntryReason::TopOfStackInvalid;

		//If we have got this far, it appears that the firmware is valid. Initiate the CAN bus startup check or skip it
		bool const check_can = boot::customization::enableStartupCanBusCheck && destination != boot::BackupDomain::magic::app_skip_can_check;
		return check_can ? boot::EntryReason::StartupCanBusCheck : boot::EntryReason::DontEnter;
	}

	void configure_system_clock() {
#ifdef BOOT_STM32F1

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //Enable external oscilator
		while (bit::all_cleared(RCC->CR, RCC_CR_HSERDY)); //wait for it to stabilize

		bit::set(std::ref(RCC->CFGR),
			RCC_CFGR_PLLMULL9, //Make PLL multiply 4MHz * 9 -> 36 MHz
			RCC_CFGR_PLLSRC //clock it from PREDIV1 (divided HSE)
		);

		constexpr Frequency desiredPLLinput = 4_MHz;
		constexpr int PREDIV1 = boot::customization::HSE / desiredPLLinput;
		static_assert(PREDIV1 * desiredPLLinput == boot::customization::HSE, "Your HSE is not an integral multiple of 2 MHz!");
		bit::set(std::ref(RCC->CFGR2),
			PREDIV1-1 //divide HSE by PREDIV1 so that we get 4MHz at PLL input
			);

		bit::modify(std::ref(FLASH->ACR), FLASH_ACR_LATENCY,1); //Flash latency one wait state, because the SYSCLK is somewhat fast as fuck.

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //Enable PLL
		while (bit::all_cleared(RCC->CR, RCC_CR_PLLRDY)); //wait for it to stabilize

		bit::modify(std::ref(RCC->CFGR), RCC_CFGR_SW_0 | RCC_CFGR_SW_1, RCC_CFGR_SW_PLL); //Set PLL as system clock
		while (bit::sliceable_value{ RCC->CFGR } [3_to, 2].unshifted() != RCC_CFGR_SWS_PLL); //wait for it settle.
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2
		using namespace ufsel;

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //Start and wait for HSE stabilization
		while (bit::all_cleared(RCC->CR, RCC_CR_HSERDY));

		constexpr int PLLM = boot::customization::HSE / 1_MHz; //Frequency 1MHz at PLL input
		//PLL can further divide by 2,4,6 or 8 and multiply by 50 to 432
		constexpr int PLLN = 6*36; //We want to achieve SYSCLK 12 MHz, so lets set P = 6 and N = 36*6

		RCC->PLLCFGR = bit::set(
#ifdef RCC_PLLCFGR_PLLR
			7 << POS_FROM_MASK(RCC_PLLCFGR_PLLR), //Probably irrelevant, not used
#endif
			15 << POS_FROM_MASK(RCC_PLLCFGR_PLLQ), //Probably irrelevant, not used
			RCC_PLLCFGR_PLLSRC, //HSE used as source
			//Divide PLL output by 6 in order to get 36MHz on AHB
			0b10 << POS_FROM_MASK(RCC_PLLCFGR_PLLP),
			PLLN << POS_FROM_MASK(RCC_PLLCFGR_PLLN),
			PLLM << POS_FROM_MASK(RCC_PLLCFGR_PLLM));

		//Section 3.4.1 of reference manual - conversion table from CPU speed and voltage range to flash latency
		constexpr auto desired_latency = FLASH_ACR_LATENCY_1WS; //Voltage range 2.7-3.6 && 30MHz < HCLK < 60MHz --> 1 wait states
		bit::modify(std::ref(FLASH->ACR), FLASH_ACR_LATENCY, desired_latency);
		bit::set(std::ref(FLASH->ACR), FLASH_ACR_PRFTEN);
		while (bit::get(FLASH->ACR, FLASH_ACR_LATENCY) != desired_latency); //Wait for ack

		RCC->CFGR = bit::bitmask(
			RCC_CFGR_PPRE2_DIV1, //APB2 has full speed (12 MHz)
			RCC_CFGR_PPRE1_DIV1, //APB1 has full speed
			RCC_CFGR_HPRE_DIV1);

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //Start and wait for PLL stabilization
		while (bit::all_cleared(RCC->CR, RCC_CR_PLLRDY));

		bit::set(std::ref(RCC->CFGR), RCC_CFGR_SW_PLL); //Switch system clock to PLL and await acknowledge
		while (bit::sliceable_reference{ RCC->CFGR } [3_to, 2].unshifted()!=RCC_CFGR_SWS_PLL);

		bit::clear(std::ref(RCC->CR), RCC_CR_HSION); //Kill power to HSI
#elif defined BOOT_STM32G4

		// By default, the core voltage regulator is configured to range 1, normal mode (up to 150 MHz),
		// so no changes need to be done there. That is 0 WS up to 30 MHz, which is enough.

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //start HSE
		bit::wait_until_set(RCC->CR, RCC_CR_HSERDY);
		// we want 12 MHz SYSCLK, APBx and AHBx run at max frequency. Therefore output of VCO shall be 24 MHz
		// and SYSCLK (VCO / PLLR) will run at 12 MHz

		constexpr auto PLL_input = 1_MHz;
		constexpr auto desired_VCO = boot::SYSCLK * 2; // PLLR must divide at least by 2

		constexpr unsigned PLLM = boot::customization::HSE / PLL_input; //pre PLL divisor
		constexpr unsigned PLLN = desired_VCO / PLL_input; // multiply PLL input to VCO clock.
		constexpr unsigned PLLR = desired_VCO / boot::SYSCLK; // divides VCO freq to SYSCLK
		// Static asserts based on information in device reference manual.
		// See section 7.4.4 - description of RCC_PLLCFGR register
		static_assert(PLLR * boot::SYSCLK == desired_VCO, "Frequency prescalers must be natural numbers!");
		static_assert(PLL_input * PLLM == boot::customization::HSE, "This PLL input frequency cannot be achieved using supplied HSE frequency.");
		static_assert(PLLN >= 8 && PLLN <= 127);
		static_assert(PLLM >= 1 && PLLM <= 16);

		static_assert(PLLR % 2 == 0 && 2 <= PLLR && PLLR <= 8);

		// Keep RCC->CFGR at reset state - no dividers before AHB and APBx buses.

		RCC->PLLCFGR = bit::bitmask(
				//PLLP does not matter - it supplies clock to ADC
				//disable PLLP and PLLQ (EN bits low)
				//PLLR is the prescaler for system clock, can be 2,4,6,8
				(PLLR / 2 - 1) << RCC_PLLCFGR_PLLR_Pos, //PLLR divides by 2 from 24 to 12 MHz
				RCC_PLLCFGR_PLLREN, //enable PLLR output
				PLLN << RCC_PLLCFGR_PLLN_Pos, //set VCO output to desired frequency
				PLLM << RCC_PLLCFGR_PLLM_Pos, // configure the divider on PLL input
				RCC_PLLCFGR_PLLSRC_HSE // connect HSE to PLL input
				);

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //start PLL
		bit::wait_until_set(RCC->CR, RCC_CR_PLLRDY);

		bit::modify(std::ref(RCC->CFGR), RCC_CFGR_SWS_Msk, RCC_CFGR_SWS_PLL); // set PLL as system clock
		while (bit::get(RCC->CFGR, RCC_CFGR_SW_Msk) != RCC_CFGR_SW_PLL); // wait for the SYSCLK to switch to PLL

		bit::clear(std::ref(RCC->CR), RCC_CR_HSION); //disable internal oscillator
#else
#error "This MCU is not supported"
#endif
	}

#pragma GCC push_options
#pragma GCC optimize ("O0")
	//The O0 optimization is currently the best workaround so that the compiler does not optimize this function into a memcpy.
	//We have no control over the definition of memcpy and thus it goes to .text. Section .text is initialized by calling this function
	//hence an unsolveable circular dependency would occur.
	__attribute__((section(".executed_from_flash"))) void do_load_section(section_elem_t const* load_address, section_elem_t* begin, section_elem_t const* const end) {
		for (; begin < end; ++load_address, ++begin)
			*begin = *load_address;
	}
#pragma GCC pop_options
}

#define VMA(section) _s ## section
#define LMA(section) _load_ ## section
#define END(section) _e ## section
#define load_section(section) do_load_section(LMA(section), VMA(section), END(section));

extern "C" void Reset_Handler() __attribute__((section(".executed_from_flash")));

//Decides whether the CPU shall enter the application firmware or start listening for communication in bootloader mode
extern "C" void Reset_Handler() {
	//Beware that absolutely nothing has been enabled by now.
	// .data and .bss have not been touched yet.
	//No static constructors have been called yet either.
	//This is the first instruction executed after system reset

	boot::BackupDomain::unlock();

	boot::EntryReason const reason = determineApplicationAvailability();

	boot::BackupDomain::lock();

	if (reason == boot::EntryReason::DontEnter) {
		SCB->VTOR = boot::jumpTable.interruptVector_; //Set the address of application's interrupt vector

		std::uint32_t const* const isr_vector = reinterpret_cast<std::uint32_t const*>(boot::jumpTable.interruptVector_);

		__asm("msr msp, %0" : : "r" (isr_vector[0]));

		reinterpret_cast<void(*)()>(isr_vector[1])(); //Jump to the main application

		//can't be reached since we have overwritten our stack pointer
	}

	//Reached only if we have to enter the bootloader. We shall initialize the system now

	//Copy Bootloader code, interrupt vector and read only data to RAM. Setup system control block to use isr vector in RAM.
	load_section(text);
	load_section(isr_vector);
	load_section(rodata);
	SCB->VTOR = reinterpret_cast<std::uint32_t>(VMA(isr_vector));

	//From there, initialization shall proceed as if code was running normally from flash -> init data and bss
	std::fill(VMA(bss), END(bss), 0); //clear the .bss
	load_section(data);

	configure_system_clock();
	//Configure and start system milisecond clock
	SystemTimer::Initialize();
	__libc_init_array(); //Branch to static constructors

	boot::Bootloader::setEntryReason(reason);
	bsp::gpio::Initialize();
	bsp::can::Initialize();

	boot::main();
}
