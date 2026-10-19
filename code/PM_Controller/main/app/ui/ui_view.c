#include "ui_view.h"
#include "ui_key_hints.h"
#include "ui_home_assets.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BG            0x080b0c
#define COLOR_TEXT          0xf1f5f4
#define COLOR_MUTED         0x9caeab
#define COLOR_ACCENT        0xb1ffd3
#define COLOR_ACCENT_DIM    0x202d28 /* v4: accent at 14% over COLOR_BG. */
#define COLOR_ACTUAL        0x72c9ff
#define COLOR_GRID          0x1e2e26

#define HISTORY_POINTS      101
#define FILM_COUNT          8
#define FILM_FRAME_Y        267
#define SCALE_ZERO_X        14
#define SCALE_20M_X         112
#define SCALE_INFINITY_X    139
#define BATTERY_X           131

#define FONT_SMALL          (&ui_font_small)

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
    lv_obj_t *number_label[2];
    lv_obj_t *distance_unit[2];
    bool number_large[2];
    bool unit_visible[2];
    int unit_x[2];
    lv_obj_t *focus_pointer;
    lv_obj_t *actual_pointer;
    lv_obj_t *uptime_value;
    lv_obj_t *chart;
    lv_chart_series_t *series;
    lv_obj_t *chart_endpoint;
    bool chart_dirty;
    bool endpoint_visible;
    int32_t history[HISTORY_POINTS];
    int32_t chart_min;
    int32_t chart_max;
    lv_obj_t *axis_min;
    lv_obj_t *axis_max;
    lv_obj_t *film_count;
    lv_obj_t *film_frames[FILM_COUNT];
    int film_remaining;
    lv_obj_t *counts[2];
    lv_obj_t *motor_time;
    lv_obj_t *storage;
    lv_obj_t *brightness;
    lv_obj_t *brightness_bar;
    lv_obj_t *save_status;
    lv_obj_t *edit_status;
    unsigned page;
    bool editing;
    bool focus_visible;
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

static lv_obj_t *plain_object(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    return obj;
}

