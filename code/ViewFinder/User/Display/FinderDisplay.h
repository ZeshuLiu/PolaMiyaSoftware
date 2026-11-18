#ifndef VIEWFINDER_DISPLAY_H
#define VIEWFINDER_DISPLAY_H

#include <stdint.h>

#define FINDER_DISTANCE_MAP_SIZE 33U
#define FINDER_DISTANCE_MAP_CENTER (FINDER_DISTANCE_MAP_SIZE / 2U)

#define FINDER_DISTANCE_UNSAMPLED 0xfffeU
#define FINDER_DISTANCE_INF       0xffffU
#define FINDER_DISTANCE_TOO_CLOSE 0U

/* Distances in millimeters, indexed by [row][column]. SDM50 updates the center from its RX ISR. */
extern volatile uint16_t finder_distance_mm[FINDER_DISTANCE_MAP_SIZE][FINDER_DISTANCE_MAP_SIZE];

void FinderDisplay_Init(void);
/* Dispatches display timer and DMA events from the main loop. */
void FinderDisplay_Service(uint32_t events, uint32_t now);

#endif /* VIEWFINDER_DISPLAY_H */
