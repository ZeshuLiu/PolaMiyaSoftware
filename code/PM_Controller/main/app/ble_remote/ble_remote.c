#include "ble_remote.h"

#include "focus_position.h"
#include "wifi_pair.h"
#include "esp_attr.h"
#include "esp_bt.h"
#include "esp_intr_alloc.h"
#include "esp_ipc.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "soc/interrupts.h"
#include <stdio.h>
#include <string.h>

#define REMOTE_UUID(last) BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0, \
                                          0x93, 0xf3, 0xa3, 0xb5, last, 0x00, 0x40, 0x7e)
#define REMOTE_VERSION 2U
#define STATUS_BYTES 17U
#define COMMAND_BYTES 7U
#define MAX_FOCUS_MM 20000U
#define PAIR_TIMEOUT_MS 35000U

enum {
    RANGE_VALID = 1 << 0, RANGE_NO_TARGET = 1 << 1,
    ACTUAL_VALID = 1 << 2, ACTUAL_INFINITY = 1 << 3,
    TARGET_VALID = 1 << 4, TARGET_INFINITY = 1 << 5,
    FOCUS_READY = 1 << 6, LINK_BONDED = 1 << 7,
};
enum { FOCUS_ACCEPTED = 1, FOCUS_RANGE, FOCUS_BUSY, FOCUS_UNAVAILABLE };
typedef struct {
    uint32_t mm;
    uint16_t sequence;
} focus_request_t;

static const char *TAG = "ble_remote";
static const ble_uuid128_t service_uuid = REMOTE_UUID(0x01);
static const ble_uuid128_t status_uuid = REMOTE_UUID(0x02);
static const ble_uuid128_t focus_uuid = REMOTE_UUID(0x03);
static uint16_t status_handle;
static uint8_t address_type;
static char device_name[16];
static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static struct {
    bool active, stop, authenticated, subscribed, pending;
    bool bonded, advertise_requested, security_requested, disconnect_requested, disconnecting;
    uint16_t connection, last_sequence;
    TickType_t connected_at;
    uint32_t session, passkey;
    uint8_t result;
    uint8_t status[STATUS_BYTES];
    focus_request_t request;
    const char *message;
} remote = {.connection = BLE_HS_CONN_HANDLE_NONE, .message = "READY"};

/* Espressif's NimBLE examples register the SDK-provided store callbacks. */
void ble_store_config_init(void);

static void IRAM_ATTR probe_isr(void *arg)
{
    (void)arg;
}

static void probe_controller_irq(void *arg)
{
    esp_err_t *result = arg;
    intr_handle_t handle = NULL;
    int flags = ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_INTRDISABLED;
#if !CONFIG_BT_CTRL_RUN_IN_FLASH_ONLY
    flags |= ESP_INTR_FLAG_IRAM;
#endif
    *result = esp_intr_alloc(ETS_RWBLE_INTR_SOURCE, flags, probe_isr, NULL, &handle);
    if (*result == ESP_OK) *result = esp_intr_free(handle);
}

esp_err_t ble_controller_preflight(void)
{
    /* Check the non-shared level-3 IRQ on its owning core. This is a probe,
     * not a reservation for the upcoming NimBLE controller initialization. */
    if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_IDLE)
        return ESP_ERR_INVALID_STATE;
    esp_err_t result = ESP_FAIL;
#if CONFIG_FREERTOS_UNICORE
    probe_controller_irq(&result);
#else
    const esp_err_t ipc = esp_ipc_call_blocking(CONFIG_BT_CTRL_PINNED_TO_CORE,
                                              probe_controller_irq, &result);
    if (ipc != ESP_OK) return ipc;
#endif
    return result;
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value)
{
    put16(p, (uint16_t)value);
    put16(p + 2, (uint16_t)(value >> 16));
}

