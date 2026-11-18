#include "st7735.h"

#include <stdbool.h>
#include <stddef.h>

#include "Board/PIN.h"
#include "Events/AppEvents.h"
#include "debug.h"

#define CMD_SWRESET 0x01U
#define CMD_SLPOUT  0x11U
#define CMD_NORON   0x13U
#define CMD_INVOFF  0x20U
#define CMD_INVON   0x21U
#define CMD_DISPON  0x29U
#define CMD_CASET   0x2aU
#define CMD_RASET   0x2bU
#define CMD_RAMWR   0x2cU
#define CMD_MADCTL  0x36U
#define CMD_COLMOD  0x3aU

/* Initial 128x128 panel defaults. Calibrate these against the installed module. */
#define PANEL_COLUMN_OFFSET 2U
#define PANEL_ROW_OFFSET    3U
#define PANEL_MADCTL        0xc8U /* MX | MY | BGR */
#define PANEL_INVERT_COLORS false

#define BACKLIGHT_PWM_HZ    2000U
#define BACKLIGHT_STEPS     100U

/* Remap SPI1 TX to DMA2 channel 7 so USART3 RX can use DMA1 channel 3. */
#define LCD_TX_DMA          DMA2_Channel7
#define DMA_POLL_US         10U
#define DMA_TIMEOUT_US      100000U

static struct {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
    const uint8_t *pixels;
} pending_strip;

static bool dma_available;
static volatile uint32_t dma_result;

void DMA2_Channel7_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void WriteByte(uint8_t value)
{
    while ((LCD_SPI->STATR & SPI_I2S_FLAG_TXE) == 0U) {
    }
    LCD_SPI->DATAR = value;
}

static bool SpiIdle(void)
{
    const uint16_t status = LCD_SPI->STATR;
    return (status & (SPI_I2S_FLAG_TXE | SPI_I2S_FLAG_BSY)) == SPI_I2S_FLAG_TXE;
}

static void WaitForWrite(void)
{
    while (!SpiIdle()) {
    }
}

static void BeginCommand(uint8_t command)
{
    GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN);
    GPIO_ResetBits(LCD_CS_N_PORT, LCD_CS_N_PIN);
    Delay_Us(1);
    WriteByte(command);
    WaitForWrite();
    GPIO_SetBits(LCD_DC_PORT, LCD_DC_PIN);
}

static void EndWrite(void)
{
    WaitForWrite();
    Delay_Us(1);
    GPIO_SetBits(LCD_CS_N_PORT, LCD_CS_N_PIN);
    Delay_Us(1);
}

static void WriteCommand(uint8_t command, const uint8_t *data, uint8_t length)
{
    BeginCommand(command);
    for (uint8_t i = 0; i < length; ++i) {
        WriteByte(data[i]);
    }
    EndWrite();
}

static void WriteAxis(uint8_t command, uint16_t first, uint16_t size)
{
    const uint16_t last = first + size - 1U;
    const uint8_t bounds[] = {first >> 8, first & 0xffU, last >> 8, last & 0xffU};
    WriteCommand(command, bounds, sizeof(bounds));
}

static void BeginPixels(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    WriteAxis(CMD_CASET, x + PANEL_COLUMN_OFFSET, width);
    WriteAxis(CMD_RASET, y + PANEL_ROW_OFFSET, height);
    BeginCommand(CMD_RAMWR);
}

static inline void WritePixel(uint16_t color)
{
    WriteByte((uint8_t)(color >> 8));
    WriteByte((uint8_t)color);
}

static bool RectFits(uint16_t x, uint16_t y, uint16_t width, uint16_t height)
{
    return x < ST7735_WIDTH && y < ST7735_HEIGHT &&
           width > 0U && width <= ST7735_WIDTH - x &&
           height > 0U && height <= ST7735_HEIGHT - y;
}

void ST7735_FillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                     uint16_t color)
{
    ST7735_WaitForWriteRect();
    if (!RectFits(x, y, width, height)) {
        return;
    }

    BeginPixels(x, y, width, height);
    for (uint32_t remaining = (uint32_t)width * height; remaining > 0U; --remaining) {
        WritePixel(color);
    }
    EndWrite();
}

static void WriteWireBytes(const uint8_t *pixels, uint16_t length)
{
    while (length > 0U) {
        WriteByte(*pixels++);
        --length;
    }
    EndWrite();
}

