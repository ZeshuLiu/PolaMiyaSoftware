#include "ui_menu_view.h"
#include "ui_home_assets.h"
#include "ui_key_hints.h"
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG 0x080f10
#define CARD 0x142720
#define BORDER 0x2d463b
#define TEXT 0xe0eee7
#define MUTED 0x91afa0
#define MINT 0x79f5a7
#define INK 0x09291e
#define AMBER 0xffbe58
#define BLUE 0x69d6ff

typedef struct {
    lv_obj_t *root, *title, *value, *unit, *bar, *arrow, *dots;
    lv_obj_t *segment, *pill, *option[2];
    int height;
    unsigned flags;
    int32_t shown_value;
    bool shown_valid;
} menu_card_t;

typedef struct {
    lv_obj_t *root;
    menu_card_t *cards;
    unsigned selected;
} menu_list_t;

static struct {
    lv_obj_t *root, *title, *counter, *current, *current_check;
    lv_obj_t *option_list, *footer, *status, *hint;
    menu_list_t settings, network;
    menu_card_t back;
    unsigned list_item, cursor;
    int32_t applied;
    bool applied_valid;
    ui_menu_phase_t phase;
    uint32_t footer_color;
    bool compact, network_page;
} view;

static void show(lv_obj_t *obj, bool visible)
{
    lv_obj_set_flag(obj, LV_OBJ_FLAG_HIDDEN, !visible);
}

static bool set_text(lv_obj_t *obj, const char *text)
{
    if (strcmp(lv_label_get_text(obj), text) == 0) return false;
    lv_label_set_text(obj, text);
    return true;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    if (color) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    }
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, int x, int y, int w, const char *text,
                        const lv_font_t *font)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, font->line_height);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(TEXT), 0);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(obj, text);
    return obj;
}

static lv_obj_t *line(lv_obj_t *parent, int x, int y,
                       const lv_point_precise_t *points, unsigned count)
{
    lv_obj_t *obj = lv_line_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_line_set_points(obj, points, count);
    lv_obj_set_style_line_width(obj, 2, 0);
    lv_obj_set_style_line_rounded(obj, true, 0);
    return obj;
}

