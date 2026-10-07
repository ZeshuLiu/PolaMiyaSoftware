#ifndef FOCUS_ADC_H
#define FOCUS_ADC_H

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 正向扫描 IN0/1/4/5/9/16/17；每 100 ms 请求一帧，超时门限为 10 ms。 */
#define FOCUS_ADC_CHANNEL_COUNT       7U
#define FOCUS_ADC_SCAN_TIMEOUT_MS    10U

typedef enum
{
  FOCUS_ADC_ERROR_NONE = 0U,
  FOCUS_ADC_ERROR_HAL_START,
  FOCUS_ADC_ERROR_HAL,
  FOCUS_ADC_ERROR_TIMEOUT,
  FOCUS_ADC_ERROR_FRAME_ORDER,
  FOCUS_ADC_ERROR_CALIBRATION,
  FOCUS_ADC_ERROR_BAD_VREF
} FocusAdcError;

/* 同一轮扫描的完整结果。原始值为 12 位计数，引脚电压单位为 mV。
 * 外部 NTC、分压和 IPROPI 参数未核实，暂不换算实际温度、电源轨电压或电机电流。 */
typedef struct
{
  uint16_t raw_ntc1;       /* NTC1 原始计数：ADC_IN0 / PA0 */
  uint16_t raw_3v3;        /* 3V3 采样原始计数：ADC_IN1 / PA1 */
  uint16_t raw_ntc2;       /* NTC2 原始计数：ADC_IN4 / PA4 */
  uint16_t raw_6v;         /* 6V 采样原始计数：ADC_IN5 / PA5 */
  uint16_t raw_motor;      /* 电机电流采样原始计数：ADC_IN9 / PB1，DRV8251A IPROPI */
  uint16_t raw_temperature;/* MCU 内部温度传感器原始计数：ADC_IN16 */
  uint16_t raw_vrefint;    /* 内部参考电压原始计数：ADC_IN17 */

  uint16_t ntc1_pin_mv;          /* NTC1 ADC 引脚电压，mV */
  uint16_t rail_3v3_pin_mv;      /* 3V3 分压后引脚电压，mV */
  uint16_t ntc2_pin_mv;          /* NTC2 ADC 引脚电压，mV */
  uint16_t rail_6v_pin_mv;       /* 6V 分压后引脚电压，mV */
  uint16_t motor_ipropi_pin_mv;  /* IPROPI 引脚电压，mV */
  uint16_t temperature_sensor_mv; /* 内部温度传感器电压，mV */
  uint16_t vdda_mv;             /* 由 VREFINT 工厂校准字估算的 VDDA，mV */
  int16_t  mcu_temperature_centi_c; /* 单位 0.01 ℃；30 ℃单点校准和典型斜率估算 */

  uint32_t timestamp_ms;   /* 本帧请求时刻，调度时基 ms */
  uint32_t sequence;       /* 受理扫描请求后递增，失败帧也占用序号 */
  uint32_t completed_scans; /* 已完成且 VREF 校验有效的帧数 */
  uint32_t failed_scans;    /* 失败帧总数 */
  uint32_t timeout_count;  /* 扫描超时次数 */
  uint32_t hal_error_count; /* HAL、启动、帧序及初始化校准错误统计 */
  uint32_t hal_error_code; /* 失败时的 HAL ErrorCode */

  uint8_t ready;           /* 已发布结果；成功与失败均可置 1 */
  uint8_t busy;            /* 读取快照时是否正在扫描 */
  uint8_t valid;           /* 完整七通道且参考电压有效时为 1 */
  uint8_t temperature_valid;/* 温度校准字有效标记，不代表温度精度保证 */
  uint8_t raw_count;       /* 本帧已收集的通道数，正常为 7 */
  uint8_t error;           /* FocusAdcError 错误枚举 */
  uint8_t reserved[2];     /* 保留字节 */
} FocusAdcMeasurement;

/* 在 MX_ADC_Init 后由主循环上下文调用：清状态并执行 ADC 硬件自校准。
 * 时钟、通道、采样时间和触发模式来自 IOC/CubeMX；失败返回 HAL_ERROR。 */
HAL_StatusTypeDef FocusAdc_Init(ADC_HandleTypeDef *hadc);

/* 主循环请求一次七通道扫描；忙时返回 HAL_BUSY，不等待也不重启当前帧。 */
HAL_StatusTypeDef FocusAdc_Request(uint32_t now_ms);

/* 主循环处理完成、错误和超时；整数电压/温度换算在此执行，不放在 ISR 中。 */
void FocusAdc_Process(uint32_t now_ms);

/* 由共享 HAL 回调转发，在 ADC ISR 中调用；只收原始值或标记错误。 */
void FocusAdc_OnConversionComplete(ADC_HandleTypeDef *hadc);
void FocusAdc_OnError(ADC_HandleTypeDef *hadc);

/* 短临界区内读取一致快照；返回 1 表示有结果，结果是否有效另看 valid。
 * 可重复读取；新请求会清除可用标记，直到再次发布成功或失败结果。 */
uint8_t FocusAdc_GetSnapshot(FocusAdcMeasurement *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* FOCUS_ADC_H */
