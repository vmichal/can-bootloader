#pragma once

/* Define to prevent recursive inclusion -------------------------------------*/
#if !defined BOOT_STM32G4


#include <Bootloader/options.hpp>
#include <ufsel/units.hpp>
#include <ufsel/bit.hpp>

#if CAN1_used
constexpr Frequency can1_frequency = CAN1_BITRATE;
#endif

#if CAN2_used
constexpr Frequency can2_frequency = CAN2_BITRATE;
#endif

namespace bsp::can {

	// Data for CAN filter configuration
	// 11 bits standard IDs. They share prefix 0x62_, the three bits are variable (range 0x620-0x627)
	namespace filter {
		constexpr unsigned sharedPrefix = 0x62 << 4;
		constexpr unsigned mustMatch = ufsel::bit::bitmask_of_width(8) << 3;
	}

	void initialize();

}


#endif /*__CAN_H */

/* END OF FILE */
