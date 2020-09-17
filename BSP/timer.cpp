

#include "timer.hpp"
#include <library/assert.hpp>
#include <cstdint>
#include <array>

void BlockingDelay(Duration const time) {
    Timestamp const start = Timestamp::Now();
    for (;Timestamp::Now() - start < time;);
}

void SystemTimer::Initialize() {
    assert(&slow_ == TIM5);
    assert(&fast_ == TIM2);

    RCC->APB1ENR |= (RCC_APB1ENR_TIM5EN | RCC_APB1ENR_TIM2EN);

    //Configure TIM2 as the master timer
    fast_.CR1 |= TIM_CR1_URS; //Update event drives slave timer, so allow it only on overflow
    fast_.CR2 |= 0b010 << 4; //write CR2_MMS to 010 (Master mode selection - update goes to TRGO)

    slow_.SMCR |= 0b000 << 4; //write SMCR_TS to 000 (Trigger selection, selected Internal trigger 0)
    slow_.SMCR |= 0b111 << 0; //Write SMCR_SMS to 111 (Slave mode selection, External Clock mode 1 - TRGI clocks the timer)

    slow_.DIER |= TIM_DIER_UIE; //Enable update interrupt

    fast_.PSC = SYS_CLK / TIM2_frequency - 1;
    slow_.ARR = fast_.ARR = 0xff'ff; //period 2^32

    slow_.EGR |= TIM_EGR_UG; //reload both timers
    fast_.EGR |= TIM_EGR_UG;

    slow_.CNT = initialTickCount >> 16; //Fill initial counter values after updating (UE resets TIM_CNT to zero)
    fast_.CNT = initialTickCount & 0xff'ff;

    fast_.CR1 |= TIM_CR1_CEN; //enable fast counter
    slow_.CR1 |= TIM_CR1_CEN; //enable slow counter

    NVIC_EnableIRQ(TIM5_IRQn);
}

std::uint32_t SystemTimer::GetTick() {

    std::uint16_t const first_fast = fast_.CNT;
    std::uint16_t first_slow = slow_.CNT;
    std::uint16_t const second_fast = fast_.CNT;

    first_slow += (second_fast < first_fast);

    return first_slow << 16 | second_fast;
}

Timestamp SystemTimer::Now() {
    return Timestamp{GetTick()};
}

std::uint64_t SystemTimer::GetTicksSinceBoot() {
    return (static_cast<std::uint64_t>(periods_) << 32 | GetTick()) - initialTickCount;
}

LongDuration SystemTimer::GetUptime() {
    return LongDuration::fromMicroseconds(GetTicksSinceBoot());
}

extern "C" void TIM5_IRQHandler() {
    assert((TIM5->SR & TIM_SR_UIF) == TIM_SR_UIF); //There was only the update event
    TIM5->SR &= ~TIM_SR_UIF; //clear it 
    ++SystemTimer::periods_;
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
