#include "FocusUnit/Adc/focus_adc.h"

#include "stm32f0xx_hal_adc_ex.h"
#include "stm32f0xx_ll_adc.h"

#include <string.h>

#define FOCUS_ADC_RESOLUTION_COUNTS 4095UL
#define FOCUS_ADC_VREF_CAL_MIN      500U
#define FOCUS_ADC_VREF_CAL_MAX      2000U
#define FOCUS_ADC_MAX_RAW           4095U

typedef struct
{
  uint16_t raw[FOCUS_ADC_CHANNEL_COUNT];
  uint32_t timestamp_ms;
  uint32_t sequence;
  uint8_t count;
} FocusAdcRawFrame;

/* ISR 写原始帧和事件标记，主循环消费并换算；完整帧接口便于后续替换 DMA 后端。 */
static ADC_HandleTypeDef *s_hadc;
static volatile uint8_t s_busy;
static volatile uint8_t s_frame_complete;
static volatile uint8_t s_async_error; /* 0=无错误，1=HAL 错误，2=帧序/EOS 错误 */
static volatile uint8_t s_frame_count;
static volatile uint32_t s_started_at_ms;
static volatile uint32_t s_sequence;
static volatile uint32_t s_completed_scans;
static volatile uint32_t s_failed_scans;
static volatile uint32_t s_timeout_count;
static volatile uint32_t s_hal_error_count;
static volatile uint32_t s_last_hal_error_code;
static FocusAdcRawFrame s_frame;
static FocusAdcMeasurement s_measurement;
static uint8_t s_snapshot_available;
static uint8_t s_initialized;

/* 短临界区发布整帧，防止读取者看到一半新值、一半旧值。 */
static void FocusAdc_CommitMeasurement(const FocusAdcMeasurement *measurement)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  s_measurement = *measurement;
  s_snapshot_available = 1U;
  __set_PRIMASK(primask);
}

/* 12 位原始计数按 VDDA 换算为引脚 mV，加半分母进行整数四舍五入。 */
static uint32_t FocusAdc_RawToMilliVolts(uint16_t raw, uint16_t vdda_mv)
{
  return (((uint32_t)raw * (uint32_t)vdda_mv) + (FOCUS_ADC_RESOLUTION_COUNTS / 2UL)) /
         FOCUS_ADC_RESOLUTION_COUNTS;
}

/* 只接收本模块注册的 ADC，避免共享 HAL 回调误处理其他实例。 */
static uint8_t FocusAdc_IsOurHandle(ADC_HandleTypeDef *hadc)
{
  return (uint8_t)((hadc != NULL) && (hadc == s_hadc));
}

/* 失败也发布带序号/统计的结果，但 valid=0，测量值清零。 */
static void FocusAdc_PublishFailure(uint8_t error, uint8_t raw_count,
                                    uint32_t timestamp_ms, uint32_t sequence,
                                    uint32_t hal_error_code)
{
  FocusAdcMeasurement next;
  (void)memset(&next, 0, sizeof(next));
  next.timestamp_ms = timestamp_ms;
  next.sequence = sequence;
  next.completed_scans = s_completed_scans;
  next.failed_scans = s_failed_scans;
  next.timeout_count = s_timeout_count;
  next.hal_error_count = s_hal_error_count;
  next.hal_error_code = hal_error_code;
  next.ready = 1U;
  next.valid = 0U;
  next.raw_count = raw_count;
  next.error = error;

  FocusAdc_CommitMeasurement(&next);
}

/* 主循环终止失败帧，必要时停止 ADC 中断采集并更新错误统计。 */
static void FocusAdc_FinishFailure(uint8_t error, uint32_t now_ms, uint8_t stop_adc,
                                   uint32_t hal_error_code)
{
  uint32_t primask = __get_PRIMASK();
  uint8_t count;
  uint32_t timestamp_ms;
  uint32_t sequence;

  __disable_irq();
  count = s_frame_count;
  timestamp_ms = s_started_at_ms;
  sequence = s_sequence;
  s_busy = 0U;
  s_frame_complete = 0U;
  s_async_error = 0U;
  __set_PRIMASK(primask);

  if (stop_adc != 0U)
  {
    (void)HAL_ADC_Stop_IT(s_hadc);
  }

  if (error == (uint8_t)FOCUS_ADC_ERROR_TIMEOUT)
  {
    ++s_timeout_count;
  }
  else if ((error == (uint8_t)FOCUS_ADC_ERROR_HAL) ||
           (error == (uint8_t)FOCUS_ADC_ERROR_HAL_START) ||
           (error == (uint8_t)FOCUS_ADC_ERROR_FRAME_ORDER))
  {
    ++s_hal_error_count;
  }
  ++s_failed_scans;
  FocusAdc_PublishFailure(error, count, timestamp_ms, sequence, hal_error_code);
  (void)now_ms;
}

