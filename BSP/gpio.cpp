

#include "stm32f10x.h"
#include "gpio.hpp"

#include <initializer_list>
#include <array>
#include <algorithm>
#include <functional>

#include <ufsel/bit_operations.hpp>

namespace bsp::gpio {

	using namespace pins;

	namespace {
		void InitializePins(GPIOSpeed_TypeDef speed, GPIOMode_TypeDef const mode, std::initializer_list<Pin> const seq) {

			GPIO_InitTypeDef init{ .GPIO_Pin = 0, .GPIO_Speed = speed, .GPIO_Mode = mode };

			for (Pin p : seq) {
				init.GPIO_Pin = p.pin;
				GPIO_Init(p.gpio(), &init);
			}
		}

	}

	void Initialize(void)
	{

		//Enable clock to GPIOA, GPIOB
		ufsel::bit::set(std::ref(RCC->APB2ENR),
			RCC_APB2ENR_IOPAEN,
			RCC_APB2ENR_IOPBEN);

		//Most of the following pin configurations are based on section 9.1.11 GPIO configurations for device peripherals
		//from the STM32f105 reference manual


		auto const inputs_floating = { CAN1_RX, CAN2_RX };
		auto const alternate_pushpull = { CAN1_TX, CAN2_TX};

		/* INPUTS floating */
		InitializePins(GPIO_Speed_2MHz, GPIO_Mode_IN_FLOATING, inputs_floating);

		/* OUTPUTS WITH AF */
		InitializePins(GPIO_Speed_10MHz, GPIO_Mode_AF_PP, alternate_pushpull);
	}
}


/* END OF FILE */
