# 闪光触发输出

GPIO38 高电平经 R25 驱动 TLP291，默认低电平。`flash_trigger_init()` 在开机时配置 GPIO 并创建、启用 GPTimer；初始化不触发闪光。

## 触发与结束

GPIO42 快门触点接地时产生下降沿，由 `app/shutter` 的 GPIO ISR 直接调用 `flash_trigger_fire()`。接口检查忙状态后立即拉高 GPIO38，再设置并启动 1 MHz GPTimer；不创建任务、不发送任务通知、不使用 RMT。定时器报警 ISR 拉低 GPIO38 并停止计数。

默认高电平目标为 20 ms，支持 1–30 ms。高电平实际时长包含定时器配置、启动和结束中断响应延迟，允许稍有延长。拉低后留出至少 1 ms 恢复期，以实际拉低后的时间戳判定；脉冲或恢复期内的请求直接拒绝，不排队、不延长当前脉冲。

GPIO ISR 直接输出减少任务调度延迟，但不保证外部边沿到输出为零延迟。当前 GPIO ISR 未标记 IRAM 安全，GPIO/GPTimer 控制函数的 IRAM 配置也未开启；关中断、较高优先级 ISR 或 Flash cache 暂停仍可能延迟开始或结束。未修改本机 sdkconfig。接口依据本机 ESP-IDF 6.1 头文件和实现核对，参考 [GPTimer 官方文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/gptimer.html)。

## 接口

| 接口 | 内容 |
| --- | --- |
| `flash_trigger_init()` | 任务上下文初始化，先于快门中断和其他消费者 |
| `flash_trigger_fire()` | 在任务或普通 GPIO ISR 中触发默认 20 ms 脉冲 |
| `flash_trigger_fire_us(pulse_us)` | 同上，指定 1000–30000 µs |
| `flash_trigger_is_busy()` | 任务或 ISR 中查询脉冲及恢复期状态 |

成功返回表示 GPIO 已拉高且结束定时器已启动，不代表外部闪光灯已实际发光。定时器设置或启动失败会立即拉低并返回错误；可能已经产生一个短脉冲，不能视为完全未触发。共享状态由支持任务和 ISR 的临界区保护。结束 ISR 只拉低、停止计数、更新时间与忙状态，不处理 UI、统计或持久化。

快门 GPIO ISR 忽略忙状态及其他返回错误，不在中断中打印日志。当前按原始下降沿触发，没有额外消抖、释放后重新武装或回充判定；恢复期结束后的新下降沿仍可能再次触发，长于该窗口的触点抖动也可能重触发。快门计数和回充节拍尚未接入。

## 验证状态

2026-10-15：由 RMT 改为 GPIO 直接触发、GPTimer 中断结束，并接入 GPIO42 下降沿。按工程约定未测试、未运行语法检查或构建，实机未验证。需测量快门边沿至 GPIO38 上升沿延迟、高电平时长、触点抖动与光耦输出。
