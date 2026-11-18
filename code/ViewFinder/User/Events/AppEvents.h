#ifndef VIEWFINDER_APP_EVENTS_H
#define VIEWFINDER_APP_EVENTS_H

#include <stdint.h>

enum {
    APP_TICK = 1U << 0,
    APP_LCD_DONE = 1U << 1,
    APP_OPT_READY = 1U << 2,
    APP_OPT_DONE = 1U << 3,
    APP_IMU_READY = 1U << 4,
    APP_IMU_DONE = 1U << 5,
};

void AppEvents_Init(void);
/* Producers run in ISRs at the same PFIC priority. */
void AppEvents_PostFromISR(uint32_t events);
uint32_t AppEvents_Wait(void);
uint32_t AppEvents_NowMs(void);

#endif /* VIEWFINDER_APP_EVENTS_H */
