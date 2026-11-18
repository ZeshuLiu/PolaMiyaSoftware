#include "wifi_pair.h"
#include "wifi_control.h"
#include "wifi_networks.h"
#include "settings.h"
#include "ble_remote.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "soc/interrupts.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "sdkconfig.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_srp.h"
#include "psa/crypto.h"
#include "freertos/semphr.h"
#include "protocomm_ble.h"
#include "protocomm_security.h"
#include "esp_wifi.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PAIR_CREDENTIALS (1U << 0)
#define PAIR_SUCCESS (1U << 1)
#define PAIR_FAIL (1U << 2)
#define PAIR_END (1U << 3)
#define PAIR_CANCEL (1U << 4)
#define PAIR_TIMEOUT_MS 180000U
#define PAIR_SUCCESS_GRACE_MS 30000U

/* Match the default Security 2 Wi-Fi username in Espressif Android provisioning. */
#define PAIR_SECURITY_USERNAME "wifiprov"

static portMUX_TYPE pair_lock = portMUX_INITIALIZER_UNLOCKED;
static atomic_bool active;
static atomic_bool ble_connected;
static atomic_uint pending_flags;
/* Set by UI before starting the provisioning worker. */
static bool was_enabled;
static wifi_sta_config_t received;
static char pin[9], status[24] = "READY";

static void publish_status(const char *text)
{
    portENTER_CRITICAL(&pair_lock);
    snprintf(status, sizeof(status), "%s", text);
    portEXIT_CRITICAL(&pair_lock);
}

static void on_provisioning(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    uint32_t flags = 0;
    if (id == NETWORK_PROV_WIFI_CRED_RECV && data) {
        portENTER_CRITICAL(&pair_lock);
        received = *(const wifi_sta_config_t *)data;
        portEXIT_CRITICAL(&pair_lock);
        flags = PAIR_CREDENTIALS;
    } else if (id == NETWORK_PROV_WIFI_CRED_SUCCESS) {
        ESP_LOGI("wifi_pair", "Wi-Fi received an IP");
        flags = PAIR_SUCCESS;
    } else if (id == NETWORK_PROV_WIFI_CRED_FAIL) {
        const int reason = data ? (int)*(const network_prov_wifi_sta_fail_reason_t *)data : -1;
        ESP_LOGW("wifi_pair", "Wi-Fi credential/connect failure reason=%d", reason);
        flags = PAIR_FAIL;
    } else if (id == NETWORK_PROV_END) {
        ESP_LOGI("wifi_pair", "provisioning service ended");
        flags = PAIR_END;
    }
    if (flags) atomic_fetch_or(&pending_flags, flags);
}

/* Do not print either the one-time PIN or Wi-Fi credentials. */
static void on_pair_transport(void *argument, esp_event_base_t base, int32_t id, void *data)
{
    (void)argument;
    if (base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        const protocomm_ble_event_t *event = data;
        if (id == PROTOCOMM_TRANSPORT_BLE_CONNECTED) {
            atomic_store(&ble_connected, event && event->conn_status == 0);
            ESP_LOGI("wifi_pair", "BLE connected: status=%u", event ? (unsigned)event->conn_status : 0U);
        } else if (id == PROTOCOMM_TRANSPORT_BLE_DISCONNECTED) {
            atomic_store(&ble_connected, false);
            ESP_LOGW("wifi_pair", "BLE disconnected: reason=%u", event ? (unsigned)event->disconnect_reason : 0U);
        }
    } else if (base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        if (id == PROTOCOMM_SECURITY_SESSION_SETUP_OK)
            ESP_LOGI("wifi_pair", "Security 2 session established");
        else if (id == PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS)
            ESP_LOGW("wifi_pair", "Security session invalid parameters");
        else if (id == PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH)
            ESP_LOGW("wifi_pair", "Security 2 credentials mismatch");
    }
}

/* PSA's first AES-GCM setup allocates the shared AES IRQ on the calling
 * core in IDF 6.1. Do it before BLE starts, on CPU1 where IRQ slots remain. */
#if CONFIG_MBEDTLS_HARDWARE_AES && CONFIG_MBEDTLS_AES_USE_INTERRUPT && !CONFIG_FREERTOS_UNICORE
static void IRAM_ATTR aes_irq_probe_isr(void *arg)
{
    (void)arg;
}