static int read_status(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctx, void *arg)
{
    (void)conn; (void)attr; (void)arg;
    uint8_t frame[STATUS_BYTES];
    portENTER_CRITICAL(&lock);
    memcpy(frame, remote.status, sizeof(frame));
    if (!remote.authenticated || remote.stop) frame[1] &= ~FOCUS_READY;
    if (remote.bonded && !remote.stop) frame[1] |= LINK_BONDED;
    put16(frame + 14, remote.last_sequence);
    frame[16] = remote.result;
    portEXIT_CRITICAL(&lock);
    return os_mbuf_append(ctx->om, frame, sizeof(frame)) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int write_focus(uint16_t conn, uint16_t attr, struct ble_gatt_access_ctxt *ctx, void *arg)
{
    (void)attr; (void)arg;
    uint8_t frame[COMMAND_BYTES];
    uint16_t length;
    if (ble_hs_mbuf_to_flat(ctx->om, frame, sizeof(frame), &length) ||
        length != sizeof(frame)) return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    const focus_request_t request = {
        .sequence = (uint16_t)(frame[1] | (uint16_t)frame[2] << 8),
        .mm = (uint32_t)frame[3] | (uint32_t)frame[4] << 8 |
              (uint32_t)frame[5] << 16 | (uint32_t)frame[6] << 24,
    };
    if (frame[0] != REMOTE_VERSION || !request.sequence) return BLE_ATT_ERR_VALUE_NOT_ALLOWED;
    int result = 0;
    portENTER_CRITICAL(&lock);
    if (remote.stop || !remote.authenticated || conn != remote.connection)
        result = BLE_ATT_ERR_INSUFFICIENT_AUTHEN;
    else if (remote.pending)
        result = BLE_ATT_ERR_INSUFFICIENT_RES;
    else if (request.sequence != remote.last_sequence) {
        remote.request = request;
        remote.pending = true;
    }
    portEXIT_CRITICAL(&lock);
    return result;
}

static const struct ble_gatt_svc_def services[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY, .uuid = &service_uuid.u,
     .characteristics = (const struct ble_gatt_chr_def[]) {
         {.uuid = &status_uuid.u, .access_cb = read_status, .val_handle = &status_handle,
          .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY},
         {.uuid = &focus_uuid.u, .access_cb = write_focus,
          .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_AUTHEN},
         {0},
     }},
    {0},
};

static int gap_event(struct ble_gap_event *event, void *arg);

static void advertise(void)
{
    portENTER_CRITICAL(&lock);
    const bool stopping = remote.stop;
    portEXIT_CRITICAL(&lock);
    if (stopping) return;
    const struct ble_hs_adv_fields adv = {
        .flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP,
        .name = (const uint8_t *)device_name, .name_len = (uint8_t)strlen(device_name),
        .name_is_complete = 1,
    };
    const struct ble_hs_adv_fields response = {
        .uuids128 = &service_uuid, .num_uuids128 = 1, .uuids128_is_complete = 1,
    };
    const struct ble_gap_adv_params parameters = {
        .conn_mode = BLE_GAP_CONN_MODE_UND, .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    int rc = ble_gap_adv_set_fields(&adv);
    if (!rc) rc = ble_gap_adv_rsp_set_fields(&response);
    if (!rc) rc = ble_gap_adv_start(address_type, NULL, BLE_HS_FOREVER, &parameters, gap_event, NULL);
    if (rc == BLE_HS_ENOTSYNCED) return; /* host_sync schedules another attempt. */
    if (rc == BLE_HS_EALREADY) rc = 0;
    portENTER_CRITICAL(&lock);
    if (!remote.stop) {
        if (rc) remote.message = "BLE ERROR";
        else if (strcmp(remote.message, "PAIR FAILED") != 0) remote.message = "ADVERTISING";
    }
    if (rc) remote.stop = true;
    portEXIT_CRITICAL(&lock);
    if (rc) ESP_LOGW(TAG, "BLE advertising failed: %d", rc);
}

static void host_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &address_type);
    if (!rc) {
        portENTER_CRITICAL(&lock);
        remote.advertise_requested = true;
        portEXIT_CRITICAL(&lock);
    } else {
        portENTER_CRITICAL(&lock);
        remote.message = "BLE ERROR";
        remote.stop = true;
        portEXIT_CRITICAL(&lock);
        ESP_LOGW(TAG, "BLE address unavailable: %d", rc);
    }
}

static void clear_connection(void)
{
    remote.connection = BLE_HS_CONN_HANDLE_NONE;
    remote.authenticated = false;
    remote.bonded = false;
    remote.subscribed = false;
    remote.pending = false;
    remote.security_requested = false;
    remote.disconnect_requested = false;
    remote.disconnecting = false;
    remote.last_sequence = 0;
    remote.result = 0;
    ++remote.session;
}

