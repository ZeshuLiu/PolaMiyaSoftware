#include "flash_trigger.h"

#include "PIN.h"
#include "driver/gptimer.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static DRAM_ATTR struct {
    gptimer_handle_t timer;
    bool initialized;
    bool busy;
    int64_t ready_after_us;
} flash;
static DRAM_ATTR portMUX_TYPE flash_lock = portMUX_INITIALIZER_UNLOCKED;

static bool IRAM_ATTR pulse_done(gptimer_handle_t timer,
                                 const gptimer_alarm_event_data_t *event, void *context)
{
    (void)event;
    (void)context;
    portENTER_CRITICAL_ISR(&flash_lock);
    gpio_set_level(PIN_FLASH_TRG, 0);
    gptimer_stop(timer);
    /* Recovery starts at the actual falling edge, even if this ISR was late. */
    flash.ready_after_us = esp_timer_get_time() + FLASH_TRIGGER_RECOVERY_US;
    flash.busy = false;
    portEXIT_CRITICAL_ISR(&flash_lock);
    return false;
}

esp_err_t flash_trigger_init(void)
{
    if (flash.initialized) {
        return ESP_OK;
    }
    gpio_set_level(PIN_FLASH_TRG, 0);
    const gpio_config_t output = {
        .pin_bit_mask = 1ULL << PIN_FLASH_TRG,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&output);
    if (result != ESP_OK) {
        return result;
    }
    const gptimer_config_t config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 1000000,
    };
    result = gptimer_new_timer(&config, &flash.timer);
    if (result != ESP_OK) {
        return result;
    }
    const gptimer_event_callbacks_t callbacks = {.on_alarm = pulse_done};
    result = gptimer_register_event_callbacks(flash.timer, &callbacks, NULL);
    if (result == ESP_OK) {
        result = gptimer_enable(flash.timer);
    }
    if (result != ESP_OK) {
        gptimer_del_timer(flash.timer);
        flash.timer = NULL;
        return result;
    }
    flash.initialized = true;
    return ESP_OK;
}

esp_err_t flash_trigger_fire_us(uint32_t pulse_us)
{
    if (pulse_us < FLASH_TRIGGER_MIN_US || pulse_us > FLASH_TRIGGER_MAX_US) {
        return ESP_ERR_INVALID_ARG;
    }
    portENTER_CRITICAL_SAFE(&flash_lock);
    if (!flash.initialized || flash.busy || esp_timer_get_time() < flash.ready_after_us) {
        portEXIT_CRITICAL_SAFE(&flash_lock);
        return ESP_ERR_INVALID_STATE;
    }
    flash.busy = true;
    /* Raise the output before configuring its end time: no task handoff. */
    gpio_set_level(PIN_FLASH_TRG, 1);
    esp_err_t result = gptimer_set_raw_count(flash.timer, 0);
    const gptimer_alarm_config_t alarm = {.alarm_count = pulse_us};
    if (result == ESP_OK) {
        result = gptimer_set_alarm_action(flash.timer, &alarm);
    }
    if (result == ESP_OK) {
        result = gptimer_start(flash.timer);
    }
    if (result != ESP_OK) {
        gpio_set_level(PIN_FLASH_TRG, 0);
        flash.ready_after_us = esp_timer_get_time() + FLASH_TRIGGER_RECOVERY_US;
        flash.busy = false;
    }
    portEXIT_CRITICAL_SAFE(&flash_lock);
    return result;
}

esp_err_t flash_trigger_fire(void)
{
    return flash_trigger_fire_us(FLASH_TRIGGER_DEFAULT_US);
}

bool flash_trigger_is_busy(void)
{
    portENTER_CRITICAL_SAFE(&flash_lock);
    const bool busy = flash.busy || esp_timer_get_time() < flash.ready_after_us;
    portEXIT_CRITICAL_SAFE(&flash_lock);
    return busy;
}
