#ifndef VIEWFINDER_OPT4001_H
#define VIEWFINDER_OPT4001_H

#include <stdbool.h>
#include <stdint.h>

/* Nominal conversion rate: 5, 10, 20 or 40 Hz. */
#ifndef OPT4001_SAMPLE_RATE_HZ
#define OPT4001_SAMPLE_RATE_HZ 10U
#endif

/* Call from the main loop context with interrupts enabled, after board and delay init. */
bool OPT4001_Init(void);
/* Dispatches data-ready and I2C completion events in the main loop. */
void OPT4001_Service(uint32_t events, uint32_t now);
/* Returns false until a sample has passed the result CRC check. */
bool OPT4001_GetLatestMillilux(uint32_t *millilux);

#endif /* VIEWFINDER_OPT4001_H */
