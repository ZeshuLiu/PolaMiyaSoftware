#include "display.h"
#include "boot_screen.h"
#include "hc165.h"
#include "flash_trigger.h"
#include "film_motor.h"
#include "film_reload.h"
#include "usb_power.h"
#include "focus_unit.h"
#include "ui.h"
#include "lcd_st7789.h"
#include "settings.h"
#include "shutter.h"
#include "sdm50.h"
#include "esp_log.h"
#include "adc_measure.h"
#include "power_monitor.h"
#include "runtime_diagnostics.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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

typedef struct {
    SemaphoreHandle_t done;
    esp_err_t result;
} focus_init_context_t;

/* UART installs its ISR on the calling core. Keep initialization and any
 * failure cleanup on CPU1, while normal UART API calls remain task-safe. */
static void focus_init_on_cpu1(void *argument)
{
    focus_init_context_t *context = argument;
    ESP_LOGI(TAG, "FocusUnit UART init on CPU %d", (int)xPortGetCoreID());
    context->result = focus_unit_init();
    xSemaphoreGive(context->done);
    vTaskDelete(NULL);
}

static esp_err_t focus_init_with_cpu1_interrupt(void)
{
    focus_init_context_t context = {.done = xSemaphoreCreateBinary(), .result = ESP_FAIL};
    if (!context.done) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(focus_init_on_cpu1, "focus_init", 3072, &context,
                                3, NULL, 1) != pdPASS) {
        vSemaphoreDelete(context.done);
        return ESP_ERR_NO_MEM;
    }
    /* The child owns context until signaling; no timeout that could leave it
     * pointing to an expired stack frame. */
    xSemaphoreTake(context.done, portMAX_DELAY);
    const esp_err_t result = context.result;
    vSemaphoreDelete(context.done);
    return result;
}

void app_main(void)
{
    ESP_LOGI(TAG, "app_main entered");
    /* Establish the optocoupler's idle level before the slower LCD reset and
     * first frame. Full trigger initialization follows the visible boot screen. */
    ESP_ERROR_CHECK(flash_trigger_prepare_output());
    /* Motor starts braked before the slower display initialization. */
    ESP_ERROR_CHECK(film_motor_init());
    lcd_st7789_t lcd = {0};
    ESP_LOGI(TAG, "lcd_st7789_init begin");
    lcd_st7789_init(&lcd);
    ESP_LOGI(TAG, "lcd_st7789_init done");
    ESP_LOGI(TAG, "display_init begin");
    lv_display_t *display = display_init(&lcd);
    ESP_LOGI(TAG, "display_init done");
    boot_screen_start(display);

    boot_screen_begin(BOOT_STAGE_TRIGGER);
    ESP_LOGI(TAG, "flash_trigger_init begin");
    const esp_err_t flash_result = flash_trigger_init();
    if (flash_result != ESP_OK) {
        boot_screen_complete(flash_result);
        ESP_ERROR_CHECK(flash_result);
    }
    ESP_LOGI(TAG, "flash_trigger_init done");

    ESP_LOGI(TAG, "shutter_init begin");
    shutter_init();
    ESP_LOGI(TAG, "shutter_init done");
    boot_screen_complete(ESP_OK);

    boot_screen_begin(BOOT_STAGE_KEYS);
    const esp_err_t usb_result = usb_power_init();
    if (usb_result != ESP_OK) {
        ESP_LOGW(TAG, "USB detection init failed: %s", esp_err_to_name(usb_result));
    }
    ESP_LOGI(TAG, "hc165_init begin");
    hc165_init();
    ESP_LOGI(TAG, "hc165_init done");
    boot_screen_complete(ESP_OK);

    boot_screen_begin(BOOT_STAGE_RANGE);
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
    boot_screen_complete(range_result);

    boot_screen_begin(BOOT_STAGE_STORAGE);
    ESP_LOGI(TAG, "settings_init begin");
    esp_err_t storage_result = settings_init();
    ESP_LOGI(TAG, "settings_init returned: %s", esp_err_to_name(storage_result));
    if (storage_result == ESP_OK) {
        /* Count this boot once, after a complete EEPROM scan. Keep settings_init
         * read-only; a failed save leaves the increment in RAM for an explicit retry. */
        storage_result = settings_increment(SETTINGS_REBOOT_COUNT, 1);
        if (storage_result == ESP_OK) storage_result = settings_commit();
        ESP_LOGI(TAG, "Boot counter update/save: %s", esp_err_to_name(storage_result));
    }
    if (storage_result != ESP_OK) {
        ESP_LOGW(TAG, "Parameter storage startup failed: %s",
                esp_err_to_name(storage_result));
    }
    boot_screen_complete(storage_result);

    boot_screen_begin(BOOT_STAGE_ADC);
    ESP_LOGI(TAG, "adc_measure_init begin");
    const esp_err_t adc_result = adc_measure_init();
    ESP_LOGI(TAG, "adc_measure_init returned: %s", esp_err_to_name(adc_result));
    if (adc_result != ESP_OK) {
        ESP_LOGW(TAG, "ADC initialization failed: %s", esp_err_to_name(adc_result));
    }
    boot_screen_complete(adc_result);
    boot_screen_finish();

    const esp_err_t focus_result = focus_init_with_cpu1_interrupt();
    if (focus_result != ESP_OK) ESP_LOGW(TAG, "FocusUnit UART init failed: %s", esp_err_to_name(focus_result));
    const esp_err_t film_reload_result = film_reload_start();
    if (film_reload_result != ESP_OK) {
        ESP_LOGW(TAG, "Film reload task start failed: %s", esp_err_to_name(film_reload_result));
    }
    ESP_LOGI(TAG, "ui_start begin");
    ui_start(display);
    ESP_LOGI(TAG, "ui_start done");
    ESP_LOGI(TAG, "app_main initialization complete");
    runtime_diagnostics_init();

    TickType_t last_log_tick = xTaskGetTickCount();
    uint64_t pending_shutters = 0;
    bool shutter_save_pending = false;
    esp_err_t shutter_error = ESP_OK;
    while (1) {
        vTaskDelayUntil(&last_log_tick, pdMS_TO_TICKS(1000));
        const uint64_t edges = shutter_take_count();
        pending_shutters = UINT64_MAX - pending_shutters < edges
                            ? UINT64_MAX : pending_shutters + edges;
        esp_err_t count_result = ESP_OK;
        if (pending_shutters) {
            count_result = settings_increment(SETTINGS_SHUTTER_COUNT, pending_shutters);
            if (count_result == ESP_OK) {
                pending_shutters = 0;
                shutter_save_pending = true;
            }
        }
        if (count_result == ESP_OK && shutter_save_pending) {
            count_result = settings_commit_shutter();
            if (count_result == ESP_OK) shutter_save_pending = false;
        }
        if (count_result != shutter_error) {
            if (count_result == ESP_OK) ESP_LOGI(TAG, "Shutter count save recovered");
            else ESP_LOGW(TAG, "Shutter count update/save failed: %s", esp_err_to_name(count_result));
            shutter_error = count_result;
        }
        log_distance();
        log_power();
        runtime_diagnostics_poll();
    }
}
