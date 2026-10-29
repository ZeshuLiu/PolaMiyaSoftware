#include "ui_battery.h"
#include "ui_home_assets.h"

static lv_obj_t *plain(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    return obj;
}

static lv_obj_t *mask(lv_obj_t *parent, int x, int y,
                       const lv_image_dsc_t *source, uint32_t color)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(obj, source);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_style_image_recolor(obj, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

void ui_battery_create(ui_battery_t *battery, lv_obj_t *parent)
{
    /* Replace the old static outline as well as its dynamic fill, leaving the
     * brand and the right-hand key rail untouched. */
    lv_obj_t *root = plain(parent, 124, 7, 30, 19);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x080b0c), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    battery->frame = mask(root, 7, 4, &ui_battery_frame, 0x9eb7ab);
    battery->fill = plain(root, 9, 6, 15, 6);
    lv_obj_set_style_bg_color(battery->fill, lv_color_hex(0xb7e6cb), 0);
    lv_obj_set_style_bg_opa(battery->fill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(battery->fill, 1, 0);
    battery->bolt = mask(root, 1, 4, &ui_battery_bolt, 0xb7e6cb);
    /* The raised checkmark's offset is baked into its 19x10 alpha image. */
    battery->check = mask(root, 7, 4, &ui_battery_check, 0x080b0c);
    battery->slash = mask(root, 7, 4, &ui_battery_slash, 0x6b8278);
    battery->unknown = lv_label_create(root);
    lv_obj_set_pos(battery->unknown, 7, 1);
    lv_obj_set_size(battery->unknown, 19, 16);
    lv_obj_set_style_text_font(battery->unknown, &ui_font_small, 0);
    lv_obj_set_style_text_color(battery->unknown, lv_color_hex(0xffbc75), 0);
    lv_obj_set_style_text_align(battery->unknown, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(battery->unknown, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(battery->unknown, "?");
    battery->state = -1;
    battery->width = -1;
    ui_battery_update(battery, BATTERY_PENDING, -1, 0);
}

void ui_battery_update(ui_battery_t *battery, battery_status_t state,
                        int percent, uint32_t now_ms)
{
    if (state == BATTERY_NORMAL && percent < 0) state = BATTERY_PENDING;
    if (battery->state != (int)state) {
        if (state == BATTERY_CHARGING) battery->animation_started_ms = now_ms;
        const uint32_t outline = state == BATTERY_PENDING ? 0xffbc75 :
                                   state == BATTERY_ABSENT ? 0x6b8278 : 0x9eb7ab;
        lv_obj_set_style_image_recolor(battery->frame, lv_color_hex(outline), 0);
        lv_obj_set_flag(battery->bolt, LV_OBJ_FLAG_HIDDEN, state != BATTERY_CHARGING);
        lv_obj_set_flag(battery->check, LV_OBJ_FLAG_HIDDEN, state != BATTERY_FULL);
        lv_obj_set_flag(battery->slash, LV_OBJ_FLAG_HIDDEN, state != BATTERY_ABSENT);
        lv_obj_set_flag(battery->unknown, LV_OBJ_FLAG_HIDDEN, state != BATTERY_PENDING);
        battery->state = (int)state;
    }
    int width = 0;
    if (state == BATTERY_CHARGING) {
        unsigned step = ((now_ms - battery->animation_started_ms) % 1800) / 240;
        if (step > 6) step = 6;
        width = 3 + (int)step * 2;
    } else if (state == BATTERY_FULL) {
        width = 15;
    } else if (state == BATTERY_NORMAL) {
        width = (15 * percent + 99) / 100;
    }
    if (width != battery->width) {
        lv_obj_set_flag(battery->fill, LV_OBJ_FLAG_HIDDEN, width == 0);
        if (width > 0) lv_obj_set_width(battery->fill, width);
        battery->width = width;
    }
}
