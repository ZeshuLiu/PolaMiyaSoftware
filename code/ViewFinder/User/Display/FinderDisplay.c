#include "FinderDisplay.h"

#include <string.h>

#include "Events/AppEvents.h"
#include "Drivers/SDM50/sdm50.h"
#include "Drivers/ST7735/st7735.h"

#define FRAME_WIDTH      93U
#define FRAME_HEIGHT     91U
#define FRAME_LEFT       ((ST7735_WIDTH - FRAME_WIDTH) / 2U)
#define FRAME_TOP        ((ST7735_HEIGHT - FRAME_HEIGHT) / 2U)
#define FRAME_RIGHT      (FRAME_LEFT + FRAME_WIDTH - 1U)
#define FRAME_BOTTOM     (FRAME_TOP + FRAME_HEIGHT - 1U)
#define CORNER_LENGTH    9U

#define MAP_CELLS        29U
#define MAP_FIRST_CELL   (FINDER_DISTANCE_MAP_CENTER - MAP_CELLS / 2U)
#define CELL_PIXELS      3U
#define MAP_PIXELS       (MAP_CELLS * CELL_PIXELS)
#define MAP_LEFT         ((ST7735_WIDTH - MAP_PIXELS) / 2U)
#define MAP_TOP          ((ST7735_HEIGHT - MAP_PIXELS) / 2U)
#define STRIP_CELL_ROWS  10U
#define STRIP_HEIGHT     (STRIP_CELL_ROWS * CELL_PIXELS)

#define COLOR_BLACK      0x0000U
#define COLOR_WHITE      0xf7beU
#define COLOR_GREEN      0x8ff4U
#define COLOR_DIM        0x9d53U
#define BACKLIGHT_START  20U

#define ACT_X            79U
#define ACT_Y            10U
#define DISTANCE_CELLS   6U
#define EXPOSURE_CELLS   10U
#define CHAR_WIDTH       6U
#define CHAR_HEIGHT      7U
#define ACT_PERIOD       25U
#define MAP_PERIOD       100U
#define LCD_TIMEOUT_MS 100U
#define STALE_POLLS      (MAP_PERIOD / ACT_PERIOD)

/* 5x7 glyphs in a six-pixel cell, matching the finder preview. */
static const uint8_t glyphs['m' - ' ' + 1][CHAR_HEIGHT] = {
    ['-' - ' '] = { 0,  0,  0, 14,  0,  0,  0},
    ['.' - ' '] = { 0,  0,  0,  0,  0,  6,  6},
    ['0' - ' '] = {14, 17, 19, 21, 25, 17, 14},
    ['1' - ' '] = { 4, 12,  4,  4,  4,  4, 14},
    ['2' - ' '] = {14, 17,  1,  2,  4,  8, 31},
    ['3' - ' '] = {30,  1,  1, 14,  1,  1, 30},
    ['4' - ' '] = { 2,  6, 10, 18, 31,  2,  2},
    ['5' - ' '] = {31, 16, 16, 30,  1,  1, 30},
    ['6' - ' '] = {14, 16, 16, 30, 17, 17, 14},
    ['7' - ' '] = {31,  1,  2,  4,  8,  8,  8},
    ['8' - ' '] = {14, 17, 17, 14, 17, 17, 14},
    ['9' - ' '] = {14, 17, 17, 15,  1,  1, 14},
    ['A' - ' '] = {14, 17, 17, 31, 17, 17, 17},
    ['C' - ' '] = {15, 16, 16, 16, 16, 16, 15},
    ['E' - ' '] = {31, 16, 16, 30, 16, 16, 31},
    ['F' - ' '] = {31, 16, 16, 30, 16, 16, 16},
    ['I' - ' '] = {14,  4,  4,  4,  4,  4, 14},
    ['N' - ' '] = {17, 25, 25, 21, 19, 19, 17},
    ['O' - ' '] = {14, 17, 17, 17, 17, 17, 14},
    ['R' - ' '] = {30, 17, 17, 30, 20, 18, 17},
    ['T' - ' '] = {31,  4,  4,  4,  4,  4,  4},
    ['V' - ' '] = {17, 17, 17, 17, 17, 10,  4},
    ['m' - ' '] = { 0,  0, 26, 21, 21, 21, 21},
};

