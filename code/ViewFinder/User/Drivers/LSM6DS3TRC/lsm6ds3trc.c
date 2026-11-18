#include "lsm6ds3trc.h"

#include "Board/PIN.h"
#include "Events/AppEvents.h"
#include "debug.h"

#define REG_INT1_CTRL       0x0dU
#define REG_WHO_AM_I        0x0fU
#define REG_CTRL1_XL        0x10U
#define REG_CTRL2_G         0x11U
#define REG_CTRL3_C         0x12U
#define REG_CTRL4_C         0x13U
#define REG_STATUS          0x1eU
#define REG_OUTX_L_G        0x22U

#define DEVICE_ID           0x6aU
#define CTRL3_RESET         0x01U
#define CTRL3_IF_INC        0x04U
#define CTRL3_BDU           0x40U
#define CTRL4_I2C_DISABLE   0x04U
#define INT1_DRDY_G         0x02U
#define STATUS_XL_READY     0x01U
#define STATUS_G_READY      0x02U
#define SPI_READ_BIT        0x80U

#define SPI_TIMEOUT_US      100U
#define RESET_TIMEOUT_MS    20U
#define READ_TIMEOUT_MS     5U
#define AXIS_BYTES          12U

#if LSM6DS3TRC_ODR_HZ == 26U
#define ODR_BITS            0x20U
#elif LSM6DS3TRC_ODR_HZ == 52U
#define ODR_BITS            0x30U
#elif LSM6DS3TRC_ODR_HZ == 104U
#define ODR_BITS            0x40U
#else
#error "LSM6DS3TRC_ODR_HZ must be 26, 52 or 104"
#endif

static volatile uint32_t ready_sequence;
static uint32_t handled_sequence;
static bool initialized;
static bool sample_valid;
static LSM6DS3TRC_Sample latest_sample;

typedef enum { IMU_IDLE, IMU_STATUS_READ, IMU_AXES_READ } ReadPhase;
static ReadPhase read_phase;
static uint32_t read_started_at;
static uint32_t requested_sequence;
static volatile uint8_t spi_rx[AXIS_BYTES];
static volatile uint8_t spi_index;
static volatile uint8_t spi_length;
static volatile bool spi_done;
static volatile bool spi_error;

void SPI2_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

void EXTI4_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));

static void InitSpi(void)
{
    SPI_InitTypeDef config = {
        .SPI_Direction = SPI_Direction_2Lines_FullDuplex,
        .SPI_Mode = SPI_Mode_Master,
        .SPI_DataSize = SPI_DataSize_8b,
        .SPI_CPOL = SPI_CPOL_High,
        .SPI_CPHA = SPI_CPHA_2Edge,
        .SPI_NSS = SPI_NSS_Soft,
        .SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_32,
        .SPI_FirstBit = SPI_FirstBit_MSB,
        .SPI_CRCPolynomial = 7,
    };

    SPI_I2S_DeInit(IMU_SPI);
    SPI_Init(IMU_SPI, &config);
    SPI_NSSInternalSoftwareConfig(IMU_SPI, SPI_NSSInternalSoft_Set);
    SPI_Cmd(IMU_SPI, ENABLE);
}

static bool WaitSpi(uint16_t flag, bool set)
{
    for (unsigned elapsed = 0; elapsed < SPI_TIMEOUT_US; ++elapsed) {
        if (((IMU_SPI->STATR & flag) != 0U) == set) {
            return true;
        }
        Delay_Us(1);
    }
    return false;
}

static bool ExchangeByte(uint8_t tx, uint8_t *rx)
{
    if (!WaitSpi(SPI_I2S_FLAG_TXE, true)) {
        return false;
    }
    SPI_I2S_SendData(IMU_SPI, tx);
    if (!WaitSpi(SPI_I2S_FLAG_RXNE, true)) {
        return false;
    }
    *rx = (uint8_t)SPI_I2S_ReceiveData(IMU_SPI);
    return true;
}

