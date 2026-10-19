#ifndef PM_UI_VIEW_H
#define PM_UI_VIEW_H

#include "lvgl.h"
#include "sdm50.h"
#include "settings.h"
#include "focus_distance.h"
#include "power_monitor.h"

enum { UI_HOME, UI_INFO, UI_SETUP, UI_PAGE_COUNT };

/* HOME middle-key list order: SETTINGS -> FOCUS -> INFO -> SETTINGS. */
enum {
    UI_HOME_FUNC_SETTINGS,
    UI_HOME_FUNC_FOCUS,
    UI_HOME_FUNC_INFO,
    UI_HOME_FUNC_COUNT,
};

/* Internal layout API. Caller holds lvgl_port_lock for every call. */
void ui_view_create(lv_display_t *display);
void ui_view_show(unsigned page);
/* -1 means unknown; otherwise 0..100 percent. */
void ui_view_battery(int percent);
void ui_view_power(const power_sample_t *sample);
void ui_view_range(const focus_distance_t *focus, const sdm50_sample_t *actual);
/* Dim the focused distance while the parameter blink phase is off. */
void ui_view_focus_blink(bool visible);
/* 0..8 remaining instant-film shots. Values above 8 are clamped. */
void ui_view_film(unsigned remaining);
void ui_view_history(int32_t distance_mm, unsigned elapsed_slots);
void ui_view_uptime(uint64_t seconds);
void ui_view_info(const settings_values_t *values, const settings_status_t *status,
                  bool available);
void ui_view_settings(unsigned percent, bool editing, const char *message);
/* Update the right-hand key hints for HOME or the INFO/SETUP pages. */
void ui_view_key_hints(unsigned page, unsigned home_func, bool focus_editing,
                       bool setup_editing);

#endif
