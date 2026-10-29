#ifndef PM_CHARGE_STATUS_H
#define PM_CHARGE_STATUS_H

#include "hc165.h"
#include "power_monitor.h"

typedef enum {
    BATTERY_NORMAL,
    BATTERY_CHARGING,
    BATTERY_FULL,
    BATTERY_ABSENT,
    BATTERY_PENDING,
} battery_status_t;

/* UI task only, including when another page is visible. No task/timer owned here. */
battery_status_t charge_status_update(uint32_t now_ms, const hc165_inputs_t *inputs,
                                       const power_sample_t *power);

#endif
