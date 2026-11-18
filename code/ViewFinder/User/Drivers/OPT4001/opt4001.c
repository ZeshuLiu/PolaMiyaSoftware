#include "opt4001.h"

#include "Board/PIN.h"
#include "Events/AppEvents.h"
#include "debug.h"

#define OPT4001_ADDR          (0x44U << 1)
#define REG_RESULT            0x00U
#define REG_CONFIG            0x0aU
#define REG_INT_CONFIG        0x0bU
#define REG_DEVICE_ID         0x11U
#define DEVICE_ID_MASK        0x3fffU
#define DEVICE_ID             0x0121U
#define CFG_AUTO_CONTINUOUS   0x3030U
#define INT_RESERVED          0x8000U
#define INT_OUTPUT            0x0010U
#define INT_EACH_CONVERSION   0x0004U
#define INT_I2C_BURST         0x0001U

#if OPT4001_SAMPLE_RATE_HZ == 5U
#define CONVERSION_CODE       9U /* 200 ms */
#elif OPT4001_SAMPLE_RATE_HZ == 10U
#define CONVERSION_CODE       8U /* 100 ms */
#elif OPT4001_SAMPLE_RATE_HZ == 20U
#define CONVERSION_CODE       7U /* 50 ms */
#elif OPT4001_SAMPLE_RATE_HZ == 40U
#define CONVERSION_CODE       6U /* 25 ms */
#else
#error "OPT4001_SAMPLE_RATE_HZ must be 5, 10, 20 or 40"
#endif

#define I2C_CLOCK_HZ          100000U
#define I2C_TIMEOUT_US        1000U
#define READ_TIMEOUT_MS       5U
#define I2C_READ_IRQS         (I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR)
#define I2C_ERRORS            (I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_AF |                                I2C_STAR1_OVR | I2C_STAR1_TIMEOUT)

static volatile uint32_t ready_sequence;
static uint32_t handled_sequence;
static uint32_t latest_millilux;
static bool sample_valid;
static bool initialized;

typedef enum {
    READ_IDLE, READ_START_WRITE, READ_ADDR_WRITE, READ_POINTER,
    READ_START_READ, READ_ADDR_READ, READ_FIRST, READ_LAST_THREE, READ_LAST_TWO,
    READ_DONE, READ_FAILED,
} ReadPhase;

static volatile ReadPhase read_phase;
static volatile uint8_t result_bytes[4];
static uint32_t read_started_at;

void I2C1_EV_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void I2C1_ER_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void EXTI9_5_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void InitBus(void)
{
    I2C_InitTypeDef bus = {
        .I2C_ClockSpeed = I2C_CLOCK_HZ,
        .I2C_Mode = I2C_Mode_I2C,
        .I2C_DutyCycle = I2C_DutyCycle_2,
        .I2C_Ack = I2C_Ack_Enable,
        .I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit,
    };

    I2C_DeInit(OPT_I2C);
    I2C_Init(OPT_I2C, &bus);
    I2C_Cmd(OPT_I2C, ENABLE);
}

static bool WaitIdle(void)
{
    for (unsigned elapsed = 0; elapsed < I2C_TIMEOUT_US; ++elapsed) {
        if ((OPT_I2C->CTLR1 & I2C_CTLR1_STOP) == 0U &&
            I2C_GetFlagStatus(OPT_I2C, I2C_FLAG_BUSY) == RESET) {
            return true;
        }
        Delay_Us(1);
    }
    return false;
}

static bool WaitStatus(uint16_t flag)
{
    for (unsigned elapsed = 0; elapsed < I2C_TIMEOUT_US; ++elapsed) {
        const uint16_t status = OPT_I2C->STAR1;
        if ((status & I2C_ERRORS) != 0U) {
            return false;
        }
        if ((status & flag) != 0U) {
            return true;
        }
        Delay_Us(1);
    }
    return false;
}

