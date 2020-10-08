

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

Timestamp Timestamp::Now() { 
    return SystemTimer::Now();
}

/* END OF FILE */
