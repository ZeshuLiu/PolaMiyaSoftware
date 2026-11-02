#include "ui_view.h"
#include "ui_key_hints.h"
#include "ui_home_assets.h"
#include "ui_eject.h"
#include "ui_settings.h"
#include "ui_battery.h"

#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define COLOR_BG            0x080f10
#define COLOR_TEXT          0xf1f5f4
#define COLOR_MUTED         0x9caeab
#define COLOR_ACCENT        0x79f5a7
#define COLOR_ACCENT_DIM    0x477e5d
#define COLOR_ACTUAL        0x69d6ff
#define COLOR_GRID          0x1e2e26

#define FILM_COUNT          8
#define FILM_FRAME_Y        291
#define SCALE_ZERO_X        14
#define SCALE_20M_X         103
#define SCALE_INFINITY_X    121
#define DISTANCE_UNIT_WIDTH 14
#define DISTANCE_UNIT_GAP   3

/* INFO-only compact fallback; HOME text must stay at least 14 px. */
#define FONT_INFO_COMPACT   (&ui_font_info_compact)

/* NEAR belongs only to the measured distance, not the FocusUnit interface. */
typedef enum {
    UI_DISTANCE_UNAVAILABLE,
    UI_DISTANCE_FINITE,
    UI_DISTANCE_INFINITY,
    UI_DISTANCE_NEAR,
} ui_distance_state_t;

static struct {
    lv_obj_t *pages[UI_PAGE_COUNT];
    ui_battery_t battery;
    lv_obj_t *battery_values;
    lv_obj_t *motor_values;
    lv_obj_t *number_label[2];
    lv_obj_t *distance_unit[2];
    bool unit_visible[2];
    int unit_x[2];
    lv_obj_t *focus_card;
    lv_obj_t *focus_notice;
    lv_obj_t *focus_pointer;
    lv_obj_t *actual_pointer;
    lv_obj_t *uptime_value;
    lv_obj_t *film_count;
    lv_obj_t *film_frames[FILM_COUNT];
    int film_remaining;
    lv_obj_t *counts[2];
    lv_obj_t *motor_time;
    lv_obj_t *focus_status;
    lv_obj_t *focus_position;
    lv_obj_t *storage;
    unsigned page;
    uint32_t focus_text_color;
    uint8_t focus_text_opacity;
    bool focus_editing;
} view;

static void set_text(lv_obj_t *obj, const char *value)
{
    /* Unchanged text must not invalidate the label or allocate again. */
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

static void fit_info_value(lv_obj_t *obj)
{
    /* A prefixed uint64 counter fits the 120 px row at 9 px. Keep normal
     * readings at 14 px and retain every digit without scrolling. */
    const char *text = lv_label_get_text(obj);
    const lv_font_t *font = lv_text_get_width(text, (uint32_t)strlen(text),
                                              &ui_font_body_bold, 0) > 120
                                ? FONT_INFO_COMPACT : &ui_font_body_bold;
    if (lv_obj_get_style_text_font(obj, 0) != font) {
        lv_obj_set_style_text_font(obj, font, 0);
    }
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
        label_font(obj, 12, 5, 120, title, COLOR_TEXT, &ui_font_body_bold);
    }
    rectangle(obj, 12, 30, 116, 1, COLOR_GRID);
    if (footer) {
        lv_obj_t *index = label(obj, 12, 301, 132, footer, COLOR_MUTED);
        lv_obj_set_style_text_align(index, LV_TEXT_ALIGN_CENTER, 0);
    }
    return obj;
}

