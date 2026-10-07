#include "display.h"
#include "hc165.h"
#include "flash_trigger.h"
#include "lcd_test.h"
#include "lcd_st7789.h"
#include "settings.h"
#include "esp_log.h"

void app_main(void)
{
    ESP_ERROR_CHECK(flash_trigger_init());
    hc165_init();

    const esp_err_t storage_result = settings_init();
    if (storage_result != ESP_OK) {
        ESP_LOGW("main", "Parameter storage initialization failed: %s",
                 esp_err_to_name(storage_result));
    }

    lcd_st7789_t lcd = {0};
    lcd_st7789_init(&lcd);

    lv_display_t *display = display_init(&lcd);
    lcd_test_start(display, &lcd);
}
