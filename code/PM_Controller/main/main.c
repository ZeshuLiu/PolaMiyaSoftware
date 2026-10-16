#include "display.h"
#include "hc165.h"
#include "flash_trigger.h"
#include "lcd_test.h"
#include "lcd_st7789.h"
#include "settings.h"
#include "shutter.h"
#include "sdm50.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "app_main entered");
    ESP_LOGI(TAG, "flash_trigger_init begin");
    ESP_ERROR_CHECK(flash_trigger_init());
    ESP_LOGI(TAG, "flash_trigger_init done");

    ESP_LOGI(TAG, "shutter_init begin");
    shutter_init();
    ESP_LOGI(TAG, "shutter_init done");

    ESP_LOGI(TAG, "hc165_init begin");
    hc165_init();
    ESP_LOGI(TAG, "hc165_init done");

    ESP_LOGI(TAG, "sdm50_init begin");
    esp_err_t range_result = sdm50_init();
    ESP_LOGI(TAG, "sdm50_init returned: %s", esp_err_to_name(range_result));
    if (range_result == ESP_OK) {
        ESP_LOGI(TAG, "sdm50_start begin");
        range_result = sdm50_start();
        ESP_LOGI(TAG, "sdm50_start returned: %s", esp_err_to_name(range_result));
    }
    if (range_result != ESP_OK) {
        ESP_LOGW(TAG, "SDM50 initialization/start failed: %s",
                 esp_err_to_name(range_result));
    }

    ESP_LOGI(TAG, "settings_init begin");
    const esp_err_t storage_result = settings_init();
    ESP_LOGI(TAG, "settings_init returned: %s", esp_err_to_name(storage_result));
    if (storage_result != ESP_OK) {
        ESP_LOGW(TAG, "Parameter storage initialization failed: %s",
                 esp_err_to_name(storage_result));
    }

    lcd_st7789_t lcd = {0};
    ESP_LOGI(TAG, "lcd_st7789_init begin");
    lcd_st7789_init(&lcd);
    ESP_LOGI(TAG, "lcd_st7789_init done");

    ESP_LOGI(TAG, "display_init begin");
    lv_display_t *display = display_init(&lcd);
    ESP_LOGI(TAG, "display_init done");

    ESP_LOGI(TAG, "lcd_test_start begin");
    lcd_test_start(display, &lcd);
    ESP_LOGI(TAG, "lcd_test_start done");
    ESP_LOGI(TAG, "app_main initialization complete");
}
