#include "ui_eject.h"
#include "ui_home_assets.h"
#include "ui_battery.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define EJECT_GREEN       0x47f56c
#define EJECT_INK         0x10291b
#define EJECT_HEADER_H    31
#define EJECT_FULL_MA     1500
#define EJECT_CYCLE_MS    2600

static struct {
    lv_obj_t *root;
    lv_obj_t *body;
    ui_battery_t battery;
    lv_obj_t *time;
    lv_obj_t *current;
    lv_obj_t *arc;
    lv_obj_t *tip;
    lv_obj_t *paper;
    int arc_ma;
    bool tip_visible;
    int paper_y;
    lv_opa_t paper_opa;
} eject;

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
}

static lv_obj_t *fill(lv_obj_t *parent, int x, int y, int w, int h,
                       uint32_t color, int radius)
{
    lv_obj_t *obj = box(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, radius, 0);
    return obj;
}

static lv_obj_t *picture(lv_obj_t *parent, int x, int y,
                          const lv_image_dsc_t *source)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    return obj;
}

static lv_obj_t *number(int y, int h, const lv_font_t *font)
{
    lv_obj_t *obj = lv_label_create(eject.body);
    lv_obj_set_pos(obj, 12, y - EJECT_HEADER_H);
    lv_obj_set_size(obj, 132, h);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(EJECT_INK), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(obj, "");
    return obj;
}

static void text(lv_obj_t *obj, const char *value)
{
    if (strcmp(lv_label_get_text(obj), value) != 0) lv_label_set_text(obj, value);
}