bool ST7735_BeginWriteRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height,
                           const uint8_t *pixels)
{
    if (pending_strip.pixels != NULL || pixels == NULL || !RectFits(x, y, width, height)) {
        return false;
    }

    const uint16_t length = (uint16_t)((uint32_t)width * height * 2U);
    BeginPixels(x, y, width, height);
    if (!dma_available) {
        WriteWireBytes(pixels, length);
        return true;
    }

    pending_strip.x = x;
    pending_strip.y = y;
    pending_strip.width = width;
    pending_strip.height = height;
    pending_strip.pixels = pixels;
    dma_result = 0;

    DMA_ClearFlag(DMA2, DMA_FLAG_GL7);
    LCD_TX_DMA->MADDR = (uint32_t)(uintptr_t)pixels;
    DMA_SetCurrDataCounter(LCD_TX_DMA, length);

    /* Publish the completed strip before the DMA reads SRAM. */
    __asm volatile ("fence iorw, iorw" ::: "memory");
    DMA_Cmd(LCD_TX_DMA, ENABLE);
    SPI_I2S_DMACmd(LCD_SPI, SPI_I2S_DMAReq_Tx, ENABLE);
    return true;
}

static void StopDma(void)
{
    SPI_I2S_DMACmd(LCD_SPI, SPI_I2S_DMAReq_Tx, DISABLE);
    DMA_Cmd(LCD_TX_DMA, DISABLE);
    DMA_ClearFlag(DMA2, DMA_FLAG_GL7);
}

void DMA2_Channel7_IRQHandler(void)
{
    const uint32_t flags = DMA2->INTFR & (DMA_FLAG_TC7 | DMA_FLAG_TE7);
    if (flags != 0U) {
        StopDma();
        dma_result = flags;
        AppEvents_PostFromISR(APP_LCD_DONE);
    }
}

static void FinishDmaWrite(bool success)
{
    EndWrite();
    if (!success) {
        /* Replay the entire strip and use polling for subsequent strips. */
        dma_available = false;
        BeginPixels(pending_strip.x, pending_strip.y, pending_strip.width, pending_strip.height);
        WriteWireBytes(pending_strip.pixels,
                       (uint16_t)((uint32_t)pending_strip.width * pending_strip.height * 2U));
    }
    pending_strip.pixels = NULL;
}

bool ST7735_CompleteWriteRect(void)
{
    if (pending_strip.pixels == NULL) {
        return true;
    }

    const uint32_t result = dma_result;
    if (result == 0U || !SpiIdle()) {
        return false;
    }
    /* DMA TC marks the last write to DATAR. SPI must also finish clocking it. */
    FinishDmaWrite(result == DMA_FLAG_TC7);
    return true;
}

void ST7735_RecoverWriteRect(void)
{
    if (pending_strip.pixels != NULL) {
        NVIC_DisableIRQ(DMA2_Channel7_IRQn);
        StopDma();
        NVIC_EnableIRQ(DMA2_Channel7_IRQn);
        FinishDmaWrite(false);
    }
}

void ST7735_WaitForWriteRect(void)
{
    for (uint32_t waited = 0; waited < DMA_TIMEOUT_US; waited += DMA_POLL_US) {
        if (ST7735_CompleteWriteRect()) {
            return;
        }
        Delay_Us(DMA_POLL_US);
    }
    if (!ST7735_CompleteWriteRect()) {
        ST7735_RecoverWriteRect();
    }
}

static void InitWriteDma(void)
{
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA2, ENABLE);
    DMA_RemapChannel_Config(DMA_RemapCH_SPI1, ENABLE);
    DMA_DeInit(LCD_TX_DMA);

    DMA_InitTypeDef config = {
        .DMA_PeripheralBaseAddr = (uint32_t)(uintptr_t)&LCD_SPI->DATAR,
        .DMA_DIR = DMA_DIR_PeripheralDST,
        .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
        .DMA_MemoryInc = DMA_MemoryInc_Enable,
        .DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte,
        .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
        .DMA_Mode = DMA_Mode_Normal,
        .DMA_Priority = DMA_Priority_Medium,
    };
    DMA_Init(LCD_TX_DMA, &config);
    DMA_ITConfig(LCD_TX_DMA, DMA_IT_TC | DMA_IT_TE, ENABLE);
    NVIC_SetPriority(DMA2_Channel7_IRQn, 0);
    NVIC_EnableIRQ(DMA2_Channel7_IRQn);
    dma_available = true;
}