static bool aes_prepared;
typedef struct { SemaphoreHandle_t done; psa_status_t status; esp_err_t irq; } aes_prepare_t;
static void prepare_aes_on_cpu1(void *argument)
{
    aes_prepare_t *ctx = argument;
    intr_handle_t probe = NULL;
    ctx->irq = esp_intr_alloc(ETS_AES_INTR_SOURCE,
                              esp_intr_level_to_flags(CONFIG_MBEDTLS_AES_INTERRUPT_LEVEL) | ESP_INTR_FLAG_INTRDISABLED,
                              aes_irq_probe_isr, NULL, &probe);
    if (ctx->irq == ESP_OK) ctx->irq = esp_intr_free(probe);
    if (ctx->irq != ESP_OK) {
        xSemaphoreGive(ctx->done);
        vTaskDelete(NULL);
        return;
    }
    const uint8_t key_bytes[16] = {0};
    mbedtls_svc_key_id_t key = MBEDTLS_SVC_KEY_ID_INIT;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_aead_operation_t op = PSA_AEAD_OPERATION_INIT;
    psa_status_t status = psa_crypto_init();
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attr, 128);
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);
    if (status == PSA_SUCCESS) status = psa_import_key(&attr, key_bytes, sizeof(key_bytes), &key);
    if (status == PSA_SUCCESS) status = psa_aead_encrypt_setup(&op, key, PSA_ALG_GCM);
    (void)psa_aead_abort(&op);
    if (!mbedtls_svc_key_id_is_null(key)) (void)psa_destroy_key(key);
    psa_reset_key_attributes(&attr);
    ctx->status = status;
    xSemaphoreGive(ctx->done);
    vTaskDelete(NULL);
}

static esp_err_t prepare_aes_interrupt(void)
{
    if (aes_prepared) return ESP_OK;
    aes_prepare_t ctx = {.done = xSemaphoreCreateBinary(), .status = PSA_ERROR_GENERIC_ERROR};
    if (!ctx.done) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(prepare_aes_on_cpu1, "aes_prepare", 4096, &ctx, 4, NULL, 1) != pdPASS) {
        vSemaphoreDelete(ctx.done);
        return ESP_ERR_NO_MEM;
    }
    xSemaphoreTake(ctx.done, portMAX_DELAY);
    vSemaphoreDelete(ctx.done);
    if (ctx.irq != ESP_OK) {
        ESP_LOGE("wifi_pair", "AES IRQ preflight on CPU1 failed: %s", esp_err_to_name(ctx.irq));
        return ctx.irq;
    }
    if (ctx.status != PSA_SUCCESS) {
        ESP_LOGE("wifi_pair", "AES preparation failed: PSA %ld", (long)ctx.status);
        return ESP_FAIL;
    }
    aes_prepared = true;
    ESP_LOGI("wifi_pair", "AES-GCM prepared on CPU1");
    return ESP_OK;
}
#else
static esp_err_t prepare_aes_interrupt(void) { return ESP_OK; }
#endif

