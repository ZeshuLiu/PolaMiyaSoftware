#ifndef PM_WIFI_CONTROL_H
#define PM_WIFI_CONTROL_H
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

/* UI task creates the shared network worker before Wi-Fi, pairing or web. */
esp_err_t wifi_control_prepare(void);
/* Wake the worker after a web-service state change. */
esp_err_t wifi_control_notify_web(void);

/* The synchronous request is for the pairing task only;
 * the shared network worker handles application on/off requests. Never call it from the UI. */
esp_err_t wifi_control_set_enabled(bool enabled);
/* Queue a switch operation; completion is reported through the status API. */
esp_err_t wifi_control_request_enabled(bool enabled);
bool wifi_control_busy(void);
esp_err_t wifi_control_last_error(void);
bool wifi_control_is_enabled(void);
/* True only after the station obtains an IPv4 address. */
bool wifi_control_is_connected(void);
bool wifi_control_has_network(void);
/* Copy a current IPv4 address, if available. */
bool wifi_control_ip(char *buffer, size_t length);
/* Suppress ordinary connection attempts while the provisioning manager owns Wi-Fi. */
void wifi_control_provisioning(bool active);
void wifi_control_mark_network_saved(void);
#endif
