#include "FocusUnit/Motor/focus_motor_home.h"
#include "FocusUnit/Encoder/focus_encoder.h"
#include "FocusUnit/Adc/focus_adc.h"

#if FOCUS_MOTOR_HOME_MAX_ATTEMPTS < 2U || FOCUS_MOTOR_HOME_MAX_ATTEMPTS > 255U
#error "HOME_MAX_ATTEMPTS must be in 2..255"
#endif
#if FOCUS_MOTOR_HOME_TOLERANCE_COUNTS <= 0 || FOCUS_MOTOR_HOME_TOLERANCE_COUNTS > 2147483647
#error "HOME_TOLERANCE_COUNTS must be positive and fit int32_t"
#endif
#if FOCUS_MOTOR_HOME_BACKOFF_DUTY_PERMILLE > 1000U
#error "HOME_BACKOFF_DUTY_PERMILLE must be in 0..1000"
#endif
#if FOCUS_MOTOR_HOME_BACKOFF_MS == 0U || FOCUS_MOTOR_HOME_BACKOFF_MS >= 0x80000000U
#error "HOME_BACKOFF_MS must be in 1..2147483647"
#endif

typedef enum {
    HOME_GLIDE = 0,
    HOME_BACKOFF_PENDING,
    HOME_BACKOFF
} HomePhase;

/* .noinit 位于启动清零区间之外，保留 NRST/软件/看门狗复位前的故障。
 * 只在 RCC 的 POR/PDR 标志有效时清除，不写 Flash；需两个链接脚本保留此段。 */
#define HOME_FAULT_MAGIC 0x46484F4DU
typedef struct {
    uint32_t magic;
    uint32_t magic_inverse;
    uint32_t attempts;
    int32_t error_counts;
} HomeRetainedFault;
static volatile HomeRetainedFault s_retained_fault __attribute__((section(".noinit")));

static FocusMotor_HomeStatus s_status;
static HomePhase s_phase;
static FocusMotor_Direction s_home_direction;
static uint32_t s_backoff_start_ms;
static uint32_t s_glide_start_ms;

/* 仅在 Motor Init 的首次调用执行，重复 Init 不能清除故障。 */
void FocusMotor_HomeBoot(void)
{
    s_status = (FocusMotor_HomeStatus){0};
    if ((RCC->CSR & RCC_CSR_PORRSTF) != 0U) {
        s_retained_fault.magic = 0U;
        s_retained_fault.magic_inverse = 0U;
    } else if ((s_retained_fault.magic == HOME_FAULT_MAGIC) &&
               (s_retained_fault.magic_inverse == ~HOME_FAULT_MAGIC)) {
        s_status.work_state = FOCUS_MOTOR_WORK_ERROR;
        s_status.attempts = (uint8_t)s_retained_fault.attempts;
        s_status.error_counts = s_retained_fault.error_counts;
    }
    /* 避免旧 POR 标志使下一次软件/外部复位误判为重新上电。 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void FocusMotor_HomeStart(FocusMotor_Direction direction)
{
    if (s_status.work_state == FOCUS_MOTOR_WORK_ERROR) return;
    s_status = (FocusMotor_HomeStatus){0};
    s_home_direction = direction;
    s_phase = HOME_GLIDE;
}

/* 先保存诊断信息，再写有效标记；复位后仍能上报错误和最后一次触底位置。 */
void FocusMotor_HomeFail(void)
{
    s_status.work_state = FOCUS_MOTOR_WORK_ERROR;
    s_status.zero_valid = 0U;
    s_retained_fault.attempts = s_status.attempts;
    s_retained_fault.error_counts = s_status.error_counts;
    s_retained_fault.magic_inverse = ~HOME_FAULT_MAGIC;
    __DMB();
    s_retained_fault.magic = HOME_FAULT_MAGIC;
    __DMB();
}

const FocusMotor_HomeStatus *FocusMotor_HomeGetStatus(void)
{
    return &s_status;
}

/* 10 ms 调度推进；用调度器绝对时间管理 1 s 退回，能跨 uint32_t 时基回绕。 */
void FocusMotor_HomeStep(uint32_t now_ms, uint8_t touched, FocusMotor_HomeCommand *command)
{
    command->mode = FOCUS_MOTOR_MODE_STOP;
    command->output.direction = s_home_direction;
    command->output.duty_permille = 0U;
    if (s_status.work_state != FOCUS_MOTOR_WORK_INITIALIZING) return;

    switch (s_phase) {
    case HOME_GLIDE:
        if (touched != 0U) {
            FocusEncoderMeasurement encoder;
            FocusEncoder_GetSnapshot(&encoder);
            s_status.error_counts = encoder.position;
            ++s_status.attempts;
            /* 不使用 abs(int32_t)，避免 INT32_MIN 取绝对值溢出。
             * 首次触底只建候选零位，后续每次比较后仍重新置零。 */
            const int32_t tolerance = (int32_t)FOCUS_MOTOR_HOME_TOLERANCE_COUNTS;
            uint8_t matched = (s_status.attempts > 1U) &&
                (encoder.position > -tolerance) && (encoder.position < tolerance);
            FocusEncoder_Zero();
            if (matched != 0U) {
                s_status.zero_valid = 1U;
                s_status.work_state = FOCUS_MOTOR_WORK_NORMAL;
            } else if (s_status.attempts >= FOCUS_MOTOR_HOME_MAX_ATTEMPTS) {
                FocusMotor_HomeFail();
            } else {
                /* 本轮已制动；下一次 Step 才开始反向输出并建立计时基准。 */
                s_phase = HOME_BACKOFF_PENDING;
            }
            return;
        }
        command->mode = FOCUS_MOTOR_MODE_GLIDE;
        command->output.duty_permille = FOCUS_MOTOR_GLIDE_DUTY_PERMILLE;
        return;
    case HOME_BACKOFF_PENDING:
        s_backoff_start_ms = now_ms;
        s_phase = HOME_BACKOFF;
        break;
    case HOME_BACKOFF:
        if ((uint32_t)(now_ms - s_backoff_start_ms) >= FOCUS_MOTOR_HOME_BACKOFF_MS) {
            s_phase = HOME_GLIDE;
            command->mode = FOCUS_MOTOR_MODE_GLIDE;
            command->output.duty_permille = FOCUS_MOTOR_GLIDE_DUTY_PERMILLE;
            return;
        }
        break;
    }
    command->mode = FOCUS_MOTOR_MODE_OPEN_LOOP;
    command->output.direction = (s_home_direction == FOCUS_MOTOR_DIR_A) ?
                               FOCUS_MOTOR_DIR_B : FOCUS_MOTOR_DIR_A;
    command->output.duty_permille = FOCUS_MOTOR_HOME_BACKOFF_DUTY_PERMILLE;
}

void FocusMotor_GlideMonitorStart(uint32_t now_ms)
{
    s_glide_start_ms = now_ms;
}

/* ADC timestamp 是请求时刻；严格晚于本次 PWM 驱动开始才用于停止判定。
 * 无符号差限制在半个时基周期内，同时排除之前的高电流帧。 */
uint8_t FocusMotor_GlideMonitorStopped(void)
{
    FocusAdcMeasurement adc;
    if (FocusAdc_GetSnapshot(&adc) == 0U) return 0U;
    uint32_t age = adc.timestamp_ms - s_glide_start_ms;
    return (age != 0U) && (age < 0x80000000U) &&
           (adc.raw_motor > FOCUS_MOTOR_STALL_RAW_THRESHOLD);
}
