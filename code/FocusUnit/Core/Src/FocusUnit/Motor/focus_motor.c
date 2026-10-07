#include "FocusUnit/Motor/focus_motor.h"

#define MOTOR_PERIOD_COUNTS 2400U
#define MOTOR_STARTUP_MS 1U       /* 上电阶段至少 1 ms，覆盖驱动唤醒时间；实际由 10 ms 任务推进 */
#define MOTOR_BRAKE_WAKE_MS 1U
#define MOTOR_RAMP_UP_MS 3000U
#define MOTOR_HOLD_MS 3000U
#define MOTOR_RAMP_DOWN_MS 3000U
#define MOTOR_BRAKE_MS 3000U
#define MOTOR_MOTOR_CYCLE_MS 12000U
#define MOTOR_CYCLE_MS 24000U
#define MOTOR_PERMILLE 1000U
#define MOTOR_DIRECTION_GUARD_MS 1U

FocusMotor_Config FocusMotor_ConfigData = { MOTOR_PERMILLE, FOCUS_MOTOR_DIR_A, MOTOR_PERIOD_COUNTS };
volatile FocusMotor_Snapshot FocusMotor_State;

static TIM_HandleTypeDef *s_tim;
static uint32_t s_start_ms;
static uint32_t s_last_update_ms;
static uint32_t s_stage_start_ms;
static uint32_t s_transition_start_ms;
static FocusMotor_Direction s_active_direction;
static uint8_t s_transition_pending;
static uint8_t s_running;

/* 用无符号差值计算间隔，正常周期内可跨 32 位毫秒时基回绕。 */
static uint32_t elapsed(uint32_t now, uint32_t then)
{
    return now - then; /* 无符号减法处理毫秒时基回绕 */
}

/* 成对更新 TIM1 CH3/CH2 比较预装载值，避免两路在不同更新事件生效。 */
static void set_ccr(uint32_t a, uint32_t b)
{
    uint32_t cr1 = s_tim->Instance->CR1;
    /* 暂禁更新事件，两路预装载写完后恢复，在下一次共同 UEV 生效。 */
    s_tim->Instance->CR1 = cr1 | TIM_CR1_UDIS;
    __HAL_TIM_SET_COMPARE(s_tim, TIM_CHANNEL_3, a);
    __HAL_TIM_SET_COMPARE(s_tim, TIM_CHANNEL_2, b);
    s_tim->Instance->CR1 = cr1;
}

/* 驱动千分比换算为周期计数；1000 返回 ARR+1，实现持续高电平。 */
static uint32_t duty_to_counts(uint16_t duty)
{
    if (duty == 0U) return 0U;
    if (duty >= MOTOR_PERMILLE) return MOTOR_PERIOD_COUNTS;
    return ((uint32_t)duty * MOTOR_PERIOD_COUNTS + 500U) / MOTOR_PERMILLE;
}

/* 一侧固定高、另一侧 PWM；驱动与双高制动交替，属于开环占空比控制。 */
static void write_drive(FocusMotor_Direction direction, uint16_t duty)
{
    uint32_t drive_counts = duty_to_counts(duty);
    uint32_t brake_fraction = MOTOR_PERIOD_COUNTS - drive_counts;
    uint32_t a_counts, b_counts;

    /* DRV8251A：A=10、B=01，PWM 其余时间为 11 制动；00 为滑行/睡眠。 */
    if (direction == FOCUS_MOTOR_DIR_A) {
        a_counts = MOTOR_PERIOD_COUNTS; /* PA10 / IN1 固定高 */
        b_counts = brake_fraction;      /* PA9 / IN2 低电平区间驱动 A */
    } else {
        a_counts = brake_fraction;      /* PA10 / IN1 低电平区间驱动 B */
        b_counts = MOTOR_PERIOD_COUNTS; /* PA9 / IN2 固定高 */
    }
    set_ccr(a_counts, b_counts);
}

/* IN1/IN2 双高制动；正常 Stop 使用此状态，异常总停机另走 FaultStop 双低。 */
static void write_brake(void)
{
    set_ccr(MOTOR_PERIOD_COUNTS, MOTOR_PERIOD_COUNTS);
}