/* 主循环换算完整帧：先验证 VREF，再计算引脚电压和 MCU 温度估计。 */
static void FocusAdc_PublishComplete(const FocusAdcRawFrame *frame)
{
  FocusAdcMeasurement next;
  uint16_t vref_cal;
  uint32_t vdda_mv;
  uint32_t temp_mv;
  uint32_t temp_cal_mv;
  int32_t temp_centi_c;
  uint16_t temp_cal_raw;

  (void)memset(&next, 0, sizeof(next));
  /* 正向扫描按通道号排序：0、1、4、5、9、16、17，必须与 IOC 保持一致。 */
  next.raw_ntc1 = frame->raw[0];
  next.raw_3v3 = frame->raw[1];
  next.raw_ntc2 = frame->raw[2];
  next.raw_6v = frame->raw[3];
  next.raw_motor = frame->raw[4];
  next.raw_temperature = frame->raw[5];
  next.raw_vrefint = frame->raw[6];
  next.timestamp_ms = frame->timestamp_ms;
  next.sequence = frame->sequence;
  next.raw_count = frame->count;

  vref_cal = *VREFINT_CAL_ADDR;
  if ((frame->raw[6] == 0U) || (vref_cal < FOCUS_ADC_VREF_CAL_MIN) ||
      (vref_cal > FOCUS_ADC_VREF_CAL_MAX))
  {
    ++s_failed_scans;
    next.completed_scans = s_completed_scans;
    next.failed_scans = s_failed_scans;
    next.timeout_count = s_timeout_count;
    next.hal_error_count = s_hal_error_count;
    next.ready = 1U;
    next.error = (uint8_t)FOCUS_ADC_ERROR_BAD_VREF;
    FocusAdc_CommitMeasurement(&next);
    return;
  }

  /* 工厂在 3.3 V 下记录 VREFINT_CAL：VDDA = 校准字 × 3300 / 当前 VREFINT。 */
  vdda_mv = ((uint32_t)vref_cal * VREFINT_CAL_VREF + (frame->raw[6] / 2U)) /
            frame->raw[6];
  if ((vdda_mv < 1800U) || (vdda_mv > 5000U))
  {
    ++s_failed_scans;
    next.completed_scans = s_completed_scans;
    next.failed_scans = s_failed_scans;
    next.timeout_count = s_timeout_count;
    next.hal_error_count = s_hal_error_count;
    next.ready = 1U;
    next.error = (uint8_t)FOCUS_ADC_ERROR_BAD_VREF;
    FocusAdc_CommitMeasurement(&next);
    return;
  }

  next.vdda_mv = (uint16_t)vdda_mv;
  next.ntc1_pin_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[0], (uint16_t)vdda_mv);
  next.rail_3v3_pin_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[1], (uint16_t)vdda_mv);
  next.ntc2_pin_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[2], (uint16_t)vdda_mv);
  next.rail_6v_pin_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[3], (uint16_t)vdda_mv);
  next.motor_ipropi_pin_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[4], (uint16_t)vdda_mv);
  next.temperature_sensor_mv = (uint16_t)FocusAdc_RawToMilliVolts(frame->raw[5], (uint16_t)vdda_mv);

  /* 本型号只有 30 ℃工厂单点校准。统一到校准电压条件，使用典型 4.3 mV/℃
   * 斜率估算；没有两点温度校准。温度校准字无效不影响其他引脚电压。 */
  temp_mv = FocusAdc_RawToMilliVolts(frame->raw[5], (uint16_t)vdda_mv);
  temp_cal_raw = *TEMPSENSOR_CAL1_ADDR;
  temp_cal_mv = FocusAdc_RawToMilliVolts(temp_cal_raw,
                                          TEMPSENSOR_CAL_VREFANALOG);
  if ((temp_cal_raw != 0U) && (temp_cal_raw != 0xFFFFU) &&
      (temp_cal_raw <= FOCUS_ADC_MAX_RAW))
  {
    temp_centi_c = (int32_t)TEMPSENSOR_CAL1_TEMP * 100 +
                   (((int32_t)temp_cal_mv - (int32_t)temp_mv) * 1000) / 43;
    if (temp_centi_c > INT16_MAX)
    {
      temp_centi_c = INT16_MAX;
    }
    else if (temp_centi_c < INT16_MIN)
    {
      temp_centi_c = INT16_MIN;
    }
    next.mcu_temperature_centi_c = (int16_t)temp_centi_c;
    next.temperature_valid = 1U;
  }

  ++s_completed_scans;
  next.completed_scans = s_completed_scans;
  next.failed_scans = s_failed_scans;
  next.timeout_count = s_timeout_count;
  next.hal_error_count = s_hal_error_count;
  next.ready = 1U;
  next.valid = 1U;
  next.error = (uint8_t)FOCUS_ADC_ERROR_NONE;
  FocusAdc_CommitMeasurement(&next);
}