static void host_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset: %d", reason);
    portENTER_CRITICAL(&lock);
    clear_connection();
    remote.advertise_requested = false;
    portEXIT_CRITICAL(&lock);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
        const TickType_t now = xTaskGetTickCount();
        portENTER_CRITICAL(&lock);
        if (event->connect.status) {
            remote.advertise_requested = true;
        } else {
            remote.connection = event->connect.conn_handle;
            remote.connected_at = now;
            remote.security_requested = true;
            if (!remote.stop) remote.message = "PAIRING...";
        }
        portEXIT_CRITICAL(&lock);
        break;
    }
    case BLE_GAP_EVENT_PASSKEY_ACTION: {
        if (event->passkey.params.action != BLE_SM_IOACT_DISP) return BLE_HS_EINVAL;
        struct ble_sm_io io = {.action = BLE_SM_IOACT_DISP, .passkey = esp_random() % 1000000U};
        portENTER_CRITICAL(&lock);
        remote.passkey = io.passkey;
        if (!remote.stop) remote.message = "PIN";
        portEXIT_CRITICAL(&lock);
        const int rc = ble_sm_inject_io(event->passkey.conn_handle, &io);
        if (rc) ESP_LOGW(TAG, "Passkey submission failed: %d", rc);
        return rc;
    }
    case BLE_GAP_EVENT_ENC_CHANGE: {
        const uint16_t conn = event->enc_change.conn_handle;
        struct ble_gap_conn_desc desc;
        const bool verified = !event->enc_change.status && !ble_gap_conn_find(conn, &desc) &&
                              desc.sec_state.encrypted && desc.sec_state.authenticated;
        portENTER_CRITICAL(&lock);
        if (!remote.stop && remote.connection == conn && !remote.disconnecting) {
            remote.authenticated = verified;
            remote.bonded = verified && desc.sec_state.bonded;
            remote.disconnect_requested = !verified;
            remote.message = verified ? "CONNECTED" : "PAIR FAILED";
        }
        portEXIT_CRITICAL(&lock);
        if (!verified) ESP_LOGW(TAG, "BLE authentication failed: %d", event->enc_change.status);
        break;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        /* A phone that forgot its keys may pair again at the same strength.
         * Only that phone's stale record is removed; PIN auth still applies. */
        const struct ble_gap_repeat_pairing *pairing = &event->repeat_pairing;
        struct ble_gap_conn_desc desc;
        if (!pairing->new_authenticated || !pairing->new_sc || !pairing->new_bonding ||
            pairing->new_key_size < pairing->cur_key_size ||
            ble_gap_conn_find(pairing->conn_handle, &desc) ||
            ble_store_util_delete_peer(&desc.peer_id_addr))
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    case BLE_GAP_EVENT_DISCONNECT:
        portENTER_CRITICAL(&lock);
        if (!remote.stop && !remote.authenticated) remote.message = "PAIR FAILED";
        clear_connection();
        remote.advertise_requested = true;
        portEXIT_CRITICAL(&lock);
        ESP_LOGI(TAG, "BLE disconnected: %d", event->disconnect.reason);
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == status_handle) {
            portENTER_CRITICAL(&lock);
            remote.subscribed = event->subscribe.cur_notify;
            portEXIT_CRITICAL(&lock);
        }
        break;
    default:
        break;
    }
    return 0;
}

static void host_task(void *worker)
{
    nimble_port_run();
    xTaskNotifyGive(worker);
    vTaskDelete(NULL);
}

