#ifndef PM_WIFI_NETWORKS_H
#define PM_WIFI_NETWORKS_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define WIFI_NETWORK_LIMIT 3U
#define WIFI_NETWORK_SSID_MAX 32U
#define WIFI_NETWORK_PASSWORD_MAX 63U

typedef struct {
    uint8_t ssid_length;
    uint8_t password_length;
    uint8_t ssid[WIFI_NETWORK_SSID_MAX];
    uint8_t password[WIFI_NETWORK_PASSWORD_MAX];
} wifi_network_t;

/* Called in a task after successful nvs_flash_init(). Oldest entry first.
 * SSIDs are byte strings; password is an ASCII WPA2/3 passphrase, or empty
 * for an open network. No credentials are printed. */
esp_err_t wifi_networks_load(wifi_network_t networks[WIFI_NETWORK_LIMIT], size_t *count);
/* Promote/update an existing SSID or evict oldest on the fourth new SSID.
 * Caller should save only after a successful connection. */
esp_err_t wifi_networks_save(const wifi_network_t *network);
#endif