/* 保存中断状态后屏蔽，保证快照字段成组发布。 */
static void lock_interrupts(uint32_t *primask)
{
    *primask = __get_PRIMASK();
    __disable_irq();
}

/* 仅在进入前允许中断时重新使能。 */
static void unlock_interrupts(uint32_t primask)
{
    if (primask == 0U) __enable_irq();
}

/* 发布物理方向、逻辑符号、驱动占空比和阶段时长；符号不代表机械运动反馈。 */
static void publish_snapshot(FocusMotor_Stage stage, FocusMotor_Direction direction,
                             uint16_t duty, uint32_t now, uint32_t origin)
{
    uint32_t primask;
    lock_interrupts(&primask);
    FocusMotor_State.stage = stage;
    FocusMotor_State.direction = direction;
    FocusMotor_State.logical_sign = (direction == FocusMotor_ConfigData.positive_direction) ? 1 : -1;
    FocusMotor_State.duty_permille = duty;
    FocusMotor_State.stage_elapsed_ms = elapsed(now, origin);
    unlock_interrupts(primask);
}

/* 锁存模块错误并停止演示，已绑定定时器时请求制动。 */
static void enter_error(void)
{
    uint32_t primask;
    if (s_tim != NULL) write_brake();
    s_running = 0U;
    lock_interrupts(&primask);
    FocusMotor_State.stage = FOCUS_MOTOR_STAGE_ERROR;
    FocusMotor_State.error = 1U;
    FocusMotor_State.duty_permille = 0U;
    unlock_interrupts(primask);
}

/* 验证 TIM1 固定周期与配置，先给双低输出，再启动两路 PWM 和演示状态机。 */
HAL_StatusTypeDef FocusMotor_Init(TIM_HandleTypeDef *htim, uint32_t now_ms)
{
    HAL_StatusTypeDef status;
    if ((htim == NULL) || (htim->Instance != TIM1) ||
        (htim->Instance->ARR != (MOTOR_PERIOD_COUNTS - 1U)) || (htim->Instance->PSC != 0U) ||
        (FocusMotor_ConfigData.max_duty_permille > MOTOR_PERMILLE) ||
        ((FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_A) &&
         (FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_B))) {
        return HAL_ERROR;
    }
    s_tim = htim;
    s_start_ms = now_ms;
    s_last_update_ms = now_ms;
    s_stage_start_ms = now_ms;
    s_transition_start_ms = now_ms;
    s_active_direction = FOCUS_MOTOR_DIR_A;
    s_transition_pending = 0U;
    s_running = 1U;
    FocusMotor_State.stage = FOCUS_MOTOR_STAGE_STARTUP;
    FocusMotor_State.direction = s_active_direction;
    FocusMotor_State.logical_sign = (s_active_direction == FocusMotor_ConfigData.positive_direction) ? 1 : -1;
    FocusMotor_State.duty_permille = 0U;
    FocusMotor_State.initialized = 0U;
    FocusMotor_State.error = 0U;
    FocusMotor_State.stage_elapsed_ms = 0U;
    FocusMotor_State.update_late_count = 0U;

    /* 启动两路输出前先写 00 滑行/睡眠；之后由状态机切换到 11 制动唤醒。 */
    __HAL_TIM_ENABLE_OCxPRELOAD(htim, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_OCxPRELOAD(htim, TIM_CHANNEL_3);
    set_ccr(0U, 0U);
    status = HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2);
    if (status != HAL_OK) { enter_error(); return status; }
    status = HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3);
    if (status != HAL_OK) { enter_error(); return status; }
    FocusMotor_State.initialized = 1U;
    return HAL_OK;
}

