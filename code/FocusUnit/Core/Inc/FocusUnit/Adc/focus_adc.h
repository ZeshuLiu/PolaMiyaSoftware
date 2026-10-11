#ifndef FOCUS_ADC_H
#define FOCUS_ADC_H

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 正向扫描 IN0/1/4/5/9/16/17；每 100 ms 请求一帧。 */
#define FOCUS_ADC_CHANNEL_COUNT       7U

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
  uint32_t sequence;       /* 受理扫描请求后递增 */

  uint8_t ready;           /* 已完成七通道采集并发布结果 */
  uint8_t busy;            /* 读取快照时是否正在扫描 */
  uint8_t valid;           /* VREFINT 非零，可执行电压和温度换算 */
} FocusAdcMeasurement;

/* 在 MX_ADC_Init 后由主循环上下文调用：清状态并执行 ADC 硬件自校准。
 * 时钟、通道、采样时间和触发模式来自 IOC/CubeMX；失败返回 HAL_ERROR。 */
HAL_StatusTypeDef FocusAdc_Init(ADC_HandleTypeDef *hadc);

/* 主循环请求一次七通道扫描；忙时返回 HAL_BUSY，不等待也不重启当前帧。 */
HAL_StatusTypeDef FocusAdc_Request(uint32_t now_ms);

/* 主循环处理完成帧或 HAL 错误；电压/温度换算在此执行，无扫描超时检查。 */
void FocusAdc_Process(uint32_t now_ms);

/* 由共享 HAL 回调转发，在 ADC ISR 中调用；只收原始值或标记错误。 */
void FocusAdc_OnConversionComplete(ADC_HandleTypeDef *hadc);
void FocusAdc_OnError(ADC_HandleTypeDef *hadc);

/* Init/Request/Process/GetSnapshot 均在主循环调用；调用者提供有效指针。
 * 不屏蔽中断。返回 1 表示已发布完整帧，valid 表示可换算。
 * 新请求清除可用标记，直到下一次完整帧发布。 */
uint8_t FocusAdc_GetSnapshot(FocusAdcMeasurement *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* FOCUS_ADC_H */
