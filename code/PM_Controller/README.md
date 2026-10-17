# PM_Controller 固件

2026-10-18：用户确认当前竖屏 UI、三键映射和 ADC 接入版本“测试 OK”，据此保存本地 Git 版本。本轮未代为构建、测试或烧录；ADC 实际采样率、计量精度和其它专项指标未提供量化记录。具体按键功能统一在 [ui.c 的 handle_keys()](main/app/ui/ui.c) 中实现，扫描识别与图标显示分别由 HC165 驱动和 UI 视图负责。

同日后续：用户报告距离主屏 IDLE1 看门狗，补充堆栈停在 `lv_draw_sw_line()`。本轮已减少无变化数码段/样式重绘和隐藏页更新、取消 UI 过期周期补跑，并增加慢帧日志；没有调整看门狗或 SDK 配置。用户已复测并确认 OK；本轮未代为编译、测试或烧录，详见 [显示排查记录](main/app/display/README.md)。

对应 PCB：PM_Controller Rev2.x，当前为 Rev2.0。模组：ESP32-S3-WROOM-1-N8R8，8 MB Flash、8 MB Octal PSRAM。开发环境为 ESP-IDF 6.1。当前 FreeRTOS tick 为 1 kHz（`CONFIG_FREERTOS_HZ=1000`）；按键扫描目标周期 5 ms，EEPROM 忙等待延时为 1 tick。消抖、长按、LVGL 定时与 GPTimer 目标脉宽仍按各自的时间单位配置。

`app_main()` 将闪光输出初始化为低电平，接入 GPIO42 快门下降沿中断，启动 74HC165 按键扫描，加载 24C64 参数，初始化官方 ST7789 驱动和 LVGL，并启动三页竖屏 UI。KEY1–KEY3 用于页面导航、背光调节和显式保存；KEY4–KEY6 暂未接入业务。主程序每 1 s 打印测距及 ADC/电源快照。参数修改与持久化提交分开。引脚和双 OTA 分区已整理，电机、蓝牙、Wi-Fi 和 OTA 业务尚未接入。

2026-10-07：按用户要求移除本地主机测试、桩及测试缓存，保留固件内的屏幕方向与偏移测试。下文主机测试结果为清理前的验证记录。

2026-10-15：参数加载合并重复校验，编码使用入口已验证的 RAM 值，三阶段保存共用写入回读函数；保留稳定 ID、版本兼容、轮换记录和旧版导入。按键扫描与背光开关移除固定 GPIO 操作的逐次错误检查，初始化检查保留。HC165 改为 SPI3、Mode 2、500 kHz 硬件读取并启用 DMA，GPIO 单独控制 /PL，按键事件规则不变。本轮用户确认编译通过；未运行测试，未上板验证。

2026-10-15 提交状态：用户确认当前固件可编译，未上板验证；本轮未代为构建或运行测试。`.clangd` 保存编辑器参数过滤和 Picolibc 头文件路径；本机 `.vscode/settings.json` 按现有规则忽略，其中使用 Espressif clangd 并关闭重复 IntelliSense。工具链升级或换机后需调整路径。

2026-10-17：新增启动阶段、快门 GPIO 初始化及按键日志。用户上板确认固件可以运行，按键日志正常，硬件初步工作；不代表各模块已完成验收。本机 IPC 任务栈为 4096 字节，主任务栈为 8192 字节，按键扫描任务栈为 4096 字节；前两项保存在未跟踪的 `sdkconfig` 中，换环境需手动核对。本轮未代为构建或运行测试。

## 工程文件

2026-10-15：新增 [SDM50 测距驱动](main/drivers/sdm50/README.md)，UART1（GPIO4/5）、460800 baud、UHCI DMA 收发。DMA 中断直接解析并更新距离，同核任务负责 EOF 后重新挂载；提供启停应答和带时间戳的结果快照。开机初始化并尝试开启测距，失败记录警告后继续启动。用户确认编译通过；未运行测试，未上板验证。

