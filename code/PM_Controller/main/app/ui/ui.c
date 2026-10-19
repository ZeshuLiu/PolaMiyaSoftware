#include "ui.h"
#include "ui_view.h"

#include "hc165.h"
#include "lcd_st7789.h"
#include "sdm50.h"
#include "settings.h"
#include "power_monitor.h"
#include "film_motor.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HISTORY_INTERVAL_US 100000LL
#define FOCUS_STEP_MM 50
#define FOCUS_MIN_MM 50
#define FOCUS_MAX_MM 20000
#define FOCUS_BLINK_HALF_US 312500LL

static const char *TAG = "ui";

/* Only the UI task changes this state after startup. No widget pointer escapes. */
static struct {
    unsigned page;
    unsigned brightness;
    unsigned home_func;
    bool editing;
    bool focus_editing;
    bool motor_running;
    bool focus_target_active;
    int32_t focus_target_mm;
    const char *save_message;
} ui;

/* Replace this placeholder when the film-counter driver is connected.
 * 0..8 remaining shots; the UI task samples it every 50 ms. */
static volatile unsigned film_remaining = 6;

void ui_set_film_remaining(unsigned remaining)
{
    film_remaining = remaining > 8 ? 8 : remaining;
}

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
        } else if (ui.page == UI_HOME) {
            if (ui.focus_editing) {
                ui.focus_editing = false;
            } else {
                switch (ui.home_func) {
                case UI_HOME_FUNC_SETTINGS:
                    ui.page = UI_SETUP;
                    ui.editing = false;
                    break;
                case UI_HOME_FUNC_FOCUS:
                    /* Seed the first edit from the current reading, not the
                     * startup WAITING placeholder. Later edits retain the target. */
                    if (!ui.focus_target_active) {
                        sdm50_sample_t sample;
                        if (sdm50_get_sample(&sample) == ESP_OK) {
                            const focus_distance_t focus = focus_distance_get(&sample);
                            if (focus.state == FOCUS_DISTANCE_FINITE) {
                                ui.focus_target_mm = (int32_t)focus.distance_mm;
                                if (ui.focus_target_mm < FOCUS_MIN_MM) ui.focus_target_mm = FOCUS_MIN_MM;
                                if (ui.focus_target_mm > FOCUS_MAX_MM) ui.focus_target_mm = FOCUS_MAX_MM;
                            }
                        }
                    }
                    ui.focus_target_active = true;
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

    if (ui.page == UI_HOME) {
        if (ui.motor_running) {
            /* Keep EJECT selected; a K2 short press remains the stop action. */
            return;
        } else if (ui.focus_editing) {
            if (next) {
                ui.focus_target_mm = ui.focus_target_mm + FOCUS_STEP_MM >= FOCUS_MAX_MM
                                         ? FOCUS_MAX_MM
                                         : ui.focus_target_mm + FOCUS_STEP_MM;
            } else {
                ui.focus_target_mm = ui.focus_target_mm - FOCUS_STEP_MM <= FOCUS_MIN_MM
                                         ? FOCUS_MIN_MM
                                         : ui.focus_target_mm - FOCUS_STEP_MM;
            }
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
        ui.page = (ui.page + (next ? 1 : UI_PAGE_COUNT - 1)) % UI_PAGE_COUNT;
        ui.focus_editing = false;
    }
}

static void ui_task(void *argument)
{
    (void)argument;
    int64_t history_tick = esp_timer_get_time() / HISTORY_INTERVAL_US;
    while (1) {
        handle_keys(hc165_take_events());
        sdm50_sample_t sample = {.status = SDM50_RX_ERROR};
        if (sdm50_get_sample(&sample) != ESP_OK) {
            sample.status = SDM50_RX_ERROR;
        }
        focus_distance_t focus = focus_distance_get(&sample);
        if (ui.focus_target_active) {
            /* Local setpoint placeholder until the FocusUnit position command
             * and calibration are connected. */
            focus.state = FOCUS_DISTANCE_FINITE;
            focus.distance_mm = (uint32_t)ui.focus_target_mm;
            focus.using_actual = false;
        }

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
        const bool focus_visible = !ui.focus_editing ||
                                   ((now / FOCUS_BLINK_HALF_US) % 2 == 0);

        ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
        ui_view_show(ui.page);
        ui_view_key_hints(ui.page, ui.home_func,
                          ui.focus_editing || ui.motor_running, ui.editing);
        ui_view_range(&focus, &sample);
        ui_view_focus_blink(focus_visible);
        ui_view_power(&power);
        if (elapsed_slots > 0) {
            ui_view_history(sample.status == SDM50_VALID ? sample.distance_mm
                                                        : LV_CHART_POINT_NONE,
                            (unsigned)elapsed_slots);
            history_tick = current_tick;
        }
        ui_view_film(film_remaining);
        ui_view_uptime((uint64_t)(now / 1000000));
        ui_view_info(&values, &status, settings_available);
        const char *message = ui.save_message;
        if (!message) {
            message = !settings_available || !status.writable ? "STORAGE ERROR" :
                      status.dirty ? "UNSAVED" : "";
        }
        ui_view_settings(ui.brightness, ui.editing, message);
        lvgl_port_unlock();
        /* Do not replay missed UI ticks after waiting for a slow render/EEPROM.
         * History already uses monotonic time and marks missing sample slots. */
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void ui_start(lv_display_t *display)
{
    ui.home_func = UI_HOME_FUNC_FOCUS;
    ui.focus_target_mm = 2500;

    settings_values_t values;
    ui.brightness = settings_get(&values) == ESP_OK ? (unsigned)values.backlight_percent : 100;
    lcd_st7789_set_brightness(ui.brightness);
    ESP_ERROR_CHECK(lvgl_port_lock(0) ? ESP_OK : ESP_ERR_TIMEOUT);
    ui_view_create(display);
    sdm50_sample_t sample = {.status = SDM50_WAITING};
    const focus_distance_t focus = focus_distance_get(&sample);
    ui_view_range(&focus, &sample);
    ui_view_key_hints(UI_HOME, ui.home_func, false, false);
    ui_view_film(film_remaining);
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
