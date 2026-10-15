#include "sdm50.h"

#include "PIN.h"
#include "driver/uart.h"
#include "driver/uhci.h"
#include "esp_attr.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define RX_SIZE 64
#define COMMAND_TIMEOUT_MS 200
#define STALE_US 100000

static const char *TAG = "sdm50";
static uhci_controller_handle_t controller;
static TaskHandle_t rx_task;
static SemaphoreHandle_t command_lock;
static SemaphoreHandle_t command_ack;
static portMUX_TYPE sample_lock = portMUX_INITIALIZER_UNLOCKED;
static DMA_ATTR uint8_t rx_buffer[RX_SIZE];
static DMA_ATTR uint8_t tx_buffer[8];
static sdm50_sample_t latest = {.status = SDM50_WAITING};
static bool initialized;
static bool pending_command;
static uint8_t expected_mode;
static int64_t waiting_since_us;

/* 解析器只由 DMA ISR 使用；跨 DMA 分段保留半帧。 */
static uint8_t frame[8];
static size_t frame_size;

static void discard_first_byte(void)
{
    for (size_t i = 1; i < frame_size; ++i) {
        frame[i - 1] = frame[i];
    }
    --frame_size;
}

static void parse_byte(uint8_t byte, BaseType_t *woken)
{
    frame[frame_size++] = byte;
    while (frame_size) {
        size_t required;
        if (frame[0] == 0x5c) {
            required = 4;
        } else if (frame[0] == 0x5a) {
            if (frame_size < 3) {
                return;
            }
            /* 本手册应答负载为 2 或 4 字节。 */
            if (frame[2] != 2 && frame[2] != 4) {
                discard_first_byte();
                continue;
            }
            required = frame[2] + 4;
        } else {
            discard_first_byte();
            continue;
        }
        if (frame_size < required) {
            return;
        }
        uint8_t sum = 0;
        for (size_t i = 1; i < required; ++i) {
            sum += frame[i];
        }
        if (sum != 0xff) {
            portENTER_CRITICAL_ISR(&sample_lock);
            ++latest.checksum_errors;
            portEXIT_CRITICAL_ISR(&sample_lock);
            discard_first_byte();
            continue;
        }

        if (frame[0] == 0x5c) {
            const uint16_t distance = frame[1] | ((uint16_t)frame[2] << 8);
            portENTER_CRITICAL_ISR(&sample_lock);
            latest.distance_mm = distance;
            latest.timestamp_us = esp_timer_get_time();
            ++latest.sequence;
            latest.status = distance >= 50 && distance < 50000
                          ? SDM50_VALID : SDM50_NO_TARGET;
            portEXIT_CRITICAL_ISR(&sample_lock);
        } else if (frame[1] == 0x8a && frame[2] == 2 &&
                   frame[4] == 0 && (frame[3] == 0 || frame[3] == 2)) {
            bool matched;
            portENTER_CRITICAL_ISR(&sample_lock);
            latest.status = frame[3] == 0 ? SDM50_STOPPED : SDM50_WAITING;
            waiting_since_us = esp_timer_get_time();
            matched = pending_command && frame[3] == expected_mode;
            if (matched) {
                pending_command = false;
            }
            portEXIT_CRITICAL_ISR(&sample_lock);
            if (matched) {
                xSemaphoreGiveFromISR(command_ack, woken);
            }
        }
        /* 错位恢复后可能已含下一帧的前几个字节。 */
        frame_size -= required;
        for (size_t i = 0; i < frame_size; ++i) {
            frame[i] = frame[i + required];
        }
    }
}

static bool on_receive(uhci_controller_handle_t handle,
                       const uhci_rx_event_data_t *event, void *context)
{
    (void)handle;
    (void)context;
    BaseType_t woken = pdFALSE;
    if (event->data == NULL) {
        frame_size = 0;
        portENTER_CRITICAL_ISR(&sample_lock);
        ++latest.rx_errors;
        latest.status = SDM50_RX_ERROR;
        portEXIT_CRITICAL_ISR(&sample_lock);
    } else {
        for (size_t i = 0; i < event->recv_size; ++i) {
            parse_byte(event->data[i], &woken);
        }
    }
    if (event->flags.totally_received || event->data == NULL) {
        vTaskNotifyGiveFromISR(rx_task, &woken);
    }
    return woken == pdTRUE;
}

static void receive_task(void *argument)
{
    (void)argument;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* 同核运行，保证回调返回后才重新挂载；不在 ISR 调用普通驱动 API。 */
        esp_err_t result = uhci_receive(controller, rx_buffer, sizeof(rx_buffer));
        if (result != ESP_OK) {
            portENTER_CRITICAL(&sample_lock);
            ++latest.rx_errors;
            latest.status = SDM50_RX_ERROR;
            portEXIT_CRITICAL(&sample_lock);
            ESP_LOGE(TAG, "DMA receive stopped: %s", esp_err_to_name(result));
            vTaskSuspend(NULL);
        }
    }
}