| 文件 | 用途 |
| --- | --- |
| [AGENT.md](AGENT.md) | 开发约定、接口约束、执行限制及验证边界 |
| [main/main.c](main/main.c) | 程序入口 |
| [main/PIN.h](main/PIN.h) | PCB 引脚定义，只记录连接，不初始化 GPIO |
| [main/drivers/st7789/](main/drivers/st7789/) | SPI、官方 ST7789 驱动初始化和背光 |
| [main/drivers/hc165/](main/drivers/hc165/README.md) | 74HC165 读取、6 个按键消抖及长短按标志位 |
| [main/drivers/eeprom/](main/drivers/eeprom/README.md) | 官方 I²C、24C64 分页读写及写完成轮询 |
| [main/drivers/flash_trigger/](main/drivers/flash_trigger/README.md) | GPIO 直接触发、GPTimer 中断结束、忙状态及默认低电平 |
| [main/drivers/sdm50/](main/drivers/sdm50/README.md) | UART1/UHCI DMA 测距、中断更新结果和启停应答 |
| [main/drivers/adc_measure/](main/drivers/adc_measure/README.md) | ADC1 最低速连续 DMA、两路校准与滤波快照 |
| [main/app/shutter/](main/app/shutter/) | GPIO42 快门下降沿 ISR 直接触发闪光 |
| [main/app/settings/](main/app/settings/README.md) | 参数定义、RAM 缓存、CRC、轮换保存与旧版导入 |
| [main/app/display/](main/app/display/) | 官方 esp_lvgl_port 初始化和显示配置 |
| [main/app/ui/](main/app/ui/README.md) | CASE 7 竖屏 UI、测距曲线、KEY1–3 菜单及背光设置 |
| [main/app/focus_distance/](main/app/focus_distance/README.md) | 对焦物距获取接口；当前借用实测距离，等待 FocusUnit 通信和位置标定 |
| [main/app/power_monitor/](main/app/power_monitor/README.md) | 电池电压、单节锂电池电量估算和 IPROPI 电流换算 |
| [main/app/lcd_test/](main/app/lcd_test/) | LVGL 测试控件和定时切换 |
| [main/idf_component.yml](main/idf_component.yml) | 固定官方移植组件和 LVGL 版本 |
| [partitions.csv](partitions.csv) | 8 MB Flash 自定义分区表 |
| CMakeLists.txt、main/CMakeLists.txt | ESP-IDF 构建配置 |
| sdkconfig | 本机配置，当前不纳入 Git；sdkconfig.defaults 后续再整理 |

测距边界显示：合法返回值小于 50 mm 时实测显示 NEAR，下指针停在零端点；NO_TARGET（50000 及以上返回值）按约定显示 INF，指针移到无穷远。20～49.999 m 仍显示真实数值，只把指针限在 20 m。过期与通信错误仍显示短横线；FocusUnit 接口不增加 NEAR 状态。主屏移除距离状态文字行，BAT 改为电池图标，电量数据不可用时显示 `?`。此次改动未编译、未上板验证。

## 引脚

下表按当前 [PCB](../../../PolaMiyaHardware/电路/主控器/PM_Controller/PM_Controller.kicad_pcb) 的 U1 焊盘网络核对。数字是 ESP32 GPIO 编号，不是模组焊盘号。