/* 清理状态并执行硬件自校准；外设扫描配置已由 MX_ADC_Init 完成。 */
HAL_StatusTypeDef FocusAdc_Init(ADC_HandleTypeDef *hadc)
{
  if ((hadc == NULL) || (hadc->Instance == NULL))
  {
    return HAL_ERROR;
  }

  s_hadc = hadc;
  s_busy = 0U;
  s_frame_complete = 0U;
  s_async_error = 0U;
  s_frame_count = 0U;
  s_started_at_ms = 0U;
  s_sequence = 0U;
  s_completed_scans = 0U;
  s_failed_scans = 0U;
  s_timeout_count = 0U;
  s_hal_error_count = 0U;
  s_last_hal_error_code = 0U;
  s_snapshot_available = 0U;
  s_initialized = 0U;
  (void)memset(&s_frame, 0, sizeof(s_frame));
  (void)memset(&s_measurement, 0, sizeof(s_measurement));

  if (HAL_ADCEx_Calibration_Start(hadc) != HAL_OK)
  {
    ++s_failed_scans;
    ++s_hal_error_count;
    FocusAdc_PublishFailure((uint8_t)FOCUS_ADC_ERROR_CALIBRATION, 0U, 0U, 0U,
                            hadc->ErrorCode);
    return HAL_ERROR;
  }

  s_initialized = 1U;
  return HAL_OK;
}