/* 每 10 ms 按绝对时间推进 A/B 各四个 3 s 阶段，不使用阻塞延时。 */
void FocusMotor_Update(uint32_t now_ms)
{
    uint32_t total_elapsed, cycle, cycle_index, motor_phase, phase;
    uint32_t stage_origin, demo_origin, update_gap, primask;
    FocusMotor_Stage stage;
    FocusMotor_Direction requested_direction;
    uint16_t duty;
    uint8_t prior_brake_was_long_enough;

    if ((FocusMotor_State.initialized == 0U) || (FocusMotor_State.error != 0U) || (s_running == 0U)) return;
    update_gap = elapsed(now_ms, s_last_update_ms);
    s_last_update_ms = now_ms;
    if (update_gap > 10U) {
        lock_interrupts(&primask);
        FocusMotor_State.update_late_count++;
        unlock_interrupts(primask);
    }
    if ((FocusMotor_ConfigData.max_duty_permille > MOTOR_PERMILLE) ||
        ((FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_A) &&
         (FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_B))) {
        enter_error();
        return;
    }

    if ((FocusMotor_State.stage == FOCUS_MOTOR_STAGE_STARTUP) &&
        (elapsed(now_ms, s_start_ms) < MOTOR_STARTUP_MS)) {
        publish_snapshot(FOCUS_MOTOR_STAGE_STARTUP, s_active_direction, 0U, now_ms, s_start_ms);
        return;
    }
    if (FocusMotor_State.stage == FOCUS_MOTOR_STAGE_STARTUP) {
        write_brake();
        s_stage_start_ms = now_ms;
        publish_snapshot(FOCUS_MOTOR_STAGE_BRAKE_WAKE, s_active_direction, 0U, now_ms, now_ms);
        return;
    }
    if ((FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE_WAKE) &&
        (elapsed(now_ms, s_stage_start_ms) < MOTOR_BRAKE_WAKE_MS)) {
        publish_snapshot(FOCUS_MOTOR_STAGE_BRAKE_WAKE, s_active_direction, 0U, now_ms, s_stage_start_ms);
        return;
    }

    total_elapsed = elapsed(now_ms, s_stage_start_ms) - MOTOR_BRAKE_WAKE_MS;
    cycle_index = total_elapsed / MOTOR_CYCLE_MS;
    cycle = total_elapsed % MOTOR_CYCLE_MS;
    /* 每圈重建附近的时间基准，避免长期运行后时基回绕使演示相位跳变。 */
    s_stage_start_ms += cycle_index * MOTOR_CYCLE_MS;
    motor_phase = cycle % MOTOR_MOTOR_CYCLE_MS;
    demo_origin = s_stage_start_ms + MOTOR_BRAKE_WAKE_MS;
    requested_direction = (cycle < MOTOR_MOTOR_CYCLE_MS) ? FOCUS_MOTOR_DIR_A : FOCUS_MOTOR_DIR_B;

    if (motor_phase < MOTOR_RAMP_UP_MS) {
        stage = FOCUS_MOTOR_STAGE_RAMP_UP;
        phase = motor_phase;
        stage_origin = demo_origin + ((cycle >= MOTOR_MOTOR_CYCLE_MS) ? MOTOR_MOTOR_CYCLE_MS : 0U);
        duty = (uint16_t)(((uint32_t)FocusMotor_ConfigData.max_duty_permille * phase) / MOTOR_RAMP_UP_MS);
    } else if (motor_phase < (MOTOR_RAMP_UP_MS + MOTOR_HOLD_MS)) {
        stage = FOCUS_MOTOR_STAGE_HOLD;
        stage_origin = demo_origin + ((cycle >= MOTOR_MOTOR_CYCLE_MS) ? MOTOR_MOTOR_CYCLE_MS : 0U) + MOTOR_RAMP_UP_MS;
        duty = FocusMotor_ConfigData.max_duty_permille;
    } else if (motor_phase < (MOTOR_RAMP_UP_MS + MOTOR_HOLD_MS + MOTOR_RAMP_DOWN_MS)) {
        stage = FOCUS_MOTOR_STAGE_RAMP_DOWN;
        phase = motor_phase - MOTOR_RAMP_UP_MS - MOTOR_HOLD_MS;
        stage_origin = demo_origin + ((cycle >= MOTOR_MOTOR_CYCLE_MS) ? MOTOR_MOTOR_CYCLE_MS : 0U) + MOTOR_RAMP_UP_MS + MOTOR_HOLD_MS;
        duty = (uint16_t)(((uint32_t)FocusMotor_ConfigData.max_duty_permille * (MOTOR_RAMP_DOWN_MS - phase)) / MOTOR_RAMP_DOWN_MS);
    } else {
        stage = FOCUS_MOTOR_STAGE_BRAKE;
        stage_origin = demo_origin + ((cycle >= MOTOR_MOTOR_CYCLE_MS) ? MOTOR_MOTOR_CYCLE_MS : 0U) +
                       MOTOR_RAMP_UP_MS + MOTOR_HOLD_MS + MOTOR_RAMP_DOWN_MS;
        duty = 0U;
    }

    prior_brake_was_long_enough = (FocusMotor_State.stage == FOCUS_MOTOR_STAGE_BRAKE) &&
        ((FocusMotor_State.stage_elapsed_ms + update_gap) >= MOTOR_BRAKE_MS);
    if (requested_direction != s_active_direction) {
        if (s_transition_pending != 0U) {
            if (elapsed(now_ms, s_transition_start_ms) < MOTOR_DIRECTION_GUARD_MS) {
                write_brake();
                publish_snapshot(FOCUS_MOTOR_STAGE_BRAKE, s_active_direction, 0U,
                                 now_ms, s_transition_start_ms);
                return;
            }
            s_transition_pending = 0U;
        } else if (prior_brake_was_long_enough == 0U) {
            /* 主循环延迟可能跳过原定制动阶段，换向前补至少 1 ms 制动间隔。
             * 这是电气切换保护，不保证机械转子已停稳。 */
            write_brake();
            s_transition_pending = 1U;
            s_transition_start_ms = now_ms;
            publish_snapshot(FOCUS_MOTOR_STAGE_BRAKE, s_active_direction, 0U,
                             now_ms, s_transition_start_ms);
            return;
        }
        s_active_direction = requested_direction;
    } else {
        s_transition_pending = 0U;
    }

    if ((stage == FOCUS_MOTOR_STAGE_BRAKE) || (duty == 0U)) write_brake();
    else write_drive(s_active_direction, duty);
    publish_snapshot(stage, s_active_direction, duty, now_ms, stage_origin);
}

