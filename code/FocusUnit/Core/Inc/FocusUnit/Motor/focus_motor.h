#ifndef FOCUS_MOTOR_H
#define FOCUS_MOTOR_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

typedef enum {
    FOCUS_MOTOR_STAGE_STARTUP = 0,
    FOCUS_MOTOR_STAGE_BRAKE_WAKE,
    FOCUS_MOTOR_STAGE_RAMP_UP,
    FOCUS_MOTOR_STAGE_HOLD,
    FOCUS_MOTOR_STAGE_RAMP_DOWN,
    FOCUS_MOTOR_STAGE_BRAKE,
    FOCUS_MOTOR_STAGE_ERROR
} FocusMotor_Stage;

typedef enum {
    FOCUS_MOTOR_DIR_A = 0,
    FOCUS_MOTOR_DIR_B = 1
} FocusMotor_Direction;

typedef struct {
    uint16_t max_duty_permille;
    /* Logical positive sign only; demo physical order is always A then B. */
    FocusMotor_Direction positive_direction;
    uint16_t period_counts;
} FocusMotor_Config;

typedef struct {
    FocusMotor_Stage stage;
    /* Current physical motor direction, independent of logical_sign. */
    FocusMotor_Direction direction;
    int8_t logical_sign;
    uint16_t duty_permille;
    uint8_t initialized;
    uint8_t error;
    uint32_t stage_elapsed_ms;
    uint32_t update_late_count;
} FocusMotor_Snapshot;

extern FocusMotor_Config FocusMotor_ConfigData;
extern volatile FocusMotor_Snapshot FocusMotor_State;

/* Init, Update, Stop and SetPositiveDirection are main-loop only. */
HAL_StatusTypeDef FocusMotor_Init(TIM_HandleTypeDef *htim, uint32_t now_ms);
void FocusMotor_Update(uint32_t now_ms);
void FocusMotor_Stop(void);
/* Snapshot reads are safe from an ISR; the mutators above must not be called there. */
void FocusMotor_GetSnapshot(FocusMotor_Snapshot *snapshot);
HAL_StatusTypeDef FocusMotor_SetPositiveDirection(FocusMotor_Direction direction);

#endif