static void RecoverBus(void)
{
    if (I2C_GetFlagStatus(OPT_I2C, I2C_FLAG_MSL) != RESET) {
        I2C_GenerateSTOP(OPT_I2C, ENABLE);
        WaitIdle();
    }
    InitBus();
}

static bool StartAddress(uint8_t direction)
{
    if (!WaitIdle()) {
        return false;
    }
    I2C_GenerateSTART(OPT_I2C, ENABLE);
    if (!WaitStatus(I2C_STAR1_SB)) {
        return false;
    }
    I2C_Send7bitAddress(OPT_I2C, OPT4001_ADDR, direction);
    return WaitStatus(I2C_STAR1_ADDR);
}

static bool SendByte(uint8_t byte)
{
    if (!WaitStatus(I2C_STAR1_TXE)) {
        return false;
    }
    I2C_SendData(OPT_I2C, byte);
    return true;
}

static bool SelectRegister(uint8_t reg)
{
    if (!StartAddress(I2C_Direction_Transmitter)) {
        return false;
    }
    (void)OPT_I2C->STAR2; /* STAR1 was read by WaitStatus; this clears ADDR. */
    return SendByte(reg);
}

static bool WriteRegister(uint8_t reg, uint16_t value)
{
    if (!SelectRegister(reg) || !SendByte((uint8_t)(value >> 8)) ||
        !SendByte((uint8_t)value) || !WaitStatus(I2C_STAR1_BTF)) {
        RecoverBus();
        return false;
    }
    I2C_GenerateSTOP(OPT_I2C, ENABLE);
    if (!WaitIdle()) {
        RecoverBus();
        return false;
    }
    return true;
}

/* The startup probe uses the two-byte master receiver sequence. */
static bool ReadDeviceId(uint8_t id[2])
{
    if (!SelectRegister(REG_DEVICE_ID) || !WaitStatus(I2C_STAR1_BTF)) {
        RecoverBus();
        return false;
    }
    I2C_GenerateSTOP(OPT_I2C, ENABLE);
    if (!WaitIdle()) {
        RecoverBus();
        return false;
    }

    I2C_NACKPositionConfig(OPT_I2C, I2C_NACKPosition_Next);
    I2C_AcknowledgeConfig(OPT_I2C, ENABLE);
    if (!StartAddress(I2C_Direction_Receiver)) {
        RecoverBus();
        return false;
    }

    __disable_irq();
    (void)OPT_I2C->STAR2;
    I2C_AcknowledgeConfig(OPT_I2C, DISABLE);
    __enable_irq();
    if (!WaitStatus(I2C_STAR1_BTF)) {
        RecoverBus();
        return false;
    }

    __disable_irq();
    I2C_GenerateSTOP(OPT_I2C, ENABLE);
    id[0] = I2C_ReceiveData(OPT_I2C);
    id[1] = I2C_ReceiveData(OPT_I2C);
    __enable_irq();
    if (!WaitIdle()) {
        RecoverBus();
        return false;
    }
    I2C_NACKPositionConfig(OPT_I2C, I2C_NACKPosition_Current);
    I2C_AcknowledgeConfig(OPT_I2C, ENABLE);
    return true;
}

static bool DecodeMillilux(const uint8_t result[4], uint32_t *millilux)
{
    const uint32_t packed = ((uint32_t)result[0] << 24) | ((uint32_t)result[1] << 16) |
                            ((uint32_t)result[2] << 8) | result[3];
    const uint32_t payload = packed >> 4;
    const uint32_t exponent = payload >> 24;
    const uint32_t mantissa = (payload >> 4) & 0xfffffU;

    /* TI register 01h checks four parities across exponent, mantissa and counter. */
    const unsigned crc = (unsigned)__builtin_parity(payload) |
                         ((unsigned)__builtin_parity(payload & 0x0aaaaaaaU) << 1) |
                         ((unsigned)__builtin_parity(payload & 0x08888888U) << 2) |
                         ((unsigned)__builtin_parity(payload & 0x00808080U) << 3);
    if (exponent > 8U || crc != (packed & 0xfU)) {
        return false;
    }

    /* SOT-5x3: 0.0004375 lux per scaled count, or 7/16 millilux. */
    *millilux = ((mantissa << exponent) * 7U + 8U) / 16U;
    return true;
}

