#ifndef FOCUS_ADC_H
#define FOCUS_ADC_H

#include "stm32f0xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* One coherent scan. External sensor divider/current transfer functions are
 * deliberately not applied until their component values are confirmed. */
typedef struct
{
  uint16_t raw_ntc1;       /* ADC_IN0 / PA0 */
  uint16_t raw_3v3;        /* ADC_IN1 / PA1 */
  uint16_t raw_ntc2;       /* ADC_IN4 / PA4 */
  uint16_t raw_6v;         /* ADC_IN5 / PA5 */
  uint16_t raw_motor;      /* ADC_IN9 / PB1, DRV8251A IPROPI */
  uint16_t raw_temperature;/* ADC_IN16 */
  uint16_t raw_vrefint;    /* ADC_IN17 */

  uint16_t ntc1_pin_mv;
  uint16_t rail_3v3_pin_mv;
  uint16_t ntc2_pin_mv;
  uint16_t rail_6v_pin_mv;
  uint16_t motor_ipropi_pin_mv;
  uint16_t temperature_sensor_mv;
  uint16_t vdda_mv;
  int16_t  mcu_temperature_centi_c; /* calibrated at 30 C; typical slope */

  uint32_t timestamp_ms;   /* request time for this scan */
  uint32_t sequence;       /* incremented when a scan is requested */
  uint32_t completed_scans;
  uint32_t failed_scans;
  uint32_t timeout_count;
  uint32_t hal_error_count;
  uint32_t hal_error_code;

  uint8_t ready;           /* set after a completed or failed attempt */
  uint8_t busy;
  uint8_t valid;           /* true only for a complete scan with valid VREF */
  uint8_t temperature_valid;/* false if the device calibration word is invalid */
  uint8_t raw_count;
  uint8_t error;
  uint8_t reserved[2];
} FocusAdcMeasurement;

/* Initializes module state and performs the STM32F0 ADC self-calibration.
 * ADC clocking, channel sequence, sampling time and trigger mode are supplied
 * by CubeMX-generated ADC configuration. */
HAL_StatusTypeDef FocusAdc_Init(ADC_HandleTypeDef *hadc);

/* Start one seven-channel software-triggered scan. Call from the main loop. */
HAL_StatusTypeDef FocusAdc_Request(uint32_t now_ms);

/* Consume completion/error events and enforce timeout from the main loop. */
void FocusAdc_Process(uint32_t now_ms);

/* Forward these from the shared HAL callbacks. Runs in ADC interrupt context. */
void FocusAdc_OnConversionComplete(ADC_HandleTypeDef *hadc);
void FocusAdc_OnError(ADC_HandleTypeDef *hadc);

/* Coherent copy; returns 1 when a measurement snapshot is available. */
uint8_t FocusAdc_GetSnapshot(FocusAdcMeasurement *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* FOCUS_ADC_H */
