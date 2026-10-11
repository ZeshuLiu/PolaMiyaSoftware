#include "FocusUnit/Encoder/focus_encoder.h"

static TIM_HandleTypeDef *encoder_timer;
static FocusEncoderMeasurement measurement;
static uint16_t previous_counter;
static int8_t encoder_sign = 1;

/* 启动 TIM3 硬件四倍频计数；各业务接口均由主循环调用。 */
HAL_StatusTypeDef FocusEncoder_Init(TIM_HandleTypeDef *htim, uint32_t now_ms)
{
  HAL_StatusTypeDef result = HAL_TIM_Encoder_Start(htim, TIM_CHANNEL_ALL);
  encoder_timer = htim;
  encoder_sign = 1;
  measurement = (FocusEncoderMeasurement){0};
  previous_counter = (uint16_t)__HAL_TIM_GET_COUNTER(htim);
  measurement.raw_counter = previous_counter;
  measurement.timestamp_ms = now_ms;
  return result;
}

/* 每 1 ms 读取 16 位 CNT，将双向回绕后的增量累计为 32 位位置。 */
void FocusEncoder_Update(uint32_t now_ms)
{
  uint16_t current = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
  uint16_t difference = (uint16_t)(current - previous_counter);
  int32_t delta = (difference < 0x8000U) ? (int32_t)difference : (int32_t)difference - 65536;
  delta *= encoder_sign;
  previous_counter = current;
  measurement.raw_counter = current;
  measurement.elapsed_ms = now_ms - measurement.timestamp_ms;
  measurement.timestamp_ms = now_ms;
  measurement.sequence++;
  measurement.delta = delta;
  measurement.direction = (delta > 0) ? 1 : ((delta < 0) ? -1 : 0);
  /* 用无符号加法实现 32 位回绕，避免有符号加法溢出；ARM GCC 转回有符号值。 */
  measurement.position = (int32_t)((uint32_t)measurement.position + (uint32_t)delta);
}

/* 清软件位置并重取 CNT 基准，硬件计数器继续工作。 */
void FocusEncoder_Zero(void)
{
  measurement.position = 0;
  measurement.delta = 0;
  measurement.direction = 0;
  previous_counter = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
  measurement.raw_counter = previous_counter;
  measurement.sequence++;
}

/* 调用者传入 +1 或 -1；重建基准，保留已累计位置。 */
void FocusEncoder_SetSign(int8_t sign)
{
  encoder_sign = sign;
  measurement.delta = 0;
  measurement.direction = 0;
  previous_counter = (uint16_t)__HAL_TIM_GET_COUNTER(encoder_timer);
  measurement.raw_counter = previous_counter;
  measurement.sequence++;
}

/* 主循环复制最近一次更新结果，不屏蔽中断。 */
void FocusEncoder_GetSnapshot(FocusEncoderMeasurement *snapshot)
{
  *snapshot = measurement;
}
