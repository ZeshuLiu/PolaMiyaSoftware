#include "sdm50.h"

#include <stdbool.h>

#include "Board/PIN.h"
#include "Communication/ControllerLink.h"
#include "Display/FinderDisplay.h"
#include "debug.h"

#define SDM50_BAUDRATE       460800U
#define SDM50_START_DELAY_MS 200U
#define SDM50_START_ATTEMPTS 3U
#define RANGE_HEADER         0x5cU
#define RESPONSE_HEADER      0x5aU
#define RANGE_FRAME_SIZE     4U
#define MAX_FRAME_SIZE       8U
#define RX_DMA_SIZE          64U
#define RX_DMA_CHANNEL       DMA1_Channel3
#define TX_DMA_CHANNEL       DMA1_Channel2
#define UART_RX_ERRORS       (USART_FLAG_ORE | USART_FLAG_FE | USART_FLAG_NE | USART_FLAG_PE)

/* Keep the command in SRAM for DMA. */
static uint8_t start_command[] = {0x5a, 0x0a, 0x02, 0x02, 0x00, 0xf1};
static volatile uint8_t rx_dma[RX_DMA_SIZE];
static uint16_t rx_read_pos;
static volatile bool tx_active;

static uint8_t frame[MAX_FRAME_SIZE];
static uint8_t frame_size;
static volatile bool start_confirmed;
static volatile uint16_t latest_distance_mm;
static volatile bool distance_pending;

void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void PublishDistance(uint16_t distance_mm)
{
    uint16_t map_distance = distance_mm;
    if (distance_mm < SDM50_MIN_DISTANCE_MM) {
        map_distance = FINDER_DISTANCE_TOO_CLOSE;
    } else if (distance_mm >= SDM50_NO_TARGET_MM) {
        map_distance = FINDER_DISTANCE_INF;
    }
    finder_distance_mm[FINDER_DISTANCE_MAP_CENTER][FINDER_DISTANCE_MAP_CENTER] = map_distance;

    latest_distance_mm = distance_mm;
    distance_pending = true;
}

static void DiscardBytes(uint8_t count)
{
    frame_size -= count;
    for (uint8_t i = 0; i < frame_size; ++i) {
        frame[i] = frame[i + count];
    }
}

static void ParseByte(uint8_t byte)
{
    frame[frame_size++] = byte;
    while (frame_size != 0U) {
        uint8_t required;
        if (frame[0] == RANGE_HEADER) {
            required = RANGE_FRAME_SIZE;
        } else if (frame[0] == RESPONSE_HEADER) {
            if (frame_size < 3U) {
                return;
            }
            if (frame[2] != 2U && frame[2] != 4U) {
                DiscardBytes(1);
                continue;
            }
            required = frame[2] + 4U;
        } else {
            DiscardBytes(1);
            continue;
        }

        if (frame_size < required) {
            return;
        }

        uint8_t sum = 0;
        for (uint8_t i = 1; i < required; ++i) {
            sum += frame[i];
        }
        if (sum != 0xffU) {
            DiscardBytes(1);
            continue;
        }

        if (frame[0] == RANGE_HEADER) {
            start_confirmed = true;
            PublishDistance((uint16_t)frame[1] | ((uint16_t)frame[2] << 8));
            ControllerLink_ForwardLegacyRange(frame);
        } else if (frame[1] == 0x8aU && frame[2] == 2U &&
                   frame[3] == 2U && frame[4] == 0U) {
            start_confirmed = true;
        }
        DiscardBytes(required);
    }
}

static uint16_t RxWritePos(void)
{
    const uint16_t remaining = DMA_GetCurrDataCounter(RX_DMA_CHANNEL);
    return (RX_DMA_SIZE - remaining) % RX_DMA_SIZE;
}

static void DrainRxDma(void)
{
    const uint16_t end = RxWritePos();
    __asm volatile ("fence iorw, iorw" ::: "memory");
    while (rx_read_pos != end) {
        ParseByte(rx_dma[rx_read_pos]);
        rx_read_pos = (rx_read_pos + 1U) % RX_DMA_SIZE;
    }
}

static void DiscardRxDma(void)
{
    frame_size = 0;
    rx_read_pos = RxWritePos();
}

static void RestartRxDma(void)
{
    USART_DMACmd(UART2_USART, USART_DMAReq_Rx, DISABLE);
    DMA_Cmd(RX_DMA_CHANNEL, DISABLE);
    (void)UART2_USART->STATR;
    (void)USART_ReceiveData(UART2_USART);
    DMA_ClearFlag(DMA1, DMA_FLAG_GL3);
    DMA_SetCurrDataCounter(RX_DMA_CHANNEL, RX_DMA_SIZE);
    rx_read_pos = 0;
    frame_size = 0;
    DMA_Cmd(RX_DMA_CHANNEL, ENABLE);
    USART_DMACmd(UART2_USART, USART_DMAReq_Rx, ENABLE);
}

void USART3_IRQHandler(void)
{
    const uint16_t status = UART2_USART->STATR;
    if ((status & UART_RX_ERRORS) != 0U) {
        (void)USART_ReceiveData(UART2_USART);
        DiscardRxDma();
    } else if ((status & USART_FLAG_IDLE) != 0U) {
        /* If a new byte is pending, let DMA read DATAR after the status read. */
        if ((status & USART_FLAG_RXNE) == 0U) {
            (void)USART_ReceiveData(UART2_USART);
        }
        DrainRxDma();
    }
}