volatile uint16_t finder_distance_mm[FINDER_DISTANCE_MAP_SIZE][FINDER_DISTANCE_MAP_SIZE];

/* DMA sends one strip while the main loop fills the other. */
static uint8_t draw_pixels[2][MAP_PIXELS * STRIP_HEIGHT * 2U];
static unsigned next_map_row = MAP_CELLS;
static bool map_strip_prepared;
static bool write_active;
static bool write_completed;
static uint32_t write_start_tick;
static uint32_t last_act_tick;
static uint32_t last_map_tick;

_Static_assert(MAP_PIXELS <= FRAME_WIDTH - 2U && MAP_PIXELS <= FRAME_HEIGHT - 2U,
               "The distance map must fit inside the finder frame");
_Static_assert(MAP_PIXELS * STRIP_HEIGHT >= ST7735_WIDTH * CHAR_HEIGHT,
               "The strip buffer must fit a screen-wide text row");
static char displayed_actual[DISTANCE_CELLS + 1U];
static uint8_t polls_without_sample;

static inline void StoreWirePixel(uint8_t *line, unsigned x, uint16_t color)
{
    line[2U * x] = (uint8_t)(color >> 8);
    line[2U * x + 1U] = (uint8_t)color;
}

static bool BeginText(uint8_t x, uint8_t y, const char *text, uint8_t cells,
                     uint16_t color)
{
    if (x >= ST7735_WIDTH || y > ST7735_HEIGHT - CHAR_HEIGHT) {
        return false;
    }
    const uint8_t visible_cells = (uint8_t)((ST7735_WIDTH - x) / CHAR_WIDTH);
    if (cells > visible_cells) {
        cells = visible_cells;
    }
    if (cells == 0U) {
        return false;
    }

    const uint16_t width = cells * CHAR_WIDTH;
    uint8_t *pixels = draw_pixels[0];
    memset(pixels, 0, width * CHAR_HEIGHT * 2U);

    for (uint8_t i = 0; i < cells && text[i] != '\0'; ++i) {
        const uint8_t *glyph = glyphs[(uint8_t)text[i] - ' '];
        for (uint8_t row = 0; row < CHAR_HEIGHT; ++row) {
            for (uint8_t col = 0; col < CHAR_WIDTH - 1U; ++col) {
                if ((glyph[row] & (0x10U >> col)) != 0U) {
                    StoreWirePixel(pixels, row * width + i * CHAR_WIDTH + col, color);
                }
            }
        }
    }
    return ST7735_BeginWriteRect(x, y, width, CHAR_HEIGHT, pixels);
}

static void DrawStartupText(uint8_t x, uint8_t y, const char *text, uint8_t cells,
                            uint16_t color)
{
    if (BeginText(x, y, text, cells, color)) {
        ST7735_WaitForWriteRect();
    }
}

static bool DrawActual(const char *text)
{
    if (strcmp(displayed_actual, text) == 0 ||
        !BeginText(ACT_X, ACT_Y, text, DISTANCE_CELLS, COLOR_WHITE)) {
        return false;
    }
    memcpy(displayed_actual, text, strlen(text) + 1U);
    return true;
}

