#ifndef VIEWFINDER_SDM50_H
#define VIEWFINDER_SDM50_H

#include <stdbool.h>
#include <stdint.h>

#define SDM50_MIN_DISTANCE_MM 50U
#define SDM50_NO_TARGET_MM 50000U

/* Call once after BoardPins_Init() and Delay_Init(). */
void SDM50_Init(void);

/* Main-loop consumer. New samples overwrite unconsumed ones. */
bool SDM50_TakeLatestDistance(uint16_t *distance_mm);

#endif /* VIEWFINDER_SDM50_H */
