#include "film_latch.h"

#include "PIN.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define FILM_LATCH_CLEAR_MS 100

static const char *TAG = "film_latch";

esp_err_t film_latch_init(void)
{
    /* Q2 is off at GPIO7=0: do not erase a retained event during startup. */
    ESP_RETURN_ON_ERROR(gpio_set_level(PIN_ESP_FILM_CLR, 0), TAG, "clear idle");
    const gpio_config_t output = {
        .pin_bit_mask = 1ULL << PIN_ESP_FILM_CLR,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&output), TAG, "clear GPIO");
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << PIN_ESP_FILM_TRG,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        /* Q1 drain has the board's R9 pull-up to +3.3 V. */
    };
    return gpio_config(&input);
}

bool film_latch_pending(void)
{
    /* U3 D=0: a clock edge takes Q low, Q1 turns off, GPIO6 becomes high. */
    return gpio_get_level(PIN_ESP_FILM_TRG) != 0;
}

esp_err_t film_latch_clear(void)
{
    /* GPIO7=1 turns Q2 on and asserts U3 /PRE, restoring Q=1. Q1 then
     * pulls GPIO6 low. The signal name CLR denotes event acknowledgement. */
    ESP_RETURN_ON_ERROR(gpio_set_level(PIN_ESP_FILM_CLR, 1), TAG, "assert clear");
    vTaskDelay(pdMS_TO_TICKS(FILM_LATCH_CLEAR_MS));
    ESP_RETURN_ON_ERROR(gpio_set_level(PIN_ESP_FILM_CLR, 0), TAG, "release clear");
    return film_latch_pending() ? ESP_ERR_INVALID_RESPONSE : ESP_OK;
}
