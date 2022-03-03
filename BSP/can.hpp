#pragma once

/* Define to prevent recursive inclusion -------------------------------------*/
#if !defined BOOT_STM32G4


#include <Bootloader/options.hpp>
#include <ufsel/units.hpp>
#include <ufsel/bit.hpp>

constexpr Frequency can1_frequency = 500'000_Hz;
constexpr Frequency can2_frequency = 1'000'000_Hz;

namespace bsp::can {

	// Data for CAN filter configuration
	// 11 bits standard IDs. They share prefix 0x62_, the three bits are variable (range 0x620-0x627)
	namespace filter {
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = ufsel::bit::bitmask_of_width(8) << 3;
	}

	void Initialize();

}


#endif /*__CAN_H */

/* END OF FILE */
