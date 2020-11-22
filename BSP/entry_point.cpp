/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <Bootloader/flash.hpp>
#include <Bootloader/bootloader.hpp>
#include <ufsel/bit_operations.hpp>

#include "stm32f10x.h"

#include "gpio.hpp"
#include "can.hpp"
#include "timer.hpp"

#include <cstdint>

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

	extern std::uint32_t _sdata[]; //Points to the start of data in RAM
	extern std::uint32_t _edata[]; //Points past the end of data in RAM
	extern std::uint32_t _load_data[]; //Points to the start of data in FLASH

	extern std::uint32_t _sbss[]; //Points to the beginning of .bss in RAM
	extern std::uint32_t _ebss[]; //Points past the end of .bss in RAM

	extern std::uint32_t _stext[]; //Points to the beginning of .text in RAM
	extern std::uint32_t _etext[]; //Points past the end of .text in RAM
	extern std::uint32_t _load_text[]; //Points to the start of .text in FLASH

	extern std::uint32_t     _sisr_vector[]; //Points to the beginning of .isr_vector in RAM
	extern std::uint32_t     _eisr_vector[]; //Points past the end of .isr_vector in RAM
	extern std::uint32_t _load_isr_vector[]; //Points to the start of .isr_vector in FLASH

	extern std::uint32_t     _srodata[]; //Points to the beginning of .rodata in RAM
	extern std::uint32_t     _erodata[]; //Points past the end of .rodata in RAM
	extern std::uint32_t _load_rodata[]; //Points to the start of .rodata in FLASH

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

	//Initializes system clock to max frequencies: 72MHz SYSCLK, AHB, APB2; 36MHz APB1
	void configure_system_clock() {

		bit::set(std::ref(RCC->CR), RCC_CR_HSEON); //Enable external oscilator
		while (bit::all_cleared(RCC->CR, RCC_CR_HSERDY)); //wait for it to stabilize

		bit::set(std::ref(RCC->CFGR),
			RCC_CFGR_PLLMULL9, //Make PLL multiply 8MHz * 9 -> 72 MHz (max frequency)
			RCC_CFGR_PLLSRC, //clock it from 8 MHz HSE
			RCC_CFGR_PPRE1_DIV2, //Divide clock for APB1 by 2 (max 36 MHz)
			RCC_CFGR_ADCPRE_DIV6 //Divide by six for adc (max 14 MHz)
		);

		bit::modify(std::ref(FLASH->ACR), FLASH_ACR_LATENCY, FLASH_ACR_LATENCY_2); //Flash latency of two wait states

		bit::set(std::ref(RCC->CR), RCC_CR_PLLON); //Enable PLL
		while (bit::all_cleared(RCC->CR, RCC_CR_PLLRDY)); //wait for it to stabilize

		bit::modify(std::ref(RCC->CFGR), RCC_CFGR_SW_0 | RCC_CFGR_SW_1, RCC_CFGR_SW_PLL); //Set PLL as system clock
		while (bit::sliceable_value{ RCC->CFGR } [bit::slice{ 3,2 }].unshifted() != RCC_CFGR_SWS_PLL); //wait for it settle.

		//Configure and start system milisecond clock
		SystemTimer::Initialize();
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

	bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN); //Enable clock to backup domain, as wee need to access the backup reg D1

	boot::EntryReason const reason = determineApplicationAvailability();
	//Disable clock to backup registers (to make the application feel as if no bootloader was present)
	bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN);

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
	__libc_init_array(); //Branch to static constructors

	boot::Bootloader::setEntryReason(reason);
	bsp::gpio::Initialize();
	bsp::can::Initialize();

	boot::main();
}