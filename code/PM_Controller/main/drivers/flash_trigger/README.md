# 闪光触发输出

PM_Controller Rev2.0：GPIO38 高电平通过 R25 驱动 U16 TLP291。默认低电平，R24 提供 10 kΩ 下拉。`flash_trigger_init()` 已加入启动流程，初始化不发送脉冲。

## 脉宽与实现

默认高电平保持 20 ms，再保持 1 ms 低电平恢复期，之后持续低电平待机。使用官方 RMT TX，1 MHz 分辨率、一个 copy-encoder 符号，不依赖主循环或 RTOS 延时拉低输出。恢复期用于给光耦关断留出余量。

旧版在 [gpio.c](../../../../../archive/PM_Controller_Rev1_x/MainController/Core/Src/gpio.c) 的快门中断中拉高输出，再在 [main.c](../../../../../archive/PM_Controller_Rev1_x/MainController/Core/Src/main.c) 的 10 ms 处理分支中分两次处理后拉低，正常约 10–20 ms，主循环阻塞时可能延长。20 ms 是据此选取的初始值，尚未针对实料和目标闪光灯实测定型。

[TLP291 手册](https://toshiba.semicon-storage.com/info/TLP291_datasheet_en_20190527.pdf?did=12884&prodName=TLP291) 在指定负载下给出的典型开关时间为微秒量级，不能据此推定全部闪光灯的最小触发脉宽。高电平保持时间是触发接点的导通时间，实际闪光持续时间由闪光灯决定。RMT 接口参考 [ESP-IDF 官方文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/rmt.html)。

## 接口

| 接口 | 内容 |
| --- | --- |
| `flash_trigger_init()` | 创建 RMT 通道、编码器和完成回调；重复调用复用已有资源 |
| `flash_trigger_fire()` | 请求一个 20 ms 高电平脉冲，不等待发送完成 |
| `flash_trigger_fire_us(pulse_us)` | 指定 1–30 ms 高电平，单位 µs；结束后保留 1 ms 低电平恢复期 |
| `flash_trigger_is_busy()` | 提交调用未结束、脉冲未完成或恢复期未结束时返回 true |

所有外部接口用于任务上下文，初始化应先于消费者。成功返回表示请求已提交，不代表外部闪光灯已实际闪光。未初始化或忙时返回 `ESP_ERR_INVALID_STATE`，非法脉宽返回 `ESP_ERR_INVALID_ARG`，底层错误原样返回；不排队、不延长当前脉冲。发送错误会释放忙状态，初始化错误释放资源并使引脚下拉待机。

RMT 异步使用的符号和完成回调状态保存在内部 RAM。忙状态及提交状态由临界区保护，避免完成中断先于提交函数返回时出现交错调用。完成回调只清除忙状态，不调用 UI 或持久化接口。

```c
#include "flash_trigger.h"

/* 初始化已由 app_main() 完成，业务事件需要闪光时调用。 */
esp_err_t result = flash_trigger_fire();
/* 调用者检查 result；忙时由业务层决定忽略还是稍后重试。 */
```

当前仅实现输出，不接入 GPIO42 快门输入，不自动触发或增加快门计数。快门消抖、闪光回充节拍和触发时刻由业务层安排；旧版的 1 s 快门间隔限制未放入输出驱动。

## 验证状态

用户要求停止测试后，已停止后续测试和语法检查，并移除本轮新增的闪光测试文件。此前完成的检查不替代实机验证。GPIO 实际波形、光耦关断和目标闪光灯兼容性均待接板确认；未运行完整固件构建或触发实际闪光灯。
