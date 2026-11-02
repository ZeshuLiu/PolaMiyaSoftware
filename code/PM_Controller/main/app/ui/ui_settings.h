#ifndef PM_UI_SETTINGS_H
#define PM_UI_SETTINGS_H
#include "hc165.h"
#include "lvgl.h"

/* UI task only. Init/open/keys/refresh do not hold the LVGL lock. */
void ui_settings_init(void);
void ui_settings_open(void);
bool ui_settings_handle_keys(hc165_key_events_t keys); /* true: return to HOME */
void ui_settings_refresh(void);
/* Create once and render under the LVGL lock. */
lv_obj_t *ui_settings_create(lv_obj_t *screen);
void ui_settings_render(void);
#endif