static void remote_task(void *arg)
{
    (void)arg;
    bool initialized = false;
    struct ble_hs_cfg previous_cfg;
    uint8_t mac[6];
    esp_err_t error = nvs_flash_init();
    if (error == ESP_OK) error = esp_read_mac(mac, ESP_MAC_BT);
    if (error == ESP_OK) error = ble_controller_preflight();
    if (error == ESP_OK) error = nimble_port_init();
    if (error != ESP_OK) goto done;
    initialized = true;
    previous_cfg = ble_hs_cfg;
    snprintf(device_name, sizeof(device_name), "POLA_%02X%02X%02X", mac[3], mac[4], mac[5]);
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_store_config_init();
    ble_hs_cfg.sync_cb = host_sync;
    ble_hs_cfg.reset_cb = host_reset;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_HS_IO_DISPLAY_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_sc_only = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
#if !CONFIG_BT_NIMBLE_NVS_PERSIST
    ESP_LOGW(TAG, "BLE bond persistence disabled; enable BT_NIMBLE_NVS_PERSIST");
#endif
    if (ble_svc_gap_device_name_set(device_name) || ble_gatts_count_cfg(services) ||
        ble_gatts_add_svcs(services)) {
        error = ESP_FAIL;
        goto done;
    }
    if (xTaskCreatePinnedToCore(host_task, "nimble_host", NIMBLE_HS_STACK_SIZE,
                               xTaskGetCurrentTaskHandle(), configMAX_PRIORITIES - 4,
                               NULL, NIMBLE_CORE) != pdPASS) {
        error = ESP_ERR_NO_MEM;
        goto done;
    }
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(250));
        const TickType_t now = xTaskGetTickCount();
        portENTER_CRITICAL(&lock);
        const bool connected = remote.authenticated;
        const uint16_t conn = remote.connection;
        const bool pairing_timed_out = conn != BLE_HS_CONN_HANDLE_NONE && !connected &&
            !remote.disconnecting && now - remote.connected_at >= pdMS_TO_TICKS(PAIR_TIMEOUT_MS);
        const bool disconnect = !remote.disconnecting &&
            (remote.disconnect_requested || pairing_timed_out);
        const bool security = remote.security_requested && !disconnect;
        const bool restart_adv = remote.advertise_requested && conn == BLE_HS_CONN_HANDLE_NONE;
        remote.disconnecting |= disconnect;
        remote.security_requested = false;
        remote.advertise_requested = false;
        if (disconnect) remote.message = "PAIR FAILED";
        const bool stopping = remote.stop;
        const bool notify = connected && remote.subscribed;
        portEXIT_CRITICAL(&lock);
        if (stopping) break;
        /* Pairing callbacks hand off disconnect and advertising to this task. */
        if (disconnect) {
            if (pairing_timed_out) ESP_LOGW(TAG, "BLE authentication timed out");
            const int rc = ble_gap_terminate(conn, BLE_ERR_REM_USER_CONN_TERM);
            if (rc && rc != BLE_HS_ENOTCONN && rc != BLE_HS_EALREADY) {
                ESP_LOGW(TAG, "BLE disconnect failed: %d", rc);
                ble_hs_sched_reset(rc);
            }
        }
        if (security) {
            const int rc = ble_gap_security_initiate(conn);
            if (rc && rc != BLE_HS_ENOTCONN && rc != BLE_HS_EALREADY) {
                ESP_LOGW(TAG, "BLE security request failed: %d", rc);
                portENTER_CRITICAL(&lock);
                if (remote.connection == conn) remote.disconnect_requested = true;
                portEXIT_CRITICAL(&lock);
            }
        }
        if (restart_adv) advertise();
        if (notify) ble_gatts_chr_updated(status_handle);
    }
    int rc = nimble_port_stop();
    while (rc) {
        ESP_LOGW(TAG, "NimBLE stop failed: %d", rc);
        vTaskDelay(pdMS_TO_TICKS(1000));
        rc = nimble_port_stop();
    }
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

done:
    if (initialized) {
        const esp_err_t deinit = nimble_port_deinit();
        ble_hs_cfg = previous_cfg;
        if (error == ESP_OK) error = deinit;
    }
    if (error != ESP_OK) ESP_LOGW(TAG, "BLE remote failed: %s", esp_err_to_name(error));
    portENTER_CRITICAL(&lock);
    clear_connection();
    remote.active = false;
    if (error != ESP_OK) remote.message = "BLE ERROR";
    else if (strcmp(remote.message, "STOPPING") == 0) remote.message = "READY";
    portEXIT_CRITICAL(&lock);
    vTaskDelete(NULL);
}

esp_err_t ble_remote_start(void)
{
    if (wifi_pair_active() || ble_remote_active()) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&lock);
    remote.active = true;
    remote.stop = false;
    remote.advertise_requested = false;
    remote.message = "STARTING";
    memset(remote.status, 0, sizeof(remote.status));
    remote.status[0] = REMOTE_VERSION;
    portEXIT_CRITICAL(&lock);
    if (xTaskCreate(remote_task, "ble_remote", 4096, NULL, 3, NULL) == pdPASS) return ESP_OK;
    portENTER_CRITICAL(&lock);
    remote.active = false;
    remote.message = "BLE ERROR";
    portEXIT_CRITICAL(&lock);
    return ESP_ERR_NO_MEM;
}

