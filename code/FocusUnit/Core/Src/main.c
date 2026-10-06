/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FocusUnit/Scheduler/focus_scheduler.h"
#include "FocusUnit/Adc/focus_adc.h"
#include "FocusUnit/Motor/focus_motor.h"
#include "FocusUnit/Encoder/focus_encoder.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Coherent module snapshots exposed for SWD watch; no serial business yet. */
volatile FocusAdcMeasurement g_focus_adc_measurement;
volatile FocusEncoderMeasurement g_focus_encoder_measurement;
volatile FocusMotor_Snapshot g_focus_motor_state;
volatile FocusSchedulerStatus g_focus_scheduler_status;
volatile uint32_t g_focus_fault;
/* Set to 1 through SWD or a future control layer; consumed at the next
   encoder task. The module also exposes FocusEncoder_Zero() to C callers. */
volatile uint8_t g_focus_encoder_zero_request;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void FocusUnit_FaultStop(void)
{
  /* Also works before PWM initialization. Disable both bridge inputs without
     waiting for the next PWM update, HAL lock, scheduler, or interrupts. */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIOA->BSRR = (uint32_t)(GPIO_PIN_9 | GPIO_PIN_10) << 16U;
  MODIFY_REG(GPIOA->MODER, GPIO_MODER_MODER9 | GPIO_MODER_MODER10,
             GPIO_MODER_MODER9_0 | GPIO_MODER_MODER10_0);
  __DSB();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_ADC_Init();
  MX_TIM3_Init();
  MX_TIM14_Init();
  /* USER CODE BEGIN 2 */
  __HAL_DBGMCU_FREEZE_TIM14();
  __HAL_DBGMCU_FREEZE_TIM3();
  /* Keep PWM running while halted: freezing TIM1 may hold a drive level.
     A breakpoint is not a motor stop; call FocusMotor_Stop before halting. */
  __HAL_DBGMCU_UNFREEZE_TIM1();
  if (FocusAdc_Init(&hadc) != HAL_OK)
  {
    g_focus_fault = 1U;
    Error_Handler();
  }
  if (FocusEncoder_Init(&htim3, 0U) != HAL_OK)
  {
    g_focus_fault = 2U;
    Error_Handler();
  }
  if (FocusMotor_Init(&htim1, 0U) != HAL_OK)
  {
    g_focus_fault = 3U;
    Error_Handler();
  }
  if (FocusScheduler_Init(&htim14) != HAL_OK)
  {
    g_focus_fault = 4U;
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    const FocusSchedulerDispatch dispatch = FocusScheduler_Take();
    FocusAdcMeasurement adc_snapshot;
    if ((dispatch.flags & FOCUS_TASK_ENCODER) != 0U)
    {
      FocusEncoderMeasurement encoder_snapshot;
      FocusEncoder_Update(dispatch.now_ms);
      const uint32_t irq_state = __get_PRIMASK();
      __disable_irq();
      const uint8_t zero_requested = g_focus_encoder_zero_request;
      g_focus_encoder_zero_request = 0U;
      __set_PRIMASK(irq_state);
      if (zero_requested != 0U)
      {
        FocusEncoder_Zero();
      }
      FocusEncoder_GetSnapshot(&encoder_snapshot);
      g_focus_encoder_measurement = encoder_snapshot;
    }
    FocusAdc_Process(dispatch.now_ms);
    if ((dispatch.flags & FOCUS_TASK_MOTOR) != 0U)
    {
      FocusMotor_Snapshot motor_snapshot;
      FocusMotor_Update(dispatch.now_ms);
      FocusMotor_GetSnapshot(&motor_snapshot);
      g_focus_motor_state = motor_snapshot;
    }
    if (FocusAdc_GetSnapshot(&adc_snapshot) != 0U)
    {
      g_focus_adc_measurement = adc_snapshot;
    }
    if ((dispatch.flags & FOCUS_TASK_ADC) != 0U)
    {
      FocusSchedulerStatus scheduler_snapshot;
      /* Busy/failure is recorded by ADC; do not block other tasks to retry. */
      (void)FocusAdc_Request(dispatch.now_ms);
      FocusScheduler_GetSnapshot(&scheduler_snapshot);
      g_focus_scheduler_status = scheduler_snapshot;
    }
    FocusScheduler_Idle();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL12;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  FocusScheduler_OnTick(htim);
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
  FocusAdc_OnConversionComplete(adc);
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc)
{
  FocusAdc_OnError(adc);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  FocusUnit_FaultStop();
  if (g_focus_fault == 0U)
  {
    g_focus_fault = 5U;
  }
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
