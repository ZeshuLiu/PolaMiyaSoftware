#include "shutter.h"

#include "PIN.h"
#include "flash_trigger.h"
#include "esp_err.h"

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
    ESP_ERROR_CHECK(gpio_config(&input));
    /* Flash/GPTimer control APIs currently use flash-resident code. */
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(PIN_SHUT_TRG, shutter_edge, NULL));
    ESP_ERROR_CHECK(gpio_set_intr_type(PIN_SHUT_TRG, GPIO_INTR_NEGEDGE));
    ESP_ERROR_CHECK(gpio_intr_enable(PIN_SHUT_TRG));
}
