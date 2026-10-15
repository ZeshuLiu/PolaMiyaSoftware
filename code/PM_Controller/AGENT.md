# PM_Controller 工程约定

更新：2026-10-15。适用范围：`code/PM_Controller/`，PM_Controller Rev2.x，当前 PCB 为 Rev2.0。

## 开发要求

- 代码应适合人类阅读：名称表达用途，函数职责明确，注释说明时序、单位、生命周期和必要原因；避免无用封装及过度通用化。
- 优先使用 ESP-IDF 官方外设驱动及官方 `esp_lvgl_port`。项目代码负责板级配置、器件协议和业务逻辑。
- 不要过度防御编程：在外部数据入口和资源初始化处保留必要检查，内部按明确的调用契约工作，避免重复校验、固定 GPIO 操作逐次检查及没有实际需求的回退分支。保留通信错误、存储完整性、范围边界和并发保护。
- 尽量使用硬件外设实现通信、波形和计时，优先采用硬件 SPI、I²C、UART、PWM、RMT 等；只有硬件资源或器件时序确实不支持时才使用软件模拟，并说明原因。
- 参数存储保留通用设计和后续扩展能力：稳定 ID、可变长度字段、版本兼容、轮换记录与旧版导入不因代码精简而删除。
- 引脚集中在 `main/PIN.h`，数字是 ESP32 GPIO 编号，不是模组焊盘号。新增驱动从该文件取引脚。
- 驱动放在 `main/drivers/<模块>/`，应用放在 `main/app/<模块>/`；驱动与参数管理不直接修改 UI。
- 文档使用简洁中文。改变接口、参数或实现状态时，同步模块 README、工程 README 和[驱动实现状态](驱动实现状态.md)。状态文件只保留 Markdown 表格。
- 用户要求先给方案时只检查和提出方案；明确要求实现后再修改。器件确认以用户确认及当前硬件连接为准，不根据旧符号名覆盖已确认事实。

## 当前执行限制

- 用户已明确要求“不要测试”。未获得新的明确指示前，不新增、运行或扩展测试，也不运行编译器语法检查。
- 完整构建、重新配置、烧录和接板验证由用户手动执行；不主动运行 `idf.py reconfigure`、`build`、`flash`、`monitor`。
- `sdkconfig.defaults` 暂不整理，不创建或修改。本机 `sdkconfig` 不主动修改，不纳入 Git。
- 2026-10-07 用户要求清理本地测试：按键、参数存储的主机测试、桩及缓存已移除，保留固件内的屏幕测试。不要根据历史记录自动恢复测试代码或执行检查。
- 可以读取代码、文档、硬件资料及用户提供的日志。没有实测记录时，明确写“实机未验证”，不把源码检查等同于运行或硬件验证。

## 平台与目录

| 项目 | 当前约定 |
| --- | --- |
| 主控 | ESP32-S3-WROOM-1-N8R8；Rev2 主控不含 STM32 |
| SDK | ESP-IDF 6.1 |
| 存储 | 8 MB Flash；8 MB Octal PSRAM |
| 图形依赖 | `espressif/esp_lvgl_port 2.9.0`、`lvgl/lvgl 9.3.0`，由 `main/idf_component.yml` 固定 |
| 入口 | `main/main.c`；源文件、包含目录及依赖在 `main/CMakeLists.txt` 注册 |
| 历史固件 | `../../archive/PM_Controller_Rev1_x/`，冻结保存，仅作参考 |
| 对焦固件 | `../FocusUnit/`，独立工程，不属于本主控修改范围 |
| 硬件 | `../../../PolaMiyaHardware/电路/主控器/PM_Controller/` |
| 生成内容 | `build/`、`managed_components/`、`.cache/`，不手工修改或纳入业务代码 |

启动顺序：

```text
flash_trigger_init → shutter_init → hc165_init → sdm50_init / sdm50_start → settings_init
→ lcd_st7789_init → display_init → lcd_test_start
```

闪光、按键和显示初始化失败停止启动。参数存储失败记录错误并继续显示启动；参数锁成功分配后，器件错误时仍可使用 RAM 默认值，持久化提交被禁止。

## 模块接口与约束