static void pair_task(void *arg)
{
    (void)arg;
    esp_err_t error;
    bool registered = false, ble_registered = false, security_registered = false;
    bool manager_initialized = false, success = false, irq_unavailable = false;
    char *salt = NULL, *verifier = NULL;
    int verifier_len = 0;
    char service[16];
    uint8_t mac[6];
    network_prov_security2_params_t security = {0};
    wifi_sta_config_t candidate = {0};
    wifi_network_t network = {0};
    network_prov_mgr_config_t manager = {
        .scheme = network_prov_scheme_ble,
        /* Preserve the BLE controller for future device-control features. */
        .scheme_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
        .app_event_handler = NETWORK_PROV_EVENT_HANDLER_NONE,
        .network_prov_wifi_conn_cfg = {.wifi_conn_attempts = 3},
    };
    ESP_LOGI("wifi_pair", "starting Wi-Fi for provisioning");
    error = wifi_control_set_enabled(true);
    if (error != ESP_OK) goto done;
    error = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (error != ESP_OK) goto done;
    snprintf(service, sizeof(service), "PROV_%02X%02X%02X", mac[3], mac[4], mac[5]);
    error = prepare_aes_interrupt();
    if (error != ESP_OK) goto done;
    ESP_LOGI("wifi_pair", "generating Security 2 verifier");
    error = esp_srp_gen_salt_verifier(PAIR_SECURITY_USERNAME, sizeof(PAIR_SECURITY_USERNAME) - 1,
                                       pin, 8, &salt, 16, &verifier, &verifier_len);
    if (error != ESP_OK) goto done;
    if (!salt || !verifier || verifier_len <= 0 || verifier_len > UINT16_MAX) { error = ESP_FAIL; goto done; }
    security.salt = salt; security.salt_len = 16;
    security.verifier = verifier; security.verifier_len = (uint16_t)verifier_len;
    error = esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, on_provisioning, NULL);
    if (error != ESP_OK) goto done;
    registered = true;
    error = esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, on_pair_transport, NULL);
    if (error != ESP_OK) goto done;
    ble_registered = true;
    error = esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, on_pair_transport, NULL);
    if (error != ESP_OK) goto done;
    security_registered = true;
    ESP_LOGI("wifi_pair", "provisioning manager init; internal free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    error = network_prov_mgr_init(manager);
    if (error != ESP_OK) goto done;
    manager_initialized = true;
    /* SDK's automatic stop runs BLE teardown on the default event loop.
     * Keep it active and tear it down from this worker after the phone has
     * had time to read the successful result. */
    error = network_prov_mgr_disable_auto_stop(1000);
    if (error != ESP_OK) goto done;
    error = ble_controller_preflight();
    if (error != ESP_OK) {
        irq_unavailable = true;
        ESP_LOGW("wifi_pair", "BLE interrupt preflight failed: %s", esp_err_to_name(error));
        goto done;
    }
    ESP_LOGI("wifi_pair", "BLE interrupt preflight passed; allocation is not reserved");
    ESP_LOGI("wifi_pair", "starting BLE: internal free=%u largest=%u; DMA free=%u largest=%u; worker stack minimum=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
             (unsigned)uxTaskGetStackHighWaterMark(NULL));
    error = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_2, &security, service, NULL);
    if (error != ESP_OK) goto done;
    char display[24];
    snprintf(display, sizeof(display), "PIN %s", pin);
    publish_status(display);
    const TickType_t started = xTaskGetTickCount();
    TickType_t success_at = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        const uint32_t flags = atomic_exchange(&pending_flags, 0);
        if (flags & PAIR_CREDENTIALS) {
            portENTER_CRITICAL(&pair_lock);
            candidate = received;
            portEXIT_CRITICAL(&pair_lock);
        }
        if (flags & PAIR_SUCCESS) {
            size_t ssid_len = strnlen((const char *)candidate.ssid, sizeof(candidate.ssid));
            size_t pass_len = strnlen((const char *)candidate.password, sizeof(candidate.password));
            if (ssid_len > 0 && ssid_len <= WIFI_NETWORK_SSID_MAX && pass_len <= WIFI_NETWORK_PASSWORD_MAX) {
                network.ssid_length = (uint8_t)ssid_len;
                network.password_length = (uint8_t)pass_len;
                memcpy(network.ssid, candidate.ssid, ssid_len);
                memcpy(network.password, candidate.password, pass_len);
                error = wifi_networks_save(&network);
                if (error == ESP_OK) {
                    success = true;
                    success_at = xTaskGetTickCount();
                    wifi_control_mark_network_saved();
                    /* Successful provisioning leaves Wi-Fi on, including
                     * when pairing started from an off state. */
                    const esp_err_t saved = settings_save_wifi_enabled(true);
                    if (saved == ESP_OK) publish_status("CONNECTED");
                    else {
                        ESP_LOGW("wifi_pair", "Wi-Fi preference save failed: %s", esp_err_to_name(saved));
                        publish_status("SAVE ERROR");
                    }
                }
                else publish_status("SAVE ERROR");
            } else publish_status("SAVE ERROR");
        }
        if (flags & PAIR_FAIL) {
            /* Keep the failure state and BLE service available so the phone
             * can read the result. User can cancel and start a new session. */
            ESP_LOGW("wifi_pair", "Wi-Fi connect failed; waiting for phone to read result");
            publish_status("PAIR FAILED");
        }
        if (flags & PAIR_CANCEL) { publish_status("CANCELLED"); break; }
        if (flags & PAIR_END) break;
        const TickType_t now = xTaskGetTickCount();
        if (success && (TickType_t)(now - success_at) >= pdMS_TO_TICKS(PAIR_SUCCESS_GRACE_MS)) {
            ESP_LOGI("wifi_pair", "success grace elapsed; stopping from worker");
            break;
        }
        if (!success && (TickType_t)(now - started) >= pdMS_TO_TICKS(PAIR_TIMEOUT_MS)) {
            publish_status("PAIR TIMEOUT");
            break;
        }
    }
