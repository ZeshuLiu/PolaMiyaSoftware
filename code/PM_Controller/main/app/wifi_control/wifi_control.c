#include "wifi_control.h"
#include "wifi_web.h"
#include "wifi_networks.h"
#include "wifi_pair.h"
#include "settings.h"
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static bool initialized;
static atomic_bool enabled, has_network, got_ip, provisioning, command_pending;
static atomic_int command_error;
static bool nvs_ready, netif_ready, event_ready, station_created, handler_registered;
static bool ip_handler_registered, disconnect_handler_registered, stop_handler_registered;

/* Only the network worker accesses this snapshot. The event loop queues
 * small events and never reads NVS or builds a Wi-Fi configuration. */
typedef enum {
    CONNECTION_START, CONNECTION_DISCONNECTED, WIFI_ON, WIFI_OFF, WEB_UPDATE,
} network_command_kind_t;
typedef struct { SemaphoreHandle_t done; esp_err_t result; } switch_completion_t;
typedef struct {
    network_command_kind_t kind;
    unsigned generation;
    switch_completion_t *completion; /* Non-NULL only for the pairing task. */
} network_command_t;
static QueueHandle_t network_commands;
static atomic_uint connection_generation;

static wifi_network_t networks[WIFI_NETWORK_LIMIT];
static size_t network_count;
static size_t next_network;
static unsigned remaining_attempts;

static esp_err_t set_enabled(bool requested);

static esp_err_t connect_saved(size_t index)
{
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, networks[index].ssid, networks[index].ssid_length);
    memcpy(config.sta.password, networks[index].password, networks[index].password_length);
    esp_err_t error = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (error == ESP_OK) error = esp_wifi_set_config(WIFI_IF_STA, &config);
    memset(&config, 0, sizeof(config));
    return error == ESP_OK ? esp_wifi_connect() : error;
}

static void process_connection_event(const network_command_t *event, unsigned *handled_generation)
{
    if (!atomic_load(&enabled) || atomic_load(&provisioning) ||
        event->generation != atomic_load(&connection_generation)) return;
    if (*handled_generation != event->generation) {
        network_count = 0;
        remaining_attempts = 0;
        *handled_generation = event->generation;
    }
    if (event->kind == CONNECTION_START) {
        network_count = 0;
        remaining_attempts = 0;
        const esp_err_t error = wifi_networks_load(networks, &network_count);
        if (error != ESP_OK) {
            ESP_LOGW("wifi", "Saved network read failed: %s", esp_err_to_name(error));
            return;
        }
        if (!atomic_load(&enabled) || atomic_load(&provisioning) ||
            event->generation != atomic_load(&connection_generation)) return;
        if (!network_count) return;
        next_network = network_count - 1;
        remaining_attempts = (unsigned)network_count - 1;
    } else {
        if (!remaining_attempts || !network_count) return;
        --remaining_attempts;
        next_network = next_network ? next_network - 1 : network_count - 1;
    }
    const esp_err_t result = connect_saved(next_network);
    if (result != ESP_OK) ESP_LOGW("wifi", "Connect request failed: %s", esp_err_to_name(result));
}

static void network_worker(void *arg)
{
    (void)arg;
    network_command_t event;
    unsigned handled_generation = 0;
    while (xQueueReceive(network_commands, &event, portMAX_DELAY) == pdTRUE) {
        if (event.kind == WIFI_ON || event.kind == WIFI_OFF) {
            esp_err_t result = set_enabled(event.kind == WIFI_ON);
            /* Internal pairing toggles are temporary. Only UI/startup requests
             * persist the preference; do not save unrelated pending edits. */
            if (result == ESP_OK && !event.completion) {
                result = settings_save_wifi_enabled(event.kind == WIFI_ON);
                if (result != ESP_OK) ESP_LOGW("wifi", "Preference save failed: %s", esp_err_to_name(result));
            }
            if (result != ESP_OK) ESP_LOGW("wifi", "Switch or save failed: %s", esp_err_to_name(result));
            if (event.completion) {
                event.completion->result = result;
                xSemaphoreGive(event.completion->done);
            } else {
                atomic_store(&command_error, result);
                atomic_store(&command_pending, false);
            }
        } else if (event.kind == CONNECTION_START || event.kind == CONNECTION_DISCONNECTED) {
            process_connection_event(&event, &handled_generation);
        }
        /* Also reconcile after a non-web event, so queue saturation cannot
         * lose a page-close or network-change request. */
        wifi_web_process();
    }
    vTaskDelete(NULL);
}

