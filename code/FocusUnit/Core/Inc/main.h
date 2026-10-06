/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void FocusUnit_FaultStop(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define ADC_NTC1_Pin GPIO_PIN_0
#define ADC_NTC1_GPIO_Port GPIOA
#define ADC_3V3_Pin GPIO_PIN_1
#define ADC_3V3_GPIO_Port GPIOA
#define USART1_TX_Pin GPIO_PIN_2
#define USART1_TX_GPIO_Port GPIOA
#define USART1_RX_Pin GPIO_PIN_3
#define USART1_RX_GPIO_Port GPIOA
#define ADC_NTC2_Pin GPIO_PIN_4
#define ADC_NTC2_GPIO_Port GPIOA
#define ADC_6V_Pin GPIO_PIN_5
#define ADC_6V_GPIO_Port GPIOA
#define MOT_ENC_A_Pin GPIO_PIN_6
#define MOT_ENC_A_GPIO_Port GPIOA
#define MOT_ENC_B_Pin GPIO_PIN_7
#define MOT_ENC_B_GPIO_Port GPIOA
#define ADC_MT_Pin GPIO_PIN_1
#define ADC_MT_GPIO_Port GPIOB
#define MotorPWM_B_Pin GPIO_PIN_9
#define MotorPWM_B_GPIO_Port GPIOA
#define MotorPWM_A_Pin GPIO_PIN_10
#define MotorPWM_A_GPIO_Port GPIOA
#define SWDIO_Pin GPIO_PIN_13
#define SWDIO_GPIO_Port GPIOA
#define SWCLK_Pin GPIO_PIN_14
#define SWCLK_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
