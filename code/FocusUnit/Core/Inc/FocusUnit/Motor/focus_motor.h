#ifndef FOCUS_MOTOR_H
#define FOCUS_MOTOR_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

/* 上电低输入 -> 制动唤醒 -> A/B 各四阶段；异常状态停止演示。 */
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
    uint16_t max_duty_permille; /* 最大驱动占空比，0..1000 对应 0..100% */
    /* 只定义逻辑正向；演示物理顺序始终先 A 后 B。 */
    FocusMotor_Direction positive_direction;
    uint16_t period_counts; /* 默认 2400；当前实现使用固定 MOTOR_PERIOD_COUNTS，非可变周期接口 */
} FocusMotor_Config;

typedef struct {
    FocusMotor_Stage stage; /* 当前演示阶段 */
    /* 当前物理方向；制动时保留方向标记，不代表电机仍在转动。 */
    FocusMotor_Direction direction;
    int8_t logical_sign;   /* direction 与正向映射比较得到的 +1/-1 */
    uint16_t duty_permille; /* 驱动占空比，千分比；0 表示制动 */
    uint8_t initialized;   /* 两路 PWM 已启动 */
    uint8_t error;         /* 模块异常标记 */
    uint32_t stage_elapsed_ms; /* 当前阶段已过时间，ms */
    uint32_t update_late_count; /* Update 间隔超过 10 ms 的次数 */
} FocusMotor_Snapshot;

/* 主循环持有配置；修改最大占空比须保持 0..1000，当前无速度/位置闭环。 */
extern FocusMotor_Config FocusMotor_ConfigData;
extern volatile FocusMotor_Snapshot FocusMotor_State;

/* Init/Update/Stop/SetPositiveDirection 只在主循环上下文调用。
 * Init 校验 TIM1 周期并启动演示；Update 建议每 10 ms 调用。 */
HAL_StatusTypeDef FocusMotor_Init(TIM_HandleTypeDef *htim, uint32_t now_ms);
void FocusMotor_Update(uint32_t now_ms);
/* 写入双高制动并停止演示；后续 Update 不再自动恢复，重启需 Init。 */
void FocusMotor_Stop(void);
/* 短临界区读取一致快照，可在 ISR 调用。 */
void FocusMotor_GetSnapshot(FocusMotor_Snapshot *snapshot);
/* 接受 A/B，非法值返回 HAL_ERROR；当前允许运行中修改，只影响逻辑符号，
 * 不改变物理演示顺序。快照符号在下一次状态发布时更新。 */
HAL_StatusTypeDef FocusMotor_SetPositiveDirection(FocusMotor_Direction direction);

#endif
