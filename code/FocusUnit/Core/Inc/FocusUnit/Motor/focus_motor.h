#ifndef FOCUS_MOTOR_H
#define FOCUS_MOTOR_H

#include "stm32f0xx_hal.h"
#include <stdint.h>

#define FOCUS_MOTOR_GLIDE_DUTY_PERMILLE 700U
/* 定位参数均为暂定值，需实机调整。 */
#define FOCUS_MOTOR_MOVE_TOLERANCE_COUNTS 50
#define FOCUS_MOTOR_MOVE_DUTY_PERMILLE 800U
#define FOCUS_MOTOR_MOVE_SETTLE_MS 1000U
#define FOCUS_MOTOR_STALL_RAW_THRESHOLD 1700U
#define FOCUS_MOTOR_MOVE_STALL_FRAMES 3U
/* 寻零允差为编码器四倍频计数；严格小于才有效。首次触底也计入次数。 */
#define FOCUS_MOTOR_HOME_DIRECTION FOCUS_MOTOR_DIR_B
#define FOCUS_MOTOR_HOME_TOLERANCE_COUNTS 50
#define FOCUS_MOTOR_HOME_MAX_ATTEMPTS 5U
#define FOCUS_MOTOR_HOME_BACKOFF_DUTY_PERMILLE 700U
#define FOCUS_MOTOR_HOME_BACKOFF_MS 1000U

typedef enum {
    FOCUS_MOTOR_DIR_A = 0,
    FOCUS_MOTOR_DIR_B = 1
} FocusMotor_Direction;

/* 开环模式只接受方向和占空比，测试波形不属于电机模式。 */
typedef enum {
    FOCUS_MOTOR_MODE_STOP = 0,
    FOCUS_MOTOR_MODE_GLIDE,
    FOCUS_MOTOR_MODE_OPEN_LOOP,
    FOCUS_MOTOR_MODE_ERROR,
    FOCUS_MOTOR_MODE_MOVE_RAW
} FocusMotor_Mode;

/* 工作状态与输出模式分开；寻零及反向退回都属于初始化，供后续 UART 上报。 */
typedef enum {
    FOCUS_MOTOR_WORK_INITIALIZING = 0,
    FOCUS_MOTOR_WORK_NORMAL = 1,
    FOCUS_MOTOR_WORK_ERROR = 2
} FocusMotor_WorkState;

typedef enum {
    FOCUS_MOTOR_MOVE_IDLE = 0,
    FOCUS_MOTOR_MOVE_MOVING,
    FOCUS_MOTOR_MOVE_REACHED,
    FOCUS_MOTOR_MOVE_STALLED,
    FOCUS_MOTOR_MOVE_CANCELED
} FocusMotor_MoveState;

typedef struct {
    FocusMotor_Direction direction;
    uint16_t duty_permille; /* 0..1000，0 为制动 */
} FocusMotor_OutputCommand;

typedef struct {
    uint16_t max_duty_permille; /* 开环测试最大占空比，0..1000 */
    FocusMotor_Direction positive_direction; /* 默认逻辑正向，测试物理顺序仍为 A 后 B */
} FocusMotor_Config;

typedef struct {
    FocusMotor_Mode mode;
    FocusMotor_Direction direction;
    int8_t logical_sign;
    uint16_t duty_permille; /* 当前输出，0 为双高制动 */
    uint8_t initialized;
    uint8_t error;
    uint8_t glide_completed; /* 普通 Glide 达到阈值，或整个寻零流程成功完成 */
    FocusMotor_WorkState work_state;
    uint8_t seek_zero;       /* 正在执行完整寻零流程，包含反向退回 */
    uint8_t zero_valid;      /* 相邻两次触底位置通过允差比较 */
    uint8_t home_attempts;   /* 触底次数，包含首次建立候选零位 */
    int32_t target_raw; /* 最近一次受理的定位目标 */
    FocusMotor_MoveState move_state; /* 定位状态；REACHED 后继续监测并在超出允差时修正 */
    int32_t home_error_counts; /* 最近触底清零前的编码器位置 */
} FocusMotor_Snapshot;

extern FocusMotor_Config FocusMotor_ConfigData;
extern volatile FocusMotor_Snapshot FocusMotor_State;

/* 初始化输出并制动；调度器启动 700 ms 后自动寻零，成功后制动等待指令。
 * 主循环仅在调度器 10 ms 电机任务中调用 Step。
 * now_ms 传给启动等待及寻零流程，PWM 输出层不保存时间戳。 */
HAL_StatusTypeDef FocusMotor_Init(TIM_HandleTypeDef *htim);
void FocusMotor_Step(uint32_t now_ms);
/* seek_zero=1：寻零、触底校验及重试；seek_zero=0：普通 Glide，只制动不置零。
 * 初始化期间不接受外部控制；错误锁存后不响应指令，重新上电才能恢复。 */
void FocusMotor_Glide(FocusMotor_Direction direction, uint8_t seek_zero);
/* 直接给定开环方向和占空比，下一次 Step 应用；无渐变、等待或阶段推进。
 * 调用者提供 A/B 方向及 0..1000 占空比。 */
void FocusMotor_OpenLoop(FocusMotor_Direction direction, uint16_t duty_permille);
/* 正常状态下立即双高制动；初始化/错误状态忽略该指令。 */
void FocusMotor_Stop(void);
/* RAW 为当前零点的编码器软件累计四倍频位置；要求正方向映射与编码器增量一致。
 * HAL_OK 表示受理并替换旧目标；进入 ±50（含边界）后制动并确认稳定。
 * HAL_BUSY 表示未初始化、寻零中或零位无效；HAL_ERROR 表示故障锁存。
 * 制动稳定后标记到位，超出允差时重新修正，直到被其他命令取消；调用均在主循环。 */
HAL_StatusTypeDef FocusMotor_MoveRaw(int32_t target_raw);
/* 保留一致快照接口，可从 ISR 读取。 */
void FocusMotor_GetSnapshot(FocusMotor_Snapshot *snapshot);
/* 仅正常 STOP 模式允许修改；运动或未就绪返回 HAL_BUSY，故障或无效方向返回 HAL_ERROR。 */
HAL_StatusTypeDef FocusMotor_SetPositiveDirection(FocusMotor_Direction direction);

#endif
