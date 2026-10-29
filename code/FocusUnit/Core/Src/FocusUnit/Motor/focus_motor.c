#include "FocusUnit/Motor/focus_motor.h"
#include "FocusUnit/Motor/focus_motor_home.h"
#include "FocusUnit/Motor/test.h"
#include "FocusUnit/Encoder/focus_encoder.h"
#include "FocusUnit/Adc/focus_adc.h"

#define MOTOR_PERIOD_COUNTS 2400U
#define MOTOR_PERMILLE 1000U
#define MOTOR_BOOT_HOME_DELAY_MS 700U

typedef enum {
    BOOT_WAIT = 0,
    BOOT_HOME,
    BOOT_DONE
} MotorBootPhase;

FocusMotor_Config FocusMotor_ConfigData = { MOTOR_PERMILLE, FOCUS_MOTOR_DIR_A };
volatile FocusMotor_Snapshot FocusMotor_State;

static TIM_HandleTypeDef *s_tim;
static FocusMotor_Mode s_mode;
static FocusMotor_Direction s_direction;
static uint16_t s_open_loop_duty;
static uint8_t s_initialized;
static uint8_t s_boot_checked;
static uint8_t s_seek_zero;
static uint8_t s_glide_active;
static uint8_t s_glide_completed;
static int32_t s_target_raw;
static FocusMotor_MoveState s_move_state;
static uint8_t s_move_active;
static uint8_t s_move_settling;
static uint32_t s_move_settle_start_ms;
static uint32_t s_move_drive_start_ms;
static uint32_t s_move_adc_sequence;
static uint8_t s_move_adc_seen;
static uint8_t s_move_stall_frames;
static MotorBootPhase s_boot_phase;

/* 成对写入比较预装载，在同一次 PWM 更新事件生效。 */
static void set_ccr(uint32_t a, uint32_t b)
{
    uint32_t cr1 = s_tim->Instance->CR1;
    s_tim->Instance->CR1 = cr1 | TIM_CR1_UDIS;
    __HAL_TIM_SET_COMPARE(s_tim, TIM_CHANNEL_3, a);
    __HAL_TIM_SET_COMPARE(s_tim, TIM_CHANNEL_2, b);
    s_tim->Instance->CR1 = cr1;
}

/* 千分比转换为硬件比较值；100% 使用 ARR+1。 */
static uint32_t duty_to_counts(uint16_t duty)
{
    if (duty >= MOTOR_PERMILLE) return MOTOR_PERIOD_COUNTS;
    return ((uint32_t)duty * MOTOR_PERIOD_COUNTS + 500U) / MOTOR_PERMILLE;
}

/* A=10、B=01；PWM 剩余区间为双高制动，0 占空比也是制动。 */
static void write_drive(FocusMotor_Direction direction, uint16_t duty)
{
    uint32_t brake_counts = MOTOR_PERIOD_COUNTS - duty_to_counts(duty);
    if (direction == FOCUS_MOTOR_DIR_A) set_ccr(MOTOR_PERIOD_COUNTS, brake_counts);
    else set_ccr(brake_counts, MOTOR_PERIOD_COUNTS);
}

static void write_brake(void)
{
    set_ccr(MOTOR_PERIOD_COUNTS, MOTOR_PERIOD_COUNTS);
}

/* 快照允许 ISR 读取，保留成组复制保护；不在此管理调度或时序。 */
static void publish_snapshot(uint16_t duty)
{
    const FocusMotor_HomeStatus *home = FocusMotor_HomeGetStatus();
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    FocusMotor_State.mode = s_mode;
    FocusMotor_State.direction = s_direction;
    FocusMotor_State.logical_sign =
        (s_direction == FocusMotor_ConfigData.positive_direction) ? 1 : -1;
    FocusMotor_State.duty_permille = duty;
    FocusMotor_State.initialized = s_initialized;
    FocusMotor_State.error = (home->work_state == FOCUS_MOTOR_WORK_ERROR);
    FocusMotor_State.glide_completed = s_glide_completed;
    FocusMotor_State.work_state = home->work_state;
    FocusMotor_State.seek_zero = s_seek_zero;
    FocusMotor_State.zero_valid = home->zero_valid;
    FocusMotor_State.home_attempts = home->attempts;
    FocusMotor_State.home_error_counts = home->error_counts;
    FocusMotor_State.target_raw = s_target_raw;
    FocusMotor_State.move_state = s_move_state;
    __set_PRIMASK(primask);
}

