

#include <stm32f10x.h>
#include <core_cm3.h>
#include "timer.hpp"
#include <library/assert.hpp>
#include <cstdint>
#include <array>
#include <ufsel/bit_operations.hpp>

void BlockingDelay(Duration const time) {
    Timestamp const start = Timestamp::Now();
    for (;Timestamp::Now() - start < time;);
}

void SystemTimer::Initialize() {
    SysTick->VAL = 0; //reset the systick timer
    SysTick->LOAD = SYS_CLK / SysTickFrequency - 1;

    using namespace ufsel::bit;
    set(std::ref(SysTick->CTRL),
        SysTick_CTRL_CLKSOURCE, //drive the SysTick from HCLK (72 MHz)
        SysTick_CTRL_TICKINT, //enable the interrupt
        SysTick_CTRL_ENABLE //enable the counter
        );
}

std::uint32_t SystemTimer::GetTick() {
    return ticks;
}

Timestamp SystemTimer::Now() {
    return Timestamp{GetTick()};
}

Duration SystemTimer::GetUptime() {
    return Duration::fromMicroseconds(GetTick());
}

extern "C" void SysTick_Handler() {
    ++SystemTimer::ticks;
}

void MicrosecondTimer::Initialize()
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, ENABLE);

    TIM6->CR1 |= TIM_CR1_URS; //Let both timers assert the update flag only on counter overflow
    TIM7->CR1 |= TIM_CR1_URS;

    TIM_TimeBaseInitStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStruct.TIM_Prescaler = SYS_CLK / TIM6_frequency - 1;
    TIM_TimeBaseInitStruct.TIM_Period = 0xffff;
    TIM_TimeBaseInitStruct.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseInitStruct);

    TIM_TimeBaseInitStruct.TIM_Prescaler = SYS_CLK / TIM7_frequency - 1;
    TIM_TimeBaseInit(TIM7, &TIM_TimeBaseInitStruct);

}


Timestamp Timestamp::Now() { 
    return SystemTimer::Now();
}

bool Timestamp::TimeElapsed(Duration const duration) const {
    return Timestamp::Now() - *this > duration;
}

SysTickTimer::SysTickTimer() : startTime{ Timestamp::Now() } {}

void SysTickTimer::Restart() {
    startTime = Timestamp::Now();
}

Duration SysTickTimer::GetTimeElapsed() const {
    return Timestamp::Now() - startTime;
}

/* END OF FILE */
