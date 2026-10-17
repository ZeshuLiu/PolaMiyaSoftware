#include "display.h"
#include "hc165.h"
#include "flash_trigger.h"
#include "ui.h"
#include "lcd_st7789.h"
#include "settings.h"
#include "shutter.h"
#include "sdm50.h"
#include "esp_log.h"
#include "adc_measure.h"
#include "power_monitor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

static void log_power(void)
{
    power_sample_t sample;
    power_monitor_get(&sample);
    ESP_LOGI(TAG, "ADC BAT: raw=%u, pin=%u mV, battery=%lu mV, est=%d%%, valid=%d; "
             "MOTOR: raw=%u, pin=%u mV, current=%lu mA, valid=%d; error=%s, overflows=%lu",
             (unsigned)sample.adc.battery.raw, (unsigned)sample.adc.battery.voltage_mv,
             (unsigned long)sample.battery_mv, sample.battery_percent, sample.battery_valid,
             (unsigned)sample.adc.motor.raw, (unsigned)sample.adc.motor.voltage_mv,
             (unsigned long)sample.motor_current_ma, sample.motor_valid,
             esp_err_to_name(sample.adc.error), (unsigned long)sample.adc.overflows);
}

static void log_distance(void)
{
    sdm50_sample_t sample;
    const esp_err_t result = sdm50_get_sample(&sample);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "SDM50 read failed: %s", esp_err_to_name(result));
        return;
    }

    switch (sample.status) {
    case SDM50_VALID:
        ESP_LOGI(TAG, "SDM50 distance: %u mm", (unsigned)sample.distance_mm);
        break;
    case SDM50_NO_TARGET:
        ESP_LOGI(TAG, "SDM50: INF (no target/out of range)");
        break;
    case SDM50_TOO_CLOSE:
        ESP_LOGW(TAG, "SDM50: NEAR (<50 mm)");
        break;
    case SDM50_STALE:
        ESP_LOGW(TAG, "SDM50: stale data");
        break;
    case SDM50_WAITING:
        ESP_LOGI(TAG, "SDM50: waiting for data");
        break;
    case SDM50_STOPPED:
        ESP_LOGI(TAG, "SDM50: stopped");
        break;
    case SDM50_RX_ERROR:
        ESP_LOGW(TAG, "SDM50: receive error");
        break;
    default:
        ESP_LOGW(TAG, "SDM50: unknown status %d", (int)sample.status);
        break;
    }
}

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

    ESP_LOGI(TAG, "adc_measure_init begin");
    const esp_err_t adc_result = adc_measure_init();
    ESP_LOGI(TAG, "adc_measure_init returned: %s", esp_err_to_name(adc_result));
    if (adc_result != ESP_OK) {
        ESP_LOGW(TAG, "ADC initialization failed: %s", esp_err_to_name(adc_result));
    }

    lcd_st7789_t lcd = {0};
    ESP_LOGI(TAG, "lcd_st7789_init begin");
    lcd_st7789_init(&lcd);
    ESP_LOGI(TAG, "lcd_st7789_init done");

    ESP_LOGI(TAG, "display_init begin");
    lv_display_t *display = display_init(&lcd);
    ESP_LOGI(TAG, "display_init done");

    ESP_LOGI(TAG, "ui_start begin");
    ui_start(display);
    ESP_LOGI(TAG, "ui_start done");
    ESP_LOGI(TAG, "app_main initialization complete");

    TickType_t last_log_tick = xTaskGetTickCount();
    while (1) {
        vTaskDelayUntil(&last_log_tick, pdMS_TO_TICKS(1000));
        log_distance();
        log_power();
    }
}
