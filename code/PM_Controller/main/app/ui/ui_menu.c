#include "ui_menu.h"
#include <limits.h>
#include <string.h>

static bool is_choice(const ui_menu_item_t *item)
{
    return item->type == UI_MENU_CHOICE || item->type == UI_MENU_SUBMENU;
}

esp_err_t ui_menu_init(ui_menu_t *menu, const ui_menu_item_t *items,
                       ui_menu_value_t *values, unsigned count)
{
    if (!menu || !items || !values || !count) return ESP_ERR_INVALID_ARG;
    for (unsigned i = 0; i < count; ++i) {
        const ui_menu_item_t *item = &items[i];
        if (!item->title || item->type > UI_MENU_BACK) return ESP_ERR_INVALID_ARG;
        if (item->type == UI_MENU_NUMBER && (!item->number.read || !item->number.write ||
            item->number.step <= 0 || item->number.minimum > item->number.maximum))
            return ESP_ERR_INVALID_ARG;
        if (item->type == UI_MENU_ACTION && !item->action.run) return ESP_ERR_INVALID_ARG;
        if (is_choice(item)) {
            if (!item->choice.read || !item->choice.write || !item->choice.labels ||
                !item->choice.count || item->choice.count > INT32_MAX) return ESP_ERR_INVALID_ARG;
            for (unsigned j = 0; j < item->choice.count; ++j)
                if (!item->choice.labels[j]) return ESP_ERR_INVALID_ARG;
        }
    }
    memset(menu, 0, sizeof(*menu));
    menu->items = items;
    menu->values = values;
    menu->count = count;
    ui_menu_refresh(menu);
    return ESP_OK;
}

void ui_menu_refresh(ui_menu_t *menu)
{
    for (unsigned i = 0; i < menu->count; ++i) {
        const ui_menu_item_t *item = &menu->items[i];
        ui_menu_value_t *value = &menu->values[i];
        *value = (ui_menu_value_t){.valid = true, .enabled = true};
        if (item->type == UI_MENU_NUMBER) {
            value->valid = item->number.read(&value->value) == ESP_OK &&
                value->value >= item->number.minimum && value->value <= item->number.maximum;
            value->enabled = value->valid;
        } else if (is_choice(item)) {
            value->valid = item->choice.read(&value->value) == ESP_OK && value->value >= 0 &&
                           (unsigned)value->value < item->choice.count;
            value->enabled = value->valid;
        } else if (item->type == UI_MENU_ACTION) {
            value->busy = item->action.busy && item->action.busy();
            value->enabled = !value->busy && (!item->action.enabled || item->action.enabled());
            value->status = item->action.status ? item->action.status() : NULL;
        }
    }
}

void ui_menu_cancel(ui_menu_t *menu)
{
    /* Numeric changes were already applied to RAM. Choice candidates were not. */
    menu->phase = UI_MENU_BROWSE;
    menu->error = ESP_OK;
}

static unsigned next_index(unsigned index, unsigned count, int direction)
{
    return direction < 0 ? (index ? index - 1 : count - 1) : (index + 1) % count;
}

static ui_menu_result_t write_value(ui_menu_t *menu, ui_menu_write_t write, int32_t value)
{
    menu->error = write(value);
    ui_menu_refresh(menu);
    return menu->error == ESP_OK ? UI_MENU_CHANGED : UI_MENU_FAILED;
}

ui_menu_result_t ui_menu_move(ui_menu_t *menu, int direction)
{
    const ui_menu_item_t *item = &menu->items[menu->selected];
    menu->error = ESP_OK;
    if (menu->phase == UI_MENU_BROWSE) {
        menu->selected = next_index(menu->selected, menu->count, direction);
    } else if (menu->phase == UI_MENU_LIST) {
        menu->cursor = next_index(menu->cursor, item->choice.count + 1, direction);
    } else if (item->type == UI_MENU_CHOICE) {
        menu->candidate = (int32_t)next_index((unsigned)menu->candidate, item->choice.count, direction);
    } else if (item->type == UI_MENU_NUMBER) {
        if (!menu->values[menu->selected].valid) {
            menu->error = ESP_ERR_INVALID_STATE;
            return UI_MENU_FAILED;
        }
        const int32_t current = menu->values[menu->selected].value;
        int64_t next = (int64_t)current - (int64_t)direction * item->number.step;
        if (next < item->number.minimum) next = item->number.minimum;
        if (next > item->number.maximum) next = item->number.maximum;
        if (next != current) return write_value(menu, item->number.write, (int32_t)next);
    }
    return UI_MENU_NO_CHANGE;
}

ui_menu_result_t ui_menu_confirm(ui_menu_t *menu)
{
    const ui_menu_item_t *item = &menu->items[menu->selected];
    const ui_menu_value_t *value = &menu->values[menu->selected];
    menu->error = ESP_OK;
    if (menu->phase == UI_MENU_LIST && menu->cursor == item->choice.count) {
        ui_menu_cancel(menu);
        return UI_MENU_NO_CHANGE;
    }
    if (menu->phase == UI_MENU_EDIT && item->type == UI_MENU_NUMBER) {
        menu->phase = UI_MENU_BROWSE;
        return UI_MENU_NO_CHANGE;
    }
    if (menu->phase == UI_MENU_LIST || menu->phase == UI_MENU_EDIT) {
        if (!value->valid) {
            menu->error = ESP_ERR_INVALID_STATE;
            return UI_MENU_FAILED;
        }
        const int32_t selected = menu->phase == UI_MENU_LIST ? (int32_t)menu->cursor : menu->candidate;
        ui_menu_result_t result = UI_MENU_NO_CHANGE;
        if (selected != value->value) result = write_value(menu, item->choice.write, selected);
        if (result != UI_MENU_FAILED) menu->phase = UI_MENU_BROWSE;
        return result;
    }
    if (item->type == UI_MENU_BACK) return UI_MENU_LEAVE;
    if (!value->enabled) {
        menu->error = ESP_ERR_INVALID_STATE;
        return UI_MENU_FAILED;
    }
    if (item->type == UI_MENU_ACTION) {
        menu->error = item->action.run();
        ui_menu_refresh(menu);
        return menu->error == ESP_OK ? UI_MENU_STARTED : UI_MENU_FAILED;
    }
    if (item->type == UI_MENU_SUBMENU) {
        menu->cursor = (unsigned)value->value;
        menu->phase = UI_MENU_LIST;
    } else {
        menu->candidate = value->value;
        menu->phase = UI_MENU_EDIT;
    }
    return UI_MENU_NO_CHANGE;
}
