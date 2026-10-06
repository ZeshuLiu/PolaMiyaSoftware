#include <assert.h>
#include <stdio.h>
#include "stm32f0xx_hal.h"

TIM_TypeDef test_tim14;
uint32_t test_primask;
uint32_t test_sleep_calls;
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer)
{
  assert(timer->Instance == TIM14);
  assert(test_primask == 1U);
  assert(timer->Instance->CNT == 0U);
  assert((timer->Instance->SR & TIM_FLAG_UPDATE) == 0U);
  return HAL_OK;
}

/* Compile the real source; direct internal access only seeds the clock-wrap
   boundary without executing 2^32 test iterations. */
#include "../../../Core/Src/FocusUnit/Scheduler/focus_scheduler.c"

int main(void)
{
  TIM_HandleTypeDef timer = {TIM14};
  TIM_TypeDef other_instance = {0};
  TIM_HandleTypeDef other = {&other_instance};
  FocusSchedulerStatus status;
  FocusSchedulerDispatch dispatch;
  assert(FocusScheduler_Init(NULL) == HAL_ERROR);
  test_tim14.SR = TIM_FLAG_UPDATE;
  assert(FocusScheduler_Init(&timer) == HAL_OK);
  FocusScheduler_OnTick(&other);
  assert(FocusScheduler_Now() == 0U);
  for (unsigned i = 0; i < 99U; ++i) FocusScheduler_OnTick(&timer);
  FocusScheduler_GetSnapshot(&status);
  assert((status.pending_flags & FOCUS_TASK_ADC) == 0U);
  FocusScheduler_OnTick(&timer);
  dispatch = FocusScheduler_Take();
  assert(dispatch.now_ms == 100U);
  assert(dispatch.flags == (FOCUS_TASK_ENCODER | FOCUS_TASK_MOTOR | FOCUS_TASK_ADC));
  FocusScheduler_GetSnapshot(&status);
  assert(status.pending_flags == 0U);
  assert(status.encoder_coalesced == 99U && status.motor_coalesced == 9U);
  assert(status.adc_coalesced == 0U);
  assert(status.max_encoder_delay_ms == 99U && status.max_motor_delay_ms == 90U);
  assert(status.max_adc_delay_ms == 0U);
  FocusScheduler_Idle();
  assert(test_sleep_calls == 1U && test_primask == 0U);
  FocusScheduler_OnTick(&timer);
  FocusScheduler_Idle();
  assert(test_sleep_calls == 1U);
  dispatch = FocusScheduler_Take();
  assert(dispatch.flags == FOCUS_TASK_ENCODER);
  test_primask = 1U;
  FocusScheduler_Idle();
  assert(test_sleep_calls == 1U && test_primask == 1U);
  assert(FocusScheduler_Init(&timer) == HAL_OK && test_primask == 1U);
  test_primask = 0U;
  scheduler_status.now_ms = UINT32_MAX - 1U;
  motor_divider = 9U;
  adc_divider = 99U;
  FocusScheduler_OnTick(&timer);
  FocusScheduler_OnTick(&timer);
  dispatch = FocusScheduler_Take();
  assert(dispatch.now_ms == 0U);
  assert(dispatch.flags == (FOCUS_TASK_ENCODER | FOCUS_TASK_MOTOR | FOCUS_TASK_ADC));
  FocusScheduler_GetSnapshot(&status);
  assert(status.max_encoder_delay_ms == 1U && status.max_adc_delay_ms == 1U);
  puts("scheduler logic: PASS");
  return 0;
}
