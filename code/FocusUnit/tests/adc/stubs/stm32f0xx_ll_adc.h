#ifndef TEST_STM32F0XX_LL_ADC_H
#define TEST_STM32F0XX_LL_ADC_H
#include <stdint.h>
extern uint16_t test_vref_cal;
extern uint16_t test_temp_cal;
#define VREFINT_CAL_ADDR (&test_vref_cal)
#define VREFINT_CAL_VREF 3300U
#define TEMPSENSOR_CAL1_ADDR (&test_temp_cal)
#define TEMPSENSOR_CAL1_TEMP 30
#define TEMPSENSOR_CAL_VREFANALOG 3300U
#endif
