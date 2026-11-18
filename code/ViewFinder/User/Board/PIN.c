#include "PIN.h"

static void InitPin(GPIO_TypeDef *port, uint16_t pin, GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef config = {
        .GPIO_Pin = pin,
        .GPIO_Speed = GPIO_Speed_Medium,
        .GPIO_Mode = mode,
    };

    GPIO_Init(port, &config);
}

static void InitAlternatePin(GPIO_TypeDef *port, uint16_t pin, uint8_t source,
                             uint8_t af, GPIOMode_TypeDef mode)
{
    GPIO_PinAFConfig(port, source, af);
    InitPin(port, pin, mode);
}

void BoardPins_Init(void)
{
    RCC_PB2PeriphClockCmd(RCC_PB2Periph_AFIO | RCC_PB2Periph_GPIOA |
                         RCC_PB2Periph_GPIOB | RCC_PB2Periph_GPIOC, ENABLE);

    /* Load the startup levels before enabling the output drivers. */
    GPIO_SetBits(LCD_BL_PWM_PORT, LCD_BL_PWM_PIN);
    GPIO_SetBits(LCD_CS_N_PORT, LCD_CS_N_PIN);
    GPIO_SetBits(IMU_CS_N_PORT, IMU_CS_N_PIN);
    GPIO_ResetBits(LCD_RST_N_PORT, LCD_RST_N_PIN);
    GPIO_ResetBits(LCD_DC_PORT, LCD_DC_PIN);

    InitPin(LCD_BL_PWM_PORT, LCD_BL_PWM_PIN, GPIO_Mode_Out_PP);
    InitPin(LCD_CS_N_PORT, LCD_CS_N_PIN, GPIO_Mode_Out_PP);
    InitPin(IMU_CS_N_PORT, IMU_CS_N_PIN, GPIO_Mode_Out_PP);
    InitPin(LCD_RST_N_PORT, LCD_RST_N_PIN, GPIO_Mode_Out_PP);
    InitPin(LCD_DC_PORT, LCD_DC_PIN, GPIO_Mode_Out_PP);

    InitAlternatePin(LCD_SCK_PORT, LCD_SCK_PIN, LCD_SCK_SOURCE,
                     LCD_SCK_AF, GPIO_Mode_AF_PP);
    InitAlternatePin(LCD_MOSI_PORT, LCD_MOSI_PIN, LCD_MOSI_SOURCE,
                     LCD_MOSI_AF, GPIO_Mode_AF_PP);

    InitAlternatePin(IMU_SCK_PORT, IMU_SCK_PIN, IMU_SCK_SOURCE,
                     IMU_SCK_AF, GPIO_Mode_AF_PP);
    InitAlternatePin(IMU_MISO_PORT, IMU_MISO_PIN, IMU_MISO_SOURCE,
                     IMU_MISO_AF, GPIO_Mode_IN_FLOATING);
    InitAlternatePin(IMU_MOSI_PORT, IMU_MOSI_PIN, IMU_MOSI_SOURCE,
                     IMU_MOSI_AF, GPIO_Mode_AF_PP);

    InitAlternatePin(OPT_SCL_PORT, OPT_SCL_PIN, OPT_SCL_SOURCE,
                     OPT_SCL_AF, GPIO_Mode_AF_OD);
    InitAlternatePin(OPT_SDA_PORT, OPT_SDA_PIN, OPT_SDA_SOURCE,
                     OPT_SDA_AF, GPIO_Mode_AF_OD);

    InitAlternatePin(TXD1_PORT, TXD1_PIN, TXD1_SOURCE, TXD1_AF, GPIO_Mode_AF_PP);
    InitAlternatePin(RXD1_PORT, RXD1_PIN, RXD1_SOURCE, RXD1_AF, GPIO_Mode_IPU);
    InitAlternatePin(TXD2_PORT, TXD2_PIN, TXD2_SOURCE, TXD2_AF, GPIO_Mode_AF_PP);
    InitAlternatePin(RXD2_PORT, RXD2_PIN, RXD2_SOURCE, RXD2_AF, GPIO_Mode_IPU);

    /* OPT_INT has an external pull-up. Drivers select edges and enable IRQs. */
    InitPin(OPT_INT_PORT, OPT_INT_PIN, GPIO_Mode_IN_FLOATING);
    InitPin(IMU_INT1_PORT, IMU_INT1_PIN, GPIO_Mode_IN_FLOATING);
    GPIO_EXTILineConfig(OPT_INT_PORT_SOURCE, OPT_INT_PIN_SOURCE);
    GPIO_EXTILineConfig(IMU_INT1_PORT_SOURCE, IMU_INT1_PIN_SOURCE);
}
