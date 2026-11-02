#include "focus_unit.h"
#include "PIN.h"
#include "driver/uart.h"
#include <string.h>

#define PORT UART_NUM_2
#define CMD_STATUS 0x01
#define TIMEOUT_MS 100U
#define ACTIVE_POLL_MS 100U
#define IDLE_POLL_MS 1000U
#define STALE_MS 2500U
#define MAX_FRAME 73

static struct {
    bool ready, inflight, sending, valid, comm_failed;
    uint8_t next_seq, seq, cmd, retries, frame[MAX_FRAME], frame_len, tx_offset;
    uint8_t rx[MAX_FRAME], rx_len;
    uint32_t sent_ms, last_poll_ms, last_status_ms, command_ms;
    uint32_t poll_ms;
    bool command_rejected;
    focus_unit_snapshot_t status;
} link;

static uint16_t crc16(const uint8_t *data, unsigned length)
{
    uint16_t crc = 0xffff;
    while (length--) {
        crc ^= (uint16_t)*data++ << 8;
        for (unsigned i = 0; i < 8; ++i)
            crc = (uint16_t)((crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1);
    }
    return crc;
}

static int32_t signed32(const uint8_t *p)
{
    uint32_t value = (uint32_t)p[0] | (uint32_t)p[1] << 8 |
                     (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
    /* Conversion through int64 keeps the two's-complement interpretation defined. */
    return value <= INT32_MAX ? (int32_t)value : (int32_t)((int64_t)value - 0x100000000LL);
}

/* UART FIFO writes are nonblocking; a partial frame resumes on later UI ticks. */
static void begin_transmit(uint32_t now_ms)
{
    link.sending = true;
    link.tx_offset = 0;
    link.sent_ms = now_ms;
}

static void advance_transmit(uint32_t now_ms)
{
    const int written = uart_tx_chars(PORT, (const char *)link.frame + link.tx_offset,
                                      link.frame_len - link.tx_offset);
    if (written > 0) link.tx_offset += (uint8_t)written;
    if (link.tx_offset == link.frame_len) {
        link.sending = false;
        link.sent_ms = now_ms; /* Response deadline starts after the last byte is queued. */
    }
}

static void start_request(uint8_t cmd, int32_t raw, uint32_t now_ms)
{
    const uint8_t len = cmd == FOCUS_UNIT_MOVE || cmd == FOCUS_UNIT_SET_CURRENT ? 4 :
                        cmd == FOCUS_UNIT_HOME ? 2 : 0;
    link.frame[0] = 0xa5; link.frame[1] = 0x5a;
    link.frame[2] = 1; link.frame[3] = 0;
    link.seq = link.next_seq++; link.cmd = cmd;
    link.frame[4] = link.seq; link.frame[5] = cmd; link.frame[6] = len;
    if (len == 4) {
        const uint32_t value = (uint32_t)raw;
        for (unsigned i = 0; i < 4; ++i) link.frame[7+i] = (uint8_t)(value >> (8*i));
    } else if (len == 2) {
        link.frame[7] = 1; /* B direction */
        link.frame[8] = 1; /* complete homing */
    }
    if (cmd == CMD_STATUS) link.last_poll_ms = now_ms;
    /* Sending a control is not a communication failure. Keep the last fresh
     * status for display; the application uses status_sequence to require a
     * new query before accepting any post-command completion or STOP state. */
    const uint16_t crc = crc16(link.frame + 2, 5 + len);
    link.frame[7+len] = (uint8_t)crc; link.frame[8+len] = (uint8_t)(crc >> 8);
    link.frame_len = 9 + len;
    link.retries = 0; link.inflight = true;
    begin_transmit(now_ms);
}

static void receive_frame(const uint8_t *p, uint32_t now_ms)
{
    const uint8_t len = p[6], *data = p + 7;
    if (!link.inflight || p[2] != 1 || p[3] != 1 || p[4] != link.seq || p[5] != link.cmd) return;
    if (len != 1 && !(link.cmd == CMD_STATUS && len == 24 && data[0] == 0)) return;
    if (data[0] > 7) return;
    if (link.cmd == CMD_STATUS && len == 1 && data[0] == 0) return;
    if (link.cmd == CMD_STATUS && len == 24 &&
        (data[1] > 2 || data[2] > 4 || data[3] > 1 ||
         ((uint16_t)data[4] | (uint16_t)data[5] << 8) > 1000 ||
         data[6] > 1 || data[7] > 1 || data[8] > 1 || data[22] > 4 || data[23] > 1)) return;
    link.inflight = false; link.sending = false;
    if (link.cmd == CMD_STATUS && len == 24) {
        focus_unit_snapshot_t *s = &link.status;
        s->work_state=data[1]; s->mode=data[2]; s->direction=data[3];
        s->duty=(uint16_t)data[4] | (uint16_t)data[5]<<8;
        s->initialized=data[6]; s->zero_valid=data[7]; s->seek_zero=data[8];
        s->home_attempts=data[9]; s->home_error_counts=signed32(data+10);
        s->position=signed32(data+14); s->target_raw=signed32(data+18);
        s->move_state=data[22]; s->glide_completed=data[23];
        link.last_status_ms=now_ms; link.valid=true; link.comm_failed=false;
        ++s->status_sequence;
    } else if (link.cmd != CMD_STATUS) {
        /* An ACK is acceptance only; status polling supplies actual progress. */
        link.status.command_result=data[0];
        link.command_rejected=data[0] != 0; link.command_ms=now_ms;
        ++link.status.control_sequence;
        link.comm_failed=false;
        link.last_poll_ms=now_ms-link.poll_ms;
    } else {
        link.valid=false; link.comm_failed=true;
        link.last_poll_ms=now_ms;
    }
}

static void parse_rx(uint32_t now_ms)
{
    while (link.rx_len >= 2) {
        if (link.rx[0] != 0xa5 || link.rx[1] != 0x5a) {
            memmove(link.rx, link.rx+1, --link.rx_len); continue;
        }
        if (link.rx_len < 7) return;
        if (link.rx[6] > 64) { memmove(link.rx, link.rx+1, --link.rx_len); continue; }
        const unsigned length = 9 + link.rx[6];
        if (link.rx_len < length) return;
        const uint16_t expected=(uint16_t)link.rx[length-2] | (uint16_t)link.rx[length-1]<<8;
        if (crc16(link.rx+2, length-4) != expected) {
            memmove(link.rx, link.rx+1, --link.rx_len); continue;
        }
        receive_frame(link.rx, now_ms);
        link.rx_len -= length;
        memmove(link.rx, link.rx+length, link.rx_len);
    }
}

esp_err_t focus_unit_init(void)
{
    const uart_config_t config = {.baud_rate=460800, .data_bits=UART_DATA_8_BITS,
        .parity=UART_PARITY_DISABLE, .stop_bits=UART_STOP_BITS_1,
        .flow_ctrl=UART_HW_FLOWCTRL_DISABLE, .source_clk=UART_SCLK_DEFAULT};
    esp_err_t err=uart_param_config(PORT, &config);
    if (err != ESP_OK) return err;
    err=uart_set_pin(PORT, PIN_ESP_TXD2, PIN_ESP_RXD2, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err=uart_driver_install(PORT, 512, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;
    link.ready=true;
    link.poll_ms=IDLE_POLL_MS;
    /* Unsigned subtraction makes the first status request due immediately. */
    link.last_poll_ms=(uint32_t)-IDLE_POLL_MS;
    return ESP_OK;
}

esp_err_t focus_unit_request(focus_unit_command_t command, int32_t raw, uint32_t now_ms)
{
    if (command != FOCUS_UNIT_STOP && command != FOCUS_UNIT_HOME &&
        command != FOCUS_UNIT_MOVE && command != FOCUS_UNIT_SET_CURRENT)
        return ESP_ERR_INVALID_ARG;
    if (!link.ready || link.inflight) return ESP_ERR_INVALID_STATE;
    start_request((uint8_t)command, raw, now_ms);
    advance_transmit(now_ms); /* Queue control bytes now, without another UI tick. */
    return ESP_OK;
}

void focus_unit_set_active(bool active)
{
    link.poll_ms = active ? ACTIVE_POLL_MS : IDLE_POLL_MS;
}

void focus_unit_process(uint32_t now_ms)
{
    if (!link.ready) return;
    const uint32_t status_before = link.status.status_sequence;
    uint8_t bytes[64];
    int count;
    unsigned budget = 512;
    while (budget && (count=uart_read_bytes(PORT, bytes,
            budget < sizeof(bytes) ? budget : sizeof(bytes), 0)) > 0) {
        budget -= (unsigned)count;
        for (int i=0; i<count; ++i) {
            if (link.rx_len == sizeof(link.rx)) { memmove(link.rx, link.rx+1, --link.rx_len); }
            link.rx[link.rx_len++]=bytes[i];
            parse_rx(now_ms);
        }
    }
    /* Read buffered responses before deciding that the transaction timed out. */
    if (link.inflight && (uint32_t)(now_ms-link.sent_ms) >= TIMEOUT_MS) {
        if (link.retries++ == 0) begin_transmit(now_ms);
        else {
            link.inflight=false; link.sending=false; link.valid=false;
            link.comm_failed=true; link.rx_len=0;
            if (link.cmd != CMD_STATUS) {
                link.status.command_result=0xff; /* Result unknown; never replay with a new SEQ. */
                link.command_rejected=true; link.command_ms=now_ms;
                ++link.status.control_sequence;
            }
            link.last_poll_ms=now_ms; /* No automatic replay of uncertain motion. */
        }
    }
    if (!link.inflight && link.status.status_sequence == status_before) {
        /* Let the application act on a new status before starting another
         * query, even when a slow UI cycle already exceeded the poll period. */
        if ((uint32_t)(now_ms-link.last_poll_ms) >= link.poll_ms) start_request(CMD_STATUS, 0, now_ms);
    }
    if (link.inflight && link.sending) advance_transmit(now_ms);
}

focus_unit_snapshot_t focus_unit_snapshot(uint32_t now_ms)
{
    focus_unit_snapshot_t s=link.status;
    s.valid=link.ready && !link.comm_failed && link.valid &&
            (uint32_t)(now_ms-link.last_status_ms) < STALE_MS;
    s.command_rejected=link.ready && !link.comm_failed && link.command_rejected &&
                     (s.valid || (uint32_t)(now_ms-link.command_ms) < STALE_MS);
    return s;
}
