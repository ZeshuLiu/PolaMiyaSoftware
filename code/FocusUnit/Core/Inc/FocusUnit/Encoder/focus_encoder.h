#ifndef FOCUS_ENCODER_H
#define FOCUS_ENCODER_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

typedef enum {
  FOCUS_ENCODER_STATUS_OK = 0U,
  FOCUS_ENCODER_STATUS_NOT_INITIALIZED = 1U << 0,
  FOCUS_ENCODER_STATUS_HAL_ERROR = 1U << 1,
  FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA = 1U << 2,
  FOCUS_ENCODER_STATUS_POSITION_SATURATED = 1U << 3
} FocusEncoderStatus;

typedef struct {
  int32_t position;
  int32_t delta;
  uint16_t raw_counter;
  int8_t direction;
  uint32_t timestamp_ms;
  uint32_t elapsed_ms;
  uint32_t sequence;
  uint8_t valid;
  uint32_t status;
} FocusEncoderMeasurement;

/* Call Init once after TIM3 initialization. Call Update from the 1 ms main-loop
 * task (never from an encoder-edge interrupt). The CNT modular difference is
 * unambiguous only when actual travel between samples is strictly below
 * 32768 quadrature counts. At exactly 32768 the update is marked invalid;
 * multiple turns between samples cannot be inferred from CNT alone. Timestamp
 * subtraction by uint32_t remains valid across HAL tick wrap; elapsed_ms makes
 * task delay visible. With TIM3 IC1/2Filter=4, CKD=DIV1 and 48 MHz timer clock,
 * the F0 filter samples at 24 MHz and requires 6 consecutive samples (about
 * 250 ns); input pulses around the qualification boundary are phase-dependent.
 */
HAL_StatusTypeDef FocusEncoder_Init(TIM_HandleTypeDef *htim, uint32_t now_ms);
void FocusEncoder_Update(uint32_t now_ms);
void FocusEncoder_Zero(void);
void FocusEncoder_SetSign(int8_t sign);
void FocusEncoder_GetSnapshot(FocusEncoderMeasurement *snapshot);

#endif
