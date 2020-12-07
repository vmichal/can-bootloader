

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H
#define __CAN_H


#include <Bootloader/options.hpp>
#include <library/units.hpp>

constexpr Frequency can1_frequency = 500'000_Hz;
constexpr Frequency can2_frequency = 1'000'000_Hz;

namespace bsp::can {

	void Initialize();

}


#endif /*__CAN_H */

/* END OF FILE */