void ble_remote_stop(void)
{
    portENTER_CRITICAL(&lock);
    if (remote.active && !remote.stop) {
        remote.stop = true;
        remote.message = "STOPPING";
    }
    portEXIT_CRITICAL(&lock);
}

bool ble_remote_active(void)
{
    portENTER_CRITICAL(&lock);
    const bool active = remote.active;
    portEXIT_CRITICAL(&lock);
    return active;
}

bool ble_remote_connected(void)
{
    portENTER_CRITICAL(&lock);
    const bool connected = remote.authenticated;
    portEXIT_CRITICAL(&lock);
    return connected;
}

const char *ble_remote_status(void)
{
    static char pin_text[16];
    portENTER_CRITICAL(&lock);
    const char *message = remote.message;
    const unsigned passkey = remote.passkey;
    portEXIT_CRITICAL(&lock);
    if (strcmp(message, "PIN") != 0) return message;
    snprintf(pin_text, sizeof(pin_text), "PIN %06u", passkey);
    return pin_text;
}

bool ble_remote_update(const sdm50_sample_t *sample, const focus_distance_t *focus,
                       bool actual_valid, uint32_t actual_mm, bool actual_infinity,
                       bool local_controls_idle)
{
    portENTER_CRITICAL(&lock);
    if (!remote.active || remote.stop) {
        portEXIT_CRITICAL(&lock);
        return false;
    }
    const bool pending = remote.pending && remote.authenticated;
    const uint32_t session = remote.session;
    const focus_request_t request = remote.request;
    portEXIT_CRITICAL(&lock);

    focus_calibration_t cal;
    const bool calibrated = focus_calibration_get(&cal) == ESP_OK;
    const bool available = calibrated && focus->state == FOCUS_DISTANCE_READY;
    const bool idle = local_controls_idle && focus_position_idle();
    uint8_t result = 0;
    bool accepted = false;
    if (pending) {
        int32_t raw;
        if (!available) result = FOCUS_UNAVAILABLE;
        else if (!idle) result = FOCUS_BUSY;
        else if (request.mm > MAX_FOCUS_MM ||
                 !focus_distance_to_raw_with_calibration(&cal, request.mm, !request.mm, &raw))
            result = FOCUS_RANGE;
        else {
            /* This is the admission point. A disconnect after this point does
             * not cancel a target already accepted by the focus task. */
            portENTER_CRITICAL(&lock);
            const bool current = !remote.stop && remote.session == session && remote.authenticated;
            portEXIT_CRITICAL(&lock);
            if (current) {
                accepted = focus_position_set_target(raw) == ESP_OK;
                result = accepted ? FOCUS_ACCEPTED : FOCUS_UNAVAILABLE;
            }
        }
    }

    uint8_t frame[STATUS_BYTES] = {REMOTE_VERSION};
    if (sample->status == SDM50_VALID) {
        frame[1] |= RANGE_VALID;
        put16(frame + 2, sample->distance_mm);
    } else if (sample->status == SDM50_NO_TARGET) frame[1] |= RANGE_NO_TARGET;
    if (actual_valid) {
        frame[1] |= ACTUAL_VALID | (actual_infinity ? ACTUAL_INFINITY : 0);
        put32(frame + 4, actual_mm);
    }
    if (accepted || (available && focus->distance_valid)) {
        const bool infinity = accepted ? !request.mm : focus->infinity;
        frame[1] |= TARGET_VALID | (infinity ? TARGET_INFINITY : 0);
        put32(frame + 8, accepted ? request.mm : focus->distance_mm);
    }
    if (available && idle && !accepted) frame[1] |= FOCUS_READY;
    if (calibrated) put16(frame + 12, (uint16_t)cal.min_distance_mm);

    portENTER_CRITICAL(&lock);
    if (remote.active && !remote.stop) {
        memcpy(remote.status, frame, sizeof(frame));
        if (pending && remote.session == session) {
            remote.pending = false;
            remote.last_sequence = request.sequence;
            remote.result = result;
        }
    }
    portEXIT_CRITICAL(&lock);
    return accepted;
}
