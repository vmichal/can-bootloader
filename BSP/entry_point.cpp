/*
 * eForce CAN Bootloader
 *
 * Written by Vojtìch Michal
 *
 * Copyright (c) 2020 eforce FEE Prague Formula
 */

#include <Bootloader/main.hpp>
#include <ufsel/bit_operations.hpp>

#include "stm32f10x.h"

#include "gpio.hpp"
#include "can.hpp"
#include "timer.hpp"

namespace bsp {

	using namespace ufsel;

	//Initializes system clock to max frequencies: 72MHz SYSCLK, AHB, APB2; 32MHz APB1
	extern "C" void pre_main() {
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
		while (bit::sliceable_value{ RCC->CFGR } [bit::slice{ 1,0 }] != RCC_CFGR_SWS_PLL); //wait for it settle

		//Configure and start system microsecond clock
		SystemTimer::Initialize();
	}

	extern "C" int main(void) {

		MicrosecondTimer::Initialize();

		gpio::Initialize();

		Init_CAN_Interfaces();

		gpio::LED_Blue_On();
		gpio::LED_Orange_On();

		boot::main();
	}

	extern "C" int _write(int file, std::uint8_t const* ptr, int len) {

		boot::ser0.Write(ptr, len);

		return len;
	}

}