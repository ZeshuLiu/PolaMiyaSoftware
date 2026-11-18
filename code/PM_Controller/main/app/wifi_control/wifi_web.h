#ifndef PM_WIFI_WEB_H
#define PM_WIFI_WEB_H
#include <stdbool.h>
#include "esp_err.h"
#include <stdint.h>
/* Open only while the on-device page is active and Wi-Fi has an address. */
/* Called only by the network worker; may block during HTTP server shutdown. */
void wifi_web_process(void);
esp_err_t wifi_web_request(bool enabled);
esp_err_t wifi_web_set_network(bool available);
bool wifi_web_running(void);
uint16_t wifi_web_port(void);
esp_err_t wifi_web_error(void);
#endif