static void scrolling(lv_obj_t *obj)
{
    lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(obj, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_width(obj, 2, LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_color(obj, lv_color_hex(MINT), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_radius(obj, 1, LV_PART_SCROLLBAR);
}

static void create_card(menu_card_t *card, lv_obj_t *parent, int y, int h,
                         const ui_menu_item_t *item)
{
    static const lv_point_precise_t arrow[] = {{0,0},{7,7},{0,14}};
    static const lv_point_precise_t back[] = {{7,0},{2,5},{7,10},{2,5},{13,5}};
    card->height = h;
    card->flags = UINT_MAX;
    card->root = box(parent, 0, y, 120, h, CARD);
    lv_obj_set_style_radius(card->root, 6, 0);
    lv_obj_set_style_border_width(card->root, 1, 0);
    card->title = label(card->root, 8, 4, 104, item->title, &ui_font_body_bold);
    if (item->type == UI_MENU_BACK) {
        lv_obj_set_pos(card->title, 35, h / 2 - 11);
        lv_obj_set_width(card->title, 77);
        card->arrow = line(card->root, 12, h / 2 - 5, back, 5);
        return;
    }
    if (item->type == UI_MENU_NUMBER) {
        card->value = label(card->root, 8, 26, 104, "", &ui_font_count);
        card->unit = label(card->root, 41, 34, 70, item->number.unit ? item->number.unit : "", &ui_font_body_bold);
        card->bar = lv_bar_create(card->root);
        lv_obj_remove_style_all(card->bar);
        lv_obj_set_pos(card->bar, 70, 42);
        lv_obj_set_size(card->bar, 40, 4);
        lv_bar_set_range(card->bar, 0, 100);
        lv_obj_set_style_bg_opa(card->bar, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(card->bar, LV_OPA_COVER, LV_PART_INDICATOR);
        lv_obj_set_style_radius(card->bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(card->bar, 2, LV_PART_INDICATOR);
    } else if (item->type == UI_MENU_CHOICE && item->choice.count == 2 &&
               lv_text_get_width(item->choice.labels[0], (uint32_t)strlen(item->choice.labels[0]), &ui_font_body_bold, 0) <= 46 &&
               lv_text_get_width(item->choice.labels[1], (uint32_t)strlen(item->choice.labels[1]), &ui_font_body_bold, 0) <= 46) {
        card->segment = box(card->root, 8, 28, 104, 26, 0);
        lv_obj_set_style_radius(card->segment, 5, 0);
        lv_obj_set_style_border_width(card->segment, 1, 0);
        card->pill = box(card->segment, 1, 1, 51, 24, INK);
        lv_obj_set_style_radius(card->pill, 4, 0);
        for (unsigned i = 0; i < 2; ++i) {
            card->option[i] = label(card->segment, (int)i * 52, 3, 52, item->choice.labels[i], &ui_font_body_bold);
            lv_obj_set_style_text_align(card->option[i], LV_TEXT_ALIGN_CENTER, 0);
        }
    } else {
        card->value = label(card->root, 8, h - 31, 104, "", &ui_font_body);
        if (item->type == UI_MENU_ACTION || item->type == UI_MENU_SUBMENU || item->type == UI_MENU_PAGE) {
            card->arrow = line(card->root, 98, h / 2 - 7, arrow, 3);
            card->dots = box(card->root, 94, h / 2 - 2, 18, 4, 0);
            for (unsigned i = 0; i < 3; ++i) {
                lv_obj_t *dot = box(card->dots, (int)i * 6, 0, 3, 3, BLUE);
                lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            }
        }
    }
}

static void number_layout(menu_card_t *card, const ui_menu_item_t *item, int32_t value, bool valid)
{
    char text[16];
    if (valid) snprintf(text, sizeof(text), "%" PRId32, value);
    else snprintf(text, sizeof(text), "--");
    const char *unit = item->number.unit ? item->number.unit : "";
    const int unit_width = lv_text_get_width(unit, (uint32_t)strlen(unit), &ui_font_body_bold, 0);
    const int gap = *unit ? 4 : 0;
    const lv_font_t *font = &ui_font_count;
    int width = lv_text_get_width(text, (uint32_t)strlen(text), font, 0);
    if (width + gap + unit_width > 104) {
        font = &ui_font_body_bold;
        width = lv_text_get_width(text, (uint32_t)strlen(text), font, 0);
    }
    const bool stacked = width + gap + unit_width > 104;
    set_text(card->value, text);
    lv_obj_set_style_text_font(card->value, font, 0);
    lv_obj_set_y(card->value, stacked ? 22 : font == &ui_font_count ? 26 : 34);
    lv_obj_set_pos(card->unit, stacked ? 8 : 8 + width + gap, stacked ? 40 : 34);
    show(card->unit, valid && *unit);
    const int64_t range = (int64_t)item->number.maximum - item->number.minimum;
    const bool short_value = width + gap + unit_width <= 54;
    lv_obj_set_pos(card->bar, short_value ? 70 : 8, short_value ? 42 : card->height - 6);
    lv_obj_set_width(card->bar, short_value ? 40 : 104);
    show(card->bar, valid && range > 0 && !stacked);
    if (valid && range > 0)
        lv_bar_set_value(card->bar, (int32_t)(((int64_t)value - item->number.minimum) * 100 / range), LV_ANIM_OFF);
}

static void update_card(menu_card_t *card, const ui_menu_item_t *item,
                         const ui_menu_value_t *value, bool selected, bool editing, int32_t candidate)
{
    const bool disabled = !value->enabled;
    const unsigned flags = (selected ? 1U : 0U) | (editing ? 2U : 0U) |
                           (value->busy ? 4U : 0U) | (disabled ? 8U : 0U);
    const bool paint = flags != card->flags;
    uint32_t background = CARD, title = TEXT, detail = MUTED, border = BORDER;
    if (value->busy) { background = 0x10303b; title = detail = border = BLUE; }
    else if (disabled) title = MUTED;
    else if (selected) { background = editing ? AMBER : MINT; title = INK; detail = 0x295a40; border = background; }
    if (paint) {
        lv_obj_set_style_bg_color(card->root, lv_color_hex(background), 0);
        lv_obj_set_style_border_color(card->root, lv_color_hex(border), 0);
        lv_obj_set_style_text_color(card->title, lv_color_hex(title), 0);
        if (card->value) lv_obj_set_style_text_color(card->value, lv_color_hex(item->type == UI_MENU_NUMBER ? title : detail), 0);
        if (card->unit) lv_obj_set_style_text_color(card->unit, lv_color_hex(title), 0);
        if (card->arrow) {
            lv_obj_set_style_line_color(card->arrow, lv_color_hex(title), 0);
            show(card->arrow, item->type == UI_MENU_BACK || (!disabled && !value->busy));
        }
        if (card->dots) show(card->dots, value->busy);
        if (card->bar) {
            lv_obj_set_style_bg_color(card->bar, lv_color_hex(selected ? 0x477c57 : BORDER), LV_PART_MAIN);
            lv_obj_set_style_bg_color(card->bar, lv_color_hex(selected ? INK : MINT), LV_PART_INDICATOR);
        }
    }
    const int32_t display = editing && item->type == UI_MENU_CHOICE ? candidate : value->value;
    const bool changed = paint || card->shown_value != display || card->shown_valid != value->valid;
    if (changed && item->type == UI_MENU_NUMBER) number_layout(card, item, display, value->valid);
    else if (item->type == UI_MENU_ACTION)
        set_text(card->value, value->detail ? value->detail :
                 value->busy ? "RUNNING" : disabled ? "NOT READY" : "START");
    else if (item->type == UI_MENU_PAGE)
        set_text(card->value, "OPEN");
    else if (changed && (item->type == UI_MENU_CHOICE || item->type == UI_MENU_SUBMENU)) {
        if (card->segment) {
            show(card->pill, value->valid);
            if (value->valid) lv_obj_set_x(card->pill, display ? 52 : 1);
            lv_obj_set_style_border_color(card->segment, lv_color_hex(selected ? 0x477c57 : BORDER), 0);
            lv_obj_set_style_bg_color(card->pill, lv_color_hex(selected ? INK : MINT), 0);
            for (unsigned i = 0; i < 2; ++i)
                lv_obj_set_style_text_color(card->option[i], lv_color_hex(value->valid && (int32_t)i == display ? (selected ? background : INK) : title), 0);
        } else set_text(card->value, value->valid ? item->choice.labels[display] : "--");
    }
    card->shown_value = display;
    card->shown_valid = value->valid;
    card->flags = flags;
}

static void build_options(const ui_menu_t *menu)
{
    static const lv_point_precise_t check[] = {{0,4},{4,8},{11,0}};
    const ui_menu_item_t *item = &menu->items[menu->selected];
    lv_obj_clean(view.option_list);
    int y = 0;
    for (unsigned i = 0; i < item->choice.count; ++i) {
        lv_point_t size;
        lv_text_get_size(&size, item->choice.labels[i], &ui_font_body_bold, 0, 0, 84, LV_TEXT_FLAG_NONE);
        const int h = size.y + 12 > 32 ? size.y + 12 : 32;
        lv_obj_t *row = box(view.option_list, 0, y, 114, h, CARD);
        lv_obj_set_style_radius(row, 5, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_t *title = label(row, 8, (h - size.y) / 2, 84, item->choice.labels[i], &ui_font_body);
        lv_label_set_long_mode(title, LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_height(title, size.y);
        line(row, 97, h / 2 - 4, check, 3);
        y += h + 8;
    }
    view.list_item = menu->selected;
    view.cursor = UINT_MAX;
    view.applied = INT32_MIN;
    lv_obj_update_layout(view.option_list);
}

static void update_options(const ui_menu_t *menu)
{
    const ui_menu_item_t *item = &menu->items[menu->selected];
    const ui_menu_value_t *value = &menu->values[menu->selected];
    if (view.list_item != menu->selected) build_options(menu);
    if (view.cursor == menu->cursor && view.applied == value->value && view.applied_valid == value->valid) return;
    for (unsigned i = 0; i < item->choice.count; ++i) {
        const bool focused = menu->cursor == i;
        const bool applied = value->valid && value->value == (int32_t)i;
        lv_obj_t *row = lv_obj_get_child(view.option_list, (int32_t)i);
        lv_obj_t *title = lv_obj_get_child(row, 0);
        lv_obj_t *check = lv_obj_get_child(row, 1);
        lv_obj_set_style_bg_color(row, lv_color_hex(focused ? AMBER : CARD), 0);
        lv_obj_set_style_border_color(row, lv_color_hex(focused ? AMBER : BORDER), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(focused ? INK : TEXT), 0);
        lv_obj_set_style_text_font(title, focused ? &ui_font_body_bold : &ui_font_body, 0);
        lv_obj_set_style_line_color(check, lv_color_hex(focused ? INK : MINT), 0);
        show(check, applied);
    }
    const ui_menu_item_t back = {.title = "BACK", .type = UI_MENU_BACK};
    const ui_menu_value_t back_value = {.valid = true, .enabled = true};
    update_card(&view.back, &back, &back_value, menu->cursor == item->choice.count, false, 0);
    if (menu->cursor < item->choice.count)
        lv_obj_scroll_to_view(lv_obj_get_child(view.option_list, (int32_t)menu->cursor), LV_ANIM_OFF);
    view.cursor = menu->cursor;
    view.applied = value->value;
    view.applied_valid = value->valid;
}

static void create_list(menu_list_t *list, const ui_menu_t *menu)
{
    list->cards = calloc(menu->count, sizeof(*list->cards));
    ESP_ERROR_CHECK(list->cards ? ESP_OK : ESP_ERR_NO_MEM);
    list->selected = UINT_MAX;
    list->root = box(view.root, 8, 42, 120, view.compact ? 240 : 202, 0);
    scrolling(list->root);
    int y = 0;
    for (unsigned i = 0; i < menu->count; ++i) {
        const ui_menu_type_t type = menu->items[i].type;
        const int h = type == UI_MENU_BACK ? (view.compact ? 36 : 40) :
                      type == UI_MENU_NUMBER ? (view.compact ? 60 : 68) :
                      (type == UI_MENU_ACTION || type == UI_MENU_PAGE) && view.compact ? 56 : 64;
        create_card(&list->cards[i], list->root, y, h, &menu->items[i]);
        const bool next_is_back = i + 1 < menu->count && menu->items[i + 1].type == UI_MENU_BACK;
        y += h + (view.compact ? 8 : next_is_back ? 12 : 10);
    }
}

lv_obj_t *ui_menu_view_create(lv_obj_t *screen, const ui_menu_t *settings, const ui_menu_t *network)
{
    static const lv_point_precise_t check[] = {{0,4},{4,8},{11,0}};
    view.compact = settings->count > 3 || network->count > 3;
    view.phase = (ui_menu_phase_t)-1;
    view.footer_color = UINT32_MAX;
    view.list_item = view.cursor = UINT_MAX;
    view.root = box(screen, 0, 0, 172, 320, BG);
    view.title = label(view.root, 12, 5, 116, "SETTINGS", &ui_font_menu_title);
    box(view.root, 12, 30, 116, 1, BORDER);
    view.counter = label(view.root, 100, 6, 28, "", &ui_font_body);
    lv_obj_set_style_text_align(view.counter, LV_TEXT_ALIGN_RIGHT, 0);
    view.current = label(view.root, 12, 39, 92, "", &ui_font_body);
    lv_obj_set_style_text_color(view.current, lv_color_hex(MINT), 0);
    view.current_check = line(view.root, 108, 46, check, 3);
    lv_obj_set_style_line_color(view.current_check, lv_color_hex(MINT), 0);
    create_list(&view.settings, settings);
    create_list(&view.network, network);
    show(view.network.root, false);
    view.option_list = box(view.root, 8, 72, 120, 192, 0);
    scrolling(view.option_list);
    const ui_menu_item_t back = {.title = "BACK", .type = UI_MENU_BACK};
    create_card(&view.back, view.root, 276, 28, &back);
    lv_obj_set_x(view.back.root, 8);
    view.footer = box(view.root, 8, view.compact ? 284 : 250, 120, view.compact ? 24 : 56, view.compact ? 0 : 0x0e1d18);
    lv_obj_set_style_radius(view.footer, 6, 0);
    view.status = label(view.footer, view.compact ? 4 : 8, 4, view.compact ? 112 : 104, "", &ui_font_body_bold);
    lv_label_set_long_mode(view.status, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    view.hint = label(view.footer, 8, 30, 104, "", &ui_font_body);
    lv_obj_set_style_text_color(view.hint, lv_color_hex(MUTED), 0);
    return view.root;
}

void ui_menu_view_update(const ui_menu_t *menu, const ui_menu_footer_t *footer, bool network)
{
    const ui_menu_item_t *item = &menu->items[menu->selected];
    const ui_menu_value_t *value = &menu->values[menu->selected];
    const bool list = menu->phase == UI_MENU_LIST;
    menu_list_t *card_list = network ? &view.network : &view.settings;
    if (view.network_page != network) view.list_item = UINT_MAX;
    if (view.phase != menu->phase || view.network_page != network) {
        show(view.settings.root, !list && !network);
        show(view.network.root, !list && network);
        show(view.footer, !list);
        show(view.option_list, list); show(view.back.root, list);
        show(view.counter, list); show(view.current, list); show(view.current_check, list);
        lv_obj_set_width(view.title, list ? 84 : 116);
        if (list) {
            view.cursor = UINT_MAX;
            set_text(view.counter, "");
        }
        view.phase = menu->phase;
        view.network_page = network;
    }
    set_text(view.title, list ? item->title : network ? "NETWORK" : "SETTINGS");
    if (list) {
        char counter[32];
        snprintf(counter, sizeof(counter), "%u/%u", menu->cursor < item->choice.count ? menu->cursor + 1 : item->choice.count, item->choice.count);
        if (set_text(view.counter, counter)) {
            const int w = lv_text_get_width(counter, (uint32_t)strlen(counter), &ui_font_body, 0) + 2;
            lv_obj_set_pos(view.counter, 128 - w, 6);
            lv_obj_set_width(view.counter, w);
            lv_obj_set_width(view.title, 108 - w);
        }
        set_text(view.current, value->valid ? item->choice.labels[value->value] : "--");
        show(view.current_check, value->valid);
        update_options(menu);
        ui_key_hints_menu(menu->cursor == item->choice.count ? UI_MENU_HINT_BROWSE : UI_MENU_HINT_CHOICE);
    } else {
        for (unsigned i = 0; i < menu->count; ++i)
            update_card(&card_list->cards[i], &menu->items[i], &menu->values[i], menu->selected == i,
                        menu->selected == i && menu->phase == UI_MENU_EDIT, menu->candidate);
        if (card_list->selected != menu->selected) {
            lv_obj_scroll_to_view(card_list->cards[menu->selected].root, LV_ANIM_OFF);
            card_list->selected = menu->selected;
        }
        set_text(view.status, view.compact && !*footer->status ? footer->hint : footer->status);
        if (view.footer_color != footer->color) {
            lv_obj_set_style_text_color(view.status, lv_color_hex(footer->color), 0);
            view.footer_color = footer->color;
        }
        if (!view.compact) set_text(view.hint, footer->hint);
        ui_menu_hint_t hint = UI_MENU_HINT_BROWSE;
        if (value->busy) hint = UI_MENU_HINT_BUSY;
        else if (!value->enabled) hint = UI_MENU_HINT_DISABLED;
        else if (menu->phase == UI_MENU_EDIT)
            hint = item->type == UI_MENU_NUMBER ? UI_MENU_HINT_NUMBER : UI_MENU_HINT_CHOICE;
        else if (item->type == UI_MENU_ACTION) hint = UI_MENU_HINT_ACTION;
        else if (item->type == UI_MENU_SUBMENU || item->type == UI_MENU_PAGE) hint = UI_MENU_HINT_OPEN;
        ui_key_hints_menu(hint);
    }
}
