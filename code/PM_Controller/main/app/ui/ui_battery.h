#ifndef PM_UI_BATTERY_H
#define PM_UI_BATTERY_H

#include "lvgl.h"
#include "charge_status.h"

typedef struct {
    lv_obj_t *frame;
    lv_obj_t *fill;
    lv_obj_t *bolt;
    lv_obj_t *check;
    lv_obj_t *slash;
    lv_obj_t *unknown;
    int state;
    int width;
    uint32_t animation_started_ms;
} ui_battery_t;

/* Black-header widget at the approved (131,11), 19x10 battery outline.
 * Caller holds the LVGL lock. Only update the currently visible page. */
void ui_battery_create(ui_battery_t *battery, lv_obj_t *parent);
void ui_battery_update(ui_battery_t *battery, battery_status_t state,
                        int percent, uint32_t now_ms);

#endif
