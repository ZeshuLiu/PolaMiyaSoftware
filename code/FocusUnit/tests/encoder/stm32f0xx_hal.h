#ifndef TEST_STM32F0XX_HAL_H
#define TEST_STM32F0XX_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef struct { volatile uint32_t CNT; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
#define TIM_CHANNEL_ALL 0x3U
#define __HAL_TIM_GET_COUNTER(h) ((h)->Instance->CNT)
extern uint32_t test_primask;
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask = 1U; }
static inline void __set_PRIMASK(uint32_t v) { test_primask = v; }
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *htim, uint32_t channel);
#endif
