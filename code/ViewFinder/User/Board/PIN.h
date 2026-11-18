#ifndef VIEWFINDER_PIN_H
#define VIEWFINDER_PIN_H

#include "ch32v205.h"

/* ViewFinder Rev 1.1, CH32V205CCT6. */
#define LCD_SPI                SPI1
#define LCD_CS_N_PORT          GPIOA
#define LCD_CS_N_PIN           GPIO_Pin_4
#define LCD_SCK_PORT           GPIOA
#define LCD_SCK_PIN            GPIO_Pin_5
#define LCD_SCK_SOURCE         GPIO_PinSource5
#define LCD_SCK_AF             GPIO_AF5
#define LCD_DC_PORT            GPIOA
#define LCD_DC_PIN             GPIO_Pin_6
#define LCD_MOSI_PORT          GPIOA
#define LCD_MOSI_PIN           GPIO_Pin_7
#define LCD_MOSI_SOURCE        GPIO_PinSource7
#define LCD_MOSI_AF            GPIO_AF5
#define LCD_RST_N_PORT         GPIOC
#define LCD_RST_N_PIN          GPIO_Pin_4

/* TIM3_CH3, active low. Start as GPIO high until PWM is configured. */
#define LCD_BL_PWM_TIM         TIM3
#define LCD_BL_PWM_PORT        GPIOB
#define LCD_BL_PWM_PIN         GPIO_Pin_0
#define LCD_BL_PWM_SOURCE      GPIO_PinSource0
#define LCD_BL_PWM_AF          GPIO_AF1

#define OPT_I2C                I2C1
#define OPT_SCL_PORT           GPIOB
#define OPT_SCL_PIN            GPIO_Pin_6
#define OPT_SCL_SOURCE         GPIO_PinSource6
#define OPT_SCL_AF             GPIO_AF7
#define OPT_SDA_PORT           GPIOB
#define OPT_SDA_PIN            GPIO_Pin_7
#define OPT_SDA_SOURCE         GPIO_PinSource7
#define OPT_SDA_AF             GPIO_AF7
#define OPT_INT_PORT           GPIOB
#define OPT_INT_PIN            GPIO_Pin_8
#define OPT_INT_PORT_SOURCE    GPIO_PortSourceGPIOB
#define OPT_INT_PIN_SOURCE     GPIO_PinSource8
#define OPT_INT_EXTI_LINE      EXTI_Line8

#define IMU_SPI                SPI2
#define IMU_CS_N_PORT          GPIOB
#define IMU_CS_N_PIN           GPIO_Pin_12
#define IMU_SCK_PORT           GPIOB
#define IMU_SCK_PIN            GPIO_Pin_13
#define IMU_SCK_SOURCE         GPIO_PinSource13
#define IMU_SCK_AF             GPIO_AF5
#define IMU_MISO_PORT          GPIOB
#define IMU_MISO_PIN           GPIO_Pin_14
#define IMU_MISO_SOURCE        GPIO_PinSource14
#define IMU_MISO_AF            GPIO_AF5
#define IMU_MOSI_PORT          GPIOB
#define IMU_MOSI_PIN           GPIO_Pin_15
#define IMU_MOSI_SOURCE        GPIO_PinSource15
#define IMU_MOSI_AF            GPIO_AF5
#define IMU_INT1_PORT          GPIOB
#define IMU_INT1_PIN           GPIO_Pin_4
#define IMU_INT1_PORT_SOURCE   GPIO_PortSourceGPIOB
#define IMU_INT1_PIN_SOURCE    GPIO_PinSource4
#define IMU_INT1_EXTI_LINE     EXTI_Line4

/* Connector 1 uses separate transmit and receive peripherals. */
#define TXD1_USART             USART5
#define TXD1_PORT              GPIOB
#define TXD1_PIN               GPIO_Pin_2
#define TXD1_SOURCE            GPIO_PinSource2
#define TXD1_AF                GPIO_AF3
#define RXD1_USART             USART4
#define RXD1_PORT              GPIOB
#define RXD1_PIN               GPIO_Pin_1
#define RXD1_SOURCE            GPIO_PinSource1
#define RXD1_AF                GPIO_AF2

#define UART2_USART            USART3
#define TXD2_PORT              GPIOB
#define TXD2_PIN               GPIO_Pin_10
#define TXD2_SOURCE            GPIO_PinSource10
#define TXD2_AF                GPIO_AF2
#define RXD2_PORT              GPIOB
#define RXD2_PIN               GPIO_Pin_11
#define RXD2_SOURCE            GPIO_PinSource11
#define RXD2_AF                GPIO_AF2

/* Keep the default WCH-Link debug function. */
#define SWDIO_PORT             GPIOA
#define SWDIO_PIN              GPIO_Pin_13
#define SWCLK_PORT             GPIOA
#define SWCLK_PIN              GPIO_Pin_14

void BoardPins_Init(void);

#endif /* VIEWFINDER_PIN_H */
