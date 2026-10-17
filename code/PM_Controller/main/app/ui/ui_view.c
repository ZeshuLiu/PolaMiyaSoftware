#include "ui_view.h"
#include "ui_key_hints.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BG     0x080b0c
#define COLOR_TEXT   0xf1f5f4
#define COLOR_MUTED  0x9caeab
#define COLOR_ACCENT 0xb1ffd3
#define COLOR_ACTUAL 0x72c9ff
#define HISTORY_POINTS 101
#define SCALE_ZERO_X 14
#define SCALE_20M_X 112
#define SCALE_INFINITY_X 139

/* NEAR belongs only to the measured distance, not the FocusUnit interface. */
typedef enum {
    UI_DISTANCE_UNAVAILABLE,
    UI_DISTANCE_FINITE,
    UI_DISTANCE_INFINITY,
    UI_DISTANCE_NEAR,
} ui_distance_state_t;

static struct {
    lv_obj_t *pages[UI_PAGE_COUNT];
    lv_obj_t *battery_fill;
    lv_obj_t *battery_unknown;
    lv_obj_t *battery_values;
    lv_obj_t *motor_values;
    lv_obj_t *segments[2][4][7];
    uint8_t digit_masks[2][4];
    bool text_number[2];
    lv_obj_t *number_override[2];
    lv_obj_t *decimal[2];
    lv_obj_t *focus_pointer;
    lv_obj_t *actual_pointer;
    lv_obj_t *uptime;
    lv_obj_t *chart;
    lv_chart_series_t *series;
    int32_t history[HISTORY_POINTS];
    lv_obj_t *axis_min;
    lv_obj_t *axis_max;
    lv_obj_t *counts[2];
    lv_obj_t *motor_time;
    lv_obj_t *storage;
    lv_obj_t *brightness;
    lv_obj_t *brightness_bar;
    lv_obj_t *save_status;
    lv_obj_t *edit_status;
    unsigned page;
    bool editing;
    int battery_percent;
} view;

static void set_text(lv_obj_t *obj, const char *value)
{
    /* Unchanged text must not restart long-counter scrolling or allocate again. */
    if (strcmp(lv_label_get_text(obj), value) != 0) {
        lv_label_set_text(obj, value);
    }
}

