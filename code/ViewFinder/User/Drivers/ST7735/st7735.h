#ifndef VIEWFINDER_ST7735_H
#define VIEWFINDER_ST7735_H

#include <stdbool.h>
#include <stdint.h>

#define ST7735_WIDTH  128U
#define ST7735_HEIGHT 128U

void ST7735_Init(void);
void ST7735_SetBacklight(uint8_t percent);

/* Coordinates are in visible pixels. */
void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint16_t color);

/* RGB565 wire order (high byte first). Returns false if the bus is occupied.
 * Keep pixels unchanged until CompleteWriteRect returns true. */
bool ST7735_BeginWriteRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                           const uint8_t *pixels);
bool ST7735_CompleteWriteRect(void);
void ST7735_RecoverWriteRect(void);
/* Blocking completion for startup drawing. */
void ST7735_WaitForWriteRect(void);

#endif /* VIEWFINDER_ST7735_H */
