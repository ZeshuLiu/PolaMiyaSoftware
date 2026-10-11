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
/* 主入口：外设初始化 -> 四个业务模块启动 -> 主循环领取任务并发布 SWD 快照。 */
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
/* SWD 观察变量由主循环复制，当前没有串口业务。
 * ADC 和编码器快照仅在主循环读取；这些副本用于调试观察。 */
volatile FocusAdcMeasurement g_focus_adc_measurement;
volatile FocusEncoderMeasurement g_focus_encoder_measurement;
volatile FocusMotor_Snapshot g_focus_motor_state;
volatile FocusSchedulerStatus g_focus_scheduler_status;
volatile uint32_t g_focus_fault; /* 1=ADC，2=编码器，3=电机，4=调度，5=其他初始化错误 */
/* SWD 置 1 后由下一次编码器任务消费并清零；C 接口也可调用 Zero。 */
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
  /* 故障停机直接将 PA9/PA10 改为低电平 GPIO，桥进入 00 滑行/睡眠。
     PWM 尚未初始化时也可调用，不等待 PWM 更新、HAL 锁或任务调度。 */
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
  /* HSI 8 MHz / 2 × 12 = 48 MHz，AHB/APB 不分频；无外部晶振时钟依赖。 */

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
  /* 调试暂停时冻结调度/编码器，但保持 PWM，避免冻结在持续驱动电平。
     断点不是停机命令；正常状态可调用 Stop，寻零期间 Stop 不会取消初始化。
     暂停调度也会暂停反向退回计时，寻零中不能用断点验证实际驱动时长。 */
  __HAL_DBGMCU_UNFREEZE_TIM1();
  /* 先校准 ADC、启动编码器和电机，最后启动调度时基；任一失败进入故障停机。 */
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
  if (FocusMotor_Init(&htim1) != HAL_OK)
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
    /* 原子领取本轮任务；同类积压只执行一次，业务使用当前绝对时刻。 */
    const FocusSchedulerDispatch dispatch = FocusScheduler_Take();
    FocusAdcMeasurement adc_snapshot;
    if ((dispatch.flags & FOCUS_TASK_ENCODER) != 0U)
    {
      FocusEncoder_Update(dispatch.now_ms);
      /* 寻零期间不能外部置零破坏比较基准；错误锁存时也不响应置零指令。 */
      const uint8_t zero_requested = g_focus_encoder_zero_request;
      g_focus_encoder_zero_request = 0U;
      if ((zero_requested != 0U) &&
          (FocusMotor_State.work_state == FOCUS_MOTOR_WORK_NORMAL))
      {
        FocusEncoder_Zero();
      }
    }
    /* 每轮处理 ADC 完整帧或 HAL 错误。 */
    FocusAdc_Process(dispatch.now_ms);
    if (FocusAdc_GetSnapshot(&adc_snapshot) != 0U)
    {
      g_focus_adc_measurement = adc_snapshot;
    }
    /* 调度器每 10 ms 触发一次，电机模块自行管理模式和堵转置零。 */
    if ((dispatch.flags & FOCUS_TASK_MOTOR) != 0U)
    {
      FocusMotor_Step(dispatch.now_ms);
      g_focus_motor_state = FocusMotor_State;
    }
    {
      /* Step 可能建立编码器零位，当轮同步观察副本。 */
      FocusEncoderMeasurement encoder_snapshot;
      FocusEncoder_GetSnapshot(&encoder_snapshot);
      g_focus_encoder_measurement = encoder_snapshot;
    }
    if ((dispatch.flags & FOCUS_TASK_ADC) != 0U)
    {
      FocusSchedulerStatus scheduler_snapshot;
      /* 上一帧未处理完则跳过本次请求，不阻塞其他任务。 */
      (void)FocusAdc_Request(dispatch.now_ms);
      FocusScheduler_GetSnapshot(&scheduler_snapshot);
      g_focus_scheduler_status = scheduler_snapshot;
    }
    /* 没有待领取任务时 WFI，ADC 完成中断或下一次 TIM14 tick 可唤醒。 */
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
/* HAL 定时器回调转发至调度模块，模块内部只接受已注册 TIM14。 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  FocusScheduler_OnTick(htim);
}

/* 单通道 EOC 回调在 ISR 内及时读 DR，避免结果被下一通道覆盖。 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc)
{
  FocusAdc_OnConversionComplete(adc);
}

/* ADC ISR 只上报错误；终止采集和发布失败结果交由主循环。 */
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
  /* 先使桥输入双低，再关中断停留，保留故障编号供 SWD 排查。 */
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