static void cancel_move(void)
{
    if (s_mode == FOCUS_MOTOR_MODE_MOVE_RAW) s_move_state = FOCUS_MOTOR_MOVE_CANCELED;
    s_move_active = 0U;
    s_move_settling = 0U;
}

/* 内部制动不修改工作状态或寻零基准，不能把初始化误发布为正常。 */
static void stop_output(void)
{
    if (s_tim != NULL) write_brake();
    s_mode = FOCUS_MOTOR_MODE_STOP;
    s_glide_active = 0U;
    s_glide_completed = 0U;
    publish_snapshot(0U);
}

static void enter_error(void)
{
    FocusMotor_TestStop();
    stop_output();
    FocusMotor_HomeFail();
    s_seek_zero = 0U;
    s_mode = FOCUS_MOTOR_MODE_ERROR;
    publish_snapshot(0U);
}

/* 开环只保存命令参数，输出由下一次调度的 Step 应用。 */
static void select_open_loop(FocusMotor_Direction direction, uint16_t duty)
{
    s_mode = FOCUS_MOTOR_MODE_OPEN_LOOP;
    s_direction = direction;
    s_open_loop_duty = duty;
    s_glide_active = 0U;
    s_glide_completed = 0U;
}

/* 内部选择 Glide，不通过公共接口，初始化与重试不能被控制入口拦截。 */
static void select_glide(FocusMotor_Direction direction)
{
    s_mode = FOCUS_MOTOR_MODE_GLIDE;
    s_direction = direction;
    s_glide_active = 0U;
    s_glide_completed = 0U;
}

/* 普通控制只允许在零位确认后的正常状态执行。 */
static uint8_t can_control(void)
{
    return (s_initialized != 0U) &&
           (FocusMotor_HomeGetStatus()->work_state == FOCUS_MOTOR_WORK_NORMAL);
}

