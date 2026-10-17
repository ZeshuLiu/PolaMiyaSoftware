# ADC 采样

主控 ESP32-S3 的两路 ADC1：

| 通道 | PIN.h | ADC | 电路 |
| --- | --- | --- | --- |
| 电池 | PIN_ADC_BAT / GPIO3 | ADC1 CH2 | U17 切换后的电池电压，经 R39=100 kΩ、R40=100 kΩ 分压，ADC_BAT 为电池电压的 1/2 |
| 电机 | PIN_ADC_MT / GPIO8 | ADC1 CH7 | DRV8251A IPROPI 经 R12=0 Ω 接 ADC_MT，R2=1 kΩ 接地 |

依据硬件仓库 `电路/主控器/PM_Controller/production/Rev_2_0/netlist.ipc` 与同目录 BOM 核对。C8、C32 在 BOM 中仍为 TBD(1uf)，R12 为 0 Ω；实际滤波响应由装配值决定。

## 采样与接口

- 使用本机 ESP-IDF 6.1 官方 `esp_adc` 连续 ADC 驱动和 GDMA，独占 ADC1 连续模式及一个 GDMA RX 通道，不同时安装 ADC1 oneshot 驱动。
- 按用户要求采用 SDK 最低连续采样率 `SOC_ADC_SAMPLE_FREQ_THRES_LOW`。ESP32-S3 本机定义为总计 611 Hz，两路交替约各 305.5 Hz；这是配置目标，实际时钟分频及采样时序待实测。
- 12 bit、12 dB 衰减，128 字节 DMA 帧、2048 字节内部结果池。读取任务优先级 4，栈 4096 字节。无逐样本日志，不在 ISR 解析数据或操作 UI。
- 每路累计至少 31 个有效样本，先求原始码均值，再通过该通道官方 curve-fitting 校准转换为 mV。正常交替采样时约每 101.5 ms 生成一组快照；DMA 帧传递和任务调度会影响实际发布时间。
- 同时提供电压均值与 1/8 IIR 电压，首次有效结果直接作为滤波初值，避免从零缓慢爬升。电池图标使用平滑值，电机电流使用本窗口均值。当前不是高速峰值、过流或堵转保护。

`adc_measure_init()` 在启动时调用一次，配置通道、校准和任务；校准 eFuse 不可用时保留原始码，电压字段不可用，禁止用“raw/4095×3.3V”代替校准。其它初始化错误返回并清理资源，主程序记录警告后继续显示。

`adc_measure_get()` 在任务上下文读取一致快照，包含原始码、mV、平滑 mV、有效/校准/饱和标志、序号、发布时间、错误和溢出次数。未产生快照、读数超时、解析异常、超过 300 ms 没有新快照时，不将旧结果标为有效。

任一采样达到 4095 时，该通道本窗口标记饱和、不参与电量或电流换算，并重置滤波初值。数据池溢出由 ISR 只计数；读取任务停止、清池、丢弃半个窗口并重新启动。恢复失败保留错误并挂起该采样任务，需重启恢复。当前未开启 `CONFIG_ADC_CONTINUOUS_ISR_IRAM_SAFE`，未修改 sdkconfig，不承诺 cache 关闭期间无丢样。

## 验证边界

仅核对本机 SDK 头文件和官方驱动源码，未测试、未编译、未烧录，实机未验证。待确认 ADC 实际频率、精度/噪声、任务栈余量、U17 导通误差及满电输入行为、C8/C32 实际滤波和电机 PWM 对 IPROPI 的影响。未改变硬件或独立 FocusUnit 固件。

官方接口：[ESP-IDF ADC 指南](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/adc/index.html)。

2026-10-18 后续验证：用户确认当前版本“测试 OK”。上述未验证记录为实现阶段状态；未收到 ADC 频率、噪声或精度的量化记录，本轮未代为运行额外测试。
