#ifndef TEST_STM32F0XX_HAL_H
#define TEST_STM32F0XX_HAL_H

#include <stdint.h>
#include <stddef.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1, HAL_BUSY = 2 } HAL_StatusTypeDef;
typedef struct { volatile uint32_t ISR; volatile uint32_t CR; volatile uint16_t DR; } ADC_TypeDef;
typedef struct { ADC_TypeDef *Instance; uint32_t ErrorCode; } ADC_HandleTypeDef;

#define RESET 0U
#define ADC_FLAG_EOS 0x00000020UL

uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __set_PRIMASK(uint32_t value);
void __DMB(void);
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Start_IT(ADC_HandleTypeDef *hadc);
HAL_StatusTypeDef HAL_ADC_Stop_IT(ADC_HandleTypeDef *hadc);

#define __HAL_ADC_GET_FLAG(__HANDLE__, __FLAG__) \
  (((__HANDLE__)->Instance->ISR & (__FLAG__)) != 0U)

#endif
