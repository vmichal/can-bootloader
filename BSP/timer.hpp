

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

struct SystemTimer {
	//uses SysTick for internal purposes

	static constexpr int ticks_per_second = SysTickFrequency.toHertz();

	//Initial tick count of the system timer
	//Set to zero for release as it enables the compiler to eliminate the entire expression involving this constant
	static constexpr std::uint32_t initialTickCount = 0x00'00'00'00;
	static constexpr Timestamp bootTime{initialTickCount};

	inline static std::uint32_t ticks = initialTickCount;
	static std::uint32_t GetTick();
	static Timestamp Now();

	static Duration GetUptime();

	static void Initialize();
};

#endif /*__TIMER_H */

/* END OF FILE */
