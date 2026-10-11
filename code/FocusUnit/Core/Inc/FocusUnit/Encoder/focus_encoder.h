#ifndef FOCUS_ENCODER_H
#define FOCUS_ENCODER_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

typedef struct {
  int32_t position;       /* 软件累计四倍频位置，可为负数，32 位溢出后回绕 */
  int32_t delta;          /* 本次增量，已应用 encoder_sign */
  uint16_t raw_counter;   /* TIM3 硬件 CNT，实际位宽为 16 位 */
  int8_t direction;       /* 本次增量方向：-1 / 0 / +1 */
  uint32_t timestamp_ms;  /* 最近更新时刻，调度时基 ms */
  uint32_t elapsed_ms;    /* 两次更新间隔，ms */
  uint32_t sequence;      /* 更新、置零或设置方向时递增 */
} FocusEncoderMeasurement;

/* 所有接口均在主循环调用，Init 成功后再使用其他接口。
 * TIM3 为 16 位硬件计数器，软件每 1 ms 累计到 int32_t 位置。
 * 模差算法要求相邻读取间实际位移小于 32768 计数；代码不再检查或锁存歧义。
 * GetSnapshot 的调用者提供有效指针，SetSign 的调用者传入 +1/-1。 */
HAL_StatusTypeDef FocusEncoder_Init(TIM_HandleTypeDef *htim, uint32_t now_ms);
void FocusEncoder_Update(uint32_t now_ms);
/* 清累计位置并重取 CNT 为基准。 */
void FocusEncoder_Zero(void);
/* 设置方向映射，保留已累计位置。 */
void FocusEncoder_SetSign(int8_t sign);
/* 复制主循环中的最近结果。 */
void FocusEncoder_GetSnapshot(FocusEncoderMeasurement *snapshot);

#endif