static void set_text_fmt(lv_obj_t *obj, const char *format, ...)
{
    /* All UI formats are bounded: the longest is one uint64 counter plus a prefix. */
    char text[96];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(text, sizeof(text), format, arguments);
    va_end(arguments);
    set_text(obj, text);
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int width,
                       const char *value, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_WRAP);
    set_text(obj, value);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *page(lv_obj_t *screen, const char *title, const char *footer)
{
    lv_obj_t *obj = rectangle(screen, 0, 0, 172, 320, COLOR_BG);
    label(obj, 12, 9, 132, title, COLOR_TEXT);
    rectangle(obj, 12, 30, 132, 1, 0x303c39);
    lv_obj_t *index = label(obj, 12, 301, 132, footer, COLOR_MUTED);
    lv_obj_set_style_text_align(index, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static void create_distance_digits(lv_obj_t *parent, unsigned number, int x, uint32_t color)
{
    memset(view.digit_masks[number], 0xff, sizeof(view.digit_masks[number]));
    /* Seven-segment digits keep the large number independent of sdkconfig fonts.
     * Segment order: top, upper-right, lower-right, bottom, lower-left,
     * upper-left, middle. All geometry is in portrait pixels. */
    const int parts[7][4] = {
        {2, 0, 7, 2}, {9, 2, 2, 9}, {9, 13, 2, 9},
        {2, 22, 7, 2}, {0, 13, 2, 9}, {0, 2, 2, 9}, {2, 11, 7, 2},
    };
    const int positions[] = {0, 14, 34, 48};
    for (unsigned digit = 0; digit < 4; ++digit) {
        for (unsigned segment = 0; segment < 7; ++segment) {
            const int *p = parts[segment];
            view.segments[number][digit][segment] = rectangle(
                parent, x + positions[digit] + p[0], 74 + p[1], p[2], p[3], color);
        }
    }
    view.decimal[number] = rectangle(parent, x + 29, 95, 3, 3, color);
    view.number_override[number] = label(parent, x, 77, 59, "", color);
    lv_label_set_long_mode(view.number_override[number], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_height(view.number_override[number], 20);
}

static void set_digit(unsigned number, unsigned digit, unsigned mask)
{
    if (view.digit_masks[number][digit] == mask) return;
    for (unsigned segment = 0; segment < 7; ++segment) {
        /* LVGL local style setters invalidate even when the value is unchanged. */
        if (((view.digit_masks[number][digit] ^ mask) & (1U << segment)) == 0) continue;
        lv_obj_set_style_bg_opa(view.segments[number][digit][segment],
                               mask & (1U << segment) ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
    view.digit_masks[number][digit] = (uint8_t)mask;
}

static int distance_scale_x(uint32_t distance_mm)
{
    /* Shifted logarithm includes exactly zero. Values above 20 m clamp to the
     * finite endpoint, never to the separate infinity position. */
    if (distance_mm >= 20000) return SCALE_20M_X;
    const float fraction = log1pf(distance_mm / 1000.0f) / log1pf(20.0f);
    return SCALE_ZERO_X + (int)((SCALE_20M_X - SCALE_ZERO_X) * fraction + 0.5f);
}

static lv_obj_t *create_pointer(lv_obj_t *parent, bool upper, uint32_t color)
{
    static const lv_point_precise_t down[] = {{0, 0}, {8, 0}, {4, 6}, {0, 0}};
    static const lv_point_precise_t up[] = {{0, 6}, {8, 6}, {4, 0}, {0, 6}};
    lv_obj_t *obj = lv_line_create(parent);
    lv_line_set_points(obj, upper ? down : up, 4);
    lv_obj_set_pos(obj, SCALE_ZERO_X - 4, upper ? 138 : 156);
    lv_obj_set_style_line_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(obj, 2, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    return obj;
}

static void create_distance_scale(lv_obj_t *parent)
{
    rectangle(parent, SCALE_ZERO_X, 150, SCALE_20M_X - SCALE_ZERO_X + 1, 1, COLOR_MUTED);
    /* Broken extension separates finite 20 m from infinity. */
    rectangle(parent, 119, 150, 4, 1, COLOR_MUTED);
    rectangle(parent, 128, 150, 4, 1, COLOR_MUTED);
    static const unsigned marks[] = {0, 1, 2, 5, 10, 20};
    for (unsigned i = 0; i < sizeof(marks) / sizeof(marks[0]); ++i) {
        const int x = distance_scale_x(marks[i] * 1000);
        rectangle(parent, x, 147, 1, 7, COLOR_MUTED);
        const int width = marks[i] >= 10 ? 22 : 16;
        lv_obj_t *tick = label(parent, x - width / 2, 170, width, "", COLOR_MUTED);
        set_text_fmt(tick, "%u", marks[i]);
        lv_obj_set_style_text_align(tick, LV_TEXT_ALIGN_CENTER, 0);
    }
    rectangle(parent, SCALE_INFINITY_X, 147, 1, 7, COLOR_MUTED);
    lv_obj_t *infinity = label(parent, 127, 170, 24, "INF", COLOR_MUTED);
    lv_obj_set_style_text_align(infinity, LV_TEXT_ALIGN_CENTER, 0);
    view.focus_pointer = create_pointer(parent, true, COLOR_ACCENT);
    view.actual_pointer = create_pointer(parent, false, COLOR_ACTUAL);
}

void ui_view_create(lv_display_t *display)
{
    view.page = UI_PAGE_COUNT; /* First show must hide the other two pages. */
    view.battery_percent = -2; /* First battery update must draw the unknown state. */
    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, LV_FONT_DEFAULT, 0);

    lv_obj_t *home = view.pages[UI_HOME] = page(screen, "PolaMiya", "1/3 HOME");
    lv_obj_t *battery = rectangle(home, 121, 11, 19, 10, COLOR_BG);
    lv_obj_set_style_bg_opa(battery, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(battery, lv_color_hex(COLOR_MUTED), 0);
    lv_obj_set_style_border_width(battery, 1, 0);
    lv_obj_set_style_radius(battery, 2, 0);
    rectangle(home, 141, 14, 2, 4, COLOR_MUTED);
    view.battery_fill = rectangle(home, 123, 13, 15, 6, COLOR_TEXT);
    view.battery_unknown = label(home, 121, 8, 19, "?", COLOR_MUTED);
    lv_obj_set_style_text_align(view.battery_unknown, LV_TEXT_ALIGN_CENTER, 0);
    ui_view_battery(-1);
    label(home, 12, 43, 62, "FOCUS", COLOR_ACCENT);
    label(home, 86, 43, 60, "ACTUAL", COLOR_ACTUAL);
    create_distance_digits(home, 0, 12, COLOR_ACCENT);
    label(home, 76, 76, 10, "/", COLOR_TEXT);
    create_distance_digits(home, 1, 86, COLOR_ACTUAL);
    create_distance_scale(home);
    label(home, 12, 190, 132, "RANGE m / 10s", COLOR_MUTED);
    view.axis_max = label(home, 12, 212, 20, "--", COLOR_MUTED);
    view.axis_min = label(home, 12, 248, 20, "--", COLOR_MUTED);
    view.chart = lv_chart_create(home);
    lv_obj_set_pos(view.chart, 35, 214);
    lv_obj_set_size(view.chart, 108, 48);
    lv_obj_set_style_bg_opa(view.chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view.chart, 0, 0);
    lv_obj_set_style_pad_all(view.chart, 0, 0);
    lv_obj_set_style_line_color(view.chart, lv_color_hex(0x303c39), LV_PART_MAIN);
    lv_obj_set_style_line_width(view.chart, 1, LV_PART_MAIN);
    lv_obj_set_style_line_width(view.chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_size(view.chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_remove_flag(view.chart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(view.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(view.chart, 3, 0);
    lv_chart_set_point_count(view.chart, HISTORY_POINTS);
    view.series = lv_chart_add_series(view.chart, lv_color_hex(COLOR_ACCENT),
                                     LV_CHART_AXIS_PRIMARY_Y);
    LV_ASSERT_MALLOC(view.series);
    for (unsigned i = 0; i < HISTORY_POINTS; ++i) {
        view.history[i] = LV_CHART_POINT_NONE;
    }
    lv_chart_set_series_ext_y_array(view.chart, view.series, view.history);
    lv_chart_set_axis_range(view.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    label(home, 35, 264, 56, "-10s", COLOR_MUTED);
    lv_obj_t *now = label(home, 99, 264, 44, "NOW", COLOR_MUTED);
    lv_obj_set_style_text_align(now, LV_TEXT_ALIGN_RIGHT, 0);
    view.uptime = label(home, 12, 282, 132, "UP 00:00:00", COLOR_TEXT);

    lv_obj_t *info = view.pages[UI_INFO] = page(screen, "DEVICE INFO", "2/3 INFO");
    label(info, 12, 51, 132, "BATTERY", COLOR_MUTED);
    view.battery_values = label(info, 12, 71, 132, "-- V / -- %", COLOR_TEXT);
    label(info, 12, 100, 132, "MOTOR / IPROPI", COLOR_MUTED);
    view.motor_values = label(info, 12, 120, 132, "-- mA / -- mV", COLOR_TEXT);
    lv_label_set_long_mode(view.battery_values, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_label_set_long_mode(view.motor_values, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_height(view.battery_values, 18);
    lv_obj_set_height(view.motor_values, 18);
    label(info, 12, 149, 132, "REBOOTS / SHOTS", COLOR_MUTED);
    view.counts[0] = label(info, 12, 169, 132, "R --", COLOR_TEXT);
    view.counts[1] = label(info, 12, 189, 132, "S --", COLOR_TEXT);
    label(info, 12, 218, 132, "MOTOR TOTAL", COLOR_MUTED);
    view.motor_time = label(info, 12, 238, 132, "--", COLOR_TEXT);
    for (unsigned i = 0; i < 2; ++i) {
        lv_label_set_long_mode(view.counts[i], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
        lv_obj_set_height(view.counts[i], 18);
    }
    lv_label_set_long_mode(view.motor_time, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_height(view.motor_time, 18);
    view.storage = label(info, 12, 270, 132, "EEPROM --", COLOR_MUTED);

    lv_obj_t *setup = view.pages[UI_SETUP] = page(screen, "SETTINGS", "3/3 SETUP");
    label(setup, 12, 54, 132, "BACKLIGHT", COLOR_MUTED);
    view.brightness = label(setup, 12, 88, 132, "100 %", COLOR_TEXT);
    view.brightness_bar = lv_bar_create(setup);
    lv_obj_set_pos(view.brightness_bar, 12, 128);
    lv_obj_set_size(view.brightness_bar, 130, 6);
    lv_obj_set_style_bg_color(view.brightness_bar, lv_color_hex(0x303c39), LV_PART_MAIN);
    lv_obj_set_style_bg_color(view.brightness_bar, lv_color_hex(COLOR_ACCENT), LV_PART_INDICATOR);
    view.save_status = label(setup, 12, 155, 132, "", COLOR_MUTED);
    view.edit_status = label(setup, 12, 155, 132, "", COLOR_ACCENT);
    ui_key_hints_create(screen);
    ui_view_show(UI_HOME);
}

void ui_view_show(unsigned page_index)
{
    if (view.page == page_index) return;
    view.page = page_index;
    for (unsigned i = 0; i < UI_PAGE_COUNT; ++i) {
        if (i == page_index) {
            lv_obj_remove_flag(view.pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(view.pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    ui_key_hints_update(view.page == UI_SETUP, view.editing);
}

void ui_view_battery(int percent)
{
    if (view.battery_percent == percent) return;
    view.battery_percent = percent;
    if (percent < 0) {
        lv_obj_add_flag(view.battery_fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(view.battery_unknown, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(view.battery_unknown, LV_OBJ_FLAG_HIDDEN);
    if (percent == 0) {
        lv_obj_add_flag(view.battery_fill, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(view.battery_fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(view.battery_fill, (15 * percent + 99) / 100);
    }
}

void ui_view_power(const power_sample_t *sample)
{
    if (view.page == UI_HOME) ui_view_battery(sample->battery_percent);
    if (view.page != UI_INFO) return;
    if (sample->battery_valid) {
        set_text_fmt(view.battery_values, "%lu.%02luV / ~%d%%",
                     (unsigned long)(sample->battery_mv / 1000),
                     (unsigned long)(sample->battery_mv % 1000 / 10), sample->battery_percent);
    } else if (sample->adc.battery.valid) {
        set_text_fmt(view.battery_values, sample->adc.battery.saturated ? "ADC SAT %u" : "ADC RAW %u",
                     (unsigned)sample->adc.battery.raw);
    } else {
        set_text(view.battery_values, "-- V / -- %");
    }
    if (sample->motor_valid) {
        set_text_fmt(view.motor_values, "%lu mA / %u mV",
                     (unsigned long)sample->motor_current_ma, (unsigned)sample->adc.motor.voltage_mv);
    } else if (sample->adc.motor.valid) {
        set_text_fmt(view.motor_values, sample->adc.motor.saturated ? "ADC SAT %u" : "ADC RAW %u",
                     (unsigned)sample->adc.motor.raw);
    } else {
        set_text(view.motor_values, "-- mA / -- mV");
    }
}

static void set_distance_number(unsigned number, ui_distance_state_t state, uint32_t mm)
{
    static const uint8_t masks[] = {0x3f, 0x06, 0x5b, 0x4f, 0x66,
                                    0x6d, 0x7d, 0x07, 0x7f, 0x6f};
    const bool valid = state == UI_DISTANCE_FINITE;
    unsigned cm = (unsigned)(((uint64_t)mm + 5) / 10);
    const bool text_number = state == UI_DISTANCE_INFINITY || state == UI_DISTANCE_NEAR ||
                             (valid && cm > 9999);
    if (state == UI_DISTANCE_INFINITY) {
        set_text(view.number_override[number], "INF");
    } else if (state == UI_DISTANCE_NEAR) {
        set_text(view.number_override[number], "NEAR");
    } else if (text_number) {
        /* Future calibrated values above 99.99 m scroll instead of wrapping digits. */
        set_text_fmt(view.number_override[number], "%u.%02u", cm / 100, cm % 100);
    } else {
        set_text(view.number_override[number], "");
    }
    if (view.text_number[number] != text_number) {
        lv_obj_set_style_bg_opa(view.decimal[number], text_number ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
        view.text_number[number] = text_number;
    }
    for (int digit = 3; digit >= 0; --digit) {
        unsigned mask = valid ? masks[cm % 10U] : 0x40;
        if (text_number) mask = 0;
        if (digit == 0 && valid && cm == 0) {
            mask = 0; /* No leading zero for distances below 10 m. */
        }
        set_digit(number, (unsigned)digit, mask);
        cm /= 10U;
    }
}

static void set_distance_pointer(lv_obj_t *pointer, ui_distance_state_t state, uint32_t mm)
{
    if (state == UI_DISTANCE_UNAVAILABLE) {
        lv_obj_add_flag(pointer, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(pointer, LV_OBJ_FLAG_HIDDEN);
    const int x = state == UI_DISTANCE_INFINITY ? SCALE_INFINITY_X :
                  state == UI_DISTANCE_NEAR ? SCALE_ZERO_X : distance_scale_x(mm);
    lv_obj_set_x(pointer, x - 4);
}

void ui_view_range(const focus_distance_t *focus, const sdm50_sample_t *actual)
{
    if (view.page != UI_HOME) return;
    const ui_distance_state_t focus_state = focus->state == FOCUS_DISTANCE_FINITE ? UI_DISTANCE_FINITE :
                                           focus->state == FOCUS_DISTANCE_INFINITY ? UI_DISTANCE_INFINITY :
                                                                                   UI_DISTANCE_UNAVAILABLE;
    const ui_distance_state_t actual_state = actual->status == SDM50_VALID ? UI_DISTANCE_FINITE :
                                            actual->status == SDM50_TOO_CLOSE ? UI_DISTANCE_NEAR :
                                            actual->status == SDM50_NO_TARGET ? UI_DISTANCE_INFINITY :
                                                                               UI_DISTANCE_UNAVAILABLE;
    set_distance_number(0, focus_state, focus->distance_mm);
    set_distance_number(1, actual_state, actual->distance_mm);
    set_distance_pointer(view.focus_pointer, focus_state, focus->distance_mm);
    set_distance_pointer(view.actual_pointer, actual_state, actual->distance_mm);
}

void ui_view_history(int32_t distance_mm, unsigned elapsed_slots)
{
    if (elapsed_slots > HISTORY_POINTS) {
        elapsed_slots = HISTORY_POINTS;
    }
    memmove(view.history, view.history + elapsed_slots,
            (HISTORY_POINTS - elapsed_slots) * sizeof(view.history[0]));
    for (unsigned i = HISTORY_POINTS - elapsed_slots; i < HISTORY_POINTS; ++i) {
        view.history[i] = LV_CHART_POINT_NONE;
    }
    view.history[HISTORY_POINTS - 1] = distance_mm;
    int32_t low = INT32_MAX;
    int32_t high = 0;
    for (unsigned i = 0; i < HISTORY_POINTS; ++i) {
        const int32_t value = view.history[i];
        if (value == LV_CHART_POINT_NONE) {
            continue;
        }
        if (value < low) low = value;
        if (value > high) high = value;
    }
    if (low == INT32_MAX) {
        set_text(view.axis_min, "--");
        set_text(view.axis_max, "--");
    } else {
        /* Axis labels are whole meters so 50 m still fits the narrow margin. */
        low = low / 1000 * 1000;
        high = (high / 1000 + 1) * 1000;
        lv_chart_set_axis_range(view.chart, LV_CHART_AXIS_PRIMARY_Y, low, high);
        set_text_fmt(view.axis_min, "%ld", (long)(low / 1000));
        set_text_fmt(view.axis_max, "%ld", (long)(high / 1000));
    }
    lv_chart_refresh(view.chart);
}

void ui_view_uptime(uint64_t seconds)
{
    if (view.page != UI_HOME) return;
    set_text_fmt(view.uptime, "UP %02lu:%02u:%02u",
                          (unsigned long)(seconds / 3600),
                          (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
}

void ui_view_info(const settings_values_t *values, const settings_status_t *status,
                  bool available)
{
    if (view.page != UI_INFO) return;
    if (!available) {
        set_text(view.counts[0], "R --");
        set_text(view.counts[1], "S --");
        set_text(view.motor_time, "--");
        set_text(view.storage, "EEPROM ERROR");
        return;
    }
    /* Preserve exact persisted counters; overflow scrolls within its own row. */
    set_text_fmt(view.counts[0], "R %" PRIu64, values->reboot_count);
    set_text_fmt(view.counts[1], "S %" PRIu64, values->shutter_count);
    const uint64_t minutes = values->motor_run_ms / 60000;
    set_text_fmt(view.motor_time, "%" PRIu64 "h %02u m",
                          minutes / 60, (unsigned)(minutes % 60));
    set_text(view.storage, !status->writable ? "EEPROM ERROR" :
                      status->dirty ? "RAM UNSAVED" :
                      status->loaded_from_eeprom ? "EEPROM OK" : "RAM DEFAULTS");
}

void ui_view_settings(unsigned percent, bool editing, const char *message)
{
    if (view.page != UI_SETUP) return;
    set_text_fmt(view.brightness, "%u %%", percent);
    if (view.editing != editing) {
        lv_obj_set_style_text_color(view.brightness, lv_color_hex(editing ? COLOR_ACCENT : COLOR_TEXT), 0);
        lv_obj_set_y(view.save_status, editing ? 180 : 155);
    }
    lv_bar_set_value(view.brightness_bar, (int32_t)percent, LV_ANIM_OFF);
    set_text(view.edit_status, editing ? "EDITING" : "");
    set_text(view.save_status, message);
    view.editing = editing;
    ui_key_hints_update(view.page == UI_SETUP, editing);
}
