

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

/* We are interested in the following signals:
	FAN1_RPM -> PA8  -> TIM1_CH1
	FAN2_RPM -> PA9  -> TIM1_CH2
	FAN3_RPM -> PA10 -> TIM1_CH3
	IMD_PWM  -> PC9  -> TIM3_CH4 (remap)
*/

/* Timer allocation:
	TIM1 - Channels 1,2,3 input capture - Fan PWM sensing
	TIM2, TIM5 are chained together for microsecond timing. TIM5 is slave (slow timer)
	TIM3 - Channel 4 - IMD PWM sensing
	TIM4 - PWM from channel 1 - Fan control
	TIM6 - microsecond benchmarking of tasks
	TIM7 - microsecond benchmarking of main loop duration
*/

constexpr auto SYS_CLK = 72'000'000_Hz; 
constexpr auto TIM6_frequency = 1'000'000_Hz;
constexpr auto TIM7_frequency = 1'000'000_Hz;
constexpr auto TIM2_frequency = 1'000'000_Hz;

class SystemTimer {
	//TIM2, TIM5 are chained together for microsecond timing. TIM5 is slave (slow timer)
	static inline TIM_TypeDef& slow_ = *TIM5, &fast_ = *TIM2;
public:
	static inline int periods_; //Two chained timers have period approx 70 minutes

	static constexpr int ticks_per_second = TIM2_frequency.toHertz();

	//Initial tick count of the system timer
	//Set to zero for release as it enables the compiler to eliminate the entire expression involving this constant
	static constexpr std::uint32_t initialTickCount = 0xfe'00'00'00;
	static constexpr Timestamp bootTime{initialTickCount};

	static std::uint32_t GetTick();
	static Timestamp Now();
	static std::uint64_t GetTicksSinceBoot();

	static LongDuration GetUptime();

	static void Initialize();
};


extern "C" void TIM5_IRQHandler();


//Wraps basic timers TIM6, TIM7 with megahertz frequency
struct MicrosecondTimer {
	static_assert(TIM7_frequency == TIM6_frequency && TIM6_frequency == 1'000'000_Hz, "You better fulfil this to get microsecond resolution.");

	//TIM6 will be used for task timing, TIM7 for timing of the whole main loop
	static inline TIM_TypeDef & single_task = *TIM6;
	static inline TIM_TypeDef & main_loop = *TIM7;

private:
	TIM_TypeDef& timer_;

public:
	MicrosecondTimer(TIM_TypeDef & timer) : timer_{timer} {
		assert((timer.CR1 & TIM_CR1_CEN) == 0); //This timer is used by someone, otherwise it would be disabled in dtor
		assert(timer.CNT == 0 && (timer.SR & TIM_SR_UIF) == 0); //It should have been reset in the destructor as well

		timer.CR1 |= TIM_CR1_CEN; //Start the counter
	}

	~MicrosecondTimer() { //Reset the underlying timer for further use
		assert(timer_.CR1 & TIM_CR1_CEN); //This timer must have been activated by the constructor
		timer_.CR1 &= ~TIM_CR1_CEN; //Disable the timer
		timer_.CNT = 0; //Clear the count register
		timer_.EGR |= TIM_EGR_UG; //Update the timer
		timer_.SR &= ~TIM_SR_UIF; //Clean update interrupt flag
	}

	std::uint32_t get() const {
		bool const overflowed = timer_.SR & TIM_SR_UIF;
		return overflowed ? std::numeric_limits<std::uint16_t>::max() : timer_.CNT;
	}

	static void Initialize();

};

#endif /*__TIMER_H */

/* END OF FILE */
