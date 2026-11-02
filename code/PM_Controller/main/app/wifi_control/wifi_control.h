#ifndef PM_WIFI_CONTROL_H
#define PM_WIFI_CONTROL_H
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* Wi-Fi defaults off on each boot. The synchronous function is for the
 * Wi-Fi worker or provisioning worker, never the UI task. */
esp_err_t wifi_control_set_enabled(bool enabled);
/* Queue a switch operation; completion is reported through the status API. */
esp_err_t wifi_control_request_enabled(bool enabled);
bool wifi_control_busy(void);
esp_err_t wifi_control_last_error(void);
bool wifi_control_is_enabled(void);
bool wifi_control_has_network(void);
/* Copy a current IPv4 address, if available. */
bool wifi_control_ip(char *buffer, size_t length);
/* Suppress ordinary connection attempts while the provisioning manager owns Wi-Fi. */
void wifi_control_provisioning(bool active);
void wifi_control_mark_network_saved(void);
#endif
