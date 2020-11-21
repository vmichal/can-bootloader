
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GPIO_H
#define __GPIO_H

#include <library/pin.hpp>

/* Outputs */

namespace bsp::gpio {

	namespace pins {

		inline constexpr Pin P(char port, unsigned short pin, PinMode mode) {
			std::uintptr_t ports[4] = {GPIOA_BASE,GPIOB_BASE, GPIOC_BASE, GPIOD_BASE};

			return Pin { ports[port - 'A'], pin , mode};
		}

		//CAN1_RX	GPIO port A, pin 44, digital – CAN RX
		constexpr Pin CAN1_RX              = P('A', 11, PinMode::input_floating);
		//CAN1_TX	GPIO port A, pin 45, digital – CAN TX
		constexpr Pin CAN1_TX              = P('A', 12, PinMode::af_pushpull);
		//CAN2_RX	GPIO port B, pin 33, digital – CAN RX
		constexpr Pin CAN2_RX              = P('B', 12, PinMode::input_floating);
		//CAN2_TX	GPIO port B, pin 34, digital – CAN TX
		constexpr Pin CAN2_TX              = P('B', 13, PinMode::af_pushpull);
	}

	void Initialize(void);
}

#endif /*__GPIO_H */

/* END OF FILE */
