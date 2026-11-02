#ifndef PM_WIFI_WEB_H
#define PM_WIFI_WEB_H
#include "esp_err.h"
/* Called from the network event task after IP acquisition/loss. */
esp_err_t wifi_web_start(void);
void wifi_web_stop(void);
#endif
