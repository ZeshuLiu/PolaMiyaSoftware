#ifndef FOCUS_SCHEDULER_H
#define FOCUS_SCHEDULER_H

#include "stm32f0xx_hal.h"

#define FOCUS_TASK_ENCODER (1UL << 0)
#define FOCUS_TASK_MOTOR   (1UL << 1)
#define FOCUS_TASK_ADC     (1UL << 2)

typedef struct
{
  uint32_t flags;
  uint32_t now_ms;
} FocusSchedulerDispatch;

typedef struct
{
  uint32_t now_ms;
  uint32_t pending_flags;
  uint32_t encoder_coalesced;
  uint32_t motor_coalesced;
  uint32_t adc_coalesced;
  uint32_t max_encoder_delay_ms;
  uint32_t max_motor_delay_ms;
  uint32_t max_adc_delay_ms;
} FocusSchedulerStatus;

HAL_StatusTypeDef FocusScheduler_Init(TIM_HandleTypeDef *timer);
void FocusScheduler_OnTick(TIM_HandleTypeDef *timer);
FocusSchedulerDispatch FocusScheduler_Take(void);
uint32_t FocusScheduler_Now(void);
void FocusScheduler_GetSnapshot(FocusSchedulerStatus *status);
void FocusScheduler_Idle(void);

#endif
