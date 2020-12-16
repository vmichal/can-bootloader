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
#include <library/assert.hpp>

#if !defined(BOOT_STM32F1) && !defined(BOOT_STM32F4)
#error "You must specify the platform to build for!"
#endif


#ifdef BOOT_STM32F1
#define BIT_MASK(name) name ## _Msk
#include "stm32f10x.h"
#include "core_cm3.h"
#else
#ifdef BOOT_STM32F4 //TODO make this more correct
#define BIT_MASK(name) name ## _Msk
#include "stm32f4xx.h"
#include "core_cm4.h"
#endif
#endif

namespace boot {

	extern "C" {
		extern std::uint16_t BootControlBackupRegisterAddress[];
	}

	struct BackupDomain {
		constexpr static std::uint16_t reset_value = 0x00'00; //value after power reset. Enter the application
		//Writing this value to the bootControlRegister requests entering the bootloader after reset
		constexpr static std::uint16_t bootloader_magic = 0xB007;
		constexpr static std::uint16_t application_magic = 0xC0DE; //enter the application

		//Memory location in backup domain used for data exchange between BL and application
		inline static std::uint16_t volatile& bootControlRegister = *BootControlBackupRegisterAddress;

		static void unlock() {
			using namespace ufsel;
#ifdef BOOT_STM32F1
			bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN); //Enable clock to backup domain, as wee need to access the backup reg D1
			bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable write protection of Backup domain
#else
#ifdef BOOT_STM32F4
			bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
			bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable backup domain protection
			constexpr int RCC_BDCR_RTCSEL_Pos = 8;
			bit::set(std::ref(RCC->BDCR), 0b10 << RCC_BDCR_RTCSEL_Pos); //select LSI as RTC clock
			bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
#endif
#endif
		}

		static void lock() {
			using namespace ufsel;
#ifdef BOOT_STM32F1
			//Disable clock to backup registers (to make the application feel as if no bootloader was present)
			bit::clear(std::ref(PWR->CR), PWR_CR_DBP); //Enable write protection of Backup domain
			bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN);
#else
#ifdef BOOT_STM32F4
			//Disable clock to backup registers (to make the application feel as if no bootloader was present)
			bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
			bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //select LSI as RTC clock
			bit::clear(std::ref(PWR->CR), PWR_CR_DBP); //Disable backup domain protection
			bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
#endif
#endif
		}
	};

	[[noreturn]]
	inline void resetTo(std::uint16_t const code) {

		assert(code == BackupDomain::application_magic || code == BackupDomain::bootloader_magic);

		BackupDomain::unlock();
		BackupDomain::bootControlRegister = code;

		SCB->AIRCR = (0x5fA << SCB_AIRCR_VECTKEYSTAT_Pos) | //magic value required for write to succeed
			BIT_MASK(SCB_AIRCR_SYSRESETREQ); //Start the system reset

		for (;;); //wait for reset
	}

}