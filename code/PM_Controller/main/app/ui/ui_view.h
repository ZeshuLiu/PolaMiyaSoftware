#ifndef PM_UI_VIEW_H
#define PM_UI_VIEW_H

#include "lvgl.h"
#include "sdm50.h"
#include "settings.h"
#include "focus_distance.h"
#include "power_monitor.h"

enum { UI_HOME, UI_INFO, UI_SETUP, UI_PAGE_COUNT };

/* Internal layout API. Caller holds lvgl_port_lock for every call. */
void ui_view_create(lv_display_t *display);
void ui_view_show(unsigned page);
/* -1 means unknown; otherwise 0..100 percent. */
void ui_view_battery(int percent);
void ui_view_power(const power_sample_t *sample);
void ui_view_range(const focus_distance_t *focus, const sdm50_sample_t *actual);
void ui_view_history(int32_t distance_mm, unsigned elapsed_slots);
void ui_view_uptime(uint64_t seconds);
void ui_view_info(const settings_values_t *values, const settings_status_t *status,
                  bool available);
void ui_view_settings(unsigned percent, bool editing, const char *message);

#endif
