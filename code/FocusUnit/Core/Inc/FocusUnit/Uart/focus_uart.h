#ifndef FOCUS_UART_H
#define FOCUS_UART_H

#include "stm32f0xx_hal.h"

/* 主循环初始化和处理；UART 回调只缓存收发状态。 */
HAL_StatusTypeDef FocusUart_Init(UART_HandleTypeDef *uart);
void FocusUart_Process(void);

#endif