/* 请求制动并关闭演示推进；PWM 继续运行，后续 Update 不自动重启。 */
void FocusMotor_Stop(void)
{
    uint32_t primask;
    if (s_tim != NULL) write_brake();
    s_running = 0U;
    lock_interrupts(&primask);
    FocusMotor_State.duty_permille = 0U;
    if (FocusMotor_State.error == 0U) {
        FocusMotor_State.stage = FOCUS_MOTOR_STAGE_BRAKE;
        FocusMotor_State.stage_elapsed_ms = 0U;
    }
    unlock_interrupts(primask);
}

/* 临界区复制状态，允许 ISR 读取；业务修改接口只由主循环调用。 */
void FocusMotor_GetSnapshot(FocusMotor_Snapshot *snapshot)
{
    uint32_t primask;
    if (snapshot == NULL) return;
    lock_interrupts(&primask);
    snapshot->stage = FocusMotor_State.stage;
    snapshot->direction = FocusMotor_State.direction;
    snapshot->logical_sign = FocusMotor_State.logical_sign;
    snapshot->duty_permille = FocusMotor_State.duty_permille;
    snapshot->initialized = FocusMotor_State.initialized;
    snapshot->error = FocusMotor_State.error;
    snapshot->stage_elapsed_ms = FocusMotor_State.stage_elapsed_ms;
    snapshot->update_late_count = FocusMotor_State.update_late_count;
    unlock_interrupts(primask);
}

/* 只改逻辑正向映射，允许运行中调用；不改 A 后 B 的物理输出顺序。 */
HAL_StatusTypeDef FocusMotor_SetPositiveDirection(FocusMotor_Direction direction)
{
    uint32_t primask;
    if ((direction != FOCUS_MOTOR_DIR_A) && (direction != FOCUS_MOTOR_DIR_B)) return HAL_ERROR;
    lock_interrupts(&primask);
    FocusMotor_ConfigData.positive_direction = direction;
    unlock_interrupts(primask);
    return HAL_OK;
}
