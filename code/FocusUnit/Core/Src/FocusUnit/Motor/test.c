#include "FocusUnit/Motor/test.h"

#define FOCUS_MOTOR_TEST_STAGE_MS 3000U
#define FOCUS_MOTOR_TEST_CYCLE_MS 24000U

/* 这是测试流程状态，不是电机驱动模式或另一个调度器。 */
typedef enum {
    TEST_STOP = 0,
    TEST_WAIT_ZERO,
    TEST_OPEN_LOOP
} MotorTestState;

static MotorTestState s_test_state;
static uint32_t s_test_start_ms;

void FocusMotor_TestInit(void)
{
    s_test_state = TEST_WAIT_ZERO;
}

void FocusMotor_TestStop(void)
{
    s_test_state = TEST_STOP;
}

/* 仅在调度器 10 ms 任务中推进，确认零位有效后建立测试时间起点。 */
uint8_t FocusMotor_TestStep(uint32_t now_ms, uint8_t zero_valid,
                           FocusMotor_OutputCommand *command)
{
    if ((s_test_state == TEST_STOP) || (zero_valid == 0U)) return 0U;
    if (s_test_state == TEST_WAIT_ZERO) {
        s_test_start_ms = now_ms;
        s_test_state = TEST_OPEN_LOOP;
    }
    FocusMotor_TestOpenLoop(now_ms, command);
    return 1U;
}

/* 时间来自调度器；测试函数只计算开环方向和占空比，不操作 PWM。 */
void FocusMotor_TestOpenLoop(uint32_t now_ms, FocusMotor_OutputCommand *command)
{
    const uint32_t direction_period = 4U * FOCUS_MOTOR_TEST_STAGE_MS;
    uint32_t elapsed_ms = now_ms - s_test_start_ms;
    uint32_t cycle = elapsed_ms % FOCUS_MOTOR_TEST_CYCLE_MS;
    uint32_t phase = cycle % direction_period;
    uint32_t stage_ms = phase % FOCUS_MOTOR_TEST_STAGE_MS;
    s_test_start_ms += (elapsed_ms / FOCUS_MOTOR_TEST_CYCLE_MS) * FOCUS_MOTOR_TEST_CYCLE_MS;
    command->direction = (cycle < direction_period) ? FOCUS_MOTOR_DIR_A : FOCUS_MOTOR_DIR_B;

    if (phase < FOCUS_MOTOR_TEST_STAGE_MS) {
        command->duty_permille = (uint16_t)((uint32_t)FocusMotor_ConfigData.max_duty_permille *
                                           stage_ms / FOCUS_MOTOR_TEST_STAGE_MS);
    } else if (phase < 2U * FOCUS_MOTOR_TEST_STAGE_MS) {
        command->duty_permille = FocusMotor_ConfigData.max_duty_permille;
    } else if (phase < 3U * FOCUS_MOTOR_TEST_STAGE_MS) {
        command->duty_permille = (uint16_t)((uint32_t)FocusMotor_ConfigData.max_duty_permille *
          (FOCUS_MOTOR_TEST_STAGE_MS - stage_ms) / FOCUS_MOTOR_TEST_STAGE_MS);
    } else {
        command->duty_permille = 0U;
    }
}
