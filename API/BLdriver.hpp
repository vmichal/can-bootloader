/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020, 2021 eForce FEE Prague Formula
 *
 * Contains functions and variables that constitute the firmware's interface to bootloader
 * Example code showing, what is necessary in order to integrate BL into your project,
 * can be found in API/integration_example.txt
 */

#pragma once

#include <cstdint>

#if !defined(BOOT_STM32F1) && !defined(BOOT_STM32F4) && !defined(BOOT_STM32F7) && !defined(BOOT_STM32F2)
#error "You must specify the platform to build for!"
#endif

namespace boot {

	extern "C" {
		extern std::uint16_t BootControlBackupRegisterAddress[];
	}

	struct BackupDomain {
		enum class magic : std::uint16_t {
			reset_value = 0x00'00, //value after power reset. Enter the application
			bootloader = 0xB007, //Writing this value to the bootControlRegister requests entering the bootloader after reset
			app_fatal_error = 0xDEAD, //The application has been unstable and could not be kept running.
			app_perform_can_check = 0xC0DE, //Request to enter the application after CAN bus check
			app_skip_can_check = 0x5CBC, //Request to enter the application immediately (without CAN bus check)
		};

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
	void resetTo(BackupDomain::magic where);
}