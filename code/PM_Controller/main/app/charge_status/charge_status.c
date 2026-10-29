#include "charge_status.h"
#include "usb_power.h"

#define INPUT_STABLE_MS 200U
#define FULL_STABLE_MS  4000U
#define PULSE_RECENT_MS 3000U

static struct {
    bool initialized;
    bool usb_candidate;
    bool usb_present;
    uint32_t usb_since_ms;
    uint32_t falls_at_connect;
    uint32_t last_falls;
    unsigned levels;
    uint32_t levels_since_ms;
} charge;

battery_status_t charge_status_update(uint32_t now_ms, const hc165_inputs_t *inputs,
                                       const power_sample_t *power)
{
    bool usb;
    if (usb_power_get(&usb) != ESP_OK) return BATTERY_PENDING;
    const unsigned levels = (inputs->charging ? 1U : 0U) | (inputs->charge_done ? 2U : 0U);
    if (!charge.initialized) {
        charge.initialized = true;
        charge.usb_candidate = charge.usb_present = usb;
        charge.usb_since_ms = charge.levels_since_ms = now_ms;
        charge.falls_at_connect = inputs->charge_falls;
        charge.last_falls = inputs->charge_falls;
        charge.levels = levels;
    }
    if (usb != charge.usb_candidate) {
        charge.usb_candidate = usb;
        charge.usb_since_ms = now_ms;
    }
    if (now_ms - charge.usb_since_ms < INPUT_STABLE_MS) return BATTERY_PENDING;
    if (usb != charge.usb_present) {
        charge.usb_present = usb;
        charge.falls_at_connect = inputs->charge_falls;
        charge.levels_since_ms = now_ms;
        charge.levels = levels;
    }
    if (levels != charge.levels) {
        charge.levels = levels;
        charge.levels_since_ms = now_ms;
    }
    if (inputs->charge_falls != charge.last_falls) {
        charge.last_falls = inputs->charge_falls;
        charge.levels_since_ms = now_ms;
    }
    if (!usb) return levels == 0 && power->battery_valid ? BATTERY_NORMAL : BATTERY_PENDING;

    /* TP4057 specifies CHG pulses with a 0.5..2 s period at CBAT=10 uF.
     * Require three new falling edges, two consecutive short periods and STB
     * asserted. This avoids interpreting a single transient or both-high as absent. */
    if (inputs->charge_done && inputs->charge_falls - charge.falls_at_connect >= 3 &&
        inputs->charge_short_periods >= 2 &&
        now_ms - inputs->charge_last_fall_ms <= PULSE_RECENT_MS) return BATTERY_ABSENT;

    const uint32_t stable_ms = now_ms - charge.levels_since_ms;
    if (levels == 1 && stable_ms >= INPUT_STABLE_MS) return BATTERY_CHARGING;
    /* Each captured no-battery pulse restarts this timer, so an absent battery is not
     * briefly displayed as full while its CHG output is high. */
    if (levels == 2 && stable_ms >= FULL_STABLE_MS) return BATTERY_FULL;
    return BATTERY_PENDING;
}