void DMA1_Channel3_IRQHandler(void)
{
    const uint32_t flags = DMA1->INTFR & (DMA_FLAG_HT3 | DMA_FLAG_TC3 | DMA_FLAG_TE3);
    if ((flags & DMA_FLAG_TE3) != 0U) {
        RestartRxDma();
        return;
    }
    if (flags == 0U) {
        return;
    }
    DMA_ClearFlag(DMA1, flags);

    if ((UART2_USART->STATR & UART_RX_ERRORS) != 0U) {
        (void)USART_ReceiveData(UART2_USART);
        DiscardRxDma();
    } else if (flags == (DMA_FLAG_HT3 | DMA_FLAG_TC3)) {
        /* Both boundaries pending means the producer may have lapped us. */
        DiscardRxDma();
    } else {
        DrainRxDma();
    }
}

void DMA1_Channel2_IRQHandler(void)
{
    const uint32_t flags = DMA1->INTFR & (DMA_FLAG_TC2 | DMA_FLAG_TE2);
    if (flags == 0U) {
        return;
    }
    USART_DMACmd(UART2_USART, USART_DMAReq_Tx, DISABLE);
    DMA_Cmd(TX_DMA_CHANNEL, DISABLE);
    DMA_ClearFlag(DMA1, DMA_FLAG_GL2);
    tx_active = false;
}

bool SDM50_TakeLatestDistance(uint16_t *distance_mm)
{
    NVIC_DisableIRQ(USART3_IRQn);
    NVIC_DisableIRQ(DMA1_Channel3_IRQn);
    const bool pending = distance_pending;
    if (pending) {
        *distance_mm = latest_distance_mm;
        distance_pending = false;
    }
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    NVIC_EnableIRQ(USART3_IRQn);
    return pending;
}

static void InitDma(void)
{
    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_DeInit(RX_DMA_CHANNEL);
    DMA_DeInit(TX_DMA_CHANNEL);

    DMA_InitTypeDef config = {
        .DMA_PeripheralBaseAddr = (uint32_t)(uintptr_t)&UART2_USART->DATAR,
        .DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)rx_dma,
        .DMA_DIR = DMA_DIR_PeripheralSRC,
        .DMA_BufferSize = RX_DMA_SIZE,
        .DMA_PeripheralInc = DMA_PeripheralInc_Disable,
        .DMA_MemoryInc = DMA_MemoryInc_Enable,
        .DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte,
        .DMA_MemoryDataSize = DMA_MemoryDataSize_Byte,
        .DMA_Mode = DMA_Mode_Circular,
        .DMA_Priority = DMA_Priority_High,
    };
    DMA_Init(RX_DMA_CHANNEL, &config);
    DMA_ITConfig(RX_DMA_CHANNEL, DMA_IT_HT | DMA_IT_TC | DMA_IT_TE, ENABLE);

    config.DMA_Memory0BaseAddr = (uint32_t)(uintptr_t)start_command;
    config.DMA_DIR = DMA_DIR_PeripheralDST;
    config.DMA_BufferSize = sizeof(start_command);
    config.DMA_Mode = DMA_Mode_Normal;
    DMA_Init(TX_DMA_CHANNEL, &config);
    DMA_ITConfig(TX_DMA_CHANNEL, DMA_IT_TC | DMA_IT_TE, ENABLE);
}

static void SendStartCommand(void)
{
    if (tx_active) {
        return;
    }
    tx_active = true;
    DMA_ClearFlag(DMA1, DMA_FLAG_GL2);
    DMA_SetCurrDataCounter(TX_DMA_CHANNEL, sizeof(start_command));
    DMA_Cmd(TX_DMA_CHANNEL, ENABLE);
    USART_DMACmd(UART2_USART, USART_DMAReq_Tx, ENABLE);
}

void SDM50_Init(void)
{
    USART_InitTypeDef uart = {
        .USART_BaudRate = SDM50_BAUDRATE,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No,
        .USART_Mode = USART_Mode_Rx | USART_Mode_Tx,
        .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
    };
    RCC_PB1PeriphClockCmd(RCC_PB1Periph_USART3, ENABLE);
    USART_Init(UART2_USART, &uart);
    InitDma();

    /* Match the controller UART priority: receive handlers share its TX queue. */
    NVIC_SetPriority(USART3_IRQn, 0);
    NVIC_SetPriority(DMA1_Channel3_IRQn, 0);
    NVIC_SetPriority(DMA1_Channel2_IRQn, 0);
    NVIC_EnableIRQ(USART3_IRQn);
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);
    NVIC_EnableIRQ(DMA1_Channel2_IRQn);

    DMA_Cmd(RX_DMA_CHANNEL, ENABLE);
    USART_DMACmd(UART2_USART, USART_DMAReq_Rx, ENABLE);
    USART_ITConfig(UART2_USART, USART_IT_IDLE, ENABLE);
    USART_ITConfig(UART2_USART, USART_IT_ERR, ENABLE);
    USART_Cmd(UART2_USART, ENABLE);

    for (uint8_t attempt = 0; attempt < SDM50_START_ATTEMPTS; ++attempt) {
        Delay_Ms(SDM50_START_DELAY_MS);
        if (start_confirmed) {
            break;
        }
        SendStartCommand();
    }
}
