#include "FocusUnit/Encoder/focus_encoder.h"
#include <limits.h>

static TIM_HandleTypeDef *encoder_timer;
static FocusEncoderMeasurement measurement;
static uint16_t previous_counter;
static int8_t encoder_sign = 1;
static uint8_t initialized;
static uint32_t latched_fault; /* 歧义或饱和后锁存，避免继续累计错误位置 */

/* 保存并屏蔽中断，保护位置与 CNT 基准的一致性。 */
static uint32_t lock_interrupts(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

/* 恢复进入临界区前的中断状态，不强制打开原本关闭的中断。 */
static void unlock_interrupts(uint32_t primask)
{
  __set_PRIMASK(primask);
}

/* 启动两路硬件编码器计数，以当前 CNT 建立软件位置零点。 */
HAL_StatusTypeDef FocusEncoder_Init(TIM_HandleTypeDef *htim, uint32_t now_ms)
{
  HAL_StatusTypeDef result;
  uint32_t primask;
  if ((htim == NULL) || (htim->Instance == NULL)) {
    return HAL_ERROR;
  }
  result = HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
  primask = lock_interrupts();
  encoder_timer = htim;
  measurement.position = 0;
  measurement.delta = 0;
  measurement.raw_counter = (uint16_t)__HAL_TIM_GET_COUNTER(htim);
  measurement.direction = 0;
  measurement.timestamp_ms = now_ms;
  measurement.elapsed_ms = 0U;
  measurement.sequence = 0U;
  measurement.valid = (result == HAL_OK) ? 1U : 0U;
  measurement.status = (result == HAL_OK) ? FOCUS_ENCODER_STATUS_OK : FOCUS_ENCODER_STATUS_HAL_ERROR;
  previous_counter = measurement.raw_counter;
  initialized = (result == HAL_OK) ? 1U : 0U;
  latched_fault = FOCUS_ENCODER_STATUS_OK;
  unlock_interrupts(primask);
  return result;
}

/* 1 ms 主循环任务：16 位模差转为有符号增量，并累计到 int32 位置。 */
void FocusEncoder_Update(uint32_t now_ms)
{
  uint16_t current;
  uint16_t unsigned_delta;
  int32_t delta;
  int64_t next;
  uint32_t primask = lock_interrupts();
  if ((initialized == 0U) || (encoder_timer == NULL)) {
    measurement.valid = 0U;
    measurement.status = FOCUS_ENCODER_STATUS_NOT_INITIALIZED;
    measurement.timestamp_ms = now_ms;
    measurement.sequence++;
    unlock_interrupts(primask);
    return;
  }
  current = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
  measurement.elapsed_ms = now_ms - measurement.timestamp_ms;
  if (latched_fault != FOCUS_ENCODER_STATUS_OK) {
    previous_counter = current;
    measurement.raw_counter = current;
    measurement.timestamp_ms = now_ms;
    measurement.sequence++;
    measurement.valid = 0U;
    measurement.status = latched_fault;
    measurement.delta = 0;
    measurement.direction = 0;
    unlock_interrupts(primask);
    return;
  }
  unsigned_delta = (uint16_t)(current - previous_counter);
  previous_counter = current;
  measurement.raw_counter = current;
  measurement.timestamp_ms = now_ms;
  measurement.sequence++;
  measurement.status = FOCUS_ENCODER_STATUS_OK;
  measurement.delta = 0;
  measurement.direction = 0;
  /* 恰好半圈无法判断正负方向；超过半圈或多圈并非都能自动检测。 */
  if (unsigned_delta == 0x8000U) {
    measurement.valid = 0U;
    measurement.status = FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA;
    latched_fault = FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA;
    unlock_interrupts(primask);
    return;
  }
  /* 65535->0 得到 +1，0->65535 得到 -1；不能直接将 CNT 当位置。 */
  delta = (unsigned_delta < 0x8000U) ? (int32_t)unsigned_delta : (int32_t)unsigned_delta - 65536;
  delta *= (int32_t)encoder_sign;
  measurement.delta = delta;
  measurement.direction = (delta > 0) ? 1 : ((delta < 0) ? -1 : 0);
  /* 先用 64 位求和，防止有符号溢出；超限饱和并锁存无效状态。 */
  next = (int64_t)measurement.position + (int64_t)delta;
  if (next > INT32_MAX) {
    measurement.position = INT32_MAX;
    measurement.status = FOCUS_ENCODER_STATUS_POSITION_SATURATED;
    latched_fault = FOCUS_ENCODER_STATUS_POSITION_SATURATED;
  } else if (next < INT32_MIN) {
    measurement.position = INT32_MIN;
    measurement.status = FOCUS_ENCODER_STATUS_POSITION_SATURATED;
    latched_fault = FOCUS_ENCODER_STATUS_POSITION_SATURATED;
  } else {
    measurement.position = (int32_t)next;
  }
  measurement.valid = (latched_fault == FOCUS_ENCODER_STATUS_OK) ? 1U : 0U;
  unlock_interrupts(primask);
}

/* 清零软件累计值与锁存故障，重取 CNT；不停止或清零硬件计数器。 */
void FocusEncoder_Zero(void)
{
  uint32_t primask = lock_interrupts();
  measurement.position = 0;
  measurement.delta = 0;
  measurement.direction = 0;
  if (initialized != 0U) {
    previous_counter = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
    measurement.raw_counter = previous_counter;
  }
  latched_fault = FOCUS_ENCODER_STATUS_OK;
  measurement.valid = (initialized != 0U) ? 1U : 0U;
  measurement.status = (initialized != 0U) ? FOCUS_ENCODER_STATUS_OK : FOCUS_ENCODER_STATUS_NOT_INITIALIZED;
  measurement.sequence++;
  unlock_interrupts(primask);
}

/* 设置累计方向映射；重建基准，保留历史位置和锁存故障。 */
void FocusEncoder_SetSign(int8_t sign)
{
  uint32_t primask = lock_interrupts();
  if ((sign == 1) || (sign == -1)) {
    encoder_sign = sign;
    measurement.delta = 0;
    measurement.direction = 0;
    if (initialized != 0U) {
      previous_counter = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
      measurement.raw_counter = previous_counter;
    }
    measurement.sequence++;
  }
  unlock_interrupts(primask);
}

/* 临界区内复制测量结构体，保证字段来自同一次更新。 */
void FocusEncoder_GetSnapshot(FocusEncoderMeasurement *snapshot)
{
  uint32_t primask;
  if (snapshot == NULL) {
    return;
  }
  primask = lock_interrupts();
  *snapshot = measurement;
  unlock_interrupts(primask);
}