static void InitBacklight(void)
{
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_TIM3, ENABLE);

    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    uint32_t timer_clock = clocks.PCLK1_Frequency;
    if ((RCC->CFGR0 & RCC_PPRE1) != RCC_PPRE1_DIV1) {
        timer_clock *= 2U;
    }

    TIM_TimeBaseInitTypeDef timebase = {
        .TIM_Prescaler = (uint16_t)(timer_clock / (BACKLIGHT_PWM_HZ * BACKLIGHT_STEPS) - 1U),
        .TIM_CounterMode = TIM_CounterMode_Up,
        .TIM_Period = BACKLIGHT_STEPS - 1U,
        .TIM_ClockDivision = TIM_CKD_DIV1,
    };
    TIM_TimeBaseInit(LCD_BL_PWM_TIM, &timebase);

    TIM_OCInitTypeDef channel = {
        .TIM_OCMode = TIM_OCMode_PWM1,
        .TIM_OutputState = TIM_OutputState_Enable,
        .TIM_Pulse = 0,
        .TIM_OCPolarity = TIM_OCPolarity_Low,
    };
    TIM_OC3Init(LCD_BL_PWM_TIM, &channel);
    TIM_OC3PreloadConfig(LCD_BL_PWM_TIM, TIM_OCPreload_Enable);
    TIM_Cmd(LCD_BL_PWM_TIM, ENABLE);

    GPIO_InitTypeDef pin = {
        .GPIO_Pin = LCD_BL_PWM_PIN,
        .GPIO_Speed = GPIO_Speed_Medium,
        .GPIO_Mode = GPIO_Mode_AF_PP,
    };
    GPIO_PinAFConfig(LCD_BL_PWM_PORT, LCD_BL_PWM_SOURCE, LCD_BL_PWM_AF);
    GPIO_Init(LCD_BL_PWM_PORT, &pin);
}

void ST7735_SetBacklight(uint8_t percent)
{
    TIM_SetCompare3(LCD_BL_PWM_TIM, percent <= 100U ? percent : 100U);
}

void ST7735_Init(void)
{
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_SPI1, ENABLE);
    SPI_InitTypeDef spi = {
        .SPI_Direction = SPI_Direction_1Line_Tx,
        .SPI_Mode = SPI_Mode_Master,
        .SPI_DataSize = SPI_DataSize_8b,
        .SPI_CPOL = SPI_CPOL_Low,
        .SPI_CPHA = SPI_CPHA_1Edge,
        .SPI_NSS = SPI_NSS_Soft,
        .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32, /* 5 MHz at 160 MHz PCLK2 */
        .SPI_FirstBit = SPI_FirstBit_MSB,
        .SPI_CRCPolynomial = 7,
    };
    SPI_Init(LCD_SPI, &spi);
    SPI_NSSInternalSoftwareConfig(LCD_SPI, SPI_NSSInternalSoft_Set);
    SPI_Cmd(LCD_SPI, ENABLE);

    GPIO_ResetBits(LCD_RST_N_PORT, LCD_RST_N_PIN);
    Delay_Ms(20);
    GPIO_SetBits(LCD_RST_N_PORT, LCD_RST_N_PIN);
    Delay_Ms(120);

    WriteCommand(CMD_SWRESET, NULL, 0);
    Delay_Ms(150);
    WriteCommand(CMD_SLPOUT, NULL, 0);
    Delay_Ms(120);

    const uint8_t madctl = PANEL_MADCTL;
    const uint8_t rgb565 = 0x05;
    WriteCommand(CMD_MADCTL, &madctl, 1);
    WriteCommand(CMD_COLMOD, &rgb565, 1);
    WriteCommand(PANEL_INVERT_COLORS ? CMD_INVON : CMD_INVOFF, NULL, 0);
    WriteCommand(CMD_NORON, NULL, 0);
    Delay_Ms(10);

    ST7735_FillRect(0, 0, ST7735_WIDTH, ST7735_HEIGHT, 0);
    WriteCommand(CMD_DISPON, NULL, 0);
    Delay_Ms(100);
    InitBacklight();
    InitWriteDma();
}