| 模块 | 位置 / 接口 | 约束 |
| --- | --- | --- |
| 屏幕板级初始化 | [drivers/st7789](main/drivers/st7789/)；`lcd_st7789_init()` | 官方 ST7789 驱动，SPI2、20 MHz、RGB565；只负责初始化、句柄和背光 |
| LVGL 接入 | [app/display](main/app/display/)；`display_init()` | 官方移植层管理任务、时钟、双缓冲、刷新、字节交换及旋转；不要另外注册 DMA 完成回调 |
| 屏幕测试 | [app/lcd_test](main/app/lcd_test/)；`lcd_test_start()` | 全部用 LVGL 对象绘制；12 组方向 / 偏移；业务界面尚未接入 |
| 按键 | [drivers/hc165](main/drivers/hc165/README.md)；`hc165_get_events()`、`hc165_take_events()` | 6 键独立消抖和长短按，两组事件掩码保持到读取并清除 |
| EEPROM | [drivers/eeprom](main/drivers/eeprom/README.md)；`eeprom_read()`、`eeprom_write()` | 用户已确认 24C64；官方 I²C、分页、ACK 轮询、错误返回及访问互斥 |
| 参数管理 | [app/settings](main/app/settings/README.md)；`settings_set()`、`settings_increment()`、`settings_commit()` | RAM 修改与保存分开，稳定 ID、小端编码、CRC 和轮换记录 |
| 闪光输出 | [drivers/flash_trigger](main/drivers/flash_trigger/README.md)；`flash_trigger_fire()` | GPIO38 直接拉高，GPTimer 中断拉低；支持任务及普通 GPIO ISR；20 ms 目标脉宽、至少 1 ms 低电平恢复，忙时拒绝、不排队 |

### LVGL 与背光

- 逻辑尺寸 172×320；旋转和偏移尚未接屏定型，34 像素是居中裁切候选值。
- LVGL 调用使用 `lvgl_port_lock()` / `lvgl_port_unlock()`；官方 LVGL 定时器回调已持有该锁。
- 两个内部 DMA 缓冲区各 5,120 像素，SPI 最大传输长度与之匹配；不要直接改成 PSRAM DMA 缓冲而不核对平台限制。
- 测试切换方向 / 偏移前通过 LVGL 清空整个 240×320 控制器 RAM；同步刷新后等待最后一批传输结束再改变窗口或点亮背光。
- GPIO9 背光低电平点亮。开关已实现，PWM 和渐变尚未实现。

### 74HC165

- GPIO39=/PL、GPIO40=CP、GPIO41=Q7。使用 SPI3、Mode 2、500 kHz、MSB first，每次接收 8 bit，使用 SPI_DMA_CH_AUTO 启用 DMA，不占用屏幕 SPI2。接收缓冲为内部 RAM 中 4 字节对齐的静态 4 字节数组，实际仅传输 1 字节；轮询等待完成后再读取。GPIO39 单独控制并行装载，不作为普通 SPI 片选。
- 在 SPI 传输前回调中产生 /PL 低脉冲，确保 SPI 模式配置完成后再装载；轮询传输由扫描任务独占，回调不阻塞、不调用业务逻辑。
- CP 空闲高电平，下降后读 Q7，上升沿移位。芯片依次输出 D7–D0，返回字节 bit0 对应 D0。
- D0–D5 是 KEY1–KEY6，低电平按下；D6/D7 是 CHG/STB，不参与长短按识别。
- 消抖 20 ms；按住达到 1 s 置一次长按位，未达到 1 s 松开置短按位；长按松开不再置短按位。
- 启动时已按住的键先释放，再接受新事件。bit0–5 分别对应 KEY1–KEY6；未清除的同类重复事件会合并，不计次数。
- `hc165_get_events()` 只读，`hc165_take_events()` 原子读取并清除，由一个业务消费者使用。CHG/STB 目前只提供采样状态，未做组合判定。
- FreeRTOS tick 配置为 1 kHz（CONFIG_FREERTOS_HZ=1000），1 tick = 1 ms；按键扫描周期 5 ms（5 ticks），EEPROM 忙等待延时 1 ms（1 tick）。若后续降低 tick 频率，需重新核对这些延时的取整结果。长按用毫秒时间戳，不按扫描次数计时。

### 24C64 与参数

