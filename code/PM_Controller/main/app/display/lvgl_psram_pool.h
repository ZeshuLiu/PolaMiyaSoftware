#ifndef PM_LVGL_PSRAM_POOL_H
#define PM_LVGL_PSRAM_POOL_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Called once by LVGL during lv_init(); owned for the display lifetime. */
void *pm_lvgl_psram_pool_alloc(size_t size);
#ifdef __cplusplus
}
#endif
#endif