esp_err_t sdm50_init(void)
{
    if (initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t ret = ESP_ERR_NO_MEM;
    command_lock = xSemaphoreCreateMutex();
    command_ack = xSemaphoreCreateBinary();
    if (!command_lock || !command_ack) {
        goto fail;
    }
    const uart_config_t uart_config = {
        .baud_rate = 460800,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    /* 不安装普通 UART ring-buffer 驱动，避免与 UHCI 争用 RX FIFO。 */
    ESP_GOTO_ON_ERROR(uart_param_config(UART_NUM_1, &uart_config), fail, TAG, "UART config");
    ESP_GOTO_ON_ERROR(uart_set_pin(UART_NUM_1, PIN_ESP_TXD1, PIN_ESP_RXD1,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), fail, TAG, "UART pins");
    ESP_GOTO_ON_ERROR(uart_set_rx_timeout(UART_NUM_1, 4), fail, TAG, "UART idle timeout");
    const uhci_controller_config_t dma_config = {
        .uart_port = UART_NUM_1,
        .tx_trans_queue_depth = 1,
        .max_transmit_size = sizeof(tx_buffer),
        .max_receive_internal_mem = sizeof(rx_buffer),
        .dma_burst_size = 0,
        .max_packet_receive = RX_SIZE,
        .rx_eof_flags = {.idle_eof = 1, .length_eof = 1},
    };
    ESP_GOTO_ON_ERROR(uhci_new_controller(&dma_config, &controller), fail, TAG, "UHCI init");
    const uhci_event_callbacks_t callbacks = {.on_rx_trans_event = on_receive};
    ESP_GOTO_ON_ERROR(uhci_register_event_callbacks(controller, &callbacks, NULL), fail, TAG, "DMA callback");
    if (xTaskCreatePinnedToCore(receive_task, "sdm50_rx", 3072, NULL, 12,
                                &rx_task, xPortGetCoreID()) != pdPASS) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    waiting_since_us = esp_timer_get_time();
    ESP_GOTO_ON_ERROR(uhci_receive(controller, rx_buffer, sizeof(rx_buffer)), fail, TAG, "DMA receive");
    initialized = true;
    return ESP_OK;

fail:
    if (rx_task) {
        vTaskDelete(rx_task);
        rx_task = NULL;
    }
    if (controller) {
        uhci_del_controller(controller);
        controller = NULL;
    }
    if (command_ack) {
        vSemaphoreDelete(command_ack);
        command_ack = NULL;
    }
    if (command_lock) {
        vSemaphoreDelete(command_lock);
        command_lock = NULL;
    }
    return ret;
}

static esp_err_t set_measuring(bool enabled)
{
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    const TickType_t timeout = pdMS_TO_TICKS(COMMAND_TIMEOUT_MS);
    if (xSemaphoreTake(command_lock, timeout) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    /* 上次 TX 超时也不能覆盖仍由 DMA 持有的静态缓冲。 */
    esp_err_t result = uhci_wait_all_tx_transaction_done(controller, COMMAND_TIMEOUT_MS);
    if (result != ESP_OK) {
        goto done;
    }
    xSemaphoreTake(command_ack, 0);
    const uint8_t mode = enabled ? 2 : 0;
    tx_buffer[0] = 0x5a;
    tx_buffer[1] = 0x0a;
    tx_buffer[2] = 2;
    tx_buffer[3] = mode;
    tx_buffer[4] = 0;
    tx_buffer[5] = (uint8_t)~(0x0a + 2 + mode);
    portENTER_CRITICAL(&sample_lock);
    expected_mode = mode;
    pending_command = true;
    portEXIT_CRITICAL(&sample_lock);
    result = uhci_transmit(controller, tx_buffer, 6);
    if (result == ESP_OK && xSemaphoreTake(command_ack, timeout) != pdTRUE) {
        result = ESP_ERR_TIMEOUT;
    }
    portENTER_CRITICAL(&sample_lock);
    pending_command = false;
    portEXIT_CRITICAL(&sample_lock);
done:
    xSemaphoreGive(command_lock);
    return result;
}

esp_err_t sdm50_start(void)
{
    return set_measuring(true);
}

esp_err_t sdm50_stop(void)
{
    return set_measuring(false);
}

esp_err_t sdm50_get_sample(sdm50_sample_t *sample)
{
    if (!sample) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    portENTER_CRITICAL(&sample_lock);
    *sample = latest;
    const int64_t reference = sample->status == SDM50_WAITING
                            ? waiting_since_us : sample->timestamp_us;
    portEXIT_CRITICAL(&sample_lock);
    if ((sample->status == SDM50_WAITING || sample->status == SDM50_VALID ||
         sample->status == SDM50_NO_TARGET) && esp_timer_get_time() - reference > STALE_US) {
        sample->status = SDM50_STALE;
    }
    return ESP_OK;
}
