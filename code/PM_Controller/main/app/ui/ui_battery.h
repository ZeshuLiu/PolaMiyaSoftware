#ifndef PM_UI_BATTERY_H
#define PM_UI_BATTERY_H

#include <stdbool.h>
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
    bool level_unknown;
} ui_battery_t;

/* Black-header widget at (131,11), 19x10 battery outline, 8x14 bolt to its left.
 * HOME repositions the root so its outline stays at (143,11).
 * Caller holds the LVGL lock. Only update the currently visible page. */
void ui_battery_create(ui_battery_t *battery, lv_obj_t *parent);
void ui_battery_update(ui_battery_t *battery, battery_status_t state,
                        int percent);

#endif