| PIN.h 定义 | GPIO | PCB 网络 / 连接 |
| --- | --- | --- |
| PIN_MOTORPWM_A | 1 | MotorPWM_A，DRV8251A IN1 |
| PIN_MOTOR_IN2 | 2 | MotorIN2，DRV8251A IN2 |
| PIN_ADC_MT | 8 | ADC_MT，电机电流采样 |
| PIN_ADC_BAT | 3 | ADC_BAT，电池电压采样 |
| PIN_USBPWR_DET | 21 | USBPWR_DET，USB 供电检测 |
| PIN_ESP_FILM_TRG | 6 | ESP_FILM_TRG，吐片事件锁存读取，经 Q1 反相 |
| PIN_ESP_FILM_CLR | 7 | ESP_FILM_CLR，经 Q2 控制锁存恢复 |
| PIN_LCD_BLK | 9 | LCD_BLK，背光控制，低电平点亮 |
| PIN_LCD_RES | 10 | LCD_RES，屏幕复位 |
| PIN_LCD_DC | 11 | LCD_DC，数据 / 命令 |
| PIN_LCD_MOSI | 12 | LCD_MOSI，屏幕 SPI 数据 |
| PIN_LCD_CLK | 13 | LCD_CLK，屏幕 SPI 时钟 |
| PIN_LCD_CS | 14 | LCD_CS，屏幕片选 |
| PIN_ESP_SCL | 15 | ESP_SCL，EEPROM I2C 时钟 |
| PIN_ESP_SDA | 16 | ESP_SDA，EEPROM I2C 数据 |
| PIN_ESP_TXD0 | 43 | 经 R47 接 ESP_TXD0，J1.2 |
| PIN_ESP_RXD0 | 44 | ESP_RXD0，J1.3 |
| PIN_ESP_TXD1 | 4 | 经 R49 接 ESP_TXD1，U9.2 |
| PIN_ESP_RXD1 | 5 | ESP_RXD1，U9.3 |
| PIN_ESP_TXD2 | 18 | 经 R48 接 ESP_TXD2，U15.2 |
| PIN_ESP_RXD2 | 17 | ESP_RXD2，U15.3 |
| PIN_MASPI_CS | 39 | MASPI_CS，74HC165 的 /PL，低电平装载 |
| PIN_MASPI_CLK | 40 | MASPI_CLK，74HC165 时钟 |
| PIN_MASPI_MISO | 41 | MASPI_MISO，74HC165 Q7 |
| PIN_FLASH_TRG | 38 | FLASH_TRG，闪光同步光耦 |
| PIN_SHUT_TRG | 42 | SHUT_TRG，快门接口 H2.2 |
| PIN_USB_DM | 19 | USB D−，经 R28 接 USB 接口 |
| PIN_USB_DP | 20 | USB D+，经 R29 接 USB 接口 |
| PIN_ESP_BOOT | 0 | ESP_BOOT，BOOT 按键及启动配置 |

74HC165 的并行输入 D0–D7 依次为 KEY1、KEY2、KEY3、KEY4、KEY5、KEY6、BAT_CHG_OD、BAT_STB_OD。这些信号不直接占用 MCU GPIO。MASPI_CS 实际是 /PL，不能按普通 SPI 片选处理。

74HC165 驱动已接入启动流程，使用 SPI3（Mode 2、500 kHz、8 bit、DMA 接收、轮询等待完成），与屏幕 SPI2 独立。6 个按键采用 20 ms 消抖，短按在松开时触发，长按在持续达到 1 s 时触发一次；事件通过 `hc165_get_events()` 或 `hc165_take_events()` 读取。接口与使用示例见[驱动说明](main/drivers/hc165/README.md)，各驱动进度见[驱动实现状态](驱动实现状态.md)。

- GPIO35/36/37 由 N8R8 的 PSRAM 占用，不能用于外设。
- GPIO45/46/47/48 在本 PCB 上未接。GPIO45/46 是启动配置脚，不作为普通空闲引脚使用。
- GPIO0 是 BOOT 启动配置脚；GPIO3 同时影响 JTAG 选择。使用电池 ADC 时不要随意改变相关 eFuse。
- GPIO39–42 已用于板上功能，调试使用 USB Serial/JTAG。GPIO19/20 使用 USB 时不能另作 GPIO。
- ESP_EN 是模组复位输入，不是 MCU GPIO，没有放入 PIN.h。

## 显示屏

