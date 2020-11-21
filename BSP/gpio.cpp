

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
		void InitializePins(std::initializer_list<Pin> const seq) {

			for (Pin const&p : seq) {

				std::uint32_t volatile & CRH = p.gpio()->CRH;
				CRH = CRH & ~ (ufsel::bit::bitmask_of_width(4) << ((p.pin - 8)*4));
				CRH = CRH | (static_cast<std::uint32_t>(p.mode_) << ((p.pin - 8)*4));
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


		InitializePins({ CAN1_RX, CAN2_RX, CAN1_TX, CAN2_TX });
	}
}


/* END OF FILE */
