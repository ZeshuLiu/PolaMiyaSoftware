#ifndef PM_UI_VIEW_H
#define PM_UI_VIEW_H

#include "lvgl.h"
#include "sdm50.h"
#include "settings.h"
#include "focus_distance.h"
#include "power_monitor.h"
#include "charge_status.h"

enum { UI_HOME, UI_INFO, UI_SETUP, UI_EJECT, UI_PAGE_COUNT };
/* EJECT is entered only by starting the motor, not by INFO/SETUP navigation. */
enum { UI_NAV_PAGE_COUNT = UI_EJECT };
/* HOME middle-key list order: SETTINGS -> FOCUS -> EJECT -> INFO -> SETTINGS. */
enum {
    UI_HOME_FUNC_SETTINGS,
    UI_HOME_FUNC_FOCUS,
    UI_HOME_FUNC_EJECT,
    UI_HOME_FUNC_INFO,
    UI_HOME_FUNC_COUNT,
};

/* Internal layout API. Caller holds lvgl_port_lock for every call. */
void ui_view_create(lv_display_t *display);
void ui_view_show(unsigned page);
/* Update only the independent EJECT page, using actual motor current and elapsed time. */
void ui_view_eject(uint64_t elapsed_ms, const power_sample_t *sample);
void ui_view_power(const power_sample_t *sample, battery_status_t battery);
void ui_view_range(const focus_distance_t *focus, const sdm50_sample_t *actual);
/* Style the target after automatic or manual selection. */
void ui_view_focus_style(bool editing, bool automatic, bool too_close, bool ready, bool blink_on);
/* 0..8 sheets; 9 displays A for the unremoved dark slide. */
void ui_view_film(unsigned remaining);
void ui_view_uptime(uint64_t seconds);
void ui_view_info(const settings_values_t *values, const settings_status_t *status,
                  bool available);
void ui_view_focus_info(const focus_distance_t *focus, const focus_unit_snapshot_t *snapshot);
/* Update the right-hand key hints for HOME or the INFO/SETUP pages. */
void ui_view_key_hints(unsigned page, unsigned home_func, bool home_selected);

#endif