2026-10-18：右侧按上 KEY3、中 KEY2、下 KEY1 显示图标，随浏览/编辑切换；16 px 提示区和淡化渐变背景代替文字按键提示，顶部区域收至 30 px，电池主体 19×10 px。按键统一为 KEY3 上/右/增加、KEY1 下/左/减少、KEY2 确认/执行；图标高度为示意位置，字体、渐变与实物对齐待用户上板确认，本轮未编译、未测试、未烧录。

ADC 已接入：GPIO3 电池电压、GPIO8 电机 IPROPI，使用官方连续 DMA 和 curve-fitting 校准，按 SDK 最低总采样率 611 Hz、两路交替约各 305.5 Hz；每路 31 样本生成约 100 ms 快照。主屏图标与 INFO 电压/估算电量、电机 mA/IPROPI mV 已接实际 ADC 快照；百分比按用户确认的单节 4.2 V 锂电池通用电压节点估算，未做电芯标定。未实现低压关机、过流或堵转保护。初始化失败时保留界面并显示未知。实现及误差边界见 [电源读数](main/app/power_monitor/README.md)，本轮未编译、未测试、未烧录。

- 驱动芯片：ST7789。
- 屏幕原始尺寸：长 320、高 172 像素。
- 安装与显示方向：竖屏，显示坐标按宽 172、高 320 像素使用。
- 接口：SPI，使用 PIN.h 中的 LCD_MOSI、LCD_CLK、LCD_CS、LCD_DC、LCD_RES 和 LCD_BLK 引脚。
- 背光低电平点亮，使用 LEDC 低速 Timer 0 / Channel 0、20 kHz、10 bit PWM。开启时约 300 ms 渐亮至设置亮度，关闭时停止渐变并立即熄灭；UI 切页不重新渐亮。设置页修改 RAM 亮度并立即应用，长按 KEY2 保存。显示方向与窗口固定为 CASE 7（ROT 180、X34/Y0）。本次背光百分比接入未上板验证。

### LVGL 接入

依赖固定为 `espressif/esp_lvgl_port 2.9.0` 和 `lvgl/lvgl 9.3.0`，与本机 ESP-IDF SPI 屏幕示例及 GUI Guider 的 LVGL 主次版本一致。组件管理器在配置工程时下载依赖，不复制旧工程的 LVGL 源码。

调用顺序：`app_main()` → `flash_trigger_init()` → `shutter_init()` → `hc165_init()` → `sdm50_init()` / `sdm50_start()` → `settings_init()` → `adc_measure_init()` → `lcd_st7789_init()` → `display_init()` → `ui_start()`。闪光输出、按键和显示初始化失败通过 `ESP_ERROR_CHECK` 停止启动；测距初始化或开启失败、参数存储失败、ADC 初始化失败记录警告并继续显示启动。参数锁成功分配后，器件错误时仍可使用 RAM 默认值，持久化提交被禁止。初始化接口只在开机时调用一次，不提供运行中销毁或重复初始化接口。

使用 ESP-IDF 官方 `esp_lcd_new_panel_st7789()`，SPI2、模式 0、40 MHz、RGB565。板级文件只配置引脚、SPI 和背光，直接输出官方 `panel`、`io` 句柄。LVGL 任务、时钟、刷新、字节交换、DMA 完成回调和硬件旋转由官方移植组件管理。

LVGL 使用两个内部 DMA 绘图缓冲区，每个 172×64 像素、22,016 字节，共 44,032 字节（43 KiB），SPI 最大传输长度使用同一宏。buff_dma=true、buff_spiram=false，不启用 psram_dma_direct。使用局部刷新，帧率测试主动触发整屏刷新。操作 LVGL 时使用 lvgl_port_lock()/lvgl_port_unlock()；定时器回调已经持锁。

### 帧率与圆角测试

2026-10-17：启动入口已改为 [三页业务 UI](main/app/ui/README.md)，方向/偏移和完整显存清屏移至 `display_init()`；正式 UI 使用默认 LVGL 刷新定时器。以下屏幕测试源码仍保留，默认不启动，不与业务 UI 同时运行。新界面未编译、未测试、未烧录。

