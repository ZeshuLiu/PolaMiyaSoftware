#include "power_monitor.h"

#include <string.h>

#define BATTERY_DIVIDER_NUMERATOR 2U /* R39=R40=100 kOhm. */
#define MOTOR_IPROPI_OHMS 1000U     /* R2; R12=0 Ohm adds no voltage gain. */
#define MOTOR_IPROPI_UA_PER_A 1575U /* DRV8251A typical mirror ratio. */

static int estimate_battery_percent(uint32_t mv)
{
    /* Generic single-cell 4.2 V Li-ion display estimate, not a fuel gauge or
     * discharge cutoff. Replace with measured cell/load data when available. */
    static const struct { uint16_t mv; uint8_t percent; } curve[] = {
        {3000, 0}, {3300, 5}, {3500, 10}, {3600, 20}, {3700, 40},
        {3800, 60}, {3900, 75}, {4000, 85}, {4100, 95}, {4200, 100},
    };
    if (mv <= curve[0].mv) return 0;
    for (unsigned i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
        if (mv <= curve[i].mv) {
            return curve[i - 1].percent +
                (mv - curve[i - 1].mv) * (curve[i].percent - curve[i - 1].percent) /
                (curve[i].mv - curve[i - 1].mv);
        }
    }
    return 100;
}

void power_monitor_get(power_sample_t *sample)
{
    memset(sample, 0, sizeof(*sample));
    sample->battery_percent = -1;
    const esp_err_t error = adc_measure_get(&sample->adc);
    if (error != ESP_OK) {
        sample->adc.error = error;
        return;
    }
    const adc_channel_sample_t *battery = &sample->adc.battery;
    const adc_channel_sample_t *motor = &sample->adc.motor;
    sample->battery_valid = battery->valid && battery->calibrated && !battery->saturated;
    sample->motor_valid = motor->valid && motor->calibrated && !motor->saturated;
    if (sample->battery_valid) {
        sample->battery_mv = battery->filtered_mv * BATTERY_DIVIDER_NUMERATOR;
        sample->battery_percent = estimate_battery_percent(sample->battery_mv);
    }
    if (sample->motor_valid) {
        /* mA = pin_mV * 1,000,000 / (R_ohm * mirror_uA_per_A).
         * This is filtered IPROPI feedback, not a peak-current protection path. */
        const uint32_t denominator = MOTOR_IPROPI_OHMS * MOTOR_IPROPI_UA_PER_A;
        sample->motor_current_ma = ((uint64_t)motor->voltage_mv * 1000000U + denominator / 2) /
                                   denominator;
    }
}
