#include "ui_key_hints.h"

#define BACKGROUND 0x080b0c
#define ICON_COLOR 0xb0c7bc
#define DONE_COLOR 0xb1ffd3
#define ICON_X 157

enum { GEAR, TUNE, DONE, NEXT, PLUS, PREVIOUS, MINUS, ICON_COUNT };
static lv_obj_t *icons[ICON_COUNT];
static int current_mode = -1;

static lv_obj_t *plain_object(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    return obj;
}

static void stroke(lv_obj_t *parent, const lv_point_precise_t *points, unsigned count,
                    uint32_t color)
{
    /* Point arrays below are static; LVGL retains their address. */
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(line, 1, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

static void ring(lv_obj_t *parent, int x, int y, int size)
{
    lv_obj_t *obj = plain_object(parent, x, y, size, size);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(ICON_COLOR), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
}

static void create_edge_fade(lv_obj_t *screen)
{
    /* 24 one-pixel columns make a faint horizontal fade. Each column uses two
     * ordinary LVGL vertical gradients, so both ends merge into the black page.
     * No permanent rail, divider, image buffer or complex gradient is required. */
    for (unsigned x = 0; x < 24; ++x) {
        const unsigned weight = x * x;
        const unsigned denominator = 23 * 23;
        const uint32_t color = ((8 + 7 * weight / denominator) << 16) |
                               ((11 + 13 * weight / denominator) << 8) |
                               (12 + 9 * weight / denominator);
        for (unsigned half = 0; half < 2; ++half) {
            lv_obj_t *obj = plain_object(screen, 148 + x, half * 160, 1, 160);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(obj, lv_color_hex(half ? color : BACKGROUND), 0);
            lv_obj_set_style_bg_grad_color(obj, lv_color_hex(half ? BACKGROUND : color), 0);
            lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
        }
    }
}

void ui_key_hints_create(lv_obj_t *screen)
{
    create_edge_fade(screen);
    /* Centers are provisional until measured on the assembled keypad. */
    const int centers[] = {160, 160, 160, 84, 84, 236, 236};
    for (unsigned i = 0; i < ICON_COUNT; ++i) {
        icons[i] = plain_object(screen, ICON_X, centers[i] - 7, 14, 14);
    }
    static const lv_point_precise_t left[] = {{9, 4}, {6, 7}, {9, 10}};
    static const lv_point_precise_t right[] = {{5, 4}, {8, 7}, {5, 10}};
    static const lv_point_precise_t horizontal[] = {{3, 7}, {11, 7}};
    static const lv_point_precise_t vertical[] = {{7, 3}, {7, 11}};
    static const lv_point_precise_t check[] = {{3, 7}, {6, 10}, {11, 4}};
    stroke(icons[PREVIOUS], left, 3, ICON_COLOR);
    stroke(icons[NEXT], right, 3, ICON_COLOR);
    stroke(icons[MINUS], horizontal, 2, ICON_COLOR);
    stroke(icons[PLUS], horizontal, 2, ICON_COLOR);
    stroke(icons[PLUS], vertical, 2, ICON_COLOR);
    stroke(icons[DONE], check, 3, DONE_COLOR);

    static const lv_point_precise_t spokes[][2] = {
        {{7, 1}, {7, 3}}, {{7, 11}, {7, 13}}, {{1, 7}, {3, 7}}, {{11, 7}, {13, 7}},
        {{2, 2}, {4, 4}}, {{10, 10}, {12, 12}}, {{2, 12}, {4, 10}}, {{10, 4}, {12, 2}},
    };
    for (unsigned i = 0; i < sizeof(spokes) / sizeof(spokes[0]); ++i) {
        stroke(icons[GEAR], spokes[i], 2, ICON_COLOR);
    }
    ring(icons[GEAR], 3, 3, 8);
    ring(icons[GEAR], 5, 5, 4);
    static const lv_point_precise_t sliders[][2] = {
        {{3, 3}, {3, 11}}, {{7, 3}, {7, 11}}, {{11, 3}, {11, 11}},
        {{1, 5}, {5, 5}}, {{5, 9}, {9, 9}}, {{9, 4}, {13, 4}},
    };
    for (unsigned i = 0; i < sizeof(sliders) / sizeof(sliders[0]); ++i) {
        stroke(icons[TUNE], sliders[i], 2, ICON_COLOR);
    }
    current_mode = -1;
    ui_key_hints_update(false, false);
}

void ui_key_hints_update(bool setup_page, bool editing)
{
    const int mode = setup_page ? (editing ? 2 : 1) : 0;
    if (current_mode == mode) return;
    const unsigned top = mode == 2 ? PLUS : NEXT;
    const unsigned middle = mode == 2 ? DONE : mode == 1 ? TUNE : GEAR;
    const unsigned bottom = mode == 2 ? MINUS : PREVIOUS;
    for (unsigned i = 0; i < ICON_COUNT; ++i) {
        if (i == top || i == middle || i == bottom) {
            lv_obj_remove_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(icons[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    current_mode = mode;
}