用户已确认 CASE 1、4、7、10 的方向/偏移正确。当前固定 CASE 7：ROT 180、172×320、X34/Y0；SPI 保持 40 MHz 和 DMA。

测试每 10 秒交替显示两页，启动先显示帧率页：

- 帧率页：红绿蓝背景上两条高对比移动条，显示帧号、FPS、平均帧间隔 AVG 和最大帧间隔 MAX。移动按实际时间计算。每帧强制刷新完整 172×320，调用 `lv_refr_now()` 后等待最后一批 SPI DMA 完成，再计为一帧；每约 1 秒更新屏幕统计并输出串口日志。
- 圆角页：显示距边缘 0/4/8/12/16 像素的白/红/黄/绿/青色矩形边框，四角 TL/TR/BL/BR 标记，以及品红色 R24 候选圆角边框。R24 只用于观察，不代表已测得屏幕圆角半径。对照可见边框选择内容安全边距。

FPS 为这套全屏画面的主控绘制、传输及调度综合吞吐，不是面板扫描频率，也不是小区域更新帧率。统计按相邻完整传输完成时间计算；切页构建和首帧不计入，返回帧率页时重新统计。圆角页静态显示，不计 FPS。

排查程序接管刷新调度，暂停默认刷新定时器，并在手动刷新及更新统计后再次暂停（LVGL 的失效请求会恢复它）；使用 1 ms LVGL 定时器驱动页面，不额外替换官方 DMA 完成回调。仅在启动时清空 240×320 显存和开启背光，页面切换不重新渐亮。业务界面接入时应移除此专用调度或恢复默认刷新定时器。

本轮按用户要求未编译、未烧录，帧率数值和圆角显示效果待实机确认；现有 build 固件不包含此次页面修改。

### 当前验证状态

PCB、PIN.h 和文档中的 29 个 GPIO 对应一致；分区表已通过 ESP-IDF 分区工具校验。

2026-10-07：4 个应用源文件、官方移植层的 LVGL 任务和显示源文件，以及固定 ROT 270 / GAP 68 的测试变体，使用本机 ESP-IDF 6.1 交叉编译器、官方发布头文件和 LVGL Kconfig 默认值通过 `-fsyntax-only` 检查。文档链接和 `git diff --check` 通过。Astra（中）对照实际组件源码完成只读审查，未发现可证实的 P1/P2 缺陷。

本次未运行完整固件构建，未修改本机 sdkconfig，未创建 sdkconfig.defaults。以上为历史验证范围。2026-10-17 用户已确认 CASE 1/4/7/10，选定 CASE 7；新设 40 MHz 的稳定性尚待上板确认。

2026-10-07：74HC165 驱动、按键识别逻辑及应用入口通过 ESP-IDF 6.1 交叉编译器语法检查。14 项主机测试执行实际 C 驱动，覆盖全部 256 种输入位序、消抖、1 s 边界、多键操作、事件保持与清除、启动时按住、充电输入隔离和时间戳回绕。真实 RTOS 调度、电气时序和按键手感尚未接板验证。

2026-10-07：U12 按用户确认的 24C64 实现，8 KB、32 字节页、双字节地址、器件地址 0x50。EEPROM 和参数服务通过语法检查及 25 项主机测试，覆盖分页、忙等待、CRC、轮换、中断写入、未知版本保护和旧版导入。采用稳定参数 ID 与明确的小端编码，64 位统计量和背光参数缓存于 RAM，外部通过 `settings_commit()` 保存。启动只读，旧版区域保留。实料地址、实际断电恢复与跨任务行为尚未接板验证；校准字段和业务保存触发尚未接入。

2026-10-07 历史实现：闪光输出使用 GPIO38 和官方 RMT，默认高电平 20 ms、低电平恢复 1 ms，提供单次触发、1–30 ms 指定脉宽及忙状态接口。初始化只保持低电平，不自动触发；快门联动尚未接入。按用户要求停止后续测试和语法检查，移除本轮新增闪光测试文件。实机波形及外部闪光灯兼容性未验证。

