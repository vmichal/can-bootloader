
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GPIO_H
#define __GPIO_H

#include <library/pin.hpp>

/* Outputs */

namespace gpio {

	namespace pins {

		inline constexpr Pin P(char port, unsigned pin) {
			std::uintptr_t ports[4] = {GPIOA_BASE,GPIOB_BASE, GPIOC_BASE, GPIOD_BASE};

			return Pin { ports[port - 'A'], uint16_t(1 << pin) };
		}

		//CAN1_RX	GPIO port A, pin 44, digital – CAN RX
		constexpr Pin CAN1_RX              = P('A', 11);
		//CAN1_TX	GPIO port A, pin 45, digital – CAN TX
		constexpr Pin CAN1_TX              = P('A', 12);
		//CAN2_RX	GPIO port B, pin 33, digital – CAN RX
		constexpr Pin CAN2_RX              = P('B', 12);
		//CAN2_TX	GPIO port B, pin 34, digital – CAN TX
		constexpr Pin CAN2_TX              = P('B', 13);
	}

	void Initialize(void);
}

#endif /*__GPIO_H */

/* END OF FILE */
