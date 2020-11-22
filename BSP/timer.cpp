

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
        BIT_MASK(SysTick_CTRL_CLKSOURCE), //drive the SysTick from HCLK (72 MHz)
        BIT_MASK(SysTick_CTRL_TICKINT), //enable the interrupt
        BIT_MASK(SysTick_CTRL_ENABLE) //enable the counter
        );
}

Timestamp SystemTimer::Now() {
    return Timestamp{ticks};
}

Duration SystemTimer::GetUptime() {
    return Duration::fromMilliseconds(ticks);
}

extern "C" void SysTick_Handler() {
    ++SystemTimer::ticks;
}

Timestamp Timestamp::Now() { 
    return SystemTimer::Now();
}

/* END OF FILE */
