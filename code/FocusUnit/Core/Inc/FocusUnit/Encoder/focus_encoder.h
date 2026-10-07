#ifndef FOCUS_ENCODER_H
#define FOCUS_ENCODER_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

/* 状态位可按位检查；半圈歧义与位置饱和会锁存，需 Zero 恢复。 */
typedef enum {
  FOCUS_ENCODER_STATUS_OK = 0U,
  FOCUS_ENCODER_STATUS_NOT_INITIALIZED = 1U << 0,
  FOCUS_ENCODER_STATUS_HAL_ERROR = 1U << 1,
  FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA = 1U << 2,
  FOCUS_ENCODER_STATUS_POSITION_SATURATED = 1U << 3
} FocusEncoderStatus;

typedef struct {
  int32_t position;       /* 从软件零点累计的四倍频计数，可为负数 */
  int32_t delta;          /* 本次增量，已应用 encoder_sign */
  uint16_t raw_counter;   /* 读取到的 TIM3 CNT 原始值 */
  int8_t direction;       /* 本次增量方向：-1 / 0 / +1 */
  uint32_t timestamp_ms;  /* 最近更新时刻，调度时基 ms */
  uint32_t elapsed_ms;    /* 两次更新间隔，用于观察任务延迟 */
  uint32_t sequence;      /* 更新、置零或有效符号设置时递增 */
  uint8_t valid;          /* 位置累计是否有效 */
  uint32_t status;        /* FocusEncoderStatus 状态位 */
} FocusEncoderMeasurement;

/* MX_TIM3_Init 后启动硬件 AB 四倍频计数；业务接口从主循环调用。
 * Update 每 1 ms 读取 CNT，不使用逐边沿中断。相邻采样实际位移须严格小于
 * 32768 计数；恰好半圈会报歧义，更大位移或多圈无法仅凭 CNT 自动检出。
 * uint32_t 时间差可跨时基回绕；elapsed_ms 记录实际采样间隔。
 * IOC 的 IC1/2Filter=4 用于输入滤波，实际线序和滤波适配仍需实机确认。 */
HAL_StatusTypeDef FocusEncoder_Init(TIM_HandleTypeDef *htim, uint32_t now_ms);
void FocusEncoder_Update(uint32_t now_ms);
/* 清累计位置和锁存错误，重取当前 CNT 为基准；硬件继续计数。 */
void FocusEncoder_Zero(void);
/* 仅接受 +1/-1；重建 CNT 基准，不翻转已累计位置，也不清锁存错误。 */
void FocusEncoder_SetSign(int8_t sign);
/* 在临界区内复制完整结果；允许 ISR 读取，NULL 时直接返回。 */
void FocusEncoder_GetSnapshot(FocusEncoderMeasurement *snapshot);

#endif