static bool TransferRegisters(uint8_t command, uint8_t *data, unsigned length)
{
    const bool read = (command & SPI_READ_BIT) != 0U;
    uint8_t rx;

    GPIO_ResetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    Delay_Us(1);
    bool ok = ExchangeByte(command, &rx);
    for (unsigned i = 0; ok && i < length; ++i) {
        ok = ExchangeByte(read ? 0U : data[i], &rx);
        if (ok && read) {
            data[i] = rx;
        }
    }
    ok = WaitSpi(SPI_I2S_FLAG_TXE, true) &&
         WaitSpi(SPI_I2S_FLAG_BSY, false) && ok;
    Delay_Us(1);
    GPIO_SetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    Delay_Us(1);

    if (!ok) {
        InitSpi();
    }
    return ok;
}

static bool WriteRegister(uint8_t reg, uint8_t value)
{
    return TransferRegisters(reg, &value, 1);
}

static bool ReadRegisters(uint8_t reg, uint8_t *data, unsigned length)
{
    return TransferRegisters(reg | SPI_READ_BIT, data, length);
}

static bool ResetDevice(void)
{
    if (!WriteRegister(REG_CTRL3_C, CTRL3_RESET)) {
        return false;
    }
    for (unsigned elapsed = 0; elapsed < RESET_TIMEOUT_MS; ++elapsed) {
        uint8_t control;
        Delay_Ms(1);
        if (ReadRegisters(REG_CTRL3_C, &control, 1) && (control & CTRL3_RESET) == 0U) {
            return true;
        }
    }
    return false;
}

void EXTI4_IRQHandler(void)
{
    if (EXTI_GetITStatus(IMU_INT1_EXTI_LINE) != RESET) {
        EXTI_ClearITPendingBit(IMU_INT1_EXTI_LINE);
        ++ready_sequence;
        AppEvents_PostFromISR(APP_IMU_READY);
    }
}

static void SetSpiInterrupts(FunctionalState enabled)
{
    SPI_I2S_ITConfig(IMU_SPI, SPI_I2S_IT_RXNE, enabled);
    SPI_I2S_ITConfig(IMU_SPI, SPI_I2S_IT_ERR, enabled);
}

static void FinishSpiFromISR(bool error)
{
    SetSpiInterrupts(DISABLE);
    spi_error = error;
    spi_done = true;
    AppEvents_PostFromISR(APP_IMU_DONE);
}

void SPI2_IRQHandler(void)
{
    const uint16_t status = IMU_SPI->STATR;
    if ((status & (SPI_I2S_FLAG_OVR | SPI_FLAG_MODF)) != 0U) {
        FinishSpiFromISR(true);
        return;
    }
    if ((status & SPI_I2S_FLAG_RXNE) == 0U) {
        return;
    }

    const uint8_t byte = (uint8_t)SPI_I2S_ReceiveData(IMU_SPI);
    const uint8_t index = spi_index;
    if (index != 0U) {
        spi_rx[index - 1U] = byte;
    }
    if (index == spi_length) {
        FinishSpiFromISR(false);
    } else {
        spi_index = index + 1U;
        SPI_I2S_SendData(IMU_SPI, 0);
    }
}

static void BeginRead(ReadPhase phase, uint8_t reg, uint8_t length, uint32_t now)
{
    read_phase = phase;
    read_started_at = now;
    spi_index = 0;
    spi_length = length;
    spi_error = false;
    spi_done = false;

    GPIO_ResetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    Delay_Us(1);
    SetSpiInterrupts(ENABLE);
    SPI_I2S_SendData(IMU_SPI, reg | SPI_READ_BIT);
}

static void AbortRead(void)
{
    SetSpiInterrupts(DISABLE);
    GPIO_SetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    InitSpi();
    read_phase = IMU_IDLE;
}

