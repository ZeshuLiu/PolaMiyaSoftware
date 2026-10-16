#include "shutter.h"

#include "PIN.h"
#include "flash_trigger.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "shutter";

static void shutter_edge(void *context)
{
    (void)context;
    /* No logging, queueing or persistence in the trigger path. Busy edges drop. */
    (void)flash_trigger_fire();
}

void shutter_init(void)
{
    const gpio_config_t input = {
        .pin_bit_mask = 1ULL << PIN_SHUT_TRG,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        /* H2 has an external pull-up; closing the contact pulls low. */
    };
    ESP_LOGI(TAG, "gpio_config begin, trigger GPIO=%d", (int)PIN_SHUT_TRG);
    ESP_ERROR_CHECK(gpio_config(&input));
    ESP_LOGI(TAG, "gpio_config done");
    /* Flash/GPTimer control APIs currently use flash-resident code. */
    ESP_LOGI(TAG, "gpio_install_isr_service begin");
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_LOGI(TAG, "gpio_install_isr_service done");
    ESP_LOGI(TAG, "gpio_isr_handler_add begin");
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_SHUT_TRG, shutter_edge, NULL));
    ESP_LOGI(TAG, "gpio_isr_handler_add done");
    ESP_LOGI(TAG, "gpio_set_intr_type begin");
    ESP_ERROR_CHECK(gpio_set_intr_type(PIN_SHUT_TRG, GPIO_INTR_NEGEDGE));
    ESP_LOGI(TAG, "gpio_set_intr_type done");
    ESP_LOGI(TAG, "gpio_intr_enable begin");
    ESP_ERROR_CHECK(gpio_intr_enable(PIN_SHUT_TRG));
    ESP_LOGI(TAG, "gpio_intr_enable done");
}