static void queue_connection_event(network_command_kind_t kind)
{
    const network_command_t event = {.kind = kind, .generation = atomic_load(&connection_generation)};
    if (xQueueSend(network_commands, &event, 0) != pdTRUE)
        ESP_LOGW("wifi", "Connection event queue full");
}

static void on_station_start(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    if (!atomic_load(&provisioning)) queue_connection_event(CONNECTION_START);
}

static void on_network_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        atomic_store(&got_ip, true);
        const ip_event_got_ip_t *event = data;
        ESP_LOGI("wifi", "LAN address: " IPSTR, IP2STR(&event->ip_info.ip));
        const esp_err_t error = wifi_web_set_network(true);
        if (error != ESP_OK) ESP_LOGW("wifi", "Web start request failed: %s", esp_err_to_name(error));
    } else {
        atomic_store(&got_ip, false);
        const esp_err_t error = wifi_web_set_network(false);
        if (error != ESP_OK) ESP_LOGW("wifi", "Web stop request failed: %s", esp_err_to_name(error));
        if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED &&
            atomic_load(&enabled) && !atomic_load(&provisioning))
            queue_connection_event(CONNECTION_DISCONNECTED);
    }
}

static esp_err_t initialize(void)
{
    esp_err_t error;
    if (!nvs_ready) {
        ESP_LOGI("wifi", "start: NVS init");
        error = nvs_flash_init();
        if (error != ESP_OK) return error; /* Never erase existing NVS automatically. */
        nvs_ready = true;
    }
    if (!netif_ready) {
        ESP_LOGI("wifi", "start: netif init");
        error = esp_netif_init();
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
        netif_ready = true;
    }
    if (!event_ready) {
        ESP_LOGI("wifi", "start: event loop");
        error = esp_event_loop_create_default();
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) return error;
        event_ready = true;
    }
    if (!station_created) {
        ESP_LOGI("wifi", "start: create station");
        if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;
        station_created = true;
    }
    if (!handler_registered) {
        error = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_START, on_station_start, NULL);
        if (error != ESP_OK) return error;
        handler_registered = true;
    }
    if (!ip_handler_registered) {
        error = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_network_event, NULL);
        if (error != ESP_OK) return error;
        ip_handler_registered = true;
    }
    if (!disconnect_handler_registered) {
        error = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, on_network_event, NULL);
        if (error != ESP_OK) return error;
        disconnect_handler_registered = true;
    }
    if (!stop_handler_registered) {
        error = esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_STOP, on_network_event, NULL);
        if (error != ESP_OK) return error;
        stop_handler_registered = true;
    }
    if (!initialized) {
        ESP_LOGI("wifi", "start: driver init; internal free=%u largest=%u",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
        /* Wi-Fi and display DMA compete for scarce internal RAM. Keep the
         * standard defaults as a ceiling, while retaining four static RX
         * buffers and a matching aggregation window for low-rate STA use. */
        if (config.static_rx_buf_num > 4) config.static_rx_buf_num = 4;
        if (config.dynamic_rx_buf_num > 16) config.dynamic_rx_buf_num = 16;
        if (config.tx_buf_type == 1 && config.dynamic_tx_buf_num > 16)
            config.dynamic_tx_buf_num = 16;
        /* PSRAM-first Wi-Fi selects static DMA TX buffers; cap those too. */
        if (config.tx_buf_type == 0 && config.static_tx_buf_num > 4)
            config.static_tx_buf_num = 4;
        if (config.ampdu_rx_enable && config.rx_ba_win > config.static_rx_buf_num)
            config.rx_ba_win = config.static_rx_buf_num;
        ESP_LOGI("wifi", "buffer limits: static_rx=%d dynamic_rx=%d tx_type=%d static_tx=%d dynamic_tx=%d rx_ba=%d",
                 config.static_rx_buf_num, config.dynamic_rx_buf_num, config.tx_buf_type,
                 config.static_tx_buf_num, config.dynamic_tx_buf_num, config.rx_ba_win);
        error = esp_wifi_init(&config);
        if (error != ESP_OK) return error;
        initialized = true;
    }
    return ESP_OK;
}

