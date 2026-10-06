#ifndef SCHEDULER_TEST_HAL_H
#define SCHEDULER_TEST_HAL_H
#include <stdint.h>
#include <stddef.h>
typedef struct { uint32_t CNT; uint32_t SR; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef enum { HAL_OK, HAL_ERROR } HAL_StatusTypeDef;
extern TIM_TypeDef test_tim14;
extern uint32_t test_primask;
extern uint32_t test_sleep_calls;
#define TIM14 (&test_tim14)
#define TIM_FLAG_UPDATE 1U
#define __get_PRIMASK() test_primask
#define __disable_irq() (test_primask = 1U)
#define __set_PRIMASK(value) (test_primask = (value))
#define __HAL_TIM_SET_COUNTER(timer, value) ((timer)->Instance->CNT = (value))
#define __HAL_TIM_CLEAR_FLAG(timer, value) ((timer)->Instance->SR &= ~(value))
#define __DSB() ((void)0)
#define __WFI() (++test_sleep_calls)
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *timer);
#endif
