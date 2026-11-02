#ifndef PM_UI_MENU_VIEW_H
#define PM_UI_MENU_VIEW_H
#include "lvgl.h"
#include "ui_menu.h"

typedef struct {
    const char *status;
    const char *hint;
    uint32_t color;
} ui_menu_footer_t;

/* One settings view, created once after menu initialization. All calls hold the
 * LVGL lock; no getters or business callbacks are invoked by this renderer. */
lv_obj_t *ui_menu_view_create(lv_obj_t *screen, const ui_menu_t *menu);
void ui_menu_view_update(const ui_menu_t *menu, const ui_menu_footer_t *footer);
#endif