static esp_err_t set_enabled(bool requested)
{
    if (requested == enabled) return ESP_OK;
    if (!requested) {
        if (atomic_load(&provisioning)) return ESP_ERR_INVALID_STATE;
        atomic_fetch_add(&connection_generation, 1);
        atomic_store(&enabled, false); /* Suppress reconnect while stopping. */
        ESP_LOGI("wifi", "stop: driver");
        const esp_err_t error = esp_wifi_stop();
        if (error == ESP_OK) { atomic_store(&got_ip, false); has_network = false; }
        else atomic_store(&enabled, true);
        return error;
    }
    esp_err_t error = initialize();
    if (error != ESP_OK) return error;
    ESP_LOGI("wifi", "start: set station mode");
    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) return error;
    wifi_network_t saved[WIFI_NETWORK_LIMIT];
    size_t saved_count = 0;
    ESP_LOGI("wifi", "start: load saved networks");
    error = wifi_networks_load(saved, &saved_count);
    memset(saved, 0, sizeof(saved));
    if (error != ESP_OK) return error;
    has_network = saved_count != 0;
    ESP_LOGI("wifi", "start: driver");
    /* STA_START can be delivered before esp_wifi_start() returns. Publish the
     * intended state first so the worker does not discard that event. */
    atomic_fetch_add(&connection_generation, 1);
    atomic_store(&enabled, true);
    error = esp_wifi_start();
    if (error != ESP_OK) { atomic_store(&enabled, false); has_network = false; return error; }
    ESP_LOGI("wifi", "start: complete");
    return ESP_OK;
}

/* Application Wi-Fi on/off requests, including those from pairing, use the
 * network worker. The SDK provisioning manager still owns its own operations.
 * No timeout: completion remains on this caller's stack. */
esp_err_t wifi_control_set_enabled(bool requested)
{
    if (!network_commands) return ESP_ERR_INVALID_STATE;
    switch_completion_t completion = {.done = xSemaphoreCreateBinary(), .result = ESP_FAIL};
    if (!completion.done) return ESP_ERR_NO_MEM;
    const network_command_t event = {.kind = requested ? WIFI_ON : WIFI_OFF, .completion = &completion};
    xQueueSend(network_commands, &event, portMAX_DELAY);
    xSemaphoreTake(completion.done, portMAX_DELAY);
    vSemaphoreDelete(completion.done);
    return completion.result;
}

/* The UI task creates the network worker lazily before any requests. The
 * event loop and provisioning task only use it after that point. */
esp_err_t wifi_control_prepare(void)
{
    if (network_commands) return ESP_OK;
    network_commands = xQueueCreate(8, sizeof(network_command_t));
    if (!network_commands) return ESP_ERR_NO_MEM;
    if (xTaskCreate(network_worker, "wifi_control", 6144, NULL, 2, NULL) != pdPASS) {
        vQueueDelete(network_commands);
        network_commands = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

/* Reconcile HTTP after every queued network event. If the queue is full, an
 * existing event will wake the worker and it will see the latest web state. */
esp_err_t wifi_control_notify_web(void)
{
    if (!network_commands) return ESP_ERR_INVALID_STATE;
    const network_command_t event = {.kind = WEB_UPDATE};
    (void)xQueueSend(network_commands, &event, 0);
    return ESP_OK;
}

esp_err_t wifi_control_request_enabled(bool requested)
{
    if (atomic_load(&provisioning) || wifi_pair_active()) return ESP_ERR_INVALID_STATE;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&command_pending, &expected, true)) return ESP_ERR_INVALID_STATE;
    const esp_err_t result = wifi_control_prepare();
    if (result != ESP_OK) { atomic_store(&command_pending, false); return result; }
    atomic_store(&command_error, ESP_OK);
    const network_command_t event = {.kind = requested ? WIFI_ON : WIFI_OFF};
    if (xQueueSend(network_commands, &event, 0) != pdTRUE) {
        atomic_store(&command_pending, false);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool wifi_control_busy(void) { return atomic_load(&command_pending); }
esp_err_t wifi_control_last_error(void) { return (esp_err_t)atomic_load(&command_error); }

bool wifi_control_is_enabled(void) { return enabled; }
bool wifi_control_has_network(void) { return has_network; }
bool wifi_control_is_connected(void) { return atomic_load(&enabled) && atomic_load(&got_ip); }

bool wifi_control_ip(char *buffer, size_t length)
{
    if (!buffer || !length || !enabled || !atomic_load(&got_ip)) return false;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip;
    if (!netif || esp_netif_get_ip_info(netif, &ip) != ESP_OK || ip.ip.addr == 0) return false;
    const int written = snprintf(buffer, length, IPSTR, IP2STR(&ip.ip));
    return written > 0 && (size_t)written < length;
}

void wifi_control_provisioning(bool active)
{
    atomic_fetch_add(&connection_generation, 1);
    atomic_store(&provisioning, active);
}

void wifi_control_mark_network_saved(void)
{
    atomic_store(&has_network, true);
}
