#ifndef PM_ADC_MEASURE_H
#define PM_ADC_MEASURE_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t raw;          /* Window mean, 31 samples/channel on ESP32-S3; 12-bit. */
    uint16_t voltage_mv;   /* Calibrated pin voltage for this window. */
    uint16_t filtered_mv;  /* 1/8 IIR, seeded from the first calibrated window. */
    bool valid;           /* Fresh raw window; false on a stream error. */
    bool calibrated;      /* Voltage fields are usable only when true. */
    bool saturated;       /* At least one raw value reached 4095. */
} adc_channel_sample_t;

typedef struct {
    adc_channel_sample_t battery;
    adc_channel_sample_t motor;
    int64_t timestamp_us;  /* Task publication time, not a hardware sample timestamp. */
    uint32_t sequence;
    uint32_t overflows;
    esp_err_t error;
} adc_measurement_t;

/* Startup once. Owns ADC1 continuous mode and one GDMA RX channel. */
esp_err_t adc_measure_init(void);
/* Task context, consistent snapshot. >300 ms without a window invalidates it. */
esp_err_t adc_measure_get(adc_measurement_t *sample);

#endif
