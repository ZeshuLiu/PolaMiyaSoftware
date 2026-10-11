#include "FocusUnit/Adc/focus_adc.h"

#include "stm32f0xx_hal_adc_ex.h"
#include "stm32f0xx_ll_adc.h"

#define FOCUS_ADC_RESOLUTION_COUNTS 4095UL

static ADC_HandleTypeDef *s_hadc;
static volatile uint16_t s_raw[FOCUS_ADC_CHANNEL_COUNT];
static volatile uint8_t s_busy;
static volatile uint8_t s_frame_complete;
static volatile uint8_t s_error;
static uint8_t s_frame_count; /* 仅 ADC ISR 修改；主循环在启动下一帧前归零 */
static uint32_t s_started_at_ms;
static uint32_t s_sequence;
static FocusAdcMeasurement s_measurement;
static uint8_t s_snapshot_available;

/* 12 位计数按 VDDA 换算为引脚 mV，整数四舍五入。 */
static uint16_t FocusAdc_RawToMilliVolts(uint16_t raw, uint16_t vdda_mv)
{
  return (uint16_t)(((uint32_t)raw * vdda_mv + FOCUS_ADC_RESOLUTION_COUNTS / 2U) /
                    FOCUS_ADC_RESOLUTION_COUNTS);
}

/* 校准 ADC；外设通道和扫描顺序来自 IOC/CubeMX。 */
HAL_StatusTypeDef FocusAdc_Init(ADC_HandleTypeDef *hadc)
{
  s_hadc = hadc;
  s_busy = 0U;
  s_frame_complete = 0U;
  s_error = 0U;
  s_frame_count = 0U;
  s_started_at_ms = 0U;
  s_sequence = 0U;
  s_measurement = (FocusAdcMeasurement){0};
  s_snapshot_available = 0U;
  return HAL_ADCEx_Calibration_Start(hadc);
}

/* 上一帧处理结束后启动下一帧，不在扫描中重置缓冲区。 */
HAL_StatusTypeDef FocusAdc_Request(uint32_t now_ms)
{
  HAL_StatusTypeDef status;
  if ((s_busy != 0U) || (s_frame_complete != 0U) || (s_error != 0U))
  {
    return HAL_BUSY;
  }
  s_frame_count = 0U;
  s_started_at_ms = now_ms;
  ++s_sequence;
  s_snapshot_available = 0U;
  s_busy = 1U;
  status = HAL_ADC_Start_IT(s_hadc);
  if (status != HAL_OK)
  {
    s_error = 1U;
  }
  return status;
}

/* ISR 完成后不再写缓冲区；主循环直接换算整帧，无需屏蔽中断。 */
void FocusAdc_Process(uint32_t now_ms)
{
  FocusAdcMeasurement next = {0};
  (void)now_ms;
  if (s_error != 0U)
  {
    (void)HAL_ADC_Stop_IT(s_hadc);
    s_busy = 0U;
    s_frame_complete = 0U;
    s_error = 0U;
    return;
  }
  if (s_frame_complete == 0U)
  {
    return;
  }
  __DMB();
  /* 固定正向扫描：IN0、IN1、IN4、IN5、IN9、温度、VREFINT。 */
  next.raw_ntc1 = s_raw[0];
  next.raw_3v3 = s_raw[1];
  next.raw_ntc2 = s_raw[2];
  next.raw_6v = s_raw[3];
  next.raw_motor = s_raw[4];
  next.raw_temperature = s_raw[5];
  next.raw_vrefint = s_raw[6];
  next.timestamp_ms = s_started_at_ms;
  next.sequence = s_sequence;
  next.ready = 1U;

  /* 只保留除零处理，不再筛选校准字或 VDDA 的经验范围。 */
  if (next.raw_vrefint != 0U)
  {
    next.vdda_mv = (uint16_t)(((uint32_t)*VREFINT_CAL_ADDR * VREFINT_CAL_VREF +
                              next.raw_vrefint / 2U) / next.raw_vrefint);
    next.ntc1_pin_mv = FocusAdc_RawToMilliVolts(next.raw_ntc1, next.vdda_mv);
    next.rail_3v3_pin_mv = FocusAdc_RawToMilliVolts(next.raw_3v3, next.vdda_mv);
    next.ntc2_pin_mv = FocusAdc_RawToMilliVolts(next.raw_ntc2, next.vdda_mv);
    next.rail_6v_pin_mv = FocusAdc_RawToMilliVolts(next.raw_6v, next.vdda_mv);
    next.motor_ipropi_pin_mv = FocusAdc_RawToMilliVolts(next.raw_motor, next.vdda_mv);
    next.temperature_sensor_mv = FocusAdc_RawToMilliVolts(next.raw_temperature, next.vdda_mv);
    /* 本型号使用 30 ℃单点工厂校准和典型 4.3 mV/℃斜率，不做温度限幅。 */
    uint16_t temp_cal_mv = FocusAdc_RawToMilliVolts(*TEMPSENSOR_CAL1_ADDR,
                                                 TEMPSENSOR_CAL_VREFANALOG);
    next.mcu_temperature_centi_c = (int16_t)((int32_t)TEMPSENSOR_CAL1_TEMP * 100 +
      ((int32_t)temp_cal_mv - next.temperature_sensor_mv) * 1000 / 43);
    next.valid = 1U;
  }
  s_measurement = next;
  s_snapshot_available = 1U;
  s_frame_complete = 0U;
}

/* 每次 EOC 立即读 DR；收满固定七通道后将缓冲区交给主循环。 */
void FocusAdc_OnConversionComplete(ADC_HandleTypeDef *hadc)
{
  if (s_busy == 0U)
  {
    return;
  }
  s_raw[s_frame_count++] = (uint16_t)HAL_ADC_GetValue(hadc);
  if (s_frame_count == FOCUS_ADC_CHANNEL_COUNT)
  {
    s_busy = 0U;
    __DMB();
    s_frame_complete = 1U;
  }
}

/* HAL 硬件错误由主循环终止采集，不另建错误帧或统计。 */
void FocusAdc_OnError(ADC_HandleTypeDef *hadc)
{
  (void)hadc;
  s_error = 1U;
}

/* 发布与读取均在主循环中执行，直接复制最近结果。 */
uint8_t FocusAdc_GetSnapshot(FocusAdcMeasurement *snapshot)
{
  if (s_snapshot_available == 0U)
  {
    return 0U;
  }
  *snapshot = s_measurement;
  snapshot->busy = s_busy;
  return 1U;
}
