

#include "stm32f10x.h"
#include "gpio.hpp"

#include <initializer_list>
#include <array>
#include <algorithm>
#include <functional>


namespace gpio {

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
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

		RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

		GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

		//Most of the following pin configurations are based on section 9.1.11 GPIO configurations for device peripherals
		//from the STM32f105 reference manual


		auto const output_push_pull_low = {LED_BLUE, LED_ORANGE};
		auto const inputs_floating = { CAN1_RX, CAN2_RX };
		auto const alternate_pushpull = { CAN1_TX, CAN2_TX};

		InitializePins(GPIO_Speed_2MHz, GPIO_Mode_Out_PP, output_push_pull_low);
		std::for_each(output_push_pull_low.begin(), output_push_pull_low.end(), std::mem_fn(&Pin::Clear));

		/* INPUTS floating */
		InitializePins(GPIO_Speed_2MHz, GPIO_Mode_IN_FLOATING, inputs_floating);
		GPIO_PinRemapConfig(GPIO_FullRemap_TIM3, ENABLE);

		/* OUTPUTS WITH AF */
		InitializePins(GPIO_Speed_10MHz, GPIO_Mode_AF_PP, alternate_pushpull);

	}
}


/* END OF FILE */