void EXTI9_5_IRQHandler(void)
{
    if (EXTI_GetITStatus(OPT_INT_EXTI_LINE) != RESET) {
        EXTI_ClearITPendingBit(OPT_INT_EXTI_LINE);
        ++ready_sequence;
        AppEvents_PostFromISR(APP_OPT_READY);
    }
}

static void FinishReadFromISR(ReadPhase result)
{
    I2C_ITConfig(OPT_I2C, I2C_READ_IRQS, DISABLE);
    read_phase = result;
    AppEvents_PostFromISR(APP_OPT_DONE);
}

void I2C1_ER_IRQHandler(void)
{
    if (read_phase != READ_IDLE && read_phase < READ_DONE) {
        FinishReadFromISR(READ_FAILED);
    }
}

void I2C1_EV_IRQHandler(void)
{
    const uint16_t status = OPT_I2C->STAR1;
    if ((status & I2C_ERRORS) != 0U) {
        I2C_ITConfig(OPT_I2C, I2C_READ_IRQS, DISABLE);
        if (read_phase != READ_IDLE && read_phase < READ_DONE) {
            FinishReadFromISR(READ_FAILED);
        }
        return;
    }

    switch (read_phase) {
    case READ_START_WRITE:
        if ((status & I2C_STAR1_SB) != 0U) {
            I2C_Send7bitAddress(OPT_I2C, OPT4001_ADDR, I2C_Direction_Transmitter);
            read_phase = READ_ADDR_WRITE;
        }
        break;
    case READ_ADDR_WRITE:
        if ((status & I2C_STAR1_ADDR) != 0U) {
            (void)OPT_I2C->STAR2;
            I2C_SendData(OPT_I2C, REG_RESULT);
            read_phase = READ_POINTER;
        }
        break;
    case READ_POINTER:
        if ((status & I2C_STAR1_BTF) != 0U) {
            read_phase = READ_START_READ;
            I2C_GenerateSTART(OPT_I2C, ENABLE);
        }
        break;
    case READ_START_READ:
        if ((status & I2C_STAR1_SB) != 0U) {
            I2C_Send7bitAddress(OPT_I2C, OPT4001_ADDR, I2C_Direction_Receiver);
            read_phase = READ_ADDR_READ;
        }
        break;
    case READ_ADDR_READ:
        if ((status & I2C_STAR1_ADDR) != 0U) {
            (void)OPT_I2C->STAR2;
            read_phase = READ_FIRST;
            I2C_ITConfig(OPT_I2C, I2C_IT_BUF, ENABLE);
        }
        break;
    case READ_FIRST:
        if ((status & I2C_STAR1_RXNE) != 0U) {
            result_bytes[0] = I2C_ReceiveData(OPT_I2C);
            I2C_ITConfig(OPT_I2C, I2C_IT_BUF, DISABLE);
            read_phase = READ_LAST_THREE;
        }
        break;
    case READ_LAST_THREE:
        if ((status & I2C_STAR1_BTF) != 0U) {
            /* BTF holds SCL until ACK is cleared for the final byte. */
            I2C_AcknowledgeConfig(OPT_I2C, DISABLE);
            result_bytes[1] = I2C_ReceiveData(OPT_I2C);
            read_phase = READ_LAST_TWO;
        }
        break;
    case READ_LAST_TWO:
        if ((status & I2C_STAR1_BTF) != 0U) {
            I2C_GenerateSTOP(OPT_I2C, ENABLE);
            result_bytes[2] = I2C_ReceiveData(OPT_I2C);
            result_bytes[3] = I2C_ReceiveData(OPT_I2C);
            FinishReadFromISR(READ_DONE);
        }
        break;
    default:
        I2C_ITConfig(OPT_I2C, I2C_READ_IRQS, DISABLE);
        break;
    }
}

