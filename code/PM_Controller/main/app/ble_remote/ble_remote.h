#ifndef PM_BLE_REMOTE_H
#define PM_BLE_REMOTE_H

#include "esp_err.h"
#include "focus_distance.h"
#include "sdm50.h"

/* UI task owns requests and lens updates. BLE work runs in its own tasks. */
esp_err_t ble_remote_start(void);
void ble_remote_stop(void);
bool ble_remote_active(void);
bool ble_remote_connected(void);
const char *ble_remote_status(void);
esp_err_t ble_controller_preflight(void);

/* Returns true if a manual target was accepted by the focus state machine. */
bool ble_remote_update(const sdm50_sample_t *sample, const focus_distance_t *focus,
                       bool actual_valid, uint32_t actual_mm, bool actual_infinity,
                       bool local_controls_idle);

#endif
