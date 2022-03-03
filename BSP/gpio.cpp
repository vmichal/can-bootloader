

#include "gpio.hpp"

#include <initializer_list>
#include <array>
#include <bit>
#include <algorithm>
#include <functional>

#include <ufsel/bit_operations.hpp>

namespace bsp::gpio {

	using namespace pins;

#ifdef BOOT_STM32F1
	void Initialize(void)
	{
		//Enable clock to GPIOA, GPIOB
		ufsel::bit::set(std::ref(RCC->APB2ENR),
			RCC_APB2ENR_IOPAEN,
			RCC_APB2ENR_IOPBEN,
			RCC_APB2ENR_AFIOEN
			);

		//Most of the following pin configurations are based on section 9.1.11 GPIO configurations for device peripherals
		//from the STM32f105 reference manual
		constexpr unsigned configBits = 4;

		for (Pin const& p : { CAN1_RX, CAN2_RX, CAN1_TX, CAN2_TX }) {

			auto const shift = (p.pin % 8) * configBits;
			auto const mask = ufsel::bit::bitmask_of_width(configBits);
			auto volatile &reg  = p.pin < 8 ? p.gpio()->CRL : p.gpio()->CRH;

			ufsel::bit::modify(std::ref(reg), mask, static_cast<std::uint32_t>(p.mode_), shift);
		}

		if constexpr (boot::customization::remapCAN2)
			ufsel::bit::modify(std::ref(AFIO->MAPR), ufsel::bit::bitmask_of_width(1), 1, std::countr_zero(AFIO_MAPR_CAN2_REMAP)); //remap CAN2


		//Disable clock to GPIOA, GPIOB. Pins are still fully functional
		ufsel::bit::clear(std::ref(RCC->APB2ENR),
			RCC_APB2ENR_IOPAEN,
			RCC_APB2ENR_IOPBEN,
			RCC_APB2ENR_AFIOEN
		);
	}
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2 || defined STM32G4

	void Initialize(void)
	{
		using namespace ufsel;
		//Enable clock to GPIOA, GPIOB (or B and D for STM32G4 instead)
#if defined STM32G4
	bit::set(std::ref(RCC->AHB2ENR),
				RCC_AHB2ENR_GPIODEN,
				RCC_AHB2ENR_GPIOBEN);

#else
		bit::set(std::ref(RCC->AHB1ENR),
			RCC_AHB1ENR_GPIOAEN,
			RCC_AHB1ENR_GPIOBEN);

#endif
		for (Pin const& p : { CAN1_RX, CAN2_RX, CAN1_TX, CAN2_TX }) {
			auto volatile & gpio = *p.gpio();

			bit::modify(std::ref(gpio.MODER), ufsel::bit::bitmask_of_width(2), 0b10, p.pin * 2);
			//alternate function pins
			bit::modify(std::ref(gpio.AFR[p.pin >= 8]), ufsel::bit::bitmask_of_width(4), 9, (p.pin % 8) * 4);

			bit::modify(std::ref(gpio.OSPEEDR), ufsel::bit::bitmask_of_width(2), 0b01, p.pin * 2);
		}
	}
#endif
}


/* END OF FILE */
