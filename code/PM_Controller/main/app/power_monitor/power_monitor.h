#ifndef PM_POWER_MONITOR_H
#define PM_POWER_MONITOR_H

#include "adc_measure.h"

typedef struct {
    adc_measurement_t adc;
    uint32_t battery_mv;       /* 2:1 divider; smoothed calibrated voltage. */
    uint32_t motor_current_ma; /* IPROPI estimate from the current window. */
    int battery_percent;      /* Voltage-based estimate, -1 when unknown. */
    bool battery_valid;
    bool motor_valid;
} power_sample_t;

/* Stateless snapshot conversion; initialization belongs to adc_measure_init(). */
void power_monitor_get(power_sample_t *sample);

#endif