done:
    if (error != ESP_OK) {
        ESP_LOGW("wifi_pair", "provisioning failed: %s; internal free=%u largest=%u; DMA free=%u largest=%u; worker stack minimum=%u",
                 esp_err_to_name(error),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA),
                 (unsigned)uxTaskGetStackHighWaterMark(NULL));
        publish_status(irq_unavailable ? "IRQ ERROR" : "PAIR ERROR");
    }
    if (manager_initialized) {
        ESP_LOGI("wifi_pair", "cleanup: manager deinit begin");
        const esp_err_t cleanup = network_prov_mgr_deinit();
        ESP_LOGI("wifi_pair", "cleanup: manager deinit returned %s", esp_err_to_name(cleanup));
    }
    ESP_LOGI("wifi_pair", "cleanup: unregister handlers");
    if (security_registered)
        (void)esp_event_handler_unregister(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, on_pair_transport);
    if (ble_registered)
        (void)esp_event_handler_unregister(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, on_pair_transport);
    if (registered)
        (void)esp_event_handler_unregister(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, on_provisioning);
    free(salt); free(verifier);
    memset(&candidate, 0, sizeof(candidate));
    memset(&network, 0, sizeof(network));
    portENTER_CRITICAL(&pair_lock);
    memset(&received, 0, sizeof(received));
    portEXIT_CRITICAL(&pair_lock);
    wifi_control_provisioning(false);
    /* On cancellation/failure restore the previous on/off state. */
    if (!success) {
        ESP_LOGI("wifi_pair", "cleanup: restore Wi-Fi state");
        if (wifi_control_set_enabled(false) == ESP_OK && was_enabled)
            (void)wifi_control_set_enabled(true);
    }
    ESP_LOGI("wifi_pair", "cleanup: complete");
    atomic_store(&ble_connected, false);
    atomic_store(&active, false);
    vTaskDelete(NULL);
}

esp_err_t wifi_pair_start(void)
{
    if (ble_remote_active()) return ESP_ERR_INVALID_STATE;
    if (atomic_exchange(&active, true)) return ESP_ERR_INVALID_STATE;
    if (wifi_control_busy()) { atomic_store(&active, false); return ESP_ERR_INVALID_STATE; }
    const esp_err_t prepared = wifi_control_prepare();
    if (prepared != ESP_OK) { atomic_store(&active, false); return prepared; }
    atomic_store(&pending_flags, 0);
    atomic_store(&ble_connected, false);
    was_enabled = wifi_control_is_enabled();
    wifi_control_provisioning(true);
    portENTER_CRITICAL(&pair_lock);
    snprintf(pin, sizeof(pin), "%08lX", (unsigned long)esp_random());
    snprintf(status, sizeof(status), "STARTING");
    portEXIT_CRITICAL(&pair_lock);
    ESP_LOGI("wifi_pair", "creating worker: stack=8192 internal free=%u largest=%u; PSRAM free=%u largest=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (xTaskCreate(pair_task, "wifi_pair", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE("wifi_pair", "worker allocation failed");
        wifi_control_provisioning(false);
        atomic_store(&active, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void wifi_pair_cancel(void)
{
    if (atomic_load(&active)) atomic_fetch_or(&pending_flags, PAIR_CANCEL);
}

bool wifi_pair_active(void) { return atomic_load(&active); }
bool wifi_pair_ble_connected(void) { return atomic_load(&ble_connected); }
const char *wifi_pair_status(void)
{
    static char copy[sizeof(status)];
    portENTER_CRITICAL(&pair_lock);
    memcpy(copy, status, sizeof(copy));
    portEXIT_CRITICAL(&pair_lock);
    return copy;
}

/* ESP-IDF 6.1.0 NimBLE declares the zero-CoC deinit as C `inline`,
 * but ble_l2cap_deinit can still emit an external call. Supply the intended
 * no-op only when both CoC and EATT are disabled; real CoC builds use IDF's
 * implementation. Remove this when the SDK provides a static-inline stub. */
#if ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(6, 1, 0) && \
    CONFIG_BT_NIMBLE_ENABLED && CONFIG_BT_NIMBLE_STATIC_TO_DYNAMIC && \
    CONFIG_BT_NIMBLE_L2CAP_COC_MAX_NUM == 0 && CONFIG_BT_NIMBLE_EATT_CHAN_NUM == 0
void ble_l2cap_coc_deinit(void)
{
}
#endif
