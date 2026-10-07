#include "hc165.h"

#include "PIN.h"
#include "hc165_keys.h"
#include "esp_err.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define HC165_SCAN_INTERVAL_MS 5
#define HC165_TASK_STACK_BYTES 2048
#define HC165_TASK_PRIORITY 3

static TaskHandle_t scan_task;
static hc165_keys_state_t keys;
static hc165_inputs_t inputs;
static hc165_key_events_t pending_events;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;

static uint8_t read_parallel_inputs(void)
{
    uint8_t raw = 0;
    ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CS, 0));
    esp_rom_delay_us(1);
    ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CS, 1));
    esp_rom_delay_us(1);

    /* Mode-2 timing: CP idles high; read Q7 on the falling half-cycle,
     * then the rising edge shifts the next bit. The first bit is D7. */
    for (int bit = 7; bit >= 0; --bit) {
        ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CLK, 0));
        esp_rom_delay_us(1);
        if (gpio_get_level(PIN_MASPI_MISO)) {
            raw |= (uint8_t)(1U << bit);
        }
        ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CLK, 1));
        esp_rom_delay_us(1);
    }
    return raw;
}

static void publish_inputs(uint8_t raw, hc165_key_events_t events)
{
    const uint8_t pressed = hc165_keys_pressed(&keys);
    portENTER_CRITICAL(&state_lock);
    inputs.raw = raw;
    inputs.pressed = pressed;
    inputs.charging = (raw & HC165_INPUT_CHG) == 0;
    inputs.charge_done = (raw & HC165_INPUT_STB) == 0;
    pending_events.short_press |= events.short_press;
    pending_events.long_press |= events.long_press;
    portEXIT_CRITICAL(&state_lock);
}

static void scan_once(void)
{
    const uint8_t raw = read_parallel_inputs();
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    const hc165_key_events_t events = hc165_keys_update(&keys, raw, now_ms);
    publish_inputs(raw, events);
}

static void scan_inputs(void *argument)
{
    (void)argument;
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t requested_interval = pdMS_TO_TICKS(HC165_SCAN_INTERVAL_MS);
    /* At 100 Hz, 5 ms rounds to zero; always wait at least one RTOS tick. */
    const TickType_t interval = requested_interval ? requested_interval : 1;

    while (true) {
        scan_once();
        xTaskDelayUntil(&last_wake, interval);
    }
}

void hc165_init(void)
{
    ESP_ERROR_CHECK(scan_task ? ESP_ERR_INVALID_STATE : ESP_OK);

    /* Keep /PL and CP high while configuring the two output pins. */
    ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CS, 1));
    ESP_ERROR_CHECK(gpio_set_level(PIN_MASPI_CLK, 1));
    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << PIN_MASPI_CS) | (1ULL << PIN_MASPI_CLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&output_config));
    const gpio_config_t input_config = {
        .pin_bit_mask = 1ULL << PIN_MASPI_MISO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_config));

    const uint8_t raw = read_parallel_inputs();
    const uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    hc165_keys_init(&keys, raw, now_ms);
    publish_inputs(raw, (hc165_key_events_t){0});

    const BaseType_t created = xTaskCreate(scan_inputs, "hc165_scan", HC165_TASK_STACK_BYTES,
                                          NULL, HC165_TASK_PRIORITY, &scan_task);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

hc165_inputs_t hc165_get_inputs(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_inputs_t snapshot = inputs;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}

hc165_key_events_t hc165_get_events(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_key_events_t snapshot = pending_events;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}

hc165_key_events_t hc165_take_events(void)
{
    portENTER_CRITICAL(&state_lock);
    const hc165_key_events_t snapshot = pending_events;
    pending_events.short_press = 0;
    pending_events.long_press = 0;
    portEXIT_CRITICAL(&state_lock);
    return snapshot;
}
