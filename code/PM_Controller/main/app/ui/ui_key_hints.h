#ifndef PM_UI_KEY_HINTS_H
#define PM_UI_KEY_HINTS_H

#include "lvgl.h"

/* Internal view helpers; caller holds the LVGL lock. Physical order: K3/K2/K1.
 * K3 = up/right/increase; K1 = down/left/decrease; K2 = confirm/execute. */
void ui_key_hints_create(lv_obj_t *screen);
void ui_key_hints_update(bool setup_page, bool editing);

#endif