static lv_obj_t *rectangle(lv_obj_t *parent, int x, int y, int w, int h,
                           uint32_t color)
{
    lv_obj_t *obj = plain_object(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *image(lv_obj_t *parent, int x, int y, const lv_image_dsc_t *source)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    return obj;
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

static lv_obj_t *label_font(lv_obj_t *parent, int x, int y, int width,
                            const char *value, uint32_t color,
                            const lv_font_t *font)
{
    lv_obj_t *obj = label(parent, x, y, width, value, color);
    lv_obj_set_style_text_font(obj, font, 0);
    return obj;
}

static lv_obj_t *page(lv_obj_t *screen, const char *title, const char *footer)
{
    lv_obj_t *obj = rectangle(screen, 0, 0, 172, 320, COLOR_BG);
    if (title) {
        label(obj, 12, 9, 132, title, COLOR_TEXT);
    }
    rectangle(obj, 12, 30, 132, 1, COLOR_GRID);
    if (footer) {
        lv_obj_t *index = label(obj, 12, 301, 132, footer, COLOR_MUTED);
        lv_obj_set_style_text_align(index, LV_TEXT_ALIGN_CENTER, 0);
    }
    return obj;
}

static void create_distance_number(lv_obj_t *parent, unsigned number, int x, int y,
                                    uint32_t color)
{
    lv_obj_t *container = plain_object(parent, x, y, 70, 44);
    view.number_label[number] = label_font(container, 0, 0, 63, "", color,
                                           &ui_font_distance);
    lv_obj_set_height(view.number_label[number], 44);
    lv_label_set_long_mode(view.number_label[number], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    view.number_large[number] = true;

    view.distance_unit[number] = label_font(container, 63, 24, 7, "m",
                                            number == 0 ? 0x719182 : 0x648696,
                                            &ui_font_uptime);
    view.unit_x[number] = 63;
    view.unit_visible[number] = false;
    lv_obj_add_flag(view.distance_unit[number], LV_OBJ_FLAG_HIDDEN);
}

static int distance_scale_x(uint32_t distance_mm)
{
    /* Shifted logarithm includes exactly zero. Values above 20 m clamp to the
     * finite endpoint, never to the separate infinity position. */
    if (distance_mm >= 20000) return SCALE_20M_X;
    const float fraction = log1pf(distance_mm / 1000.0f) / log1pf(20.0f);
    return SCALE_ZERO_X + (int)((SCALE_20M_X - SCALE_ZERO_X) * fraction + 0.5f);
}

static lv_obj_t *create_pointer(lv_obj_t *parent, bool upper)
{
    /* Fractional triangle edges are rasterized from the approved SVG. */
    return image(parent, SCALE_ZERO_X - 4, upper ? 95 : 114,
                 upper ? &ui_pointer_focus : &ui_pointer_actual);
}

static void create_distance_scale(lv_obj_t *parent)
{
    /* Ticks and labels already belong to the static HOME tiles. */
    view.focus_pointer = create_pointer(parent, true);
    view.actual_pointer = create_pointer(parent, false);
}

static void create_film_strip(lv_obj_t *parent)
{
    view.film_count = label_font(parent, 120, 244, 24, "0", COLOR_TEXT,
                                 &ui_font_count);
    lv_obj_set_style_text_color(view.film_count, lv_color_hex(0xdff7e8), 0);
    lv_obj_set_style_text_align(view.film_count, LV_TEXT_ALIGN_RIGHT, 0);

    /* Tiles partition the original strip in its blank gaps, retaining each
     * paper's fractional edges. Row padding is stored but never displayed. */
    static const int frame_x[FILM_COUNT] = {0, 15, 32, 49, 66, 83, 99, 116};
    lv_obj_t *strip = plain_object(parent, 12, FILM_FRAME_Y, 132, 19);
    for (unsigned i = 0; i < FILM_COUNT; ++i) {
        view.film_frames[i] = image(strip, frame_x[i], 0, ui_film_dim[i]);
    }
    view.film_remaining = -1;
}

static void refresh_history(void)
{
    if (!view.chart_dirty) return;

    int32_t low = INT32_MAX;
    int32_t high = 0;
    for (unsigned i = 0; i < HISTORY_POINTS; ++i) {
        const int32_t value = view.history[i];
        if (value == LV_CHART_POINT_NONE) continue;
        if (value < low) low = value;
        if (value > high) high = value;
    }

    bool range_changed = false;
    if (low == INT32_MAX) {
        set_text(view.axis_min, "--");
        set_text(view.axis_max, "--");
    } else {
        low = low / 1000 * 1000;
        high = (high / 1000 + 1) * 1000;
        range_changed = low != view.chart_min || high != view.chart_max;
        if (range_changed) {
            view.chart_min = low;
            view.chart_max = high;
            /* This setter also refreshes the native chart. */
            lv_chart_set_axis_range(view.chart, LV_CHART_AXIS_PRIMARY_Y, low, high);
        }
        set_text_fmt(view.axis_min, "%ld", (long)(low / 1000));
        set_text_fmt(view.axis_max, "%ld", (long)(high / 1000));
    }
    if (!range_changed) lv_chart_refresh(view.chart);

    /* A standard circle object marks only the latest slot, without a custom
     * drawing callback or point indicators on all 101 history samples. */
    const bool valid = view.history[HISTORY_POINTS - 1] != LV_CHART_POINT_NONE;
    if (valid) {
        lv_point_t point;
        lv_chart_get_point_pos_by_id(view.chart, view.series, HISTORY_POINTS - 1, &point);
        lv_obj_set_pos(view.chart_endpoint, lv_obj_get_x(view.chart) + point.x - 1,
                       lv_obj_get_y(view.chart) + point.y - 1);
    }
    if (view.endpoint_visible != valid) {
        if (valid) lv_obj_remove_flag(view.chart_endpoint, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(view.chart_endpoint, LV_OBJ_FLAG_HIDDEN);
        view.endpoint_visible = valid;
    }
    view.chart_dirty = false;
}

void ui_view_create(lv_display_t *display)
{
    view.page = UI_PAGE_COUNT; /* First show must hide the other pages. */
    view.battery_percent = -2; /* First battery update must draw the unknown state. */
    view.focus_visible = true;
    view.film_remaining = -1;

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, &ui_font_body, 0);

    lv_obj_t *home = view.pages[UI_HOME] = rectangle(screen, 0, 0, 172, 320, COLOR_BG);
    /* The approved SVG supplies every static label, grid line and separator.
     * Only live values are separate objects; no Windows font is needed on-board. */
    for (unsigned i = 0; i < sizeof(ui_home_tiles) / sizeof(ui_home_tiles[0]); ++i) {
        image(home, ui_home_tiles[i].x, ui_home_tiles[i].y, ui_home_tiles[i].image);
    }
    view.battery_fill = rectangle(home, BATTERY_X + 2, 13, 15, 6, 0xb7e6cb);
    lv_obj_set_style_radius(view.battery_fill, 1, 0);
    view.battery_unknown = label_font(home, BATTERY_X, 8, 19, "?", COLOR_MUTED,
                                      FONT_SMALL);
    lv_obj_set_style_text_align(view.battery_unknown, LV_TEXT_ALIGN_CENTER, 0);
    ui_view_battery(-1);

    create_distance_number(home, 0, 12, 46, COLOR_ACCENT);
    create_distance_number(home, 1, 83, 46, COLOR_ACTUAL);
    create_distance_scale(home);

    view.axis_max = label_font(home, 17, 166, 16, "--", 0x819b8e, &ui_font_axis);
    view.axis_min = label_font(home, 17, 206, 16, "--", 0x819b8e, &ui_font_axis);
    view.chart = lv_chart_create(home);
    lv_obj_remove_style_all(view.chart);
    lv_obj_set_pos(view.chart, 32, 170);
    lv_obj_set_size(view.chart, 110, 44);
    /* 2 px padding leaves the original (34,172)..(140,212) data rectangle
     * inside the chart's clip area, including the rightmost/bottom samples. */
    lv_obj_set_style_pad_all(view.chart, 2, 0);
    lv_obj_set_style_bg_opa(view.chart, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(view.chart, 0, 0);
    lv_obj_set_style_line_width(view.chart, 1, LV_PART_ITEMS);
    lv_obj_set_style_size(view.chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_remove_flag(view.chart, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_chart_set_type(view.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_update_mode(view.chart, LV_CHART_UPDATE_MODE_SHIFT);
    /* Grid lines already belong to the static background. */
    lv_chart_set_div_line_count(view.chart, 0, 0);
    lv_chart_set_point_count(view.chart, HISTORY_POINTS);
    view.series = lv_chart_add_series(view.chart, lv_color_hex(COLOR_ACTUAL),
                                       LV_CHART_AXIS_PRIMARY_Y);
    LV_ASSERT_MALLOC(view.series);
    for (unsigned i = 0; i < HISTORY_POINTS; ++i) {
        view.history[i] = LV_CHART_POINT_NONE;
    }
    lv_chart_set_series_ext_y_array(view.chart, view.series, view.history);
    view.chart_min = 0;
    view.chart_max = 1000;
    lv_chart_set_axis_range(view.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 1000);
    view.chart_endpoint = rectangle(home, 139, 211, 3, 3, 0xb4e3ff);
    lv_obj_set_style_radius(view.chart_endpoint, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(view.chart_endpoint, LV_OBJ_FLAG_HIDDEN);
    view.endpoint_visible = false;
    view.chart_dirty = true;

    create_film_strip(home);

    view.uptime_value = label_font(home, 80, 295, 64, "00:00:00", 0xd9e8e0,
                                   &ui_font_uptime);
    lv_obj_set_style_text_align(view.uptime_value, LV_TEXT_ALIGN_RIGHT, 0);

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
    lv_obj_set_style_bg_color(view.brightness_bar, lv_color_hex(0x303c39),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_color(view.brightness_bar, lv_color_hex(COLOR_ACCENT),
                              LV_PART_INDICATOR);
    view.save_status = label(setup, 12, 155, 132, "", COLOR_MUTED);
    view.edit_status = label(setup, 12, 155, 132, "", COLOR_ACCENT);

    ui_key_hints_create(screen);
    ui_key_hints_home(UI_HOME_FUNC_FOCUS, false);
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
    if (page_index == UI_HOME) refresh_history();
}

void ui_view_key_hints(unsigned page_index, unsigned home_func,
                       bool focus_editing, bool setup_editing)
{
    if (page_index == UI_HOME) {
        ui_key_hints_home(home_func, focus_editing);
    } else {
        ui_key_hints_page(page_index == UI_SETUP, setup_editing);
    }
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
                     (unsigned long)(sample->battery_mv % 1000 / 10),
                     sample->battery_percent);
    } else if (sample->adc.battery.valid) {
        set_text_fmt(view.battery_values, sample->adc.battery.saturated ? "ADC SAT %u" : "ADC RAW %u",
                     (unsigned)sample->adc.battery.raw);
    } else {
        set_text(view.battery_values, "-- V / -- %");
    }
    if (sample->motor_valid) {
        set_text_fmt(view.motor_values, "%lu mA / %u mV",
                     (unsigned long)sample->motor_current_ma,
                     (unsigned)sample->adc.motor.voltage_mv);
    } else if (sample->adc.motor.valid) {
        set_text_fmt(view.motor_values, sample->adc.motor.saturated ? "ADC SAT %u" : "ADC RAW %u",
                     (unsigned)sample->adc.motor.raw);
    } else {
        set_text(view.motor_values, "-- mA / -- mV");
    }
}

static void set_distance_number(unsigned number, ui_distance_state_t state, uint32_t mm)
{
    const bool valid = state == UI_DISTANCE_FINITE;
    const uint64_t tenths = ((uint64_t)mm + 50) / 100;
    const bool fractional = valid && tenths < 100;
    const unsigned value = (unsigned)(fractional ? tenths : ((uint64_t)mm + 500) / 1000);
    const bool large = valid && value <= 99;
    if (view.number_large[number] != large) {
        lv_obj_set_style_text_font(view.number_label[number],
                                   large ? &ui_font_distance : &ui_font_body, 0);
        lv_obj_set_y(view.number_label[number], large ? 0 : 11);
        view.number_large[number] = large;
    }

    if (state == UI_DISTANCE_INFINITY) {
        set_text(view.number_label[number], "INF");
    } else if (state == UI_DISTANCE_NEAR) {
        set_text(view.number_label[number], "NEAR");
    } else if (!valid) {
        set_text(view.number_label[number], "--");
    } else if (!large) {
        /* Future values above two integer digits keep the complete value. */
        set_text_fmt(view.number_label[number], "%u m", value);
    } else if (fractional) {
        set_text_fmt(view.number_label[number], "%u.%u", value / 10, value % 10);
    } else {
        set_text_fmt(view.number_label[number], "%u", value);
    }

    if (large) {
        const int unit_x = fractional ? 63 : 43;
        if (view.unit_x[number] != unit_x) {
            lv_obj_set_x(view.distance_unit[number], unit_x);
            view.unit_x[number] = unit_x;
        }
    }
    if (view.unit_visible[number] != large) {
        if (large) lv_obj_remove_flag(view.distance_unit[number], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(view.distance_unit[number], LV_OBJ_FLAG_HIDDEN);
        view.unit_visible[number] = large;
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

void ui_view_focus_blink(bool visible)
{
    if (view.focus_visible == visible) return;
    view.focus_visible = visible;

    const uint32_t color = visible ? COLOR_ACCENT : COLOR_ACCENT_DIM;
    lv_obj_set_style_text_color(view.number_label[0], lv_color_hex(color), 0);
}

void ui_view_film(unsigned remaining)
{
    if (remaining > FILM_COUNT) remaining = FILM_COUNT;
    if ((int)remaining == view.film_remaining) return;

    for (unsigned i = 0; i < FILM_COUNT; ++i) {
        const bool lit = i < remaining;
        if (view.film_remaining < 0 || lit != (i < (unsigned)view.film_remaining)) {
            lv_image_set_src(view.film_frames[i], lit ? ui_film_lit[i] : ui_film_dim[i]);
        }
    }
    set_text_fmt(view.film_count, "%u", remaining);
    view.film_remaining = (int)remaining;
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
    view.chart_dirty = true;
    if (view.page == UI_HOME) refresh_history();
}

void ui_view_uptime(uint64_t seconds)
{
    if (view.page != UI_HOME) return;
    set_text_fmt(view.uptime_value, "%02lu:%02u:%02u",
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
        lv_obj_set_style_text_color(view.brightness,
                                    lv_color_hex(editing ? COLOR_ACCENT : COLOR_TEXT), 0);
        lv_obj_set_y(view.save_status, editing ? 180 : 155);
    }
    lv_bar_set_value(view.brightness_bar, (int32_t)percent, LV_ANIM_OFF);
    set_text(view.edit_status, editing ? "EDITING" : "");
    set_text(view.save_status, message);
    view.editing = editing;
}
