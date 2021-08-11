/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 *
 * Implementation of functions that constitute the firmware's interface to bootloader
 * Example code showing, what is necessary in order to integrate BL into your project,
 * can be found in API/integration_example.txt
 */

#include "BLdriver.hpp"

#ifdef BOOT_STM32F1
#include "../Drivers/stm32f10x.h"
#include "../Drivers/core_cm3.h"
#elif defined(BOOT_STM32F4)
#include "../Drivers/stm32f4xx.h"
#include "../Drivers/core_cm4.h"
#elif defined(BOOT_STM32F7)
#include "../Drivers/stm32f767xx.h"
#include "../Drivers/core_cm7.h"
#endif

#include <bit>
#include <ufsel/assert.hpp>
#include <ufsel/bit_operations.hpp>

namespace boot {

	void BackupDomain::unlock() {
		using namespace ufsel;
#ifdef BOOT_STM32F1
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN); //Enable clock to backup domain, as wee need to access the backup reg D1
		bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable write protection of Backup domain
#elif defined BOOT_STM32F4
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
		bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable backup domain protection

		bit::set(std::ref(RCC->BDCR), 0b10 << std::countr_zero(RCC_BDCR_RTCSEL)); //select LSI as RTC clock
		bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
#elif defined BOOT_STM32F7
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer

		bit::set(std::ref(PWR->CR1), PWR_CR1_DBP);		//Disable backup domain protection

		bit::set(std::ref(RCC->BDCR), 0b10 << std::countr_zero(RCC_BDCR_RTCSEL)); //select LSI as RTC clock
		bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
#endif
	}

	void BackupDomain::lock() {
		//Disable clock to backup registers (to make the application feel as if no bootloader was present)
		using namespace ufsel;
#ifdef BOOT_STM32F1
		bit::clear(std::ref(PWR->CR), PWR_CR_DBP); //Enable write protection of Backup domain
		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN);
#elif defined BOOT_STM32F4
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //deactivate RTC clock

		bit::clear(std::ref(PWR->CR), PWR_CR_DBP); 	//Disable backup domain protection

		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
#elif defined BOOT_STM32F7

		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //deactivate RTC clock
		
		bit::clear(std::ref(PWR->CR1), PWR_CR1_DBP);	//Disable backup domain protection
		
		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
#endif
	}

	[[noreturn]]
	void resetTo(std::uint16_t const code) {
		assert(code == BackupDomain::application_magic || code == BackupDomain::bootloader_magic || code == BackupDomain::app_fatal_error_magic);

		BackupDomain::unlock();
		BackupDomain::bootControlRegister = code;

		NVIC_SystemReset();
		for (;;);
	}

}
