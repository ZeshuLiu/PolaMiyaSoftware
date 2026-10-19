#include "ui_key_hints.h"
#include "ui_home_assets.h"

#include <stdbool.h>

#define ICON_COLOR          0xd8f5e7
#define CANDIDATE_COLOR     0x4a6a5b
#define SELECTED_COLOR      0xffa24d
#define DONE_COLOR          0xb1ffd3
#define ARROW_COLOR         0xbfd6c9
#define ICON_X              157

enum { FUNC_SETTINGS, FUNC_FOCUS, FUNC_INFO, FUNC_COUNT };
enum { PAGE_GEAR, PAGE_TUNE, PAGE_INFO, PAGE_PLUS, PAGE_DONE, PAGE_MINUS,
       PAGE_ICON_COUNT };

typedef struct {
    lv_obj_t *slot;
    lv_obj_t *icons[FUNC_COUNT];
} home_slot_t;

static home_slot_t home_slots[3];
static lv_obj_t *page_icons[PAGE_ICON_COUNT];
static lv_obj_t *arrow_up;
static lv_obj_t *arrow_down;
static int last_mode = -1;
static unsigned last_func;
static bool last_edit;

static lv_obj_t *plain_object(lv_obj_t *parent, int x, int y, int width, int height)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, width, height);
    return obj;
}

