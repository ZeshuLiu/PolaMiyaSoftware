#ifndef PM_HC165_H
#define PM_HC165_H

#include <stdbool.h>
#include <stdint.h>

/* Bit 0..5 correspond to KEY1..KEY6, matching U6's D0..D5. */
enum {
    HC165_KEY1 = 1U << 0,
    HC165_KEY2 = 1U << 1,
    HC165_KEY3 = 1U << 2,
    HC165_KEY4 = 1U << 3,
    HC165_KEY5 = 1U << 4,
    HC165_KEY6 = 1U << 5,
    HC165_KEYS_MASK = 0x3F,
    HC165_INPUT_CHG = 1U << 6,
    HC165_INPUT_STB = 1U << 7,
};

typedef struct {
    uint8_t short_press;
    uint8_t long_press;
} hc165_key_events_t;

typedef struct {
    uint8_t raw;          /* Physical D0..D7 levels, without inversion. */
    uint8_t pressed;      /* Debounced KEY1..KEY6; 1 means pressed. */
    bool charging;       /* CHG is low in the latest sample. */
    bool charge_done;    /* STB is low in the latest sample. */
} hc165_inputs_t;

/* Call once at startup. A background task owns GPIO39/40/41 thereafter. */
void hc165_init(void);

/* Task-context APIs. Snapshots are protected against concurrent scanning. */
hc165_inputs_t hc165_get_inputs(void);
hc165_key_events_t hc165_get_events(void);   /* Read flags without clearing. */
hc165_key_events_t hc165_take_events(void);  /* Atomically read and clear both masks. */

#endif
