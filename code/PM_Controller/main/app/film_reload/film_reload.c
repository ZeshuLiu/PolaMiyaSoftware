#include "film_reload.h"

#include "film_latch.h"
#include "settings.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FILM_RELOAD_POLL_MS       1000
#define FILM_RELOAD_WAIT_MS       10000
#define FILM_RELOAD_CAPACITY      8
#define FILM_RELOAD_STACK_BYTES   3072
#define FILM_RELOAD_PRIORITY      1

static const char *TAG = "film_reload";

static void film_reload_task(void *argument)
{
    (void)argument;
    int64_t clear_after_us = 0;
    esp_err_t last_save_error = ESP_OK;
    while (1) {
        const int64_t now = esp_timer_get_time();
        if (clear_after_us == 0 && film_latch_pending()) {
            ESP_LOGI(TAG, "FILM_KEY latched; acknowledge in about 10 s");
            clear_after_us = now + (int64_t)FILM_RELOAD_WAIT_MS * 1000;
        } else if (clear_after_us != 0 && now >= clear_after_us) {
            const esp_err_t result = film_latch_clear();
            if (result == ESP_OK) {
                const esp_err_t update = settings_set(SETTINGS_FILM_REMAINING, FILM_RELOAD_CAPACITY);
                if (update == ESP_OK) {
                    ESP_LOGI(TAG, "FILM_KEY acknowledged; film remaining=8, save pending");
                } else {
                    ESP_LOGW(TAG, "Film remaining update failed: %s", esp_err_to_name(update));
                }
                clear_after_us = 0;
            } else {
                ESP_LOGW(TAG, "FILM_KEY acknowledge failed: %s; remaining unchanged",
                         esp_err_to_name(result));
                clear_after_us = now + (int64_t)FILM_RELOAD_WAIT_MS * 1000;
            }
        }
        /* Retry saving the current RAM count, never replay a decrement or a
         * reload. No-change calls do not write EEPROM or save other parameters. */
        const esp_err_t save = settings_commit_film();
        if (save != last_save_error) {
            if (save == ESP_OK) ESP_LOGI(TAG, "Film remaining save recovered");
            else ESP_LOGW(TAG, "Film remaining save failed: %s; will retry", esp_err_to_name(save));
            last_save_error = save;
        }
        vTaskDelay(pdMS_TO_TICKS(FILM_RELOAD_POLL_MS));
    }
}

esp_err_t film_reload_start(void)
{
    const esp_err_t result = film_latch_init();
    if (result != ESP_OK) return result;
    return xTaskCreate(film_reload_task, "film_reload", FILM_RELOAD_STACK_BYTES,
                       NULL, FILM_RELOAD_PRIORITY, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}
