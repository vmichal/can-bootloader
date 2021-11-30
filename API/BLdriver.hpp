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

#ifdef BUILDING_BOOTLOADER
	// Required for BootloaderMetadata
	#include <ufsel/sw_build.hpp>
#endif

namespace boot {

	struct BootloaderMetadata;

	namespace impl {
		extern "C" {
			extern std::uint16_t BootControlBackupRegisterAddress[];
			extern BootloaderMetadata bootloader_metadata_address[];
		}
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
		inline static std::uint16_t volatile& bootControlRegister = *impl::BootControlBackupRegisterAddress;

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


	/* Memory mapped structure exposing useful data about the bootloader (e.g. the software build) to the application. */
	struct BootloaderMetadata {
		constexpr static std::uint32_t expected_magics[] {0xcafe'babe, 0xb16'b00b5, 0xface'b00c};

		std::uint32_t magic0
#ifdef BUILDING_BOOTLOADER
				= expected_magics[0]
#endif
		;
		std::uint32_t commit_hash
#ifdef BUILDING_BOOTLOADER
				= ufsel::git::commit_hash()
#endif
		;
		std::uint32_t has_dirty_working_tree
#ifdef BUILDING_BOOTLOADER
				= ufsel::git::has_dirty_working_tree()
#endif
		;

		std::uint32_t magic1
#ifdef BUILDING_BOOTLOADER
				= expected_magics[1]
#endif
		;

		char build_date[16]
#ifdef BUILDING_BOOTLOADER
				= __DATE__
#endif
		;
		char build_time[16]
#ifdef BUILDING_BOOTLOADER
				= __TIME__
#endif
		;
		std::uint32_t magic2
#ifdef BUILDING_BOOTLOADER
				= expected_magics[2]
#endif
		;

		[[nodiscard]]
		bool are_magics_valid() const {
			return magic0 == expected_magics[0] && magic1 == expected_magics[1] && magic2 == expected_magics[2];
		}
	};

#ifdef  BUILDING_BOOTLOADER
	// Define a whole object with constant initializer to be stored in the memory
	__attribute__((section("bootloaderMetadataSection"))) extern const BootloaderMetadata bootloaderMetadata;
#else
	// Define only a reference for reading. No object is created, so the compiler does not attempt to initialize it
	inline BootloaderMetadata const& bootloaderMetadata = *impl::bootloader_metadata_address;
#endif

	[[noreturn]]
	void resetTo(BackupDomain::magic where);
}
