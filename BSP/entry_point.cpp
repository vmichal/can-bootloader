/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <Bootloader/main.hpp>
#include <Bootloader/flash.hpp>
#include <Bootloader/bootloader.hpp>
#include <ufsel/bit_operations.hpp>

#include "stm32f10x.h"

#include "gpio.hpp"
#include "can.hpp"
#include "timer.hpp"


namespace bsp {

	using namespace ufsel;

	/* Checks the backup domain, jump table in flash, firmware integrity etc.
	to decide whether the application or the bootloader shall be entered.*/
	boot::EntryReason bootloader_requested() {

		if (!boot::jumpTable.magicValid())
			return boot::EntryReason::InvalidMagic; //Magics do not match. Enter the bootloader

		if (!bit::all_cleared(boot::jumpTable.interruptVector_, bit::bitmask_of_width(9))) //TODO maybe make this width configurable?
			return boot::EntryReason::UnalignedInterruptVector; //The interrupt table is not properly aligned to the 512 B boundary

		//Application entry point is saved as the second word of the interrupt table
		std::uint32_t const * const interruptVector = reinterpret_cast<std::uint32_t const *>(boot::jumpTable.interruptVector_);
		if (boot::jumpTable.entryPoint_ != interruptVector[1]) //If they do not match, enter the bootloader
			return boot::EntryReason::EntryPointMismatch;

		if (boot::Flash::addressOrigin(boot::jumpTable.entryPoint_) != boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidEntryPoint;

		if (boot::Flash::addressOrigin(boot::jumpTable.interruptVector_) != boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidInterruptVector;

		//Application initial stack pointer is saved as the first word of the interrupt table
		if (boot::Flash::addressOrigin(interruptVector[0]) == boot::AddressSpace::AvailableFlash)
			return boot::EntryReason::InvalidTopOfStack;

		switch (boot::BackupDomain::bootControlRegister) {
		case boot::BackupDomain::reset_value: //enter the application after power reset
		case boot::BackupDomain::application_magic:
			return boot::EntryReason::DontEnter;

		case boot::BackupDomain::bootloader_magic:
			return boot::EntryReason::Requested; //return to enter the bootloader
		}

		//the backup domain contains unknown value
		return boot::EntryReason::backupRegisterCorrupted;
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

		//Configure and start system microsecond clock
		SystemTimer::Initialize();
	}

	//Decides whether the CPU shall enter the application firmware or start listening for communication in bootloader mode
	extern "C" void pre_main() {
		//Beware that absolutely nothing has been enabled by now. .data is filled and .bss is cleared, but that is all
		//No static constructors have been called yet either.

		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN); //Enable clock to backup domain, as wee need to access the backup reg D1

		boot::EntryReason reason = bootloader_requested();
		//Disable clock to backup registers (to make the application feel as if no bootloader was present)
		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN);

		int app_return_value = 0;

		if (reason == boot::EntryReason::DontEnter) {

			SCB->VTOR = boot::jumpTable.interruptVector_; //Set the address of application's interrupt vector
			//TODO maybe estabilish the application's stack pointer, bur probably not and then restore it to our stack pointer
			app_return_value = reinterpret_cast<int(*)()>(boot::jumpTable.entryPoint_)(); //Jump to the main application

			/*Restore the address of bootloader's interrupt vector (must be tha same as the initial
			vaule after reset, since it must have been loaded by the MCU at reset).*/
			SCB->VTOR = 0; 

			reason = boot::EntryReason::ApplicationReturned;
		}

		//Reached only if we have to enter the bootloader or the application returned from it's main
		boot::Bootloader::setEntryReason(reason, app_return_value);
		configure_system_clock();
	}

	extern "C" int main() {

		MicrosecondTimer::Initialize();

		gpio::Initialize();

		can::Initialize();

		boot::main();
	}
}