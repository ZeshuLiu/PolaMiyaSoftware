#include "FocusUnit/Uart/focus_uart.h"
#include "FocusUnit/Adc/focus_adc.h"
#include "FocusUnit/Encoder/focus_encoder.h"
#include "FocusUnit/Motor/focus_motor.h"
#include <string.h>

#define RX_RING_SIZE 128U
#define FRAME_MAX 73U
#define PAYLOAD_MAX 64U
#define RX_GAP_FLAG 0x100U

enum { RESULT_OK, RESULT_LENGTH, RESULT_PARAM, RESULT_BUSY, RESULT_ERROR,
       RESULT_NO_DATA, RESULT_SEQ_CONFLICT, RESULT_UNKNOWN };

static UART_HandleTypeDef *s_uart;
static uint8_t s_rx_byte;
static volatile uint16_t s_rx_ring[RX_RING_SIZE];
static volatile uint8_t s_rx_head, s_rx_tail, s_rx_overflow, s_rx_error;
static uint32_t s_rx_last_ms;
static uint8_t s_rx_seen;
static volatile uint8_t s_tx_busy;
static uint8_t s_frame[FRAME_MAX], s_frame_len;
static uint8_t s_request[FRAME_MAX], s_request_len, s_cached;
static uint8_t s_response[FRAME_MAX], s_response_len;
static uint8_t s_tx_frame[FRAME_MAX];

static uint16_t crc16(const uint8_t *data, uint8_t length)
{
    uint16_t crc = 0xFFFFU;
    while (length-- != 0U) {
        crc ^= (uint16_t)*data++ << 8;
        for (uint8_t i = 0U; i < 8U; ++i)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

static void put16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put32(uint8_t *p, uint32_t value)
{
    put16(p, (uint16_t)value);
    put16(p + 2, (uint16_t)(value >> 16));
}

static uint32_t get32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* 在主循环中执行命令，结果序列化到 payload。 */
static uint8_t execute(uint8_t cmd, const uint8_t *p, uint8_t len, uint8_t *out)
{
    FocusMotor_Snapshot motor;
    FocusEncoderMeasurement encoder;
    FocusAdcMeasurement adc;
    uint8_t expected;
    FocusMotor_GetSnapshot(&motor);
    switch (cmd) {
    case 0x01: case 0x02: case 0x10: expected = 0U; break;
    case 0x11: expected = 2U; break;
    case 0x12: expected = 3U; break;
    case 0x13: expected = 4U; break;
    default: out[0] = RESULT_UNKNOWN; return 1U;
    }
    if (len != expected) { out[0] = RESULT_LENGTH; return 1U; }
    if ((cmd == 0x11 && (p[0] > 1U || p[1] > 1U)) ||
        (cmd == 0x12 && (p[0] > 1U || ((uint16_t)p[1] | ((uint16_t)p[2] << 8)) > 1000U))) {
        out[0] = RESULT_PARAM; return 1U;
    }
    out[0] = RESULT_OK;
    if (cmd == 0x01) {
        FocusEncoder_GetSnapshot(&encoder);
        out[1] = (uint8_t)motor.work_state; out[2] = (uint8_t)motor.mode;
        out[3] = (uint8_t)motor.direction; put16(out + 4, motor.duty_permille);
        out[6] = motor.initialized; out[7] = motor.zero_valid; out[8] = motor.seek_zero;
        out[9] = motor.home_attempts; put32(out + 10, (uint32_t)motor.home_error_counts);
        put32(out + 14, (uint32_t)encoder.position); put32(out + 18, (uint32_t)motor.target_raw);
        out[22] = (uint8_t)motor.move_state; out[23] = motor.glide_completed;
        return 24U;
    }
    if (cmd == 0x02) {
        if (FocusAdc_GetSnapshot(&adc) == 0U) { out[0] = RESULT_NO_DATA; return 1U; }
        put32(out + 1, adc.timestamp_ms); put32(out + 5, adc.sequence); out[9] = adc.valid;
        put16(out + 10, adc.raw_ntc1); put16(out + 12, adc.raw_3v3);
        put16(out + 14, adc.raw_ntc2); put16(out + 16, adc.raw_6v);
        put16(out + 18, adc.raw_motor); put16(out + 20, adc.raw_temperature);
        put16(out + 22, adc.raw_vrefint); return 24U;
    }
    if (motor.work_state == FOCUS_MOTOR_WORK_ERROR) { out[0] = RESULT_ERROR; return 1U; }
    if ((motor.initialized == 0U) || (motor.work_state != FOCUS_MOTOR_WORK_NORMAL)) {
        out[0] = RESULT_BUSY; return 1U;
    }
    switch (cmd) {
    case 0x10: FocusMotor_Stop(); break;
    case 0x11: FocusMotor_Glide((FocusMotor_Direction)p[0], p[1]); break;
    case 0x12: FocusMotor_OpenLoop((FocusMotor_Direction)p[0],
                                    (uint16_t)p[1] | ((uint16_t)p[2] << 8)); break;
    case 0x13:
        if (FocusMotor_MoveRaw((int32_t)get32(p)) != HAL_OK) out[0] = RESULT_BUSY;
        break;
    default: break;
    }
    return 1U;
}

static void send_response(const uint8_t *frame, uint8_t length)
{
    memcpy(s_tx_frame, frame, length);
    s_tx_busy = 1U;
    if (HAL_UART_Transmit_IT(s_uart, s_tx_frame, length) != HAL_OK) s_tx_busy = 0U;
}

static void handle_frame(void)
{
    uint8_t payload[24], length;
    uint16_t crc;
    if ((s_frame[2] != 1U) || (s_frame[3] != 0U)) return;
    crc = crc16(s_frame + 2, (uint8_t)(5U + s_frame[6]));
    if (s_frame[7U + s_frame[6]] != (uint8_t)crc ||
        s_frame[8U + s_frame[6]] != (uint8_t)(crc >> 8)) return;
    /* 零位未确认时连序号冲突也返回 BUSY，不覆盖已执行命令的去重缓存。 */
    FocusMotor_Snapshot motor;
    FocusMotor_GetSnapshot(&motor);
    if (motor.zero_valid == 0U) {
        uint8_t busy[10] = {0xA5U, 0x5AU, 1U, 1U, 0U, 0U, 1U, RESULT_BUSY, 0U, 0U};
        busy[4] = s_frame[4]; busy[5] = s_frame[5];
        crc = crc16(busy + 2, 6U); put16(busy + 8, crc);
        send_response(busy, sizeof(busy));
        return;
    }
    if (s_cached != 0U && s_frame[4] == s_request[4]) {
        if (s_frame_len == s_request_len && memcmp(s_frame, s_request, s_frame_len) == 0) {
            send_response(s_response, s_response_len);
        } else {
            uint8_t conflict[10] = {0xA5U, 0x5AU, 1U, 1U, 0U, 0U, 1U, RESULT_SEQ_CONFLICT, 0U, 0U};
            conflict[4] = s_frame[4]; conflict[5] = s_frame[5];
            crc = crc16(conflict + 2, 6U); put16(conflict + 8, crc);
            send_response(conflict, sizeof(conflict));
        }
        return;
    }
    length = execute(s_frame[5], s_frame + 7, s_frame[6], payload);
    memcpy(s_request, s_frame, s_frame_len); s_request_len = s_frame_len; s_cached = 1U;
    s_response[0] = 0xA5U; s_response[1] = 0x5AU; s_response[2] = 1U; s_response[3] = 1U;
    s_response[4] = s_frame[4]; s_response[5] = s_frame[5]; s_response[6] = length;
    memcpy(s_response + 7, payload, length); crc = crc16(s_response + 2, (uint8_t)(5U + length));
    put16(s_response + 7U + length, crc); s_response_len = (uint8_t)(9U + length);
    send_response(s_response, s_response_len);
}

static void consume(uint16_t item)
{
    uint8_t byte = (uint8_t)item;
    if ((item & RX_GAP_FLAG) != 0U) s_frame_len = 0U;
    if (s_frame_len == 0U) { if (byte == 0xA5U) s_frame[s_frame_len++] = byte; return; }
    if (s_frame_len == 1U && byte != 0x5AU) {
        s_frame_len = (byte == 0xA5U) ? 1U : 0U; return;
    }
    s_frame[s_frame_len++] = byte;
    if (s_frame_len == 7U && s_frame[6] > PAYLOAD_MAX) { s_frame_len = 0U; return; }
    if (s_frame_len >= 7U && s_frame_len == (uint8_t)(9U + s_frame[6])) {
        handle_frame(); s_frame_len = 0U;
    }
}

HAL_StatusTypeDef FocusUart_Init(UART_HandleTypeDef *uart)
{
    s_uart = uart;
    return HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U);
}

void FocusUart_Process(void)
{
    if (s_rx_overflow != 0U || s_rx_error != 0U) {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        uint8_t error = s_rx_error;
        s_rx_error = 0U; s_rx_overflow = 0U; s_rx_tail = s_rx_head;
        s_frame_len = 0U;
        __set_PRIMASK(primask);
        if (error != 0U) {
            (void)HAL_UART_AbortReceive(s_uart);
            if (HAL_UART_Receive_IT(s_uart, &s_rx_byte, 1U) != HAL_OK) s_rx_error = 1U;
        }
    }
    while (s_tx_busy == 0U && s_rx_tail != s_rx_head) {
        uint16_t item = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint8_t)((s_rx_tail + 1U) % RX_RING_SIZE);
        consume(item);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart != s_uart) return;
    uint32_t now = HAL_GetTick();
    uint16_t item = s_rx_byte;
    if ((s_rx_seen != 0U) && ((uint32_t)(now - s_rx_last_ms) >= 20U)) item |= RX_GAP_FLAG;
    s_rx_last_ms = now; s_rx_seen = 1U;
    uint8_t next = (uint8_t)((s_rx_head + 1U) % RX_RING_SIZE);
    if (next == s_rx_tail) s_rx_overflow = 1U;
    else { s_rx_ring[s_rx_head] = item; s_rx_head = next; }
    if (HAL_UART_Receive_IT(uart, &s_rx_byte, 1U) != HAL_OK) s_rx_error = 1U;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *uart)
{
    if (uart == s_uart) s_tx_busy = 0U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart)
{
    if (uart == s_uart) s_rx_error = 1U;
}
