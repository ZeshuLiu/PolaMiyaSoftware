#include "ui.h"
#include "ui_view.h"

#include "hc165.h"
#include "lcd_st7789.h"
#include "sdm50.h"
#include "settings.h"
#include "power_monitor.h"
#include "charge_status.h"
#include "film_motor.h"
#include "focus_unit.h"
#include <stdint.h>
#include <limits.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FOCUS_STEP_RAW 1000
#define FOCUS_BLINK_HALF_US 312500LL
#define FILM_CONSUME_AFTER_US 5000000LL

static const char *TAG = "ui";

/* Only the UI task changes this state after startup. No widget pointer escapes. */
static struct {
    unsigned page;
    unsigned brightness;
    unsigned home_func;
    bool editing;
    bool focus_editing;
    bool motor_running;
    bool eject_pending;
    int64_t motor_started_us;
    int64_t eject_started_us;
    int64_t motor_ramp_started_us;
    const char *save_message;
} ui;

static void handle_keys(hc165_key_events_t keys)
{
    /* K3 (top): up/previous/increase. K1 (bottom): down/next/decrease.
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
        } else if (ui.page == UI_HOME || ui.page == UI_EJECT) {
            if (ui.focus_editing) {
                ui.focus_editing = false;
            } else {
                switch (ui.home_func) {
                case UI_HOME_FUNC_SETTINGS:
                    ui.page = UI_SETUP;
                    ui.editing = false;
                    break;
                case UI_HOME_FUNC_FOCUS:
                    ui.focus_editing = true;
                    break;
                case UI_HOME_FUNC_INFO:
                    ui.page = UI_INFO;
                    ui.editing = false;
                    break;
                case UI_HOME_FUNC_EJECT: {
                    const bool running = !ui.motor_running;
                    const esp_err_t result = film_motor_set_running(running);
                    if (result == ESP_OK) {
                        ui.motor_running = running;
                        const bool new_eject = !ui.eject_pending;
                        ui.eject_pending = true;
                        ui.page = running ? UI_EJECT : UI_HOME;
                        ui.motor_ramp_started_us = esp_timer_get_time();
                        if (running) ui.motor_started_us = ui.motor_ramp_started_us;
                        if (new_eject) ui.eject_started_us = ui.motor_ramp_started_us;
                        ESP_LOGI(TAG, "Film motor ramp %s", running ? "start" : "stop");
                    } else {
                        ESP_LOGW(TAG, "Film motor command failed: %s", esp_err_to_name(result));
                    }
                    break;
                }
                default:
                    break;
                }
            }
        } else {
            /* INFO: K2 keeps the previous shortcut into SETTINGS. */
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
    if (ui.page == UI_EJECT) return;

    if (ui.page == UI_HOME) {
        if (ui.focus_editing) {
            const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
            const int32_t current = focus_unit_snapshot(now_ms).desired_raw;
            int64_t target = (int64_t)current + (next ? FOCUS_STEP_RAW : -FOCUS_STEP_RAW);
            if (target < 0) target = 0;
            if (target > INT32_MAX) target = INT32_MAX;
            focus_unit_set_target((int32_t)target);
        } else {
            /* K3 moves up to the previous function, K1 down to the next. */
            ui.home_func = next ? (ui.home_func + UI_HOME_FUNC_COUNT - 1) %
                                      UI_HOME_FUNC_COUNT
                                : (ui.home_func + 1) % UI_HOME_FUNC_COUNT;
        }
    } else if (ui.page == UI_SETUP && ui.editing) {
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
        ui.page = (ui.page + (next ? 1 : UI_NAV_PAGE_COUNT - 1)) % UI_NAV_PAGE_COUNT;
        ui.focus_editing = false;
    }
}

