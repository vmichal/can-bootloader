/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <API/BLdriver.hpp>
#include <Bootloader/bootloader.hpp>
#include <Bootloader/options.hpp>
#include <ufsel/bit_operations.hpp>

#include "gpio.hpp"
#include "can.hpp"
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

	extern std::uint32_t           _sdata[]; //Points to the start of data in RAM
	extern std::uint32_t           _edata[]; //Points past the end of data in RAM
	extern std::uint32_t const _load_data[]; //Points to the start of data in FLASH

	extern std::uint32_t _sbss[]; //Points to the beginning of .bss in RAM
	extern std::uint32_t _ebss[]; //Points past the end of .bss in RAM

	extern std::uint32_t           _stext[]; //Points to the beginning of .text in RAM
	extern std::uint32_t           _etext[]; //Points past the end of .text in RAM
	extern std::uint32_t const _load_text[]; //Points to the start of .text in FLASH

	extern std::uint32_t           _sisr_vector[]; //Points to the beginning of .isr_vector in RAM
	extern std::uint32_t           _eisr_vector[]; //Points past the end of .isr_vector in RAM
	extern std::uint32_t const _load_isr_vector[]; //Points to the start of .isr_vector in FLASH

	extern std::uint32_t           _srodata[]; //Points to the beginning of .rodata in RAM
	extern std::uint32_t           _erodata[]; //Points past the end of .rodata in RAM
	extern std::uint32_t const _load_rodata[]; //Points to the start of .rodata in FLASH

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

		switch (boot::BackupDomain::bootControlRegister) {
		case boot::BackupDomain::bootloader_magic:
			return boot::EntryReason::Requested; //return to enter the bootloader
		case boot::BackupDomain::app_fatal_error_magic:
			return boot::EntryReason::ApplicationFailure;

		case boot::BackupDomain::reset_value:
		case boot::BackupDomain::application_magic:
			break; //these two options must still be validated
		default: //the backup domain contains unknown value
			return boot::EntryReason::BackupRegisterCorrupted;
		}

		if (!boot::jumpTable.magicValid())
			return boot::EntryReason::InvalidMagic; //Magics do not match. Enter the bootloader

		if (!bit::all_cleared(boot::jumpTable.interruptVector_, boot::isrVectorAlignmentMask))
			return boot::EntryReason::UnalignedInterruptVector; //The interrupt table is not properly aligned to the 512 B boundary

		if (boot::Flash::addressOrigin_located_in_flash(boot::jumpTable.interruptVector_) != boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidInterruptVector;

		//Application entry point is saved as the second word of the interrupt table. Initial stack pointer is the first word
		std::uint32_t const* const interruptVector = reinterpret_cast<std::uint32_t const*>(boot::jumpTable.interruptVector_);

		if (boot::Flash::addressOrigin_located_in_flash(interruptVector[1]) != boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidEntryPoint;

		//Application initial stack pointer is saved as the first word of the interrupt table
		if (boot::Flash::addressOrigin_located_in_flash(interruptVector[0]) == boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidTopOfStack;

		return boot::EntryReason::DontEnter;
	}

	void configure_system_clock() {
#ifdef BOOT_STM32F1

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //Enable external oscilator
		while (bit::all_cleared(RCC->CR, RCC_CR_HSERDY)); //wait for it to stabilize

		bit::set(std::ref(RCC->CFGR),
			RCC_CFGR_PLLMULL6, //Make PLL multiply 2MHz * 6 -> 12 MHz
			RCC_CFGR_PLLSRC //clock it from PREDIV1 (divided HSE)
		);

		constexpr Frequency desiredPLLinput = 2_MHz;
		constexpr int PREDIV1 = boot::HSE / desiredPLLinput;
		static_assert(PREDIV1 * desiredPLLinput == boot::HSE, "Your HSE is not an integral multiple of 2 MHz!");
		bit::set(std::ref(RCC->CFGR2),
			PREDIV1-1 //divide HSE by PREDIV1 so that we get 4MHz at PLL input
			);

		bit::modify(std::ref(FLASH->ACR), FLASH_ACR_LATENCY,0); //Flash latency zero wait states, because the SYSCLK is slow as fuck.

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //Enable PLL
		while (bit::all_cleared(RCC->CR, RCC_CR_PLLRDY)); //wait for it to stabilize

		bit::modify(std::ref(RCC->CFGR), RCC_CFGR_SW_0 | RCC_CFGR_SW_1, RCC_CFGR_SW_PLL); //Set PLL as system clock
		while (bit::sliceable_value{ RCC->CFGR } [bit::slice{ 3,2 }].unshifted() != RCC_CFGR_SWS_PLL); //wait for it settle.
#else
#ifdef BOOT_STM32F4
		using namespace ufsel;

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //Start and wait for HSE stabilization
		while (bit::all_cleared(RCC->CR, RCC_CR_HSERDY));

		constexpr int PLLM = 12; //There is 12Mhz HSE, reduced to 1MHz input to the PLL
		constexpr int PLLN = 400; //PLL multiplies frequency to 400MHz

		RCC->PLLCFGR = bit::set(
			7 << POS_FROM_MASK(RCC_PLLCFGR_PLLR), //Probably irrelevant, not used
			15 << POS_FROM_MASK(RCC_PLLCFGR_PLLQ), //Probably irrelevant, not used
			RCC_PLLCFGR_PLLSRC, //HSE used as source
			//Divide PLL output (400MHz) by 4 in order not to exceed 100MHz on AHB
			0b01 << POS_FROM_MASK(RCC_PLLCFGR_PLLP),
			PLLN << POS_FROM_MASK(RCC_PLLCFGR_PLLN),
			PLLM << POS_FROM_MASK(RCC_PLLCFGR_PLLM));

		//Section 3.4.1 of reference manual - conversion table from CPU speed and voltage range to flash latency
		constexpr auto desired_latency = FLASH_ACR_LATENCY_3WS; //Voltage range 2.7-3.6 && 90 < HCLK <= 100 --> 3 wait states
		bit::modify(std::ref(FLASH->ACR), FLASH_ACR_LATENCY, desired_latency);
		bit::set(std::ref(FLASH->ACR), FLASH_ACR_PRFTEN);
		while (bit::get(FLASH->ACR, FLASH_ACR_LATENCY) != desired_latency); //Wait for ack

		RCC->CFGR = bit::set(
			RCC_CFGR_PPRE2_DIV1, //APB2 has full speed of AHB (not divided clock)
			RCC_CFGR_PPRE1_DIV2, //APB1 is restricted to at most 50MHz
			RCC_CFGR_HPRE_DIV1);

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //Start and wait for PLL stabilization
		while (bit::all_cleared(RCC->CR, RCC_CR_PLLRDY));

		bit::set(std::ref(RCC->CFGR), RCC_CFGR_SW_PLL); //Switch system clock to PLL and await acknowledge
		while (bit::sliceable_reference{ RCC->CFGR } [bit::slice{ 3,2 }].unshifted()!=RCC_CFGR_SWS_PLL);

		bit::clear(std::ref(RCC->CR), RCC_CR_HSION); //Kill power to HSI
#else
#error "This MCU is not supported"
#endif
#endif
	}

#pragma GCC push_options
#pragma GCC optimize ("O0")
	void do_load_section(std::uint32_t const* load_address, std::uint32_t * begin, std::uint32_t const* const end) __attribute__((section(".executed_from_flash")));
	void do_load_section(std::uint32_t const* load_address, std::uint32_t * begin, std::uint32_t const* const end) {
		for (; begin != end; ++load_address, ++begin)
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