bool LSM6DS3TRC_Init(void)
{
    if (initialized) {
        return true;
    }

    RCC_PB1PeriphClockCmd(RCC_PB1Periph_SPI2, ENABLE);
    GPIO_SetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    InitSpi();

    uint8_t id;
    if (!ReadRegisters(REG_WHO_AM_I, &id, 1) || id != DEVICE_ID ||
        !ResetDevice() ||
        !WriteRegister(REG_CTRL3_C, CTRL3_BDU | CTRL3_IF_INC) ||
        !WriteRegister(REG_CTRL4_C, CTRL4_I2C_DISABLE) ||
        !WriteRegister(REG_INT1_CTRL, INT1_DRDY_G)) {
        return false;
    }

    EXTI_InitTypeDef irq = {
        .EXTI_Line = IMU_INT1_EXTI_LINE,
        .EXTI_Mode = EXTI_Mode_Interrupt,
        .EXTI_Trigger = EXTI_Trigger_Rising,
        .EXTI_LineCmd = ENABLE,
    };
    EXTI_ClearITPendingBit(IMU_INT1_EXTI_LINE);
    EXTI_Init(&irq);
    NVIC_SetPriority(EXTI4_IRQn, 0);
    NVIC_EnableIRQ(EXTI4_IRQn);

    /* Full-scale bits left at zero: +/-2 g and +/-250 degrees/s. */
    if (!WriteRegister(REG_CTRL1_XL, ODR_BITS) || !WriteRegister(REG_CTRL2_G, ODR_BITS)) {
        irq.EXTI_LineCmd = DISABLE;
        EXTI_Init(&irq);
        EXTI_ClearITPendingBit(IMU_INT1_EXTI_LINE);
        WriteRegister(REG_CTRL1_XL, 0);
        WriteRegister(REG_CTRL2_G, 0);
        return false;
    }

    NVIC_SetPriority(SPI2_IRQn, 0);
    NVIC_EnableIRQ(SPI2_IRQn);
    initialized = true;
    return true;
}

static int16_t ReadAxis(const volatile uint8_t *data)
{
    return (int16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8));
}

void LSM6DS3TRC_Service(uint32_t events, uint32_t now)
{
    if (!initialized || (events & (APP_IMU_READY | APP_IMU_DONE | APP_TICK)) == 0U) {
        return;
    }

    if (read_phase != IMU_IDLE) {
        if (spi_done && spi_error) {
            AbortRead();
            return;
        }
        if (!spi_done || (IMU_SPI->STATR & SPI_I2S_FLAG_BSY) != 0U) {
            if (now - read_started_at >= READ_TIMEOUT_MS) {
                AbortRead();
            }
            return;
        }

        Delay_Us(1);
        GPIO_SetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
        const ReadPhase completed = read_phase;
        read_phase = IMU_IDLE;

        if (completed == IMU_STATUS_READ) {
            const uint8_t status = spi_rx[0];
            if ((status & STATUS_G_READY) == 0U) {
                handled_sequence = requested_sequence;
            } else if ((status & STATUS_XL_READY) != 0U) {
                BeginRead(IMU_AXES_READ, REG_OUTX_L_G, AXIS_BYTES, now);
            }
            return;
        }

        for (unsigned axis = 0; axis < 3U; ++axis) {
            latest_sample.gyro_raw[axis] = ReadAxis(&spi_rx[axis * 2U]);
            latest_sample.accel_raw[axis] = ReadAxis(&spi_rx[6U + axis * 2U]);
        }
        ++latest_sample.sequence;
        sample_valid = true;
        handled_sequence = requested_sequence;
    }

    const uint32_t ready = ready_sequence;
    if (ready != handled_sequence) {
        requested_sequence = ready;
        BeginRead(IMU_STATUS_READ, REG_STATUS, 1, now);
    }
}

bool LSM6DS3TRC_GetLatest(LSM6DS3TRC_Sample *sample)
{
    if (sample == NULL || !sample_valid) {
        return false;
    }
    *sample = latest_sample;
    return true;
}
