#include "ui_key_hints.h"

#define BACKGROUND 0x080b0c
#define ICON_COLOR 0xd8f5e7
#define DONE_COLOR 0xb1ffd3
#define ICON_X 157
#define EDGE_COLOR 0x0c1e17
#define EDGE_BORDER_COLOR 0x1b3427
#define SEPARATOR_COLOR 0x63977d

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
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
}

static void ring(lv_obj_t *parent, int x, int y, int size)
{
    lv_obj_t *obj = plain_object(parent, x, y, size, size);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(ICON_COLOR), 0);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
}

static void edge_gradient(lv_obj_t *screen, int x, int width, uint32_t color)
{
    for (unsigned half = 0; half < 2; ++half) {
        lv_obj_t *obj = plain_object(screen, x, 20 + half * 140, width, 140);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(half ? color : BACKGROUND), 0);
        lv_obj_set_style_bg_grad_color(obj, lv_color_hex(half ? BACKGROUND : color), 0);
        lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    }
}

static void create_edge_fade(lv_obj_t *screen)
{
    /* v11: continuous 17 px rail, with 20 px clear at each end.
     * Only vertical gradients are needed; side edges remain straight. */
    edge_gradient(screen, 155, 17, EDGE_COLOR);
    edge_gradient(screen, 155, 1, EDGE_BORDER_COLOR);
    edge_gradient(screen, 171, 1, EDGE_BORDER_COLOR);
    /* Double the middle section from 76 to 152 px around y=160. */
    const int separators[] = {84, 236};
    for (unsigned i = 0; i < sizeof(separators) / sizeof(separators[0]); ++i) {
        lv_obj_t *obj = plain_object(screen, 157, separators[i], 13, 1);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(SEPARATOR_COLOR), 0);
    }
}

void ui_key_hints_create(lv_obj_t *screen)
{
    create_edge_fade(screen);
    /* Centers are provisional until measured on the assembled keypad. */
    const int centers[] = {160, 160, 160, 54, 54, 266, 266};
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
        {{2, 2}, {3, 3}}, {{11, 11}, {12, 12}}, {{2, 12}, {3, 11}}, {{11, 3}, {12, 2}},
    };
    for (unsigned i = 0; i < sizeof(spokes) / sizeof(spokes[0]); ++i) {
        stroke(icons[GEAR], spokes[i], 2, ICON_COLOR);
    }
    ring(icons[GEAR], 3, 3, 8);
    static const lv_point_precise_t sliders[][2] = {
        {{3, 2}, {3, 12}}, {{10, 2}, {10, 12}},
        {{1, 5}, {5, 5}}, {{8, 9}, {12, 9}},
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
