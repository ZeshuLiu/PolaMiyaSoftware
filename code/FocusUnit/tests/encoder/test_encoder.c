#include "FocusUnit/Encoder/focus_encoder.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

uint32_t test_primask;
static int start_result = HAL_OK;
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{ (void)htim; assert(channel == TIM_CHANNEL_ALL); return (HAL_StatusTypeDef)start_result; }

static FocusEncoderMeasurement snap(void)
{ FocusEncoderMeasurement m; FocusEncoder_GetSnapshot(&m); return m; }

int main(void)
{
  TIM_TypeDef timer = {0};
  TIM_HandleTypeDef handle = {&timer};
  FocusEncoderMeasurement m;
  assert(FocusEncoder_Init(&handle, UINT32_MAX - 1U) == HAL_OK);
  timer.CNT = 10U; FocusEncoder_Update(UINT32_MAX);
  m = snap(); assert(m.valid && m.position == 10 && m.delta == 10);
  timer.CNT = 5U; FocusEncoder_Update(0U);
  m = snap(); assert(m.valid && m.position == 5 && m.delta == -5 && m.timestamp_ms == 0U);
  assert(m.elapsed_ms == 1U);

  timer.CNT = 65530U; FocusEncoder_Zero();
  timer.CNT = 2U; FocusEncoder_Update(1U);
  m = snap(); assert(m.valid && m.position == 8 && m.delta == 8);
  FocusEncoder_SetSign(-1);
  timer.CNT = 65534U; FocusEncoder_Update(2U);
  m = snap(); assert(m.valid && m.position == 12 && m.delta == 4);
  FocusEncoder_SetSign(1);
  timer.CNT = 32766U; FocusEncoder_Update(3U);
  m = snap(); assert(!m.valid && m.status == FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA);
  timer.CNT = 32767U; FocusEncoder_Update(4U);
  m = snap(); assert(!m.valid && m.status == FOCUS_ENCODER_STATUS_AMBIGUOUS_DELTA);
  FocusEncoder_Zero(); timer.CNT = 32770U; FocusEncoder_Update(5U);
  m = snap(); assert(m.valid && m.position == 3);

  FocusEncoder_Zero();
  { int32_t i; for (i = 0; i < 65539; ++i) { timer.CNT += 32767U; FocusEncoder_Update((uint32_t)i); } }
  m = snap(); assert(m.position == INT32_MAX && !m.valid && m.status == FOCUS_ENCODER_STATUS_POSITION_SATURATED);
  FocusEncoder_Zero(); m = snap(); assert(m.valid && m.position == 0);
  timer.CNT = 0U;
  { int32_t i; for (i = 0; i < 65539; ++i) { timer.CNT -= 32767U; FocusEncoder_Update((uint32_t)i); } }
  m = snap(); assert(m.position == INT32_MIN && !m.valid && m.status == FOCUS_ENCODER_STATUS_POSITION_SATURATED);
  assert(test_primask == 0U);
  puts("encoder logic: PASS");
  return 0;
}
