
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __GPIO_H
#define __GPIO_H

#include <library/pin.hpp>
#include <Bootloader/options.hpp>

/* Outputs */

namespace bsp::gpio {

	namespace pins {

		inline constexpr Pin P(char port, unsigned short pin, PinMode mode) {
			std::uintptr_t ports[4] = {GPIOA_BASE,GPIOB_BASE, GPIOC_BASE, GPIOD_BASE};

			return Pin { ports[port - 'A'], pin , mode};
		}


#ifdef BOOT_STM32F1
#	if CAN1_used
		constexpr Pin CAN1_RX              = P(CAN1_RX_pin, PinMode::input_floating);
		constexpr Pin CAN1_TX              = P(CAN1_TX_pin, PinMode::af_pushpull);
#	endif
#	if CAN2_used
		constexpr Pin CAN2_RX              = P(CAN2_RX_pin, PinMode::input_floating);
		constexpr Pin CAN2_TX              = P(CAN2_TX_pin, PinMode::af_pushpull);
#	endif
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2 || defined BOOT_STM32G4
#	if CAN1_used
		constexpr Pin CAN1_RX              = P(CAN1_RX_pin, PinMode::alternate_function);
		constexpr Pin CAN1_TX              = P(CAN1_TX_pin, PinMode::alternate_function);
#	endif
#	if CAN2_used
		constexpr Pin CAN2_RX              = P(CAN2_RX_pin, PinMode::alternate_function);
		constexpr Pin CAN2_TX              = P(CAN2_TX_pin, PinMode::alternate_function);
#	endif
#endif
	}

	void Initialize(void);
}

#endif /*__GPIO_H */

/* END OF FILE */
