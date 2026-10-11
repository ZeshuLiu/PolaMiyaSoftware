#ifndef FOCUS_MOTOR_TEST_H
#define FOCUS_MOTOR_TEST_H

#include "FocusUnit/Motor/focus_motor.h"

/* 当前自动测试等待上电零位有效，再运行原 A/B 开环循环。 */
void FocusMotor_TestInit(void);
void FocusMotor_TestStop(void);
/* 仅由调度器触发的 Motor Step 调用；now_ms 使用调度器时基。
 * 返回 1 表示给出本次开环方向和占空比。 */
uint8_t FocusMotor_TestStep(uint32_t now_ms, uint8_t zero_valid,
                           FocusMotor_OutputCommand *command);
/* 专门的开环测试波形函数：A/B 各加速、保持、减速、制动各 3 s。
 * 时间、周期和占空比计算全部在 test.c 中完成，只输出方向和占空比。 */
void FocusMotor_TestOpenLoop(uint32_t now_ms, FocusMotor_OutputCommand *command);

#endif
