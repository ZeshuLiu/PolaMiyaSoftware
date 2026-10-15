#ifndef PM_SDM50_H
#define PM_SDM50_H

// #include <stdbool.h>
// #include <stdint.h>
#include "esp_err.h"

typedef enum {
    SDM50_WAITING,
    SDM50_VALID,
    SDM50_NO_TARGET,
    SDM50_STALE,
    SDM50_STOPPED,
    SDM50_RX_ERROR,
} sdm50_status_t;

typedef struct {
    uint16_t distance_mm;       /* 仅 status == SDM50_VALID 时用于对焦。 */
    sdm50_status_t status;
    int64_t timestamp_us;        /* 最近一次合法测距帧的时间。 */
    uint32_t sequence;           /* 合法测距帧计数，包含无目标帧。 */
    uint32_t checksum_errors;
    uint32_t rx_errors;
} sdm50_sample_t;

/* 开机调用一次；独占 UART1/UHCI。初始化成功不代表模块已响应。 */
esp_err_t sdm50_init(void);
/* 任务上下文，串行发送 DMA 命令并等待应答；超时不代表命令未执行。 */
esp_err_t sdm50_start(void);
esp_err_t sdm50_stop(void);
/* 任务上下文读取一致快照；100 ms 无新测距帧则返回 STALE。 */
esp_err_t sdm50_get_sample(sdm50_sample_t *sample);

#endif
