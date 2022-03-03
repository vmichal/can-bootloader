/*
 * eForce CAN Bootloader
 *
 * Written by Vojtěch Michal
 *
 * Copyright (c) 2020, 2021 eforce FEE Prague Formula
 *
 * Implementation of functions that constitute the firmware's interface to bootloader
 * Example code showing, what is necessary in order to integrate BL into your project,
 * can be found in API/integration_example.txt
 */

#include "BLdriver.hpp"

#ifdef BOOT_STM32F1
#include "../Drivers/stm32f10x.h"
#include "../Drivers/core_cm3.h"
#elif defined(BOOT_STM32F2)
#include "../Drivers/stm32f205xx.h"
#include "../Drivers/core_cm3.h"
#elif defined(BOOT_STM32F4)
#include "../Drivers/stm32f4xx.h"
#include "../Drivers/core_cm4.h"
#elif defined(BOOT_STM32G4)
#include "../Drivers/stm32g4xx.h"
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
#elif defined BOOT_STM32F4 || defined BOOT_STM32F2
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
		bit::set(std::ref(PWR->CR), PWR_CR_DBP); //Disable backup domain protection

		bit::set(std::ref(RCC->BDCR), 0b10 << std::countr_zero(RCC_BDCR_RTCSEL)); //select LSI as RTC clock
		bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
#elif defined BOOT_STM32F7
		bit::set(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer

		bit::set(std::ref(PWR->CR1), PWR_CR1_DBP);		//Disable backup domain protection

		bit::set(std::ref(RCC->BDCR), 0b10 << std::countr_zero(RCC_BDCR_RTCSEL)); //select LSI as RTC clock
		bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
#elif defined BOOT_STM32G4
		bit::set(std::ref(RCC->APB1ENR1), RCC_APB1ENR1_PWREN); //Enable clock to power controleer

		bit::set(std::ref(PWR->CR1), PWR_CR1_DBP);		//Disable backup domain protection

		bit::set(std::ref(RCC->BDCR), 0b10 << std::countr_zero(RCC_BDCR_RTCSEL)); //select LSI as RTC clock
		bit::set(std::ref(RCC->BDCR), RCC_BDCR_RTCEN); // enable RTC clock.
#endif
	}

	void BackupDomain::lock() {
		//Disable clock to backup registers (to make the application feel as if no bootloader was present)
		using namespace ufsel;
#ifdef BOOT_STM32F1
		bit::clear(std::ref(PWR->CR), PWR_CR_DBP); //Enable write protection of Backup domain
		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN, RCC_APB1ENR_BKPEN);
#elif defined BOOT_STM32F4  || defined BOOT_STM32F2
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //deactivate RTC clock

		bit::clear(std::ref(PWR->CR), PWR_CR_DBP); 	//Disable backup domain protection

		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Enable clock to power controleer
#elif defined BOOT_STM32F7

		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //deactivate RTC clock
		
		bit::clear(std::ref(PWR->CR1), PWR_CR1_DBP);	//Enable backup domain protection
		
		bit::clear(std::ref(RCC->APB1ENR), RCC_APB1ENR_PWREN); //Disable clock to power controleer
#elif defined BOOT_STM32G4
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCEN);
		bit::clear(std::ref(RCC->BDCR), RCC_BDCR_RTCSEL); //deactivate RTC clock

		bit::clear(std::ref(PWR->CR1), PWR_CR1_DBP);	//Enable backup domain protection

		bit::clear(std::ref(RCC->APB1ENR1), RCC_APB1ENR1_PWREN); //Disable clock to power controleer
#endif
	}

	[[noreturn]]
	void resetTo(BackupDomain::magic const where) {
		UFSEL_ASSERT_INTERNAL(where == BackupDomain::magic::app_skip_can_check || where == BackupDomain::magic::app_perform_can_check
		                      || where == BackupDomain::magic::bootloader || where == BackupDomain::magic::app_fatal_error);

		BackupDomain::unlock();
		BackupDomain::bootControlRegister = static_cast<boot_control_register_t>(where);

		NVIC_SystemReset();
		for (;;);
	}


#ifdef BUILDING_BOOTLOADER
	BootloaderMetadata const bootloaderMetadata{};
#endif

}
