#ifndef FOCUS_MOTOR_HOME_H
#define FOCUS_MOTOR_HOME_H

#include "FocusUnit/Motor/focus_motor.h"

/* Motor 内部接口，均在主循环中调用；寻零流程只生成输出命令，不操作 PWM。 */
typedef struct {
    FocusMotor_WorkState work_state;
    uint8_t zero_valid;
    uint8_t attempts;
    int32_t error_counts;
} FocusMotor_HomeStatus;

typedef struct {
    FocusMotor_Mode mode;
    FocusMotor_OutputCommand output;
} FocusMotor_HomeCommand;

/* 读取上电/复位原因，恢复跨复位故障锁存；仅上电复位清除锁存。 */
void FocusMotor_HomeBoot(void);
void FocusMotor_HomeStart(FocusMotor_Direction direction);
void FocusMotor_HomeFail(void);
const FocusMotor_HomeStatus *FocusMotor_HomeGetStatus(void);
/* touched 由 Motor 在已制动后传入；触底时先读编码器位置、比较，再置零。 */
void FocusMotor_HomeStep(uint32_t now_ms, uint8_t touched, FocusMotor_HomeCommand *command);
/* 普通 Glide 和寻零共用电流监测，只接受本次驱动开始后请求的 ADC 帧。 */
void FocusMotor_GlideMonitorStart(uint32_t now_ms);
uint8_t FocusMotor_GlideMonitorStopped(void);

#endif
