#ifndef MOTOR_TEST_HAL_STUB_H
#define MOTOR_TEST_HAL_STUB_H
#include <stdint.h>
typedef struct {
    volatile uint32_t CR1, PSC, ARR, CNT, CCMR1, CCMR2, CCR1, CCR2, CCR3, CCR4;
} TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
extern TIM_TypeDef motor_test_tim1;
#define TIM1 (&motor_test_tim1)
#define TIM_CHANNEL_2 0x00000004U
#define TIM_CHANNEL_3 0x00000008U
#define TIM_CR1_UDIS 0x00000002U
#define TIM_CR1_CEN 0x00000001U
typedef enum { HAL_OK = 0U, HAL_ERROR = 1U, HAL_BUSY = 2U } HAL_StatusTypeDef;
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel);
#define __HAL_TIM_ENABLE_OCxPRELOAD(h,c) ((void)(h), (void)(c))
void motor_test_set_compare(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t value);
#define __HAL_TIM_SET_COMPARE(h,c,v) motor_test_set_compare((h),(c),(v))
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
#endif
