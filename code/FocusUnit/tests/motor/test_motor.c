#include <assert.h>
#include <stdint.h>
#include "FocusUnit/Motor/focus_motor.h"
#include "../../Core/Src/FocusUnit/Motor/focus_motor.c"

TIM_TypeDef motor_test_tim1;
static uint32_t starts[2], start_count, fail_channel, compare_writes_without_udis;
HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t channel)
{
    (void)htim;
    starts[start_count++] = channel;
    return channel == fail_channel ? HAL_ERROR : HAL_OK;
}
void motor_test_set_compare(TIM_HandleTypeDef *htim, uint32_t channel, uint32_t value)
{
    if ((htim->Instance->CR1 & TIM_CR1_UDIS) == 0U) compare_writes_without_udis++;
    if (channel == TIM_CHANNEL_2) htim->Instance->CCR2 = value;
    else htim->Instance->CCR3 = value;
}
uint32_t __get_PRIMASK(void) { return 0U; }
void __disable_irq(void) {}
void __enable_irq(void) {}

static void reset_timer(TIM_HandleTypeDef *htim)
{
    motor_test_tim1.PSC = 0U;
    motor_test_tim1.ARR = 2399U;
    motor_test_tim1.CR1 = TIM_CR1_CEN;
    motor_test_tim1.CCR2 = motor_test_tim1.CCR3 = 0U;
    htim->Instance = TIM1;
    start_count = 0U;
    fail_channel = 0U;
    compare_writes_without_udis = 0U;
}

int main(void)
{
    TIM_HandleTypeDef tim;
    FocusMotor_Snapshot snap;

    reset_timer(&tim);
    assert(FocusMotor_SetPositiveDirection(FOCUS_MOTOR_DIR_B) == HAL_OK);
    assert(FocusMotor_Init(&tim, 1000U) == HAL_OK);
    assert(start_count == 2U && starts[0] == TIM_CHANNEL_2 && starts[1] == TIM_CHANNEL_3);
    assert(motor_test_tim1.CCR2 == 0U && motor_test_tim1.CCR3 == 0U);
    assert((motor_test_tim1.CR1 & TIM_CR1_CEN) != 0U);
    assert((motor_test_tim1.CR1 & TIM_CR1_UDIS) == 0U);
    assert(compare_writes_without_udis == 0U);
    FocusMotor_Update(1000U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_STARTUP);
    FocusMotor_Update(1001U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE_WAKE);
    assert(motor_test_tim1.CCR2 == 2400U && motor_test_tim1.CCR3 == 2400U);
    FocusMotor_Update(1002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
    assert(FocusMotor_State.direction == FOCUS_MOTOR_DIR_A && FocusMotor_State.logical_sign == -1);
    assert(motor_test_tim1.CCR3 == 2400U && motor_test_tim1.CCR2 == 2400U);
    FocusMotor_Update(2502U);
    assert(FocusMotor_State.duty_permille == 500U);
    assert(motor_test_tim1.CCR3 == 2400U && motor_test_tim1.CCR2 == 1200U);
    FocusMotor_Update(4002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_HOLD);
    assert(FocusMotor_State.duty_permille == 1000U && motor_test_tim1.CCR2 == 0U);
    FocusMotor_Update(7002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_DOWN);
    assert(FocusMotor_State.duty_permille == 1000U);
    FocusMotor_Update(8502U);
    assert(FocusMotor_State.duty_permille == 500U);
    FocusMotor_Update(10002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE);
    assert(motor_test_tim1.CCR2 == 2400U && motor_test_tim1.CCR3 == 2400U);
    FocusMotor_Update(13002U);
    assert(FocusMotor_State.direction == FOCUS_MOTOR_DIR_B);
    assert(FocusMotor_State.logical_sign == 1);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
    assert(motor_test_tim1.CCR3 == 2400U && motor_test_tim1.CCR2 == 2400U);
    FocusMotor_Update(14502U);
    assert(FocusMotor_State.duty_permille == 500U);
    assert(motor_test_tim1.CCR3 == 1200U && motor_test_tim1.CCR2 == 2400U);
    FocusMotor_Update(22002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE);
    FocusMotor_Update(25002U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
    assert(FocusMotor_State.direction == FOCUS_MOTOR_DIR_A);
    assert(FocusMotor_State.stage_elapsed_ms == 0U);
    FocusMotor_Update(26002U);
    assert(FocusMotor_State.stage_elapsed_ms == 1000U);
    FocusMotor_GetSnapshot(&snap);
    assert(snap.initialized && !snap.error);

    reset_timer(&tim);
    motor_test_tim1.ARR = 2398U;
    assert(FocusMotor_Init(&tim, 0U) == HAL_ERROR);
    reset_timer(&tim);
    fail_channel = TIM_CHANNEL_3;
    assert(FocusMotor_Init(&tim, 0U) == HAL_ERROR);
    assert(motor_test_tim1.CCR2 == 2400U && motor_test_tim1.CCR3 == 2400U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_ERROR);

    /* A delayed scheduler that skips A's brake segment must insert a full guard brake. */
    reset_timer(&tim);
    assert(FocusMotor_Init(&tim, 0xFFFFF000U) == HAL_OK);
    FocusMotor_Update(0xFFFFF000U);
    FocusMotor_Update(0xFFFFF001U);
    FocusMotor_Update(0xFFFFF002U);
    FocusMotor_Update(0xFFFFF002U + 8000U);
    FocusMotor_Update(0xFFFFF002U + 13000U);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE);
    assert(FocusMotor_State.direction == FOCUS_MOTOR_DIR_A);
    assert(motor_test_tim1.CCR2 == 2400U && motor_test_tim1.CCR3 == 2400U);
    FocusMotor_Update(0xFFFFF002U + 13001U);
    assert(FocusMotor_State.direction == FOCUS_MOTOR_DIR_B);
    assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
    assert(FocusMotor_State.update_late_count >= 1U);

    /* A near-49-day elapsed time is rebased without changing stage phase over tick wrap. */
    reset_timer(&tim);
    assert(FocusMotor_SetPositiveDirection(FOCUS_MOTOR_DIR_A) == HAL_OK);
    assert(FocusMotor_Init(&tim, 0U) == HAL_OK);
    FocusMotor_Update(0U);
    FocusMotor_Update(1U);
    FocusMotor_Update(2U);
    {
        uint32_t far_delta = 0xFFFFFE00U;
        uint32_t correction = ((far_delta % 24000U) + 24000U - 1000U) % 24000U;
        uint32_t far_now;
        far_delta -= correction;
        s_stage_start_ms = 1U;
        far_now = 2U + far_delta;
        FocusMotor_Update(far_now);
        assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
        assert(FocusMotor_State.stage_elapsed_ms == 1000U);
        FocusMotor_Update(far_now + 500U);
        assert(FocusMotor_State.stage == FOCUS_MOTOR_STAGE_RAMP_UP);
        assert(FocusMotor_State.stage_elapsed_ms == 1500U);
    }
    return 0;
}