- U12 已按用户确认的 24C64 实现：8 KB、32 字节页、双字节地址、器件地址 0x50；SCL=GPIO15、SDA=GPIO16，100 kHz，WP 接地。
- 原理图仍使用 AT24CS16 符号。该旧符号不代表当前驱动应使用 AT24CS16 的单字节 / 块地址协议。
- `settings_init()` 负责器件初始化与加载；启动只读，不自动格式化、递增计数或导入旧数据。
- 当前参数为重启次数、快门次数、电机运行毫秒数及背光百分比；均为 uint64 字段。背光参数尚未应用到 PWM，校准字段尚未定义。
- 新参数在 `settings.h` 中增加稳定 ID 和 RAM 字段，并在 `settings_record.c` 的 `parameters[]` 增加默认值、范围及计数属性。已有 ID 不重排、不复用，不直接持久化 C 结构体。
- `0x0000–0x00FF` 保留；`0x0100–0x1FFF` 为 31 个 256 字节记录，提交标记独占最后一个物理页。不要绕过参数服务改写该区域。
- 写入先使目标标记失效，再写并校验内容，最后提交；未知版本 / 字段禁止覆盖。扫描失败禁止提交。
- 业务完成或用户确认保存时调用 `settings_commit()`；相同值不重复写。失败重试只重复提交，不重复累计增量。
- `settings_import_rev1()` 仅显式导入当前 EEPROM 中 `0x20` 的旧版 v2、13 字节布局，导入后仍须显式提交，不覆盖原字节。

### 闪光输出

- GPIO38 高电平经 R25 驱动 TLP291，默认低电平；初始化不触发闪光。
- 默认高电平 20 ms，依据旧版两个 10 ms 主循环处理阶段选取；指定脉宽接口支持 1–30 ms，单位 µs。其后低电平恢复 1 ms，随后保持低电平。
- GPIO42 下降沿 ISR 直接调用 flash_trigger_fire()，检查状态后先拉高 GPIO38，再设置并启动 GPTimer；结束 ISR 拉低并停止计数。高电平允许包含定时器启动和中断延迟；恢复期从实际拉低后计时。
- 成功返回表示 GPIO 已拉高且定时器已启动，不代表灯已实际发光。GPIO42 原始下降沿已联动；当前不做额外消抖、释放后武装或回充判定，忙时丢弃边沿；回充和统计业务尚未接入。
- 当前 GPIO 中断和 GPTimer 控制调用链不支持 cache 关闭期间执行；不承诺绝对零延迟。不得仅给入口加 IRAM_ATTR 就宣称全链路 IRAM 安全。

## 任务与 ISR

- SDM50 独占 UART1（GPIO4/5）与 UHCI，460800、8N1，使用官方 UHCI DMA 收发。DMA ISR 直接解析并更新快照，同核任务只负责 EOF 后重新挂载；不要同时安装普通 UART 接收驱动。公开接口仅任务上下文；启停等待应答，UI/对焦只消费 VALID 快照。详见 `main/drivers/sdm50/README.md`；用户于 2026-10-15 确认编译通过；未运行测试、未上板验证。

- 参数、EEPROM、按键事件读取用于任务上下文；闪光触发及忙状态查询支持任务和普通 GPIO ISR，初始化仅在任务中执行。
- 快门 ISR 直接调用 flash_trigger_fire() 以减少启动延迟；不要在 ISR 中调用 EEPROM、参数提交、LVGL、打印日志或等待任务。
- 保持现有临界区和互斥锁语义；不要直接公开可被外部随意清零的共享标志变量。

## 配置、分区与修改范围

- GPIO35/36/37 由 N8R8 Octal PSRAM 占用；GPIO19/20 保留给原生 USB。GPIO39–42 已用于板上功能，调试使用 USB Serial/JTAG。
- GPIO0 是 BOOT；GPIO3 同时是电池 ADC 与 JTAG 相关启动配置脚，不随意修改相关 eFuse。
- `partitions.csv` 是 8 MB Flash 布局：两个 3 MB OTA 槽位于 `0x20000` 和 `0x320000`，`0x620000` 起暂未分配。OTA 业务尚未实现。本机 sdkconfig 的 Flash 容量仍须用户手动改为 8 MB。
- `dependencies.lock` 由组件管理器生成；依赖版本变更同步清单与文档，不手工改写托管组件源码。
- 保留用户已有改动和其它并行工作，不修改独立 FocusUnit、历史归档或硬件工程，除非当前请求明确包含。
- 未明确要求时不主动提交 Git 或推送。请求提交时，只暂存授权范围，并先更新受影响文档。

## 验证边界与后续工作

- LVGL 接入曾由 Astra（中）做源码审查，未发现可证实的 P1/P2 缺陷；该结论仅对应当时的 LVGL 改动，不代表整工程审查。
- 按键 14 项、参数存储 25 项主机检查在停止测试指令之前完成，记录见各模块 README。新增闪光测试已移除；不自动恢复测试或语法检查。
- 2026-10-15 用户确认当前版本可编译，尚未上板验证。屏幕、GPIO 时序、光耦、电机、ADC、参数断电恢复和跨任务行为均以实测为准。
- 后续模块及状态统一维护[驱动实现状态](驱动实现状态.md)，不要仅凭旧版代码判定新硬件已实现或已验证。
