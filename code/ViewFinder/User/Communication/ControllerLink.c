#include "ControllerLink.h"

#include <stdbool.h>

#include "Board/PIN.h"

#define CONTROLLER_BAUD_RATE 460800U
#define LEGACY_RANGE_SIZE    4U
#define RANGE_QUEUE_SIZE     8U

static const uint8_t probe_request[] = {0x5a, 0x0a, 0x02, 0x02, 0x00, 0xf1};
static const uint8_t probe_reply[] = {0x55, 0xaa, 0x01, 0x01, 0x01, 0xfc};
static uint8_t probe_offset;

/* SDM50 RX and controller USART handlers have equal priority and cannot preempt each other. */
static volatile uint8_t pending_replies;
static volatile uint8_t range_queue[RANGE_QUEUE_SIZE][LEGACY_RANGE_SIZE];
static volatile uint8_t range_head;
static volatile uint8_t range_tail;
static uint8_t tx_offset;
static bool sending_reply;

void USART4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void ControllerLink_Init(void)
{
    USART_InitTypeDef uart = {
        .USART_BaudRate = CONTROLLER_BAUD_RATE,
        .USART_WordLength = USART_WordLength_8b,
        .USART_StopBits = USART_StopBits_1,
        .USART_Parity = USART_Parity_No,
        .USART_Mode = USART_Mode_Tx,
        .USART_HardwareFlowControl = USART_HardwareFlowControl_None,
    };

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_USART4 | RCC_PB1Periph_USART5, ENABLE);
    USART_Init(TXD1_USART, &uart);
    uart.USART_Mode = USART_Mode_Rx;
    USART_Init(RXD1_USART, &uart);
    USART_Cmd(TXD1_USART, ENABLE);
    USART_Cmd(RXD1_USART, ENABLE);

    NVIC_SetPriority(USART4_IRQn, 0);
    NVIC_SetPriority(USART5_IRQn, 0);
    NVIC_EnableIRQ(USART4_IRQn);
    NVIC_EnableIRQ(USART5_IRQn);
    USART_ITConfig(RXD1_USART, USART_IT_RXNE, ENABLE);
}

void ControllerLink_ForwardLegacyRange(const uint8_t frame[4])
{
    const uint8_t next = (range_head + 1U) % RANGE_QUEUE_SIZE;
    if (next == range_tail) {
        return; /* Drop a whole range frame when the queue is full. */
    }

    for (uint8_t i = 0; i < LEGACY_RANGE_SIZE; ++i) {
        range_queue[range_head][i] = frame[i];
    }
    range_head = next;
    USART_ITConfig(TXD1_USART, USART_IT_TXE, ENABLE);
}

void USART4_IRQHandler(void)
{
    const uint16_t rx_errors = USART_FLAG_ORE | USART_FLAG_FE |
                               USART_FLAG_NE | USART_FLAG_PE;
    const uint16_t status = RXD1_USART->STATR;
    if ((status & (USART_FLAG_RXNE | rx_errors)) == 0U) {
        return;
    }

    /* Reading STATR then DATAR clears the receive flags. */
    const uint8_t byte = (uint8_t)USART_ReceiveData(RXD1_USART);
    if ((status & rx_errors) != 0U) {
        probe_offset = 0;
        return;
    }

    if (byte != probe_request[probe_offset]) {
        /* 0x5a appears only at the start of the probe. */
        probe_offset = (byte == probe_request[0]) ? 1U : 0U;
        return;
    }

    if (++probe_offset == sizeof(probe_request)) {
        probe_offset = 0;
        if (pending_replies < UINT8_MAX) {
            ++pending_replies;
        }
        USART_ITConfig(TXD1_USART, USART_IT_TXE, ENABLE);
    }
}

void USART5_IRQHandler(void)
{
    if (tx_offset == 0U) {
        /* A probe reply takes priority at the next frame boundary. */
        sending_reply = pending_replies != 0U;
    }

    if (sending_reply) {
        USART_SendData(TXD1_USART, probe_reply[tx_offset++]);
        if (tx_offset == sizeof(probe_reply)) {
            tx_offset = 0;
            --pending_replies;
        }
    } else if (range_tail != range_head) {
        USART_SendData(TXD1_USART, range_queue[range_tail][tx_offset++]);
        if (tx_offset == LEGACY_RANGE_SIZE) {
            tx_offset = 0;
            range_tail = (range_tail + 1U) % RANGE_QUEUE_SIZE;
        }
    }

    if (pending_replies == 0U && range_tail == range_head) {
        USART_ITConfig(TXD1_USART, USART_IT_TXE, DISABLE);
    }
}
