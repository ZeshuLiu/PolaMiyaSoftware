#include "wifi_networks.h"
#include <string.h>
#include <stdbool.h>
#include "nvs.h"

#define NETWORKS_VERSION 1U
#define NETWORKS_NAMESPACE "pm_wifi"
#define NETWORKS_KEY "networks"
/* Byte-encoded, versioned blob: version, count, then three fixed records.
 * A single NVS blob commit makes the list update one storage operation. */
#define RECORD_BYTES (2U + WIFI_NETWORK_SSID_MAX + WIFI_NETWORK_PASSWORD_MAX)
#define BLOB_BYTES (2U + WIFI_NETWORK_LIMIT * RECORD_BYTES)

static bool valid_network(const wifi_network_t *network)
{
    if (!network || network->ssid_length == 0 || network->ssid_length > WIFI_NETWORK_SSID_MAX ||
        network->password_length > WIFI_NETWORK_PASSWORD_MAX ||
        (network->password_length != 0 && network->password_length < 8)) return false;
    for (unsigned i = 0; i < network->ssid_length; ++i)
        if (network->ssid[i] == 0) return false;
    for (unsigned i = 0; i < network->password_length; ++i)
        if (network->password[i] < 0x20 || network->password[i] > 0x7e) return false;
    return true;
}

esp_err_t wifi_networks_load(wifi_network_t networks[WIFI_NETWORK_LIMIT], size_t *count)
{
    if (!networks || !count) return ESP_ERR_INVALID_ARG;
    *count = 0;
    memset(networks, 0, WIFI_NETWORK_LIMIT * sizeof(*networks));
    nvs_handle_t handle;
    esp_err_t result = nvs_open(NETWORKS_NAMESPACE, NVS_READONLY, &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    uint8_t blob[BLOB_BYTES];
    size_t length = sizeof(blob);
    result = nvs_get_blob(handle, NETWORKS_KEY, blob, &length);
    nvs_close(handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (result != ESP_OK) return result;
    if (length != sizeof(blob) || blob[0] != NETWORKS_VERSION || blob[1] > WIFI_NETWORK_LIMIT)
        return ESP_ERR_INVALID_RESPONSE;
    for (size_t i = 0; i < blob[1]; ++i) {
        const uint8_t *entry = blob + 2U + i * RECORD_BYTES;
        networks[i].ssid_length = entry[0];
        networks[i].password_length = entry[1];
        memcpy(networks[i].ssid, entry + 2U, WIFI_NETWORK_SSID_MAX);
        memcpy(networks[i].password, entry + 2U + WIFI_NETWORK_SSID_MAX, WIFI_NETWORK_PASSWORD_MAX);
        if (!valid_network(&networks[i])) {
            memset(networks, 0, WIFI_NETWORK_LIMIT * sizeof(*networks));
            return ESP_ERR_INVALID_RESPONSE;
        }
    }
    *count = blob[1];
    return ESP_OK;
}

esp_err_t wifi_networks_save(const wifi_network_t *network)
{
    if (!valid_network(network)) return ESP_ERR_INVALID_ARG;
    wifi_network_t list[WIFI_NETWORK_LIMIT];
    size_t count;
    esp_err_t result = wifi_networks_load(list, &count);
    if (result != ESP_OK) return result;
    size_t old = count;
    for (size_t i = 0; i < count; ++i) {
        if (list[i].ssid_length == network->ssid_length &&
            memcmp(list[i].ssid, network->ssid, network->ssid_length) == 0) { old = i; break; }
    }
    if (old < count && old == count - 1 &&
        list[old].password_length == network->password_length &&
        memcmp(list[old].password, network->password, network->password_length) == 0) {
        memset(list, 0, sizeof(list));
        return ESP_OK;
    }
    if (old < count) {
        memmove(&list[old], &list[old + 1], (count - old - 1) * sizeof(list[0]));
        --count;
    } else if (count == WIFI_NETWORK_LIMIT) {
        memmove(&list[0], &list[1], (count - 1) * sizeof(list[0]));
        --count;
    }
    list[count++] = *network;
    uint8_t blob[BLOB_BYTES] = {NETWORKS_VERSION, (uint8_t)count};
    for (size_t i = 0; i < count; ++i) {
        uint8_t *entry = blob + 2U + i * RECORD_BYTES;
        entry[0] = list[i].ssid_length;
        entry[1] = list[i].password_length;
        memcpy(entry + 2U, list[i].ssid, list[i].ssid_length);
        memcpy(entry + 2U + WIFI_NETWORK_SSID_MAX, list[i].password, list[i].password_length);
    }
    nvs_handle_t handle;
    result = nvs_open(NETWORKS_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        result = nvs_set_blob(handle, NETWORKS_KEY, blob, sizeof(blob));
        if (result == ESP_OK) result = nvs_commit(handle);
        nvs_close(handle);
    }
    memset(blob, 0, sizeof(blob));
    memset(list, 0, sizeof(list));
    return result;
}