/* 初始化两路 PWM；跨复位故障只在上电复位时解除，重复 Init 不能重试。 */
HAL_StatusTypeDef FocusMotor_Init(TIM_HandleTypeDef *htim)
{
    HAL_StatusTypeDef status;
    if (FocusMotor_HomeGetStatus()->work_state == FOCUS_MOTOR_WORK_ERROR) return HAL_ERROR;
    if (s_initialized != 0U) return HAL_BUSY;
    if ((htim == NULL) || (htim->Instance != TIM1) ||
        (htim->Instance->ARR != MOTOR_PERIOD_COUNTS - 1U) || (htim->Instance->PSC != 0U) ||
        (FocusMotor_ConfigData.max_duty_permille > MOTOR_PERMILLE) ||
        ((FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_A) &&
         (FocusMotor_ConfigData.positive_direction != FOCUS_MOTOR_DIR_B)) ||
        ((FOCUS_MOTOR_HOME_DIRECTION != FOCUS_MOTOR_DIR_A) &&
         (FOCUS_MOTOR_HOME_DIRECTION != FOCUS_MOTOR_DIR_B))) {
        return HAL_ERROR;
    }
    if (s_boot_checked == 0U) {
        FocusMotor_HomeBoot();
        s_boot_checked = 1U;
    }
    s_tim = htim;
    s_mode = FOCUS_MOTOR_MODE_STOP;
    s_direction = FocusMotor_ConfigData.positive_direction;
    s_open_loop_duty = 0U;
    FocusMotor_State = (FocusMotor_Snapshot){0};
    __HAL_TIM_ENABLE_OCxPRELOAD(htim, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_OCxPRELOAD(htim, TIM_CHANNEL_3);
    set_ccr(0U, 0U);
    status = HAL_TIM_PWM_Start(htim, TIM_CHANNEL_2);
    if (status != HAL_OK) { enter_error(); return status; }
    status = HAL_TIM_PWM_Start(htim, TIM_CHANNEL_3);
    if (status != HAL_OK) { enter_error(); return status; }
    s_initialized = 1U;
    /* 保留故障时仍启动调度和采集，保持制动并允许后续 UART 读取状态。 */
    if (FocusMotor_HomeGetStatus()->work_state == FOCUS_MOTOR_WORK_ERROR) {
        enter_error();
        return HAL_OK;
    }
    write_brake();
    s_boot_phase = BOOT_WAIT;
    publish_snapshot(0U);
    return HAL_OK;
}

/* 首次驱动先建立 ADC 判定基准；普通 Glide 达到阈值仅制动，不置零。 */
static void step_glide(uint32_t now_ms)
{
    if (s_glide_active == 0U) {
        FocusMotor_GlideMonitorStart(now_ms);
        s_glide_active = 1U;
    } else if (FocusMotor_GlideMonitorStopped() != 0U) {
        stop_output();
        s_glide_completed = 1U;
        publish_snapshot(0U);
        return;
    }
    write_drive(s_direction, FOCUS_MOTOR_GLIDE_DUTY_PERMILLE);
    publish_snapshot(FOCUS_MOTOR_GLIDE_DUTY_PERMILLE);
}

/* 开环只应用给定方向和占空比，没有测试阶段判断。 */
static void step_open_loop(void)
{
    write_drive(s_direction, s_open_loop_duty);
    publish_snapshot(s_open_loop_duty);
}

/* 每个 ADC 完整帧只计一次；丢帧或低于阈值时重新累计。 */
static uint8_t move_stalled(void)
{
    FocusAdcMeasurement adc;
    if (FocusAdc_GetSnapshot(&adc) == 0U) return 0U;
    uint32_t age = adc.timestamp_ms - s_move_drive_start_ms;
    if ((age == 0U) || (age >= 0x80000000U)) return 0U;
    if ((s_move_adc_seen != 0U) && (adc.sequence == s_move_adc_sequence)) return 0U;
    if ((s_move_adc_seen != 0U) && (adc.sequence != s_move_adc_sequence + 1U)) {
        s_move_stall_frames = 0U;
    }
    s_move_adc_seen = 1U;
    s_move_adc_sequence = adc.sequence;
    if (adc.raw_motor > FOCUS_MOTOR_STALL_RAW_THRESHOLD) {
        if (s_move_stall_frames < FOCUS_MOTOR_MOVE_STALL_FRAMES) ++s_move_stall_frames;
    } else {
        s_move_stall_frames = 0U;
    }
    return s_move_stall_frames >= FOCUS_MOTOR_MOVE_STALL_FRAMES;
}

/* 使用 64 位误差避免 int32 两端相减或取绝对值溢出。 */
static void step_move_raw(uint32_t now_ms)
{
    FocusEncoderMeasurement encoder;
    int64_t error;
    FocusMotor_Direction direction;
    FocusEncoder_GetSnapshot(&encoder);
    error = (int64_t)s_target_raw - (int64_t)encoder.position;
    if ((error >= -FOCUS_MOTOR_MOVE_TOLERANCE_COUNTS) &&
        (error <= FOCUS_MOTOR_MOVE_TOLERANCE_COUNTS)) {
        s_move_active = 0U;
        if (s_move_settling == 0U) {
            s_move_settling = 1U;
            s_move_settle_start_ms = now_ms;
            write_brake();
            publish_snapshot(0U);
        } else if ((s_move_state != FOCUS_MOTOR_MOVE_REACHED) &&
                   ((uint32_t)(now_ms - s_move_settle_start_ms) >= FOCUS_MOTOR_MOVE_SETTLE_MS)) {
            s_move_state = FOCUS_MOTOR_MOVE_REACHED;
            publish_snapshot(0U);
        }
        return;
    }
    s_move_settling = 0U;
    s_move_state = FOCUS_MOTOR_MOVE_MOVING;
    if ((s_move_active != 0U) && (move_stalled() != 0U)) {
        s_move_state = FOCUS_MOTOR_MOVE_STALLED;
        s_move_active = 0U;
        stop_output();
        return;
    }
    direction = (error > 0) ? FocusMotor_ConfigData.positive_direction :
        ((FocusMotor_ConfigData.positive_direction == FOCUS_MOTOR_DIR_A) ?
         FOCUS_MOTOR_DIR_B : FOCUS_MOTOR_DIR_A);
    if (s_move_active == 0U) {
        s_move_drive_start_ms = now_ms;
        s_move_adc_seen = 0U;
        s_move_stall_frames = 0U;
        s_move_active = 1U;
    }
    s_direction = direction;
    write_drive(direction, FOCUS_MOTOR_MOVE_DUTY_PERMILLE);
    publish_snapshot(FOCUS_MOTOR_MOVE_DUTY_PERMILLE);
}

/* 初始化分支：触底制动、推进寻零流程，并应用本轮输出命令。 */
static void step_home(uint32_t now_ms)
{
    const FocusMotor_HomeStatus *home = FocusMotor_HomeGetStatus();
    FocusMotor_HomeCommand home_command;
    uint8_t touched = (s_mode == FOCUS_MOTOR_MODE_GLIDE) &&
                        (s_glide_active != 0U) &&
                        (FocusMotor_GlideMonitorStopped() != 0U);
    /* 先制动，再记录位置和置零。本轮触底保持制动，下一轮才退回。 */
    if (touched != 0U) stop_output();
    FocusMotor_HomeStep(now_ms, touched, &home_command);
    if (home->work_state == FOCUS_MOTOR_WORK_ERROR) {
        enter_error();
        return;
    }
    if (home_command.mode == FOCUS_MOTOR_MODE_GLIDE) {
        if (s_mode != FOCUS_MOTOR_MODE_GLIDE) select_glide(home_command.output.direction);
        if (s_glide_active == 0U) {
            FocusMotor_GlideMonitorStart(now_ms);
            s_glide_active = 1U;
        }
        write_drive(s_direction, FOCUS_MOTOR_HOME_GLIDE_DUTY_PERMILLE);
        publish_snapshot(FOCUS_MOTOR_HOME_GLIDE_DUTY_PERMILLE);
    } else if (home_command.mode == FOCUS_MOTOR_MODE_OPEN_LOOP) {
        select_open_loop(home_command.output.direction, home_command.output.duty_permille);
        step_open_loop();
    } else {
        stop_output();
        if (home->zero_valid != 0U) {
            s_seek_zero = 0U;
            s_glide_completed = 1U;
        }
        publish_snapshot(0U);
    }
}

/* 寻零成功后触底位置设为 -500，再定位到 0；后续按命令推进运动。 */
void FocusMotor_Step(uint32_t now_ms)
{
    const FocusMotor_HomeStatus *home = FocusMotor_HomeGetStatus();
    if ((s_initialized == 0U) || (home->work_state == FOCUS_MOTOR_WORK_ERROR)) return;
    if (s_boot_phase == BOOT_WAIT) {
        if (now_ms < MOTOR_BOOT_HOME_DELAY_MS) return;
        s_boot_phase = BOOT_HOME;
        FocusMotor_HomeStart(FOCUS_MOTOR_HOME_DIRECTION);
        s_seek_zero = 1U;
        select_glide(FOCUS_MOTOR_HOME_DIRECTION);
    }
    if (home->work_state == FOCUS_MOTOR_WORK_INITIALIZING) {
        step_home(now_ms);
        if ((home->work_state == FOCUS_MOTOR_WORK_NORMAL) && (home->zero_valid != 0U)) {
            s_boot_phase = BOOT_DONE;
            (void)FocusMotor_MoveRaw(0);
        }
        return;
    }
    switch (s_mode) {
    case FOCUS_MOTOR_MODE_MOVE_RAW:
        step_move_raw(now_ms);
        break;
    case FOCUS_MOTOR_MODE_GLIDE:
        step_glide(now_ms);
        break;
    case FOCUS_MOTOR_MODE_OPEN_LOOP:
        step_open_loop();
        break;
    case FOCUS_MOTOR_MODE_STOP:
    default:
        break;
    }
}

/* 显式命令只在正常状态生效；寻零参数决定是否启动完整校验/重试流程。 */
void FocusMotor_Glide(FocusMotor_Direction direction, uint8_t seek_zero)
{
    if (can_control() == 0U) return;
    s_boot_phase = BOOT_DONE;
    FocusMotor_TestStop();
    cancel_move();
    stop_output();
    s_seek_zero = (seek_zero != 0U);
    if (s_seek_zero != 0U) FocusMotor_HomeStart(direction);
    select_glide(direction);
    publish_snapshot(0U);
}

void FocusMotor_OpenLoop(FocusMotor_Direction direction, uint16_t duty_permille)
{
    if (can_control() == 0U) return;
    s_boot_phase = BOOT_DONE;
    uint8_t was_move = (s_mode == FOCUS_MOTOR_MODE_MOVE_RAW);
    FocusMotor_TestStop();
    cancel_move();
    if (was_move != 0U) stop_output();
    select_open_loop(direction, duty_permille);
    if (was_move != 0U) publish_snapshot(0U);
}

void FocusMotor_Stop(void)
{
    if (can_control() == 0U) return;
    s_boot_phase = BOOT_DONE;
    FocusMotor_TestStop();
    cancel_move();
    stop_output();
}

HAL_StatusTypeDef FocusMotor_MoveRaw(int32_t target_raw)
{
    if (FocusMotor_HomeGetStatus()->work_state == FOCUS_MOTOR_WORK_ERROR) return HAL_ERROR;
    if ((can_control() == 0U) || (FocusMotor_HomeGetStatus()->zero_valid == 0U)) return HAL_BUSY;
    s_boot_phase = BOOT_DONE;
    FocusMotor_TestStop();
    stop_output();
    s_target_raw = target_raw;
    s_move_state = FOCUS_MOTOR_MOVE_MOVING;
    s_move_active = 0U;
    s_move_settling = 0U;
    s_mode = FOCUS_MOTOR_MODE_MOVE_RAW;
    /* 下一次电机任务检查位置、制动稳定或开始驱动。 */
    publish_snapshot(0U);
    return HAL_OK;
}

void FocusMotor_GetSnapshot(FocusMotor_Snapshot *snapshot)
{
    if (snapshot == NULL) return;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *snapshot = FocusMotor_State;
    __set_PRIMASK(primask);
}

HAL_StatusTypeDef FocusMotor_SetPositiveDirection(FocusMotor_Direction direction)
{
    if (FocusMotor_HomeGetStatus()->work_state == FOCUS_MOTOR_WORK_ERROR) return HAL_ERROR;
    if (can_control() == 0U) return HAL_BUSY;
    if (s_mode != FOCUS_MOTOR_MODE_STOP) return HAL_BUSY;
    if ((direction != FOCUS_MOTOR_DIR_A) && (direction != FOCUS_MOTOR_DIR_B)) return HAL_ERROR;
    FocusMotor_ConfigData.positive_direction = direction;
    publish_snapshot(0U);
    return HAL_OK;
}