static void ui_task(void *argument)
{
    (void)argument;
    while (1) {
        handle_keys(hc165_take_events());
        sdm50_sample_t sample = {.status = SDM50_RX_ERROR};
        if (sdm50_get_sample(&sample) != ESP_OK) {
            sample.status = SDM50_RX_ERROR;
        }
        const uint32_t focus_now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        focus_unit_process(focus_now_ms);
        const focus_unit_snapshot_t focus_snapshot = focus_unit_snapshot(focus_now_ms);
        const focus_distance_t focus = focus_distance_get(&focus_snapshot);

        power_sample_t power;
        power_monitor_get(&power);
        settings_values_t values = {0};
        settings_status_t status = {0};
        const bool settings_available = settings_get(&values) == ESP_OK &&
                                        settings_get_status(&status) == ESP_OK;
        const int64_t now = esp_timer_get_time();
        const uint32_t now_ms = (uint32_t)(now / 1000);
        const hc165_inputs_t charge_inputs = hc165_get_inputs();
        const battery_status_t battery = charge_status_update(now_ms, &charge_inputs, &power);
        const bool focus_visible = !ui.focus_editing ||
                                   ((now / FOCUS_BLINK_HALF_US) % 2 == 0);

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        ui_view_show(ui.page);
        ui_view_eject((uint64_t)(now - ui.motor_started_us) / 1000, &power);
        ui_view_key_hints(ui.page, ui.home_func,
                          ui.focus_editing || ui.motor_running, ui.editing);
        ui_view_range(&focus, &sample);
        ui_view_focus_blink(focus_visible);
        ui_view_power(&power, battery, now_ms);
        if (settings_available) ui_view_film((unsigned)values.film_remaining);
        ui_view_uptime((uint64_t)(now / 1000000));
        ui_view_info(&values, &status, settings_available);
        ui_view_focus_info(&focus, &focus_snapshot);
        const char *message = ui.save_message;
        if (!message) {
            message = !settings_available || !status.writable ? "STORAGE ERROR" :
                      status.dirty ? "UNSAVED" : "";
        }
        ui_view_settings(ui.brightness, ui.editing, message);
        lvgl_port_unlock();
        /* Page changes are immediate; settle film usage after the motor stop ramp. */
        if (ui.eject_pending && !ui.motor_running &&
            now - ui.motor_ramp_started_us >= (int64_t)FILM_MOTOR_RAMP_MS * 1000) {
            ui.eject_pending = false;
            /* The first start through the final stop ramp is one eject. UI
             * scheduling latency must not turn exactly 5 s into a consumed sheet. */
            const int64_t completed_us = ui.motor_ramp_started_us +
                                         (int64_t)FILM_MOTOR_RAMP_MS * 1000;
            const bool consumed = completed_us - ui.eject_started_us > FILM_CONSUME_AFTER_US;
            const esp_err_t result = consumed ? settings_consume_film() : settings_commit_film();
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "Film remaining save failed: %s; background save will retry",
                         esp_err_to_name(result));
            }
            ESP_LOGI(TAG, "Eject complete: duration=%lld ms, over_5s=%d",
                     (long long)((completed_us - ui.eject_started_us) / 1000), consumed);
        }
        /* Do not replay missed UI ticks after a slow render or EEPROM write. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ui_start(lv_display_t *display)
{
    ui.home_func = UI_HOME_FUNC_FOCUS;

    settings_values_t values = {0};
    const bool settings_available = settings_get(&values) == ESP_OK;
    ui.brightness = settings_available ? (unsigned)values.backlight_percent : 100;
    lcd_st7789_set_brightness(ui.brightness);
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    ui_view_create(display);
    sdm50_sample_t sample = {.status = SDM50_WAITING};
    const focus_unit_snapshot_t focus_snapshot = focus_unit_snapshot((uint32_t)(esp_timer_get_time() / 1000));
    const focus_distance_t focus = focus_distance_get(&focus_snapshot);
    ui_view_range(&focus, &sample);
    ui_view_key_hints(UI_HOME, ui.home_func, false, false);
    ui_view_film((unsigned)values.film_remaining);
    ui_view_uptime(0);
    ui_view_settings(ui.brightness, false, "");
    /* Replace the boot screen with the initial page. The backlight is already
     * on in normal startup; the driver makes that second on request a no-op. */
    lv_refr_now(display);
    lvgl_port_unlock();
    lcd_st7789_backlight(true);
    ESP_ERROR_CHECK(xTaskCreate(ui_task, "ui", 4096, NULL, 3, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "UI ready: CASE 7, 172x320; HOME list, K3 up/increase, K1 down/decrease, K2 confirm/hold save");
}
