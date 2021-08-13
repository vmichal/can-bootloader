
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
		constexpr Pin CAN1_RX              = P('A', 11, PinMode::input_floating);
		constexpr Pin CAN1_TX              = P('A', 12, PinMode::af_pushpull);
		constexpr Pin CAN2_RX              = P('B', boot::customization::remapCAN2 ? 5 : 12, PinMode::input_floating);
		constexpr Pin CAN2_TX              = P('B', boot::customization::remapCAN2 ? 6: 13, PinMode::af_pushpull);
#elif defined BOOT_STM32F4 || defined BOOT_STM32F7 || defined BOOT_STM32F2
		constexpr Pin CAN1_RX              = P('A', 11, PinMode::alternate_function);
		constexpr Pin CAN1_TX              = P('A', 12, PinMode::alternate_function);
		constexpr Pin CAN2_RX              = P('B', 12, PinMode::alternate_function);
		constexpr Pin CAN2_TX              = P('B', 13, PinMode::alternate_function);
#endif
	}

	void Initialize(void);
}

#endif /*__GPIO_H */

/* END OF FILE */