2026-10-15：闪光改为 GPIO42 下降沿 ISR 直接拉高 GPIO38，GPTimer 报警 ISR 结束脉冲，不经过任务通知或 RMT。默认目标 20 ms、恢复期至少 1 ms；支持任务和 ISR 调用。当前不做额外触点消抖或回充判定；关中断和 Flash cache 暂停仍可能延迟响应。详见[闪光说明](main/drivers/flash_trigger/README.md)。本轮用户确认编译通过；未运行测试，未上板验证。

## Flash 分区

分区表地址为 `0x8000`。

| 分区 | 起始地址 | 大小 | 用途 |
| --- | --- | --- | --- |
| nvs | 0x9000 | 64 KB | 参数、Wi-Fi 配置、射频校准数据 |
| otadata | 0x19000 | 8 KB | OTA 启动槽与升级状态 |
| phy_init | 0x1B000 | 4 KB | 预留 PHY 初始化数据，当前未使用 |
| ota_0 | 0x20000 | 3 MB | 固件槽 A，首次烧录位置 |
| ota_1 | 0x320000 | 3 MB | 固件槽 B |

0x620000 起剩余 1.875 MB 暂不分配。不设 factory 分区；OTA 状态区为空时从 ota_0 启动。OTA 接收、校验和新固件确认逻辑尚未实现，自动回滚暂未启用。

## 配置与构建

在 ESP-IDF 终端中进入本目录。目标为 esp32s3；Flash 设为 8 MB、QIO、80 MHz，PSRAM 设为 Octal、80 MHz。2026-10-08 按采购的 N8R8 调整分区表，本机 sdkconfig 的 Flash 容量仍须手动从 16 MB 改为 8 MB。

在 `menuconfig → Partition Table` 选择 `Custom partition table CSV`，文件名为 `partitions.csv`，偏移为 `0x8000`。本机 sdkconfig 已设置。

首次接入 LVGL 后先执行 `idf.py reconfigure`，由组件管理器下载固定版本依赖并更新本机配置。LVGL 保持默认字体和软件绘图配置；显示缓冲区格式在代码中明确设为 RGB565。生成的 `managed_components/` 不纳入 Git，`dependencies.lock` 应随后续已验证构建保存。本次不整理 `sdkconfig.defaults`。

```powershell
idf.py build
idf.py -p COM端口 flash monitor
```

第一次使用本分区表时按生成的完整烧录参数烧录；旧分区表地址不同，不能只替换应用镜像。

计划的运行方式：开机启动 BLE，Wi-Fi 由按键、菜单或 BLE 指令手动开启，需要升级时再执行 OTA。目前尚未实现这些流程。

## 相关工程

- [第一代主控归档](../../archive/PM_Controller_Rev1_x/)：旧 STM32 固件，不适用于本 PCB。
- [FocusUnit](../FocusUnit/)：独立对焦组件。
- [GUI Guider](../../UI设计/MainControl/)：旧界面资源，移植前需适配。
- [软件与 PCB 兼容性表](../../软件与PCB兼容性表.md)。

屏幕 SPI 上限依据：ST7789V3 V0.1，第 38 页四线串行接口写周期最小 16 ns，即 62.5 MHz。ESP32-S3 当前 80 MHz APB 分频最高合规档为 40 MHz；不能直接填 62.5 MHz（当前 IDF 会选成 80 MHz）。CS 建立和保持各设置 1 个 SPI 周期，即 25 ns，满足手册各 15 ns 要求。

缓冲对比：用户实测内部 RAM 64 行双缓冲为 23.5 FPS、平均 42.5 ms、最大 43.5 ms；整屏内部 RAM 版本 buf2 分配失败；整屏 PSRAM 版本为 19.3 FPS、平均 51.7 ms、最大 55.3 ms。当前恢复内部 RAM 64 行双缓冲，本轮未编译、未烧录。
