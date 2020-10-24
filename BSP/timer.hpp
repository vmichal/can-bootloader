

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f10x.h"
#include <library/units.hpp>
#include <library/assert.hpp>
#include <library/timer.hpp>
#include <Bootloader/options.hpp>
#include <optional>
#include <utility>
#include <array>
#include <cstdint>

/* Timer allocation:
	TIM6, TIM7 - available for microsecond timing
*/

constexpr auto SYS_CLK = 72'000'000_Hz; 
constexpr auto SysTickFrequency = 1'000_Hz;

extern "C" void SysTick_Handler();

struct SystemTimer {
	//uses SysTick for internal purposes

	static constexpr int ticks_per_second = SysTickFrequency.toHertz();

	//Initial tick count of the system timer
	//Set to zero for release as it allows the compiler to eliminate the entire expression involving this constant
	static constexpr std::uint32_t initialTickCount = 0;
	static constexpr Timestamp bootTime{initialTickCount};

	inline volatile static std::uint32_t ticks = initialTickCount;
public:
	static Timestamp Now();

	static Duration GetUptime();

	static void Initialize();
	friend void SysTick_Handler();
};

#endif /*__TIMER_H */

/* END OF FILE */