static void DrawFrame(void)
{
    ST7735_FillRect(FRAME_LEFT, FRAME_TOP, FRAME_WIDTH, 1, COLOR_WHITE);
    ST7735_FillRect(FRAME_LEFT, FRAME_BOTTOM, FRAME_WIDTH, 1, COLOR_WHITE);
    ST7735_FillRect(FRAME_LEFT, FRAME_TOP, 1, FRAME_HEIGHT, COLOR_WHITE);
    ST7735_FillRect(FRAME_RIGHT, FRAME_TOP, 1, FRAME_HEIGHT, COLOR_WHITE);

    const uint8_t rows[] = {FRAME_TOP, FRAME_BOTTOM};
    const uint8_t cols[] = {FRAME_LEFT, FRAME_RIGHT};
    for (uint8_t i = 0; i < 2U; ++i) {
        for (uint8_t j = 0; j < 2U; ++j) {
            const uint8_t x = cols[j] - j * (CORNER_LENGTH - 1U);
            const uint8_t y = rows[i] - i * (CORNER_LENGTH - 1U);
            ST7735_FillRect(x, rows[i], CORNER_LENGTH, 1, COLOR_GREEN);
            ST7735_FillRect(cols[j], y, 1, CORNER_LENGTH, COLOR_GREEN);
        }
    }
}

static uint16_t DistanceColor(uint16_t distance_mm)
{
    if (distance_mm >= FINDER_DISTANCE_UNSAMPLED) {
        return COLOR_BLACK;
    }
    if (distance_mm == FINDER_DISTANCE_TOO_CLOSE) {
        return 0xfc00U;
    }
    if (distance_mm < 2000U) {
        return 0x0580U;
    }
    if (distance_mm < 10000U) {
        return 0x0380U;
    }
    return 0x01c0U;
}

static void OverlayReticle(uint8_t *line, unsigned map_y)
{
    const unsigned center = MAP_PIXELS / 2U;
    if (map_y == center) {
        for (unsigned i = 0; i < 5U; ++i) {
            StoreWirePixel(line, center - 8U + i, COLOR_WHITE);
            StoreWirePixel(line, center + 3U + i, COLOR_WHITE);
        }
    } else if ((map_y >= center - 8U && map_y <= center - 4U) ||
               (map_y >= center + 3U && map_y <= center + 7U)) {
        StoreWirePixel(line, center, COLOR_WHITE);
    }
}

static unsigned MapStripRows(unsigned first)
{
    const unsigned remaining = MAP_CELLS - first;
    return remaining > STRIP_CELL_ROWS ? STRIP_CELL_ROWS : remaining;
}

static uint8_t *MapStripBuffer(unsigned first)
{
    return draw_pixels[(first / STRIP_CELL_ROWS) % 2U];
}

static void PrepareMapStrip(unsigned first)
{
    const unsigned line_bytes = MAP_PIXELS * 2U;
    const unsigned rows = MapStripRows(first);
    uint8_t *pixels = MapStripBuffer(first);

    for (unsigned row = 0; row < rows; ++row) {
        uint8_t *line = pixels + row * CELL_PIXELS * line_bytes;
        const volatile uint16_t *distances = &finder_distance_mm[MAP_FIRST_CELL + first + row][MAP_FIRST_CELL];
        for (unsigned col = 0; col < MAP_CELLS; ++col) {
            const uint16_t color = DistanceColor(distances[col]);
            for (unsigned dx = 0; dx < CELL_PIXELS; ++dx) {
                StoreWirePixel(line, col * CELL_PIXELS + dx, color);
            }
        }
        for (unsigned dy = 1; dy < CELL_PIXELS; ++dy) {
            memcpy(line + dy * line_bytes, line, line_bytes);
        }
        for (unsigned dy = 0; dy < CELL_PIXELS; ++dy) {
            OverlayReticle(line + dy * line_bytes, (first + row) * CELL_PIXELS + dy);
        }
    }
}

static bool BeginMapStrip(unsigned first)
{
    return ST7735_BeginWriteRect(MAP_LEFT, (uint16_t)(MAP_TOP + first * CELL_PIXELS),
                                 MAP_PIXELS, (uint16_t)(MapStripRows(first) * CELL_PIXELS),
                                 MapStripBuffer(first));
}

static bool DisplayPeriodElapsed(uint32_t now, uint32_t *last, uint32_t period)
{
    const uint32_t elapsed = now - *last;
    if (elapsed < period) {
        return false;
    }
    /* Preserve phase across redraws; skip any missed periods. */
    *last = now - elapsed % period;
    return true;
}

