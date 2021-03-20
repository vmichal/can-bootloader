

#include "gpio.hpp"

#include <initializer_list>
#include <array>
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
			RCC_APB2ENR_IOPBEN);

		//Most of the following pin configurations are based on section 9.1.11 GPIO configurations for device peripherals
		//from the STM32f105 reference manual
		constexpr unsigned configBits = 4;

		for (Pin const& p : { CAN1_RX, CAN2_RX, CAN1_TX, CAN2_TX }) {

			auto const shift = (p.pin % 8) * configBits;
			auto const mask = ufsel::bit::bitmask_of_width(configBits);
			auto volatile &reg  = p.pin < 8 ? p.gpio()->CRL : p.gpio()->CRH;

			ufsel::bit::modify(std::ref(reg), mask, static_cast<std::uint32_t>(p.mode_), shift);
		}
		//Disable clock to GPIOA, GPIOB. Pins are still fully functional
		ufsel::bit::clear(std::ref(RCC->APB2ENR),
			RCC_APB2ENR_IOPAEN,
			RCC_APB2ENR_IOPBEN);
	}
#elif BOOT_STM32F4

	void Initialize(void)
	{
		using namespace ufsel;
		//Enable clock to GPIOA, GPIOB
		bit::set(std::ref(RCC->AHB1ENR),
			RCC_AHB1ENR_GPIOAEN,
			RCC_AHB1ENR_GPIOBEN);

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
