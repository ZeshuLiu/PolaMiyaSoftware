#ifndef PM_CONTROLLER_PIN_H
#define PM_CONTROLLER_PIN_H

#include "driver/gpio.h"

/* PM_Controller PCB Rev2.0, ESP32-S3-WROOM-1-N16R8.
 * Values are ESP32 GPIO numbers, not module pad numbers.
 * This header defines wiring only; it does not initialize GPIOs.
 */

/* DRV8251A: GPIO1 -> IN1, GPIO2 -> IN2. */
#define PIN_MOTORPWM_A       GPIO_NUM_1
#define PIN_MOTOR_IN2        GPIO_NUM_2
#define PIN_ADC_MT           GPIO_NUM_8

/* Battery ADC and USB power detection. */
#define PIN_ADC_BAT          GPIO_NUM_3
#define PIN_USBPWR_DET       GPIO_NUM_21

/* Film-event latch: TRG is read through Q1; CLR drives Q2 via R11. */
#define PIN_ESP_FILM_TRG     GPIO_NUM_6
#define PIN_ESP_FILM_CLR     GPIO_NUM_7

/* LCD: BLK drives a PNP transistor, low = backlight on. */
#define PIN_LCD_BLK          GPIO_NUM_9
#define PIN_LCD_RES          GPIO_NUM_10
#define PIN_LCD_DC           GPIO_NUM_11
#define PIN_LCD_MOSI         GPIO_NUM_12
#define PIN_LCD_CLK          GPIO_NUM_13
#define PIN_LCD_CS           GPIO_NUM_14

/* I2C EEPROM. */
#define PIN_ESP_SCL          GPIO_NUM_15
#define PIN_ESP_SDA          GPIO_NUM_16

/* UART0: J1. TX passes through R47. */
#define PIN_ESP_TXD0         GPIO_NUM_43
#define PIN_ESP_RXD0         GPIO_NUM_44

/* UART1: U9 connector. TX passes through R49. */
#define PIN_ESP_TXD1         GPIO_NUM_4
#define PIN_ESP_RXD1         GPIO_NUM_5

/* UART2: U15 connector. TX passes through R48. */
#define PIN_ESP_TXD2         GPIO_NUM_18
#define PIN_ESP_RXD2         GPIO_NUM_17

/* 74HC165: CS is actually /PL (low = parallel load), not SPI chip select. */
#define PIN_MASPI_CS         GPIO_NUM_39
#define PIN_MASPI_CLK        GPIO_NUM_40
#define PIN_MASPI_MISO       GPIO_NUM_41

/* Flash optocoupler and shutter connector. */
#define PIN_FLASH_TRG        GPIO_NUM_38
#define PIN_SHUT_TRG         GPIO_NUM_42

/* Native USB. Keep these assigned to USB when USB/JTAG is used. */
#define PIN_USB_DM           GPIO_NUM_19
#define PIN_USB_DP           GPIO_NUM_20

/* BOOT button / strapping pin; low during reset enters download mode. */
#define PIN_ESP_BOOT         GPIO_NUM_0

/* GPIO35/36/37 are reserved for N16R8 Octal PSRAM.
 * GPIO45/46/47/48 are not connected on this PCB.
 * GPIO3 is also a JTAG strapping pin.
 * GPIO39..42 are wired to board functions, so use USB Serial/JTAG to debug.
 * ESP_EN is a reset input of the module, not an ESP32 GPIO.
 */

#endif /* PM_CONTROLLER_PIN_H */
