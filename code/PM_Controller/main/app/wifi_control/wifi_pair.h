#ifndef PM_WIFI_PAIR_H
#define PM_WIFI_PAIR_H
#include <stdbool.h>
#include "esp_err.h"
/* UI task initiates/cancels; the worker owns provisioning manager operations. */
esp_err_t wifi_pair_start(void);
void wifi_pair_cancel(void);
bool wifi_pair_active(void);
/* Returned string belongs to the UI task and remains valid until next call. */
const char *wifi_pair_status(void);
#endif
