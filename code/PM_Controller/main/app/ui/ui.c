#include "ui.h"
#include "ui_view.h"
#include "ui_settings.h"

#include "hc165.h"
#include "lcd_st7789.h"
#include "sdm50.h"
#include "settings.h"
#include "power_monitor.h"
#include "charge_status.h"
#include "film_motor.h"
#include "focus_unit.h"
#include "focus_position.h"
#include "focus_calibration.h"
#include <stdint.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FOCUS_BLINK_HALF_US 312500LL
#define FILM_CONSUME_AFTER_US 5000000LL

static const char *TAG = "ui";

/* Only the UI task changes this state after startup. No widget pointer escapes. */
static struct {
    unsigned page;
    unsigned home_func;
    bool focus_editing;
    bool focus_auto;
    bool focus_too_close;
    bool motor_running;
    bool eject_pending;
    int64_t motor_started_us;
    int64_t eject_started_us;
    int64_t motor_ramp_started_us;
} ui;

static void handle_keys(hc165_key_events_t keys)
{
    /* K3 (top): up/previous/increase. K1 (bottom): down/next/decrease.
     * K2 (middle) owns confirmation and execution. K4-6 are not connected. */
    if (ui.page == UI_SETUP) {
        if (ui_settings_handle_keys(keys)) ui.page = UI_HOME;
        return;
    }
    /* HOME long-press on the bottom key retracts to the calibrated infinity
     * target, regardless of which HOME function is selected. */
    if (ui.page == UI_HOME && (keys.long_press & HC165_KEY1)) {
        int32_t target;
        if (focus_distance_to_raw(0, true, &target)) {
            const esp_err_t result = focus_position_set_target(target);
            if (result == ESP_OK) {
                ui.focus_editing = false;
                ui.focus_auto = true;
                ui.focus_too_close = false;
            } else {
                ESP_LOGW(TAG, "Infinity focus rejected: %s", esp_err_to_name(result));
            }
        }
        return;
    }
    if (keys.long_press & HC165_KEY2) {
        if (ui.page == UI_HOME && ui.home_func == UI_HOME_FUNC_FOCUS && !ui.focus_editing) {
            ui.focus_editing = true;
            ui.focus_auto = false;
            ui.focus_too_close = false;
        }
        return;
    }

    if (keys.short_press & HC165_KEY2) {
        if (ui.page == UI_HOME || ui.page == UI_EJECT) {
            if (ui.focus_editing) {
                ui.focus_editing = false;
            } else {
                switch (ui.home_func) {
                case UI_HOME_FUNC_SETTINGS:
                    ui.page = UI_SETUP;
                    ui_settings_open();
                    break;
                case UI_HOME_FUNC_FOCUS: {
                    sdm50_sample_t sample;
                    int32_t target;
                    if (sdm50_get_sample(&sample) != ESP_OK) break;
                    focus_calibration_t cal;
                    if (focus_calibration_get(&cal) != ESP_OK) break;
                    const bool infinity = sample.status == SDM50_NO_TARGET;
                    const bool too_close = sample.status == SDM50_TOO_CLOSE ||
                        (sample.status == SDM50_VALID && sample.distance_mm < cal.min_distance_mm);
                    if (sample.status != SDM50_VALID && sample.status != SDM50_TOO_CLOSE && !infinity) break;
                    const uint32_t distance_mm = too_close ? cal.min_distance_mm : sample.distance_mm;
                    if (focus_distance_to_raw_with_calibration(&cal, distance_mm, infinity, &target) &&
                        focus_position_set_target(target) == ESP_OK) {
                        ui.focus_auto = true;
                        ui.focus_too_close = too_close;
                    }
                    break;
                }
                case UI_HOME_FUNC_INFO:
                    ui.page = UI_INFO;
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
            ui_settings_open();
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
            const int32_t current = focus_position_snapshot(now_ms).desired_raw;
            int32_t target;
            if (focus_distance_manual_step(current, next, &target))
                (void)focus_position_set_target(target);
        } else {
            /* K3 moves up to the previous function, K1 down to the next. */
            ui.home_func = next ? (ui.home_func + UI_HOME_FUNC_COUNT - 1) %
                                      UI_HOME_FUNC_COUNT
                                : (ui.home_func + 1) % UI_HOME_FUNC_COUNT;
        }
    } else {
        ui.page = (ui.page + (next ? 1 : UI_NAV_PAGE_COUNT - 1)) % UI_NAV_PAGE_COUNT;
        ui.focus_editing = false;
        if (ui.page == UI_SETUP) ui_settings_open();
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
        focus_position_process(focus_now_ms);
        const focus_unit_snapshot_t focus_snapshot = focus_position_snapshot(focus_now_ms);
        const focus_distance_t focus = focus_distance_get(&focus_snapshot);
        if (ui.page == UI_SETUP) ui_settings_refresh();

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
        const bool focus_visible = ((now / FOCUS_BLINK_HALF_US) % 2 == 0);

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        ui_view_show(ui.page);
        ui_view_eject((uint64_t)(now - ui.motor_started_us) / 1000, &power);
        ui_view_key_hints(ui.page, ui.home_func,
                          ui.focus_editing || ui.motor_running);
        ui_view_range(&focus, &sample);
        ui_view_focus_style(ui.focus_editing, ui.focus_auto, ui.focus_too_close,
                            focus.state == FOCUS_DISTANCE_READY, focus_visible);
        ui_view_power(&power, battery);
        if (settings_available) ui_view_film((unsigned)values.film_remaining);
        ui_view_uptime((uint64_t)(now / 1000000));
        ui_view_info(&values, &status, settings_available);
        ui_view_focus_info(&focus, &focus_snapshot);
        if (ui.page == UI_SETUP) ui_settings_render();
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
    focus_position_init();
    ui_settings_init();

    settings_values_t values = {0};
    settings_get(&values);
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    ui_view_create(display);
    sdm50_sample_t sample = {.status = SDM50_WAITING};
    const focus_unit_snapshot_t focus_snapshot = focus_position_snapshot((uint32_t)(esp_timer_get_time() / 1000));
    const focus_distance_t focus = focus_distance_get(&focus_snapshot);
    ui_view_range(&focus, &sample);
    ui_view_key_hints(UI_HOME, ui.home_func, false);
    ui_view_film((unsigned)values.film_remaining);
    ui_view_uptime(0);
    /* Replace the boot screen with the initial page. The backlight is already
     * on in normal startup; the driver makes that second on request a no-op. */
    lv_refr_now(display);
    lvgl_port_unlock();
    lcd_st7789_backlight(true);
    ESP_ERROR_CHECK(xTaskCreate(ui_task, "ui", 4096, NULL, 3, NULL) == pdPASS
                    ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "UI ready: CASE 7, 172x320; HOME list, K3 up/increase, K1 down/decrease, K2 confirm/hold save");
}
