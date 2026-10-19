#ifndef PM_UI_KEY_HINTS_H
#define PM_UI_KEY_HINTS_H

#include <stdbool.h>
#include "lvgl.h"

/* Internal view helpers; caller holds the LVGL lock. Physical order: K3/K2/K1.
 * K3 = up/right/increase; K1 = down/left/decrease; K2 = confirm/execute. */
void ui_key_hints_create(lv_obj_t *screen);
/* HOME: three-icon rolling function list. current follows UI_HOME_FUNC_*. */
void ui_key_hints_home(unsigned current, bool editing);
/* INFO/SETUP: up/down page arrows plus the current page action. */
void ui_key_hints_page(bool setup_page, bool editing);

#endif
