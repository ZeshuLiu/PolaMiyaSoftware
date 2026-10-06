#include "FocusUnit/Scheduler/focus_scheduler.h"

static TIM_HandleTypeDef *scheduler_timer;
static volatile FocusSchedulerStatus scheduler_status;
static volatile uint32_t first_encoder_ms;
static volatile uint32_t first_motor_ms;
static volatile uint32_t first_adc_ms;
static uint16_t motor_divider;
static uint16_t adc_divider;

static void RaiseTask(uint32_t task, volatile uint32_t *first_ms,
                      volatile uint32_t *coalesced)
{
  if ((scheduler_status.pending_flags & task) != 0U)
  {
    if (*coalesced != UINT32_MAX)
    {
      ++(*coalesced);
    }
  }
  else
  {
    *first_ms = scheduler_status.now_ms;
    scheduler_status.pending_flags |= task;
  }
}

HAL_StatusTypeDef FocusScheduler_Init(TIM_HandleTypeDef *timer)
{
  if ((timer == NULL) || (timer->Instance != TIM14))
  {
    return HAL_ERROR;
  }
  const uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  scheduler_timer = timer;
  scheduler_status = (FocusSchedulerStatus){0};
  first_encoder_ms = 0U;
  first_motor_ms = 0U;
  first_adc_ms = 0U;
  motor_divider = 0U;
  adc_divider = 0U;
  __HAL_TIM_SET_COUNTER(timer, 0U);
  /* HAL initialization generates UG: clear its UIF before enabling IRQ. */
  __HAL_TIM_CLEAR_FLAG(timer, TIM_FLAG_UPDATE);
  const HAL_StatusTypeDef result = HAL_TIM_Base_Start_IT(timer);
  __set_PRIMASK(irq_state);
  return result;
}

void FocusScheduler_OnTick(TIM_HandleTypeDef *timer)
{
  if ((scheduler_timer == NULL) || (timer != scheduler_timer))
  {
    return;
  }
  ++scheduler_status.now_ms;
  RaiseTask(FOCUS_TASK_ENCODER, &first_encoder_ms, &scheduler_status.encoder_coalesced);
  if (++motor_divider == 10U)
  {
    motor_divider = 0U;
    RaiseTask(FOCUS_TASK_MOTOR, &first_motor_ms, &scheduler_status.motor_coalesced);
  }
  if (++adc_divider == 100U)
  {
    adc_divider = 0U;
    RaiseTask(FOCUS_TASK_ADC, &first_adc_ms, &scheduler_status.adc_coalesced);
  }
}

static void RecordDelay(uint32_t now, uint32_t first, volatile uint32_t *maximum)
{
  const uint32_t delay = now - first;
  if (delay > *maximum)
  {
    *maximum = delay;
  }
}

FocusSchedulerDispatch FocusScheduler_Take(void)
{
  const uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  const FocusSchedulerDispatch dispatch = {scheduler_status.pending_flags,
                                           scheduler_status.now_ms};
  if ((dispatch.flags & FOCUS_TASK_ENCODER) != 0U)
  {
    RecordDelay(dispatch.now_ms, first_encoder_ms, &scheduler_status.max_encoder_delay_ms);
  }
  if ((dispatch.flags & FOCUS_TASK_MOTOR) != 0U)
  {
    RecordDelay(dispatch.now_ms, first_motor_ms, &scheduler_status.max_motor_delay_ms);
  }
  if ((dispatch.flags & FOCUS_TASK_ADC) != 0U)
  {
    RecordDelay(dispatch.now_ms, first_adc_ms, &scheduler_status.max_adc_delay_ms);
  }
  scheduler_status.pending_flags = 0U;
  __set_PRIMASK(irq_state);
  return dispatch;
}

uint32_t FocusScheduler_Now(void)
{
  return scheduler_status.now_ms;
}

void FocusScheduler_GetSnapshot(FocusSchedulerStatus *status)
{
  if (status != NULL)
  {
    const uint32_t irq_state = __get_PRIMASK();
    __disable_irq();
    *status = scheduler_status;
    __set_PRIMASK(irq_state);
  }
}

void FocusScheduler_Idle(void)
{
  const uint32_t irq_state = __get_PRIMASK();
  __disable_irq();
  if ((irq_state == 0U) && (scheduler_status.pending_flags == 0U))
  {
    /* Mask-test-WFI prevents a tick between the empty check and sleep.
       NVIC pending IRQ wakes WFI even with PRIMASK set; restore then service. */
    __DSB();
    __WFI();
  }
  __set_PRIMASK(irq_state);
}
