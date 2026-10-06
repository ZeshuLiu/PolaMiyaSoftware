#include <assert.h>
#include <stdint.h>

#include "FocusUnit/Adc/focus_adc.h"

uint16_t test_vref_cal = 1500U;
uint16_t test_temp_cal = 1775U;
static ADC_TypeDef adc_regs;
static ADC_HandleTypeDef hadc = { &adc_regs, 0U };
static uint32_t primask;
static HAL_StatusTypeDef start_status = HAL_OK;
static unsigned stop_count;

uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1U; }
void __set_PRIMASK(uint32_t value) { primask = value; }
void __DMB(void) { }
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *handle) { return handle->Instance->DR; }
HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *handle)
{
  (void)handle;
  return start_status;
}
HAL_StatusTypeDef HAL_ADC_Stop_IT(ADC_HandleTypeDef *handle)
{
  (void)handle;
  ++stop_count;
  return HAL_OK;
}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *handle)
{
  (void)handle;
  return HAL_OK;
}

static void send_rank(uint16_t raw, uint8_t eos)
{
  adc_regs.DR = raw;
  adc_regs.ISR = eos ? ADC_FLAG_EOS : 0U;
  FocusAdc_OnConversionComplete(&hadc);
}

static void test_complete_scan_and_scaling(void)
{
  FocusAdcMeasurement sample;
  const uint16_t values[FOCUS_ADC_CHANNEL_COUNT] =
    { 100U, 2000U, 3000U, 4000U, 500U, 1000U, 1500U };
  unsigned i;

  assert(FocusAdc_Init(&hadc) == HAL_OK);
  assert(FocusAdc_GetSnapshot(&sample) == 0U);
  assert(FocusAdc_Request(100U) == HAL_OK);
  for (i = 0U; i < FOCUS_ADC_CHANNEL_COUNT; ++i)
  {
    send_rank(values[i], (uint8_t)(i == FOCUS_ADC_CHANNEL_COUNT - 1U));
  }
  FocusAdc_Process(101U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 1U && sample.ready == 1U && sample.busy == 0U);
  assert(sample.error == FOCUS_ADC_ERROR_NONE);
  assert(sample.raw_ntc1 == 100U && sample.raw_vrefint == 1500U);
  assert(sample.vdda_mv == 3300U);
  assert(sample.ntc1_pin_mv == 81U);
  assert(sample.rail_3v3_pin_mv == 1612U);
  assert(sample.temperature_valid == 1U);
  assert(sample.mcu_temperature_centi_c > 17000 && sample.mcu_temperature_centi_c < 18000);
  assert(sample.sequence == 1U && sample.completed_scans == 1U);
}

static void test_early_eos_rejects_partial_frame(void)
{
  FocusAdcMeasurement sample;

  assert(FocusAdc_Request(200U) == HAL_OK);
  send_rank(1234U, 1U);
  FocusAdc_Process(201U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 0U && sample.error == FOCUS_ADC_ERROR_FRAME_ORDER);
  assert(sample.raw_count == 1U && sample.failed_scans == 1U);
  assert(stop_count == 1U);
}

static void test_timeout_across_tick_wrap(void)
{
  FocusAdcMeasurement sample;

  assert(FocusAdc_Request(UINT32_MAX - 5U) == HAL_OK);
  FocusAdc_Process(5U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 0U && sample.error == FOCUS_ADC_ERROR_TIMEOUT);
  assert(sample.timeout_count == 1U && sample.failed_scans == 2U);
  assert(stop_count == 2U);
}

static void test_start_error_hal_overrun_and_recovery(void)
{
  FocusAdcMeasurement sample;
  const uint16_t values[FOCUS_ADC_CHANNEL_COUNT] =
    { 101U, 2001U, 3001U, 4001U, 501U, 1001U, 1500U };
  unsigned i;

  start_status = HAL_ERROR;
  assert(FocusAdc_Request(300U) == HAL_ERROR);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.error == FOCUS_ADC_ERROR_HAL_START);
  assert(sample.hal_error_count == 2U);

  start_status = HAL_OK;
  hadc.ErrorCode = 0x00000002UL; /* HAL_ADC_ERROR_OVR */
  assert(FocusAdc_Request(301U) == HAL_OK);
  FocusAdc_OnError(&hadc);
  FocusAdc_Process(302U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.error == FOCUS_ADC_ERROR_HAL && sample.valid == 0U);
  assert(sample.hal_error_code == 0x00000002UL);
  assert(sample.hal_error_count == 3U && stop_count == 3U);

  hadc.ErrorCode = 0U;
  assert(FocusAdc_Request(303U) == HAL_OK);
  for (i = 0U; i < FOCUS_ADC_CHANNEL_COUNT; ++i)
  {
    send_rank(values[i], (uint8_t)(i == FOCUS_ADC_CHANNEL_COUNT - 1U));
  }
  FocusAdc_Process(304U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 1U && sample.error == FOCUS_ADC_ERROR_NONE);
  assert(sample.sequence == 6U && sample.completed_scans == 2U);
}

static void test_zero_vref_and_missing_temperature_calibration(void)
{
  FocusAdcMeasurement sample;
  const uint16_t values[FOCUS_ADC_CHANNEL_COUNT] =
    { 100U, 2000U, 3000U, 4000U, 500U, 1000U, 0U };
  const uint16_t valid_values[FOCUS_ADC_CHANNEL_COUNT] =
    { 100U, 2000U, 3000U, 4000U, 500U, 1000U, 1500U };
  unsigned i;

  assert(FocusAdc_Request(400U) == HAL_OK);
  for (i = 0U; i < FOCUS_ADC_CHANNEL_COUNT; ++i)
  {
    send_rank(values[i], (uint8_t)(i == FOCUS_ADC_CHANNEL_COUNT - 1U));
  }
  FocusAdc_Process(401U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 0U && sample.error == FOCUS_ADC_ERROR_BAD_VREF);
  assert(sample.raw_vrefint == 0U && sample.failed_scans == 5U);

  test_temp_cal = 0xFFFFU;
  assert(FocusAdc_Request(402U) == HAL_OK);
  for (i = 0U; i < FOCUS_ADC_CHANNEL_COUNT; ++i)
  {
    send_rank(valid_values[i], (uint8_t)(i == FOCUS_ADC_CHANNEL_COUNT - 1U));
  }
  FocusAdc_Process(403U);
  assert(FocusAdc_GetSnapshot(&sample) == 1U);
  assert(sample.valid == 1U && sample.temperature_valid == 0U);
  assert(sample.mcu_temperature_centi_c == 0);
  test_temp_cal = 1775U;
}

int main(void)
{
  test_complete_scan_and_scaling();
  test_early_eos_rejects_partial_frame();
  test_timeout_across_tick_wrap();
  test_start_error_hal_overrun_and_recovery();
  test_zero_vref_and_missing_temperature_calibration();
  return 0;
}