bool OPT4001_Init(void)
{
    if (initialized) {
        return true;
    }

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_I2C1, ENABLE);
    InitBus();

    uint8_t id[2];
    if (!ReadDeviceId(id) ||
        ((((uint16_t)id[0] << 8) | id[1]) & DEVICE_ID_MASK) != DEVICE_ID ||
        !WriteRegister(REG_INT_CONFIG, INT_RESERVED | INT_OUTPUT | INT_EACH_CONVERSION | INT_I2C_BURST)) {
        return false;
    }

    EXTI_InitTypeDef irq = {
        .EXTI_Line = OPT_INT_EXTI_LINE,
        .EXTI_Mode = EXTI_Mode_Interrupt,
        .EXTI_Trigger = EXTI_Trigger_Falling,
        .EXTI_LineCmd = ENABLE,
    };
    EXTI_ClearITPendingBit(OPT_INT_EXTI_LINE);
    EXTI_Init(&irq);
    NVIC_SetPriority(EXTI9_5_IRQn, 0);
    NVIC_EnableIRQ(EXTI9_5_IRQn);

    /* INT_POL is zero: active-low, open-drain data-ready pulse. */
    if (!WriteRegister(REG_CONFIG, CFG_AUTO_CONTINUOUS | (CONVERSION_CODE << 6))) {
        irq.EXTI_LineCmd = DISABLE;
        EXTI_Init(&irq);
        EXTI_ClearITPendingBit(OPT_INT_EXTI_LINE);
        return false;
    }

    NVIC_SetPriority(I2C1_EV_IRQn, 0);
    NVIC_SetPriority(I2C1_ER_IRQn, 0);
    NVIC_EnableIRQ(I2C1_EV_IRQn);
    NVIC_EnableIRQ(I2C1_ER_IRQn);
    initialized = true;
    return true;
}

void OPT4001_Service(uint32_t events, uint32_t now)
{
    if (!initialized || (events & (APP_OPT_READY | APP_OPT_DONE | APP_TICK)) == 0U) {
        return;
    }

    const ReadPhase phase = read_phase;
    if (phase == READ_DONE) {
        const uint8_t result[4] = {result_bytes[0], result_bytes[1], result_bytes[2], result_bytes[3]};
        uint32_t millilux;
        if (DecodeMillilux(result, &millilux)) {
            latest_millilux = millilux;
            sample_valid = true;
        }
        read_phase = READ_IDLE;
    } else if (phase == READ_FAILED) {
        RecoverBus();
        read_phase = READ_IDLE;
    } else if (phase != READ_IDLE) {
        if (now - read_started_at >= READ_TIMEOUT_MS) {
            I2C_ITConfig(OPT_I2C, I2C_READ_IRQS, DISABLE);
            if (read_phase < READ_DONE) {
                RecoverBus();
                read_phase = READ_IDLE;
            }
        }
        return;
    }

    const uint32_t ready = ready_sequence;
    if (ready == handled_sequence || (OPT_I2C->CTLR1 & I2C_CTLR1_STOP) != 0U ||
        I2C_GetFlagStatus(OPT_I2C, I2C_FLAG_BUSY) != RESET) {
        return;
    }

    handled_sequence = ready;
    read_started_at = now;
    I2C_NACKPositionConfig(OPT_I2C, I2C_NACKPosition_Current);
    I2C_AcknowledgeConfig(OPT_I2C, ENABLE);
    read_phase = READ_START_WRITE;
    I2C_ITConfig(OPT_I2C, I2C_IT_EVT | I2C_IT_ERR, ENABLE);
    I2C_GenerateSTART(OPT_I2C, ENABLE);
}

bool OPT4001_GetLatestMillilux(uint32_t *millilux)
{
    if (!sample_valid || millilux == NULL) {
        return false;
    }
    *millilux = latest_millilux;
    return true;
}