/* 准备帧状态后软件触发；当前帧未结束时返回 HAL_BUSY，不覆盖原始数据。 */
HAL_StatusTypeDef FocusAdc_Request(uint32_t now_ms)
{
  HAL_StatusTypeDef status;
  uint32_t primask;

  if ((s_initialized == 0U) || (s_hadc == NULL))
  {
    return HAL_ERROR;
  }
  if (s_busy != 0U)
  {
    return HAL_BUSY;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  s_busy = 1U;
  s_frame_complete = 0U;
  s_async_error = 0U;
  s_frame_count = 0U;
  s_started_at_ms = now_ms;
  ++s_sequence;
  s_frame.sequence = s_sequence;
  s_frame.timestamp_ms = now_ms;
  s_frame.count = 0U;
  s_last_hal_error_code = 0U;
  s_snapshot_available = 0U;
  __set_PRIMASK(primask);

  status = HAL_ADC_Start_IT(s_hadc);
  if (status != HAL_OK)
  {
    s_last_hal_error_code = s_hadc->ErrorCode;
    FocusAdc_FinishFailure((uint8_t)FOCUS_ADC_ERROR_HAL_START, now_ms, 0U,
                           s_last_hal_error_code);
  }
  return status;
}

/* 主循环领取 ISR 事件；优先处理错误，再处理完成或跨回绕的超时。 */
void FocusAdc_Process(uint32_t now_ms)
{
  uint32_t primask;
  uint8_t complete;
  uint8_t failure_reason = (uint8_t)FOCUS_ADC_ERROR_NONE;
  FocusAdcRawFrame completed_frame;
  uint32_t hal_error_code;

  primask = __get_PRIMASK();
  __disable_irq();
  complete = s_frame_complete;
  hal_error_code = s_last_hal_error_code;
  if (s_async_error == 2U)
  {
    failure_reason = (uint8_t)FOCUS_ADC_ERROR_FRAME_ORDER;
  }
  else if (s_async_error == 1U)
  {
    failure_reason = (uint8_t)FOCUS_ADC_ERROR_HAL;
  }
  else if ((s_busy != 0U) && (complete == 0U) &&
      ((uint32_t)(now_ms - s_started_at_ms) >= FOCUS_ADC_SCAN_TIMEOUT_MS))
  {
    failure_reason = (uint8_t)FOCUS_ADC_ERROR_TIMEOUT;
  }
  if ((complete != 0U) && (failure_reason == (uint8_t)FOCUS_ADC_ERROR_NONE))
  {
    __DMB();
    (void)memcpy(&completed_frame, &s_frame, sizeof(completed_frame));
    s_busy = 0U;
    s_frame_complete = 0U;
  }
  __set_PRIMASK(primask);

  if (failure_reason != (uint8_t)FOCUS_ADC_ERROR_NONE)
  {
    FocusAdc_FinishFailure(failure_reason, now_ms, 1U, hal_error_code);
    return;
  }

  if (complete != 0U)
  {
    FocusAdc_PublishComplete(&completed_frame);
  }
}

/* ADC ISR 逐通道读取 DR；仅第七个结果且 EOS 有效时发布完成标记。 */
void FocusAdc_OnConversionComplete(ADC_HandleTypeDef *hadc)
{
  uint8_t index;
  uint8_t eos;

  if ((FocusAdc_IsOurHandle(hadc) == 0U) || (s_busy == 0U) ||
      (s_frame_complete != 0U) || (s_async_error != 0U))
  {
    return;
  }

  /* HAL 清 EOC/EOS 前进入回调，必须立即读取 DR，防止下一通道覆盖结果。 */
  index = s_frame_count;
  if (index >= FOCUS_ADC_CHANNEL_COUNT)
  {
    s_async_error = 2U;
    return;
  }
  s_frame.raw[index] = (uint16_t)HAL_ADC_GetValue(hadc);
  ++s_frame_count;
  s_frame.count = s_frame_count;

  eos = (uint8_t)(__HAL_ADC_GET_FLAG(hadc, ADC_FLAG_EOS) != RESET);
  if (eos != 0U)
  {
    if (s_frame_count == FOCUS_ADC_CHANNEL_COUNT)
    {
      /* 原始值全部写入后再置完成标记，主循环用同样的屏障读取。 */
      __DMB();
      s_frame_complete = 1U;
    }
    else
    {
      s_async_error = 2U;
    }
  }
  else if (s_frame_count == FOCUS_ADC_CHANNEL_COUNT)
  {
    /* 收满七个结果仍无 EOS，说明扫描序列不匹配；拒绝继续拼接，避免混帧。 */
    s_async_error = 2U;
  }
}

/* ISR 只记录 HAL 错误码；停止采集和发布失败帧由主循环完成。 */
void FocusAdc_OnError(ADC_HandleTypeDef *hadc)
{
  if ((FocusAdc_IsOurHandle(hadc) != 0U) && (s_busy != 0U))
  {
    s_last_hal_error_code = hadc->ErrorCode;
    s_async_error = 1U;
  }
}

/* 复制最近发布结果；available 与 valid 不同，失败结果也可读取。 */
uint8_t FocusAdc_GetSnapshot(FocusAdcMeasurement *snapshot)
{
  uint32_t primask;
  uint8_t available;

  if (snapshot == NULL)
  {
    return 0U;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  available = s_snapshot_available;
  if (available != 0U)
  {
    (void)memcpy(snapshot, &s_measurement, sizeof(*snapshot));
    snapshot->busy = s_busy;
  }
  __set_PRIMASK(primask);
  return available;
}
