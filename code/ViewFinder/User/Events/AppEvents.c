#include "AppEvents.h"

#include "ch32v205.h"

#define TIMER_INPUT_HZ 10000U
#define TICKS_PER_MS   (TIMER_INPUT_HZ / 1000U)

static volatile uint32_t events_pending;
static volatile uint32_t now_ms;

void TIM2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void AppEvents_Init(void)
{
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM2, ENABLE);
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    uint32_t timer_clock = clocks.PCLK1_Frequency;
    if ((RCC->CFGR0 & RCC_PPRE1) != RCC_PPRE1_DIV1) {
        timer_clock *= 2U;
    }

    TIM_TimeBaseInitTypeDef timebase = {
        .TIM_Prescaler = (uint16_t)(timer_clock / TIMER_INPUT_HZ - 1U),
        .TIM_CounterMode = TIM_CounterMode_Up,
        .TIM_Period = TICKS_PER_MS - 1U,
        .TIM_ClockDivision = TIM_CKD_DIV1,
    };
    TIM_TimeBaseInit(TIM2, &timebase);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
    NVIC_SetPriority(TIM2_IRQn, 0);
    NVIC_EnableIRQ(TIM2_IRQn);
    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
        ++now_ms;
        AppEvents_PostFromISR(APP_TICK);
    }
}

void AppEvents_PostFromISR(uint32_t events)
{
    events_pending |= events;
}

uint32_t AppEvents_Wait(void)
{
    for (;;) {
        __disable_irq();
        __asm volatile ("" ::: "memory");
        const uint32_t events = events_pending;
        events_pending = 0;
        __asm volatile ("" ::: "memory");
        __enable_irq();
        if (events != 0U) {
            return events;
        }
        /* An event arriving just before WFI stays set; TIM2 will wake us again. */
        __WFI();
    }
}

uint32_t AppEvents_NowMs(void)
{
    return now_ms;
}
