/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 *
 * Contains functions and variables that constitute the firmware's interface to bootloader
 * Example code showing, what is necessary in order to integrate BL into your project,
 * can be found in API/integration_example.txt
 */

#pragma once

#include <cstdint>
#include <ufsel/bit_operations.hpp>

#if !defined(BOOT_STM32F1) && !defined(BOOT_STM32F4) && !defined(BOOT_STM32F7)
#error "You must specify the platform to build for!"
#endif

namespace boot {

	extern "C" {
		extern std::uint16_t BootControlBackupRegisterAddress[];
	}

	struct BackupDomain {
		constexpr static std::uint16_t reset_value = 0x00'00; //value after power reset. Enter the application
		//Writing this value to the bootControlRegister requests entering the bootloader after reset
		constexpr static std::uint16_t bootloader_magic = 0xB007;
		//The application has been unstable and could not be kept running.
		constexpr static std::uint16_t app_fatal_error_magic = 0xDEAD;
		constexpr static std::uint16_t application_magic = 0xC0DE; //enter the application

		//Memory location in backup domain used for data exchange between BL and application
		inline static std::uint16_t volatile& bootControlRegister = *BootControlBackupRegisterAddress;

		static void lock()
#ifdef BUILDING_BOOTLOADER
			/* Since we need this function during the execution of Reset_Handler, we must place
			it in flash memory. Normal .text is located in RAM and has not been loaded yet.*/
			__attribute__((section(".executed_from_flash")))
#endif
			;
		static void unlock()
#ifdef BUILDING_BOOTLOADER
			/* Since we need this function during the execution of Reset_Handler, we must place
			it in flash memory. Normal .text is located in RAM and has not been loaded yet.*/
			__attribute__((section(".executed_from_flash")))
#endif
		;

	};

	[[noreturn]]
	void resetTo(std::uint16_t code);
}