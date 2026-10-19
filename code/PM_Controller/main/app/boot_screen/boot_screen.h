#ifndef PM_BOOT_SCREEN_H
#define PM_BOOT_SCREEN_H

#include "esp_err.h"
#include "lvgl.h"

typedef enum {
    BOOT_STAGE_DISPLAY,
    BOOT_STAGE_TRIGGER,
    BOOT_STAGE_KEYS,
    BOOT_STAGE_RANGE,
    BOOT_STAGE_STORAGE,
    BOOT_STAGE_ADC,
    BOOT_STAGE_COUNT,
} boot_stage_t;

/* Startup task only, after display_init(). These functions own the LVGL lock.
 * No driver initialization or blocking peripheral I/O may run inside that lock. */
void boot_screen_start(lv_display_t *display);
void boot_screen_begin(boot_stage_t stage);
void boot_screen_complete(esp_err_t result);
/* After BOOT_STAGE_ADC is complete, call once before ui_start(), which replaces
 * and deletes the boot objects. Do not call this module after that handoff. */
void boot_screen_finish(void);

#endif