lv_obj_t *ui_eject_create(lv_obj_t *screen)
{
    eject.root = fill(screen, 0, 0, 172, 320, EJECT_GREEN, 0);
    eject.body = fill(eject.root, 0, EJECT_HEADER_H, 172, 320 - EJECT_HEADER_H,
                       EJECT_GREEN, 0);
    /* Static captions/ticks are small RGB565 bands, not a full-screen bitmap. */
    for (unsigned i = 0; i < sizeof(ui_eject_tiles) / sizeof(ui_eject_tiles[0]); ++i) {
        picture(eject.body, ui_eject_tiles[i].x,
                 ui_eject_tiles[i].y - EJECT_HEADER_H, ui_eject_tiles[i].image);
    }
    lv_obj_t *rail = fill(eject.body, 155, 0, 17, 289, EJECT_INK, 0);
    lv_obj_set_style_bg_opa(rail, 12, 0);
    fill(eject.body, 157, 84 - EJECT_HEADER_H, 13, 1, 0x2c9e47, 0);
    fill(eject.body, 157, 236 - EJECT_HEADER_H, 13, 1, 0x2c9e47, 0);

    eject.arc = lv_arc_create(eject.body);
    lv_obj_remove_style_all(eject.arc);
    lv_obj_remove_flag(eject.arc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_state(eject.arc, LV_STATE_FOCUSED);
    /* 121 px outer diameter minus the 7 px stroke gives a 57 px center radius. */
    lv_obj_set_pos(eject.arc, 18, 105 - EJECT_HEADER_H);
    lv_obj_set_size(eject.arc, 121, 121);
    lv_arc_set_bg_angles(eject.arc, 135, 45);
    lv_arc_set_range(eject.arc, 0, EJECT_FULL_MA);
    lv_obj_set_style_arc_width(eject.arc, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_width(eject.arc, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(eject.arc, lv_color_hex(0x3bd95f), LV_PART_MAIN);
    lv_obj_set_style_arc_color(eject.arc, lv_color_hex(EJECT_INK), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(eject.arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(eject.arc, true, LV_PART_INDICATOR);
    eject.tip = fill(eject.body, 0, 0, 4, 4, 0xb9ffd0, LV_RADIUS_CIRCLE);
    lv_obj_add_flag(eject.tip, LV_OBJ_FLAG_HIDDEN);
    eject.tip_visible = false;
    eject.arc_ma = -1;

    eject.time = number(54, 36, &ui_font_eject_time);
    eject.current = number(143, 50, &ui_font_eject_current);

    fill(eject.body, 43, 240 - EJECT_HEADER_H, 70, 6, EJECT_INK, 3);
    /* The viewport clips the paper at the slot; the single image moves down. */
    lv_obj_t *window = box(eject.body, 43, 246 - EJECT_HEADER_H, 70, 67);
    eject.paper = picture(window, 14, -42, &ui_eject_paper);
    eject.paper_y = -42;
    eject.paper_opa = LV_OPA_COVER;
    fill(eject.root, 0, 0, 172, EJECT_HEADER_H, 0x080b0c, 0);
    picture(eject.root, 12, 13, &ui_header_brand);
    ui_battery_create(&eject.battery, eject.root);
    lv_obj_add_flag(eject.root, LV_OBJ_FLAG_HIDDEN);
    return eject.root;
}

void ui_eject_update(uint64_t elapsed_ms, const power_sample_t *sample)
{
    char value[32];
    snprintf(value, sizeof(value), "%02lu:%02u.%u",
             (unsigned long)(elapsed_ms / 60000),
             (unsigned)(elapsed_ms / 1000 % 60), (unsigned)(elapsed_ms / 100 % 10));
    /* Beyond 99 minutes retain the full time in the existing smaller font. */
    const lv_font_t *time_font = elapsed_ms < 6000000 ? &ui_font_eject_time : &ui_font_eject_fallback;
    if (lv_obj_get_style_text_font(eject.time, 0) != time_font) {
        lv_obj_set_style_text_font(eject.time, time_font, 0);
    }
    text(eject.time, value);

    int arc_ma = 0;
    if (sample->motor_valid) {
        const uint32_t ma = sample->motor_current_ma;
        if (ma < 1000) {
            snprintf(value, sizeof(value), ".%03lu", (unsigned long)ma);
        } else {
            const uint64_t hundredths = ((uint64_t)ma + 5) / 10;
            snprintf(value, sizeof(value), "%lu.%02u", (unsigned long)(hundredths / 100),
                     (unsigned)(hundredths % 100));
        }
        /* Clamp only the arc; the numeric reading always retains the full value. */
        arc_ma = ma > EJECT_FULL_MA ? EJECT_FULL_MA : (int)ma;
    } else {
        /* An invalid or saturated ADC window is not a measured zero. */
        snprintf(value, sizeof(value), "--");
    }
    const lv_font_t *font = lv_text_get_width(value, strlen(value),
                                               &ui_font_eject_current, 0) <= 108
                                ? &ui_font_eject_current : &ui_font_body;
    if (lv_obj_get_style_text_font(eject.current, 0) != font) {
        lv_obj_set_style_text_font(eject.current, font, 0);
        lv_obj_set_y(eject.current, (font == &ui_font_body ? 164 : 143) - EJECT_HEADER_H);
    }
    text(eject.current, value);
    const bool tip_visible = sample->motor_valid && arc_ma > 0;
    if (tip_visible != eject.tip_visible) {
        lv_obj_set_flag(eject.tip, LV_OBJ_FLAG_HIDDEN, !tip_visible);
        eject.tip_visible = tip_visible;
    }
    if (arc_ma != eject.arc_ma) {
        lv_arc_set_value(eject.arc, arc_ma);
        const float angle = (135.0f + 270.0f * arc_ma / EJECT_FULL_MA) * 0.01745329252f;
        lv_obj_set_pos(eject.tip, (int)lroundf(78 + 57 * cosf(angle)) - 2,
                       (int)lroundf(165 + 57 * sinf(angle)) - 2 - EJECT_HEADER_H);
        eject.arc_ma = arc_ma;
    }

    /* 1.872 s travel, hold, then fade before the next paper. This is a running
     * indicator, not a film position sensor or a remaining-shot counter. */
    const unsigned phase = (unsigned)(elapsed_ms % EJECT_CYCLE_MS);
    const int y = -42 + (phase < 1872 ? (int)(59 * phase / 1872) : 59);
    const lv_opa_t opa = phase < 2132 ? LV_OPA_COVER :
                            (lv_opa_t)(LV_OPA_COVER * (EJECT_CYCLE_MS - phase) / 468);
    if (y != eject.paper_y) {
        lv_obj_set_y(eject.paper, y);
        eject.paper_y = y;
    }
    if (opa != eject.paper_opa) {
        lv_obj_set_style_image_opa(eject.paper, opa, 0);
        eject.paper_opa = opa;
    }
}

void ui_eject_battery(battery_status_t state, int percent)
{
    ui_battery_update(&eject.battery, state, percent);
}
