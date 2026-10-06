#include "FocusUnit/Encoder/focus_encoder.h"
#include <limits.h>

static TIM_HandleTypeDef *encoder_timer;
static FocusEncoderMeasurement measurement;
static uint16_t previous_counter;
static int8_t encoder_sign = 1;
static uint8_t initialized;
static uint32_t latched_fault;

static uint32_t lock_interrupts(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void unlock_interrupts(uint32_t primask)
{
  __set_PRIMASK(primask);
}

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
  if (unsigned_delta == 0x8000U) {
    measurement.valid = 0U;
    measurement.status = FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA;
    latched_fault = FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA;
    unlock_interrupts(primask);
    return;
  }
  delta = (unsigned_delta < 0x8000U) ? (int32_t)unsigned_delta : (int32_t)unsigned_delta - 65536;
  delta *= (int32_t)encoder_sign;
  measurement.delta = delta;
  measurement.direction = (delta > 0) ? 1 : ((delta < 0) ? -1 : 0);
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
