#include "ui.h"
#include "ui_view.h"

#include "hc165.h"
#include "lcd_st7789.h"
#include "sdm50.h"
#include "settings.h"
#include "power_monitor.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HISTORY_INTERVAL_US 100000LL

static const char *TAG = "ui";

/* Only the UI task changes this state after startup. No widget pointer escapes. */
static struct {
    unsigned page;
    unsigned brightness;
    bool editing;
    const char *save_message;
} ui;

static void handle_keys(hc165_key_events_t keys)
{
    /* K3 (top): up/right/increase. K1 (bottom): down/left/decrease.
     * K2 (middle) owns confirmation and execution. K4-6 are not connected. */
    if (keys.long_press & HC165_KEY2) {
        if (ui.page == UI_SETUP) {
            ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
            ui_view_settings(ui.brightness, ui.editing, "SAVING...");
            lvgl_port_unlock();
            /* EEPROM I/O can wait; never hold the display mutex while saving. */
            const esp_err_t result = settings_commit();
            ui.save_message = result == ESP_OK ? "SAVED" : "SAVE FAILED";
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "Settings save failed: %s", esp_err_to_name(result));
            }
        }
        return;
    }

    if (keys.short_press & HC165_KEY2) {
        if (ui.page == UI_SETUP) {
            ui.editing = !ui.editing;
        } else {
            ui.page = UI_SETUP;
            ui.editing = false;
        }
        return;
    }

    const bool previous = keys.short_press & HC165_KEY1;
    const bool next = keys.short_press & HC165_KEY3;
    if (previous == next) {
        return; /* No key, or conflicting simultaneous directions. */
    }
    if (ui.page == UI_SETUP && ui.editing) {
        unsigned percent = ui.brightness;
        if (next) {
            percent = percent > 95 ? 100 : percent + 5;
        } else {
            percent = percent < 5 ? 0 : percent - 5;
        }
        if (percent != ui.brightness) {
            const esp_err_t result = settings_set(SETTINGS_BACKLIGHT_PERCENT, percent);
            if (result == ESP_OK) {
                ui.brightness = percent;
                lcd_st7789_set_brightness(percent);
                ui.save_message = "UNSAVED";
            } else {
                ui.save_message = "RAM ERROR";
            }
        }
    } else {
        ui.page = (ui.page + (next ? 1 : UI_PAGE_COUNT - 1)) % UI_PAGE_COUNT;
    }
}

static void ui_task(void *argument)
{
    (void)argument;
    TickType_t wake_tick = xTaskGetTickCount();
    int64_t history_tick = esp_timer_get_time() / HISTORY_INTERVAL_US;
    while (1) {
        handle_keys(hc165_take_events());
        sdm50_sample_t sample = {.status = SDM50_RX_ERROR};
        if (sdm50_get_sample(&sample) != ESP_OK) {
            sample.status = SDM50_RX_ERROR;
        }
        const focus_distance_t focus = focus_distance_get(&sample);
        power_sample_t power;
        power_monitor_get(&power);
        settings_values_t values = {0};
        settings_status_t status = {0};
        const bool settings_available = settings_get(&values) == ESP_OK &&
                                        settings_get_status(&status) == ESP_OK;
        const int64_t now = esp_timer_get_time();
        const int64_t current_tick = now / HISTORY_INTERVAL_US;
        int64_t elapsed_slots = current_tick - history_tick;
        if (elapsed_slots > 101) elapsed_slots = 101;

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        ui_view_show(ui.page);
        ui_view_range(&focus, &sample);
        ui_view_power(&power);
        if (elapsed_slots > 0) {
            ui_view_history(sample.status == SDM50_VALID ? sample.distance_mm : LV_CHART_POINT_NONE,
                            (unsigned)elapsed_slots);
            history_tick = current_tick;
        }
        ui_view_uptime((uint64_t)(now / 1000000));
        ui_view_info(&values, &status, settings_available);
        const char *message = ui.save_message;
        if (!message) {
            message = !settings_available || !status.writable ? "STORAGE ERROR" :
                      status.dirty ? "UNSAVED" : "";
        }
        ui_view_settings(ui.brightness, ui.editing, message);
        lvgl_port_unlock();
        vTaskDelayUntil(&wake_tick, pdMS_TO_TICKS(50));
    }
}

void ui_start(lv_display_t *display)
{
    settings_values_t values;
    ui.brightness = settings_get(&values) == ESP_OK ? (unsigned)values.backlight_percent : 100;
    lcd_st7789_set_brightness(ui.brightness);
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    ui_view_create(display);
    sdm50_sample_t sample = {.status = SDM50_WAITING};
    const focus_distance_t focus = focus_distance_get(&sample);
    ui_view_range(&focus, &sample);
    ui_view_settings(ui.brightness, false, "");
    /* Request the initial page before starting the nonblocking backlight fade. */
    lv_refr_now(display);
    lvgl_port_unlock();
    lcd_st7789_backlight(true);
    ESP_ERROR_CHECK(xTaskCreate(ui_task, "ui", 4096, NULL, 3, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "UI ready: CASE 7, 172x320; K3 right/increase, K1 left/decrease, K2 confirm/hold save");
}
