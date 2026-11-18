#ifndef PM_UI_MENU_H
#define PM_UI_MENU_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    UI_MENU_NUMBER, UI_MENU_ACTION, UI_MENU_CHOICE, UI_MENU_SUBMENU, UI_MENU_PAGE, UI_MENU_BACK,
} ui_menu_type_t;

typedef esp_err_t (*ui_menu_read_t)(int32_t *value);
typedef esp_err_t (*ui_menu_write_t)(int32_t value);

/* Descriptors, option strings and the caller's cache live as long as the menu.
 * Callbacks may apply settings immediately; EEPROM commits belong to the application.
 * All callbacks and model operations run in the UI task, outside the LVGL lock. */
typedef struct {
    const char *title;
    ui_menu_type_t type;
    union {
        struct {
            int32_t minimum, maximum, step;
            const char *unit;
            ui_menu_read_t read;
            ui_menu_write_t write;
        } number;
        struct {
            const char *const *labels;
            unsigned count;
            ui_menu_read_t read;
            ui_menu_write_t write; /* Value is a zero-based option index. */
        } choice;
        struct {
            esp_err_t (*run)(void);
            bool (*enabled)(void);
            bool (*busy)(void);
            const char *(*status)(void);
            const char *(*detail)(void);
        } action;
    };
} ui_menu_item_t;

typedef struct {
    int32_t value;
    bool valid, enabled, busy;
    const char *status, *detail;
} ui_menu_value_t;

typedef enum { UI_MENU_BROWSE, UI_MENU_EDIT, UI_MENU_LIST } ui_menu_phase_t;
typedef enum {
    UI_MENU_NO_CHANGE, UI_MENU_CHANGED, UI_MENU_STARTED, UI_MENU_ENTER, UI_MENU_LEAVE, UI_MENU_FAILED,
} ui_menu_result_t;

typedef struct {
    const ui_menu_item_t *items;
    ui_menu_value_t *values;
    unsigned count, selected, cursor;
    ui_menu_phase_t phase;
    int32_t candidate;
    esp_err_t error;
} ui_menu_t;

esp_err_t ui_menu_init(ui_menu_t *menu, const ui_menu_item_t *items,
                       ui_menu_value_t *values, unsigned count);
void ui_menu_refresh(ui_menu_t *menu);
void ui_menu_cancel(ui_menu_t *menu);
/* -1 is physical UP, +1 is DOWN. Numeric editing maps UP to an increase. */
ui_menu_result_t ui_menu_move(ui_menu_t *menu, int direction);
ui_menu_result_t ui_menu_confirm(ui_menu_t *menu);
#endif
