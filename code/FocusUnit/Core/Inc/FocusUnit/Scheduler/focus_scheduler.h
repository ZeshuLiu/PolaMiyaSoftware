#ifndef FOCUS_SCHEDULER_H
#define FOCUS_SCHEDULER_H

#include "stm32f0xx_hal.h"

/* TIM14 产生 1 ms 时基，主循环分别以 1/10/100 ms 周期执行三类任务。 */
#define FOCUS_TASK_ENCODER (1UL << 0)
#define FOCUS_TASK_MOTOR   (1UL << 1)
#define FOCUS_TASK_ADC     (1UL << 2)

typedef struct
{
  uint32_t flags;  /* 原子领取的任务位；同类积压只执行一次 */
  uint32_t now_ms; /* 领取时的调度时刻，uint32_t 自然回绕 */
} FocusSchedulerDispatch;

typedef struct
{
  uint32_t now_ms;
  uint32_t pending_flags; /* 尚未领取的任务位 */
  uint32_t encoder_coalesced; /* 编码器任务积压合并次数，饱和于 UINT32_MAX */
  uint32_t motor_coalesced;   /* 电机任务积压合并次数 */
  uint32_t adc_coalesced;     /* ADC 任务积压合并次数 */
  uint32_t max_encoder_delay_ms; /* 首次挂起到领取的最长等待，ms */
  uint32_t max_motor_delay_ms;   /* 电机任务最长等待，ms */
  uint32_t max_adc_delay_ms;     /* ADC 任务最长等待，ms */
} FocusSchedulerStatus;

/* 主循环初始化并启动 TIM14 更新中断，仅接受 TIM14 句柄。 */
HAL_StatusTypeDef FocusScheduler_Init(TIM_HandleTypeDef *timer);
/* HAL 定时器回调的 ISR 入口：计时并置标志，不执行业务任务。 */
void FocusScheduler_OnTick(TIM_HandleTypeDef *timer);
/* 主循环原子领取/清除任务位，同时统计等待时间。 */
FocusSchedulerDispatch FocusScheduler_Take(void);
/* 读取调度时基，与 HAL SysTick 独立。 */
uint32_t FocusScheduler_Now(void);
/* 临界区复制统计，NULL 时直接返回。 */
void FocusScheduler_GetSnapshot(FocusSchedulerStatus *status);
/* 无待执行任务时 WFI；检查与休眠之间屏蔽中断，防止漏掉唤醒。 */
void FocusScheduler_Idle(void);

#endif