void FinderDisplay_Init(void)
{
    for (unsigned row = 0; row < FINDER_DISTANCE_MAP_SIZE; ++row) {
        for (unsigned col = 0; col < FINDER_DISTANCE_MAP_SIZE; ++col) {
            finder_distance_mm[row][col] = FINDER_DISTANCE_UNSAMPLED;
        }
    }

    ST7735_Init();
    DrawFrame();
    for (unsigned row = 0; row < MAP_CELLS; row += STRIP_CELL_ROWS) {
        PrepareMapStrip(row);
        if (BeginMapStrip(row)) {
            ST7735_WaitForWriteRect();
        }
    }
    DrawStartupText(7, 1, "FOC", 3, COLOR_DIM);
    DrawStartupText(7, 10, "--.--m", DISTANCE_CELLS, COLOR_WHITE);
    DrawStartupText(ACT_X, 1, "ACT", 3, COLOR_DIM);
    if (DrawActual("--.--m")) {
        ST7735_WaitForWriteRect();
    }
    DrawStartupText(34, 117, "EV100 --.-", EXPOSURE_CELLS, COLOR_WHITE);
    ST7735_SetBacklight(BACKLIGHT_START);
}

static bool UpdateActual(void)
{
    uint16_t distance_mm;
    if (!SDM50_TakeLatestDistance(&distance_mm)) {
        if (polls_without_sample < STALE_POLLS &&
            ++polls_without_sample == STALE_POLLS) {
            return DrawActual("--.--m");
        }
        return false;
    }
    polls_without_sample = 0;

    if (distance_mm < SDM50_MIN_DISTANCE_MM) {
        return DrawActual("NEAR");
    }
    if (distance_mm >= SDM50_NO_TARGET_MM) {
        return DrawActual("INF");
    }

    /* Truncate to 0.01 m so the largest valid reading stays below 50 m. */
    const uint16_t hundredths = distance_mm / 10U;
    const unsigned meters = hundredths / 100U;
    char text[DISTANCE_CELLS + 1U];
    uint8_t i = 0;
    if (meters >= 10U) {
        text[i++] = (char)('0' + meters / 10U);
    }
    text[i++] = (char)('0' + meters % 10U);
    text[i++] = '.';
    text[i++] = (char)('0' + (hundredths / 10U) % 10U);
    text[i++] = (char)('0' + hundredths % 10U);
    text[i++] = 'm';
    text[i] = '\0';
    return DrawActual(text);
}

static void TrackWrite(uint32_t now)
{
    write_active = !ST7735_CompleteWriteRect();
    write_completed = false;
    write_start_tick = now;
}

void FinderDisplay_Service(uint32_t events, uint32_t now)
{
    if ((events & (APP_LCD_DONE | APP_TICK)) == 0U) {
        return;
    }

    /* While DMA has one strip, prepare the next one in the other buffer. */
    if (next_map_row < MAP_CELLS && !map_strip_prepared) {
        PrepareMapStrip(next_map_row);
        map_strip_prepared = true;
    }

    if (write_active) {
        write_completed |= (events & APP_LCD_DONE) != 0U;
        if (!write_completed || !ST7735_CompleteWriteRect()) {
            if (now - write_start_tick < LCD_TIMEOUT_MS) {
                return;
            }
            ST7735_RecoverWriteRect();
        }
        write_active = false;
    }

    if (next_map_row == MAP_CELLS) {
        if (DisplayPeriodElapsed(now, &last_act_tick, ACT_PERIOD) && UpdateActual()) {
            TrackWrite(now);
            return;
        }
        if (!DisplayPeriodElapsed(now, &last_map_tick, MAP_PERIOD)) {
            return;
        }
        next_map_row = 0;
        PrepareMapStrip(next_map_row);
        map_strip_prepared = true;
    }

    if (BeginMapStrip(next_map_row)) {
        TrackWrite(now);
        next_map_row += MapStripRows(next_map_row);
        map_strip_prepared = false;
    }
}