static lv_obj_t *stroke(lv_obj_t *parent, const lv_point_precise_t *points,
                        unsigned count, int width, uint32_t color)
{
    /* Point arrays are static; LVGL retains their address. */
    lv_obj_t *line = lv_line_create(parent);
    lv_obj_remove_style_all(line);
    lv_line_set_points(line, points, count);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

static void show_obj(lv_obj_t *obj, bool visible)
{
    if (visible) {
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_icon_color(lv_obj_t *obj, uint32_t color)
{
    /* Each HOME icon group contains one white alpha image. */
    lv_obj_t *mask = lv_obj_get_child(obj, 0);
    lv_obj_set_style_image_recolor(mask, lv_color_hex(color), 0);
}

static lv_obj_t *icon_image(lv_obj_t *parent, const lv_image_dsc_t *source,
                             uint32_t color)
{
    lv_obj_t *obj = lv_image_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(obj, source);
    lv_obj_set_style_image_recolor(obj, lv_color_hex(color), 0);
    lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *create_icon_group(lv_obj_t *parent, int x, int y, int size,
                                   unsigned func)
{
    const lv_image_dsc_t *current[] = {
        &ui_icon_gear_current, &ui_icon_focus_current, &ui_icon_info_current,
    };
    const lv_image_dsc_t *candidate[] = {
        &ui_icon_gear_candidate, &ui_icon_focus_candidate, &ui_icon_info_candidate,
    };
    lv_obj_t *group = plain_object(parent, x, y, size, size == 14 ? 14 : 12);
    icon_image(group, size == 14 ? current[func] : candidate[func], ICON_COLOR);
    return group;
}

static lv_obj_t *create_arrow(lv_obj_t *screen, int y, bool up)
{
    lv_obj_t *group = plain_object(screen, ICON_X, y, 14, 14);
    icon_image(group, up ? &ui_icon_up : &ui_icon_down, ARROW_COLOR);
    return group;
}

static lv_obj_t *create_tune(lv_obj_t *screen)
{
    lv_obj_t *group = plain_object(screen, ICON_X, 153, 14, 14);
    static const lv_point_precise_t sliders[][2] = {
        {{3, 2}, {3, 12}}, {{10, 2}, {10, 12}},
        {{1, 5}, {5, 5}}, {{8, 9}, {12, 9}},
    };
    for (unsigned i = 0; i < sizeof(sliders) / sizeof(sliders[0]); ++i) {
        stroke(group, sliders[i], 2, 2, ICON_COLOR);
    }
    return group;
}

static lv_obj_t *create_plus(lv_obj_t *screen)
{
    lv_obj_t *group = plain_object(screen, ICON_X, 47, 14, 14);
    static const lv_point_precise_t horizontal[] = {{3, 7}, {11, 7}};
    static const lv_point_precise_t vertical[] = {{7, 3}, {7, 11}};
    stroke(group, horizontal, 2, 2, ICON_COLOR);
    stroke(group, vertical, 2, 2, ICON_COLOR);
    return group;
}

static lv_obj_t *create_minus(lv_obj_t *screen)
{
    lv_obj_t *group = plain_object(screen, ICON_X, 259, 14, 14);
    static const lv_point_precise_t horizontal[] = {{3, 7}, {11, 7}};
    stroke(group, horizontal, 2, 2, ICON_COLOR);
    return group;
}

static lv_obj_t *create_done(lv_obj_t *screen)
{
    lv_obj_t *group = plain_object(screen, ICON_X, 153, 14, 14);
    static const lv_point_precise_t check[] = {{3, 7}, {6, 10}, {11, 4}};
    stroke(group, check, 3, 2, DONE_COLOR);
    return group;
}

static void create_edge_fade(lv_obj_t *screen)
{
    /* Includes the original 17 px rail and its two separators, without
     * separately approximating subpixel strokes or gradient colours. */
    lv_obj_t *rail = lv_image_create(screen);
    lv_obj_remove_style_all(rail);
    lv_obj_remove_flag(rail, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_image_set_src(rail, &ui_key_rail);
    lv_obj_set_pos(rail, 155, 20);
}

void ui_key_hints_create(lv_obj_t *screen)
{
    create_edge_fade(screen);

    arrow_up = create_arrow(screen, 47, true);
    arrow_down = create_arrow(screen, 259, false);

    const int slot_x[] = {158, 157, 158};
    const int slot_y[] = {112, 153, 196};
    const int slot_size[] = {11, 14, 11};
    for (unsigned i = 0; i < 3; ++i) {
        home_slots[i].slot = plain_object(screen, slot_x[i], slot_y[i],
                                          slot_size[i], i == 1 ? 14 : 12);
        for (unsigned f = 0; f < FUNC_COUNT; ++f) {
            home_slots[i].icons[f] = create_icon_group(home_slots[i].slot, 0, 0,
                                                       slot_size[i], f);
            show_obj(home_slots[i].icons[f], false);
        }
        show_obj(home_slots[i].slot, false);
    }

    page_icons[PAGE_GEAR] = create_icon_group(screen, ICON_X, 153, 14,
                                              FUNC_SETTINGS);
    page_icons[PAGE_INFO] = create_icon_group(screen, ICON_X, 153, 14, FUNC_INFO);
    page_icons[PAGE_TUNE] = create_tune(screen);
    page_icons[PAGE_PLUS] = create_plus(screen);
    page_icons[PAGE_DONE] = create_done(screen);
    page_icons[PAGE_MINUS] = create_minus(screen);
    for (unsigned i = 0; i < PAGE_ICON_COUNT; ++i) {
        show_obj(page_icons[i], false);
    }

    last_mode = -1;
}

void ui_key_hints_home(unsigned current, bool editing)
{
    if (current >= FUNC_COUNT) current = FUNC_FOCUS;
    if (last_mode == 0 && last_func == current && last_edit == editing) return;
    last_mode = 0;
    last_func = current;
    last_edit = editing;

    show_obj(arrow_up, true);
    show_obj(arrow_down, true);
    for (unsigned i = 0; i < PAGE_ICON_COUNT; ++i) {
        show_obj(page_icons[i], false);
    }

    const unsigned previous = (current + FUNC_COUNT - 1) % FUNC_COUNT;
    const unsigned next = (current + 1) % FUNC_COUNT;
    for (unsigned i = 0; i < 3; ++i) {
        show_obj(home_slots[i].slot, true);
        const unsigned active = i == 0 ? previous : i == 1 ? current : next;
        for (unsigned f = 0; f < FUNC_COUNT; ++f) {
            show_obj(home_slots[i].icons[f], f == active);
        }
        set_icon_color(home_slots[i].icons[active],
                       i == 1 ? (editing ? SELECTED_COLOR : ICON_COLOR)
                              : CANDIDATE_COLOR);
    }
}

void ui_key_hints_page(bool setup_page, bool editing)
{
    const int mode = setup_page ? (editing ? 3 : 2) : 1;
    if (last_mode == mode) return;
    last_mode = mode;

    for (unsigned i = 0; i < 3; ++i) {
        show_obj(home_slots[i].slot, false);
    }
    for (unsigned i = 0; i < PAGE_ICON_COUNT; ++i) {
        show_obj(page_icons[i], false);
    }

    if (!setup_page) {
        show_obj(page_icons[PAGE_GEAR], true);
        show_obj(arrow_up, true);
        show_obj(arrow_down, true);
    } else if (!editing) {
        show_obj(page_icons[PAGE_TUNE], true);
        show_obj(arrow_up, true);
        show_obj(arrow_down, true);
    } else {
        show_obj(page_icons[PAGE_PLUS], true);
        show_obj(page_icons[PAGE_DONE], true);
        show_obj(page_icons[PAGE_MINUS], true);
        show_obj(arrow_up, false);
        show_obj(arrow_down, false);
    }
}
