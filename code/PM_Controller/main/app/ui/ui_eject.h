#ifndef PM_UI_EJECT_H
#define PM_UI_EJECT_H

#include "lvgl.h"
#include "power_monitor.h"
#include "charge_status.h"

/* Independent page; caller holds the LVGL lock. No task or timer owned here. */
lv_obj_t *ui_eject_create(lv_obj_t *screen);
void ui_eject_update(uint64_t elapsed_ms, const power_sample_t *sample);
void ui_eject_battery(battery_status_t state, int percent, uint32_t now_ms);

#endif