static void create_distance_number(lv_obj_t *parent, unsigned number, int x, int y,
                                    uint32_t color)
{
    lv_obj_t *container = plain_object(parent, x, y, 120, 58);
    view.number_label[number] = label_font(container, 0, 0, 120, "", color,
                                           &ui_font_distance);
    lv_obj_set_height(view.number_label[number], 58);
    lv_label_set_long_mode(view.number_label[number], LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);

    /* 20 px unit shares the distance baseline at container y=48. */
    view.distance_unit[number] = label_font(container, 0, 27, DISTANCE_UNIT_WIDTH, "m",
                                            color,
                                            &ui_font_unit);
    view.unit_x[number] = 0;
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
    return image(parent, SCALE_ZERO_X - 4, upper ? 220 : 237,
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
    view.film_count = label_font(parent, 100, 261, 24, "0", COLOR_TEXT,
                                 &ui_font_count);
    lv_obj_set_style_text_color(view.film_count, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_text_align(view.film_count, LV_TEXT_ALIGN_RIGHT, 0);

    /* Tiles partition the original strip in its blank gaps, retaining each
     * paper's fractional edges. Row padding is stored but never displayed. */
    static const int frame_x[FILM_COUNT] = {0, 13, 27, 41, 55, 70, 84, 98};
    lv_obj_t *strip = plain_object(parent, 12, FILM_FRAME_Y, 114, 15);
    for (unsigned i = 0; i < FILM_COUNT; ++i) {
        view.film_frames[i] = image(strip, frame_x[i], 0, ui_film_dim[i]);
    }
    view.film_remaining = -1;
}

void ui_view_create(lv_display_t *display)
{
    view.page = UI_PAGE_COUNT; /* First show must hide the other pages. */
    view.focus_text_color = UINT32_MAX;
    view.focus_text_opacity = 0;
    view.film_remaining = -1;

    lv_obj_t *screen = lv_display_get_screen_active(display);
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(screen, &ui_font_body, 0);

    lv_obj_t *home = view.pages[UI_HOME] = rectangle(screen, 0, 0, 172, 320, COLOR_BG);
    /* Static artwork is exported from the approved SVG. Both distance labels
     * and live values use the same on-board font and separate objects. */
    for (unsigned i = 0; i < sizeof(ui_home_tiles) / sizeof(ui_home_tiles[0]); ++i) {
        image(home, ui_home_tiles[i].x, ui_home_tiles[i].y, ui_home_tiles[i].image);
    }
    ui_battery_create(&view.battery, home);
    lv_obj_set_x(lv_obj_get_parent(view.battery.frame), 132);
    lv_obj_set_style_bg_color(lv_obj_get_parent(view.battery.frame), lv_color_hex(COLOR_BG), 0);

    view.focus_card = rectangle(home, 8, 130, 120, 85, COLOR_ACCENT);
    lv_obj_set_style_radius(view.focus_card, 4, 0);
    label_font(home, 16, 36, 100, "ACTUAL", COLOR_ACTUAL, &ui_font_home_title);
    label_font(home, 16, 134, 100, "FOCUS", 0x09291e, &ui_font_home_title);
    create_distance_number(home, 0, 8, 155, 0x09291e);
    create_distance_number(home, 1, 8, 60, COLOR_ACTUAL);
    create_distance_scale(home);
    /* Keep status inside the card, below the large I/E/C marker. The scale
     * labels occupy y=245..258, so they must not share a status row. */
    view.focus_notice = label_font(home, 12, 194, 112, "", 0x09291e, &ui_font_body);
    lv_obj_set_style_text_align(view.focus_notice, LV_TEXT_ALIGN_CENTER, 0);
    create_film_strip(home);

    lv_obj_t *info = view.pages[UI_INFO] = page(screen, "DEVICE INFO", NULL);
    lv_obj_t *uptime_card = rectangle(info, 8, 32, 120, 55, COLOR_ACCENT);
    lv_obj_set_style_radius(uptime_card, 4, 0);
    label_font(info, 16, 35, 104, "UPTIME", 0x09291e, &ui_font_info_label);
    view.uptime_value = label_font(info, 16, 56, 110, "00:00:00", 0x09291e,
                                   &ui_font_uptime);
    label_font(info, 12, 96, 120, "BATTERY", COLOR_MUTED, &ui_font_info_label);
    view.battery_values = label(info, 12, 112, 120, "-- V / -- %", COLOR_TEXT);
    label_font(info, 12, 134, 120, "MOTOR / IPROPI", COLOR_MUTED, &ui_font_info_label);
    view.motor_values = label(info, 12, 150, 120, "-- mA / -- mV", COLOR_TEXT);
    label_font(info, 12, 172, 120, "REBOOTS / SHOTS", COLOR_MUTED, &ui_font_info_label);
    view.counts[0] = label(info, 12, 188, 120, "R --", COLOR_TEXT);
    view.counts[1] = label(info, 12, 204, 120, "S --", COLOR_TEXT);
    label_font(info, 12, 222, 120, "MOTOR TOTAL", COLOR_MUTED, &ui_font_info_label);
    view.motor_time = label(info, 12, 238, 120, "--", COLOR_TEXT);
    view.focus_status = label_font(info, 12, 257, 120, "F:COMM", COLOR_MUTED, &ui_font_info_label);
    view.focus_position = label_font(info, 12, 274, 120, "FOCUS --", COLOR_TEXT, &ui_font_info_label);
    view.storage = label_font(info, 12, 298, 120, "EEPROM --", COLOR_MUTED, &ui_font_info_label);
    lv_obj_t *info_values[] = {view.battery_values, view.motor_values, view.counts[0],
                               view.counts[1], view.motor_time, view.focus_status, view.focus_position, view.storage};
    for (unsigned i = 0; i < sizeof(info_values) / sizeof(info_values[0]); ++i) {
        lv_label_set_long_mode(info_values[i], LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_height(info_values[i], 18);
    }

    view.pages[UI_SETUP] = ui_settings_create(screen);

    view.pages[UI_EJECT] = ui_eject_create(screen);

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
}

void ui_view_key_hints(unsigned page_index, unsigned home_func,
                       bool home_selected)
{
    const bool focus_editing = page_index == UI_HOME && home_selected && home_func == UI_HOME_FUNC_FOCUS;
    if (view.focus_editing != focus_editing) {
        lv_obj_set_style_bg_color(view.focus_card, lv_color_hex(focus_editing ? 0xffbe58 : COLOR_ACCENT), 0);
        view.focus_editing = focus_editing;
    }
    if (page_index == UI_EJECT) {
        ui_key_hints_eject();
    } else if (page_index == UI_HOME) {
        ui_key_hints_home(home_func, home_selected);
    } else if (page_index == UI_INFO) {
        ui_key_hints_info();
    }
}

void ui_view_eject(uint64_t elapsed_ms, const power_sample_t *sample)
{
    if (view.page == UI_EJECT) ui_eject_update(elapsed_ms, sample);
}

void ui_view_power(const power_sample_t *sample, battery_status_t battery)
{
    if (view.page == UI_HOME) {
        ui_battery_update(&view.battery, battery, sample->battery_percent);
    }
    else if (view.page == UI_EJECT) ui_eject_battery(battery, sample->battery_percent);
    if (view.page != UI_INFO) return;
    if (battery == BATTERY_ABSENT) {
        set_text(view.battery_values, "NO BATTERY");
    } else if (sample->battery_valid && battery != BATTERY_NORMAL) {
        const char *state = battery == BATTERY_CHARGING ? "CHG" :
                            battery == BATTERY_FULL ? "FULL" : "?";
        set_text_fmt(view.battery_values, "%lu.%02luV / %s",
                     (unsigned long)(sample->battery_mv / 1000),
                     (unsigned long)(sample->battery_mv % 1000 / 10), state);
    } else if (sample->battery_valid) {
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
    fit_info_value(view.battery_values);
    fit_info_value(view.motor_values);
}

static void set_distance_number(unsigned number, ui_distance_state_t state, uint32_t mm)
{
    const bool valid = state == UI_DISTANCE_FINITE;
    const uint64_t hundredths = ((uint64_t)mm + 5) / 10;
    const bool submeter = valid && hundredths < 100;
    const uint64_t tenths = ((uint64_t)mm + 50) / 100;
    const bool fractional = valid && tenths < 100;
    const unsigned value = (unsigned)(fractional ? tenths : ((uint64_t)mm + 500) / 1000);
    const bool large = valid && value <= 99;
    const lv_font_t *font = large ? &ui_font_distance :
                              valid ? &ui_font_body : &ui_font_distance_status;
    if (lv_obj_get_style_text_font(view.number_label[number], 0) != font) {
        lv_obj_set_style_text_font(view.number_label[number],
                                   font, 0);
        lv_obj_set_y(view.number_label[number], large ? 0 : valid ? 32 : 8);
        lv_obj_set_style_text_align(view.number_label[number],
                                    valid ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0);
    }

    if (state == UI_DISTANCE_INFINITY) {
        set_text(view.number_label[number], "INF");
    } else if (state == UI_DISTANCE_NEAR) {
        set_text(view.number_label[number], "NEAR");
    } else if (!valid) {
        set_text(view.number_label[number], "--");
    } else if (!large) {
        /* Future values above two integer digits keep the complete value. */
        set_text_fmt(view.number_label[number], "%u", value);
    } else if (submeter) {
        set_text_fmt(view.number_label[number], ".%02u", (unsigned)hundredths);
    } else if (fractional) {
        set_text_fmt(view.number_label[number], "%u.%u", value / 10, value % 10);
    } else {
        set_text_fmt(view.number_label[number], "%u", value);
    }

    int number_x = 0;
    if (valid) {
        /* Space Mono digits keep equal widths; the decimal has a separately
         * narrowed advance. Center number and the larger independent unit. */
        const char *text = lv_label_get_text(view.number_label[number]);
        const int width = lv_text_get_width(text, (uint32_t)strlen(text), font, 0);
        number_x = (120 - (width + DISTANCE_UNIT_GAP + DISTANCE_UNIT_WIDTH) + 1) / 2;
        const int unit_x = number_x + width + DISTANCE_UNIT_GAP;
        if (view.unit_x[number] != unit_x) {
            lv_obj_set_x(view.distance_unit[number], unit_x);
            view.unit_x[number] = unit_x;
        }
    }
    if (lv_obj_get_x(view.number_label[number]) != number_x) {
        lv_obj_set_x(view.number_label[number], number_x);
        lv_obj_set_width(view.number_label[number], 120 - number_x);
    }
    if (view.unit_visible[number] != valid) {
        if (valid) lv_obj_remove_flag(view.distance_unit[number], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(view.distance_unit[number], LV_OBJ_FLAG_HIDDEN);
        view.unit_visible[number] = valid;
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

static void set_focus_number(const focus_distance_t *focus)
{
    if (focus->state == FOCUS_DISTANCE_READY) {
        const ui_distance_state_t state = !focus->distance_valid ? UI_DISTANCE_UNAVAILABLE :
                                          focus->infinity ? UI_DISTANCE_INFINITY : UI_DISTANCE_FINITE;
        set_distance_number(0, state, focus->distance_mm);
        return;
    }
    lv_obj_t *number = view.number_label[0];
    const char *marker = focus->state == FOCUS_DISTANCE_INITIALIZING ? "I" :
                         focus->state == FOCUS_DISTANCE_ERROR ? "E" : "C";
    if (lv_obj_get_style_text_font(number, 0) != &ui_font_distance_status)
        lv_obj_set_style_text_font(number, &ui_font_distance_status, 0);
    lv_obj_set_y(number, 8);
    lv_obj_set_x(number, 0);
    lv_obj_set_width(number, 120);
    lv_obj_set_style_text_align(number, LV_TEXT_ALIGN_CENTER, 0);
    set_text(number, marker);
    if (view.unit_visible[0]) {
        lv_obj_add_flag(view.distance_unit[0], LV_OBJ_FLAG_HIDDEN);
        view.unit_visible[0] = false;
    }
}

void ui_view_range(const focus_distance_t *focus, const sdm50_sample_t *actual)
{
    if (view.page != UI_HOME) return;
    const ui_distance_state_t actual_state = actual->status == SDM50_VALID ? UI_DISTANCE_FINITE :
                                            actual->status == SDM50_TOO_CLOSE ? UI_DISTANCE_NEAR :
                                            actual->status == SDM50_NO_TARGET ? UI_DISTANCE_INFINITY :
                                                                               UI_DISTANCE_UNAVAILABLE;
    set_focus_number(focus);
    set_distance_number(1, actual_state, actual->distance_mm);
    const ui_distance_state_t focus_state = focus->state != FOCUS_DISTANCE_READY || !focus->distance_valid ?
        UI_DISTANCE_UNAVAILABLE : focus->infinity ? UI_DISTANCE_INFINITY : UI_DISTANCE_FINITE;
    set_distance_pointer(view.focus_pointer, focus_state, focus->distance_mm);
    set_distance_pointer(view.actual_pointer, actual_state, actual->distance_mm);
}

void ui_view_focus_style(bool editing, bool automatic, bool too_close, bool ready, bool blink_on)
{
    if (view.page != UI_HOME) return;
    const uint32_t color = ready && automatic ? (too_close ? 0xa00010 : 0x004080) : 0x09291e;
    const uint8_t opacity = ready && too_close && automatic && !blink_on ? LV_OPA_TRANSP :
                            ready && editing && !blink_on ? LV_OPA_40 : LV_OPA_COVER;
    if (view.focus_text_color != color) {
        lv_obj_set_style_text_color(view.number_label[0], lv_color_hex(color), 0);
        lv_obj_set_style_text_color(view.distance_unit[0], lv_color_hex(color), 0);
        view.focus_text_color = color;
    }
    if (view.focus_text_opacity != opacity) {
        lv_obj_set_style_text_opa(view.number_label[0], opacity, 0);
        lv_obj_set_style_text_opa(view.distance_unit[0], opacity, 0);
        view.focus_text_opacity = opacity;
    }
}

void ui_view_film(unsigned remaining)
{
    if (view.page != UI_HOME) return;
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

void ui_view_uptime(uint64_t seconds)
{
    if (view.page != UI_INFO) return;
    set_text_fmt(view.uptime_value, "%02lu:%02u:%02u",
                 (unsigned long)(seconds / 3600),
                 (unsigned)(seconds / 60 % 60), (unsigned)(seconds % 60));
    const char *value = lv_label_get_text(view.uptime_value);
    const lv_font_t *font = lv_text_get_width(value, (uint32_t)strlen(value), &ui_font_uptime, 0) <= 110
                                ? &ui_font_uptime :
                                  lv_text_get_width(value, (uint32_t)strlen(value), &ui_font_body, 0) <= 110
                                      ? &ui_font_body : FONT_INFO_COMPACT;
    if (lv_obj_get_style_text_font(view.uptime_value, 0) != font)
        lv_obj_set_style_text_font(view.uptime_value, font, 0);
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
    /* Preserve exact persisted counters in fixed, non-scrolling rows. */
    set_text_fmt(view.counts[0], "R %" PRIu64, values->reboot_count);
    set_text_fmt(view.counts[1], "S %" PRIu64, values->shutter_count);
    set_text_fmt(view.motor_time, "%" PRIu64 " s", values->motor_run_ms / 1000);
    fit_info_value(view.counts[0]);
    fit_info_value(view.counts[1]);
    fit_info_value(view.motor_time);
    set_text(view.storage, !status->writable ? "EEPROM ERROR" :
                      status->dirty ? "RAM UNSAVED" :
                      status->loaded_from_eeprom ? "EEPROM OK" : "RAM DEFAULTS");
}

void ui_view_focus_info(const focus_distance_t *focus, const focus_unit_snapshot_t *snapshot)
{
    if (view.page == UI_HOME) {
        set_text(view.focus_notice, snapshot->storage_error ? "SAVE ERROR" :
                 focus->state == FOCUS_DISTANCE_COMMUNICATION ? "LINK LOST" :
                 snapshot->seek_zero ? "HOMING" :
                 focus->state == FOCUS_DISTANCE_ERROR ? "FOCUS ERROR" :
                 snapshot->initialization_required ? "INIT NEEDED" : "");
    }
    if (view.page != UI_INFO) return;
    const char *state = snapshot->storage_error ? "F:SAVE ERROR" :
                        focus->state == FOCUS_DISTANCE_COMMUNICATION ? "F:COMM" :
                        focus->state == FOCUS_DISTANCE_INITIALIZING ?
                            (snapshot->command_rejected && snapshot->command_result == 3 ? "F:BUSY" : "F:INIT") :
                        focus->state == FOCUS_DISTANCE_ERROR ?
                            (snapshot->valid && snapshot->move_state == 3 ? "F:STALLED" : "F:ERROR") :
                        snapshot->mode == 4 && snapshot->move_state == 1 ? "F:MOVING" :
                        snapshot->mode == 4 && snapshot->move_state == 2 ? "F:REACHED" :
                        snapshot->mode == 1 ? "F:GLIDE" :
                        snapshot->mode == 2 ? "F:OPEN" : "F:READY";
    set_text(view.focus_status, state);
    uint32_t distance_mm = 0;
    bool infinity = false;
    if (focus->state != FOCUS_DISTANCE_READY ||
        !focus_distance_from_raw(snapshot->position, &distance_mm, &infinity)) {
        set_text(view.focus_position, "FOCUS --");
    } else if (infinity) {
        set_text(view.focus_position, "FOCUS INF");
    } else {
        const uint64_t hundredths = ((uint64_t)distance_mm + 5U) / 10U;
        set_text_fmt(view.focus_position, "FOCUS %" PRIu64 ".%02u m",
                     hundredths / 100U, (unsigned)(hundredths % 100U));
    }
    const char *text = lv_label_get_text(view.focus_position);
    const lv_font_t *font = lv_text_get_width(text, (uint32_t)strlen(text), &ui_font_info_label, 0) > 120
                                ? FONT_INFO_COMPACT : &ui_font_info_label;
    if (lv_obj_get_style_text_font(view.focus_position, 0) != font)
        lv_obj_set_style_text_font(view.focus_position, font, 0);
}
