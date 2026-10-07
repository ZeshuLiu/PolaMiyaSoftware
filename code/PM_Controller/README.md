# PM_Controller 固件

对应 PCB：PM_Controller Rev2.x，当前为 Rev2.0。模组：ESP32-S3-WROOM-1-N16R8，16 MB Flash、8 MB Octal PSRAM。开发环境为 ESP-IDF 6.1。

`app_main()` 将闪光输出初始化为低电平，启动 74HC165 按键扫描，加载 24C64 参数，初始化官方 ST7789 驱动和 LVGL，并启动屏幕方向与偏移测试。按键事件供外部读取，尚未接入 LVGL 菜单。参数修改与持久化提交分开。引脚和双 OTA 分区已整理，电机、蓝牙、Wi-Fi 和 OTA 业务尚未接入。

2026-10-07：按用户要求移除本地主机测试、桩及测试缓存，保留固件内的屏幕方向与偏移测试。下文主机测试结果为清理前的验证记录。

## 工程文件

| 文件 | 用途 |
| --- | --- |
| [AGENT.md](AGENT.md) | 开发约定、接口约束、执行限制及验证边界 |
| [main/main.c](main/main.c) | 程序入口 |
| [main/PIN.h](main/PIN.h) | PCB 引脚定义，只记录连接，不初始化 GPIO |
| [main/drivers/st7789/](main/drivers/st7789/) | SPI、官方 ST7789 驱动初始化和背光 |
| [main/drivers/hc165/](main/drivers/hc165/README.md) | 74HC165 读取、6 个按键消抖及长短按标志位 |
| [main/drivers/eeprom/](main/drivers/eeprom/README.md) | 官方 I²C、24C64 分页读写及写完成轮询 |
| [main/drivers/flash_trigger/](main/drivers/flash_trigger/README.md) | 官方 RMT 单次闪光脉冲、忙状态及默认低电平 |
| [main/app/settings/](main/app/settings/README.md) | 参数定义、RAM 缓存、CRC、轮换保存与旧版导入 |
| [main/app/display/](main/app/display/) | 官方 esp_lvgl_port 初始化和显示配置 |
| [main/app/lcd_test/](main/app/lcd_test/) | LVGL 测试控件和定时切换 |
| [main/idf_component.yml](main/idf_component.yml) | 固定官方移植组件和 LVGL 版本 |
| [partitions.csv](partitions.csv) | 16 MB Flash 自定义分区表 |
| CMakeLists.txt、main/CMakeLists.txt | ESP-IDF 构建配置 |
| sdkconfig | 本机配置，当前不纳入 Git；sdkconfig.defaults 后续再整理 |

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

74HC165 驱动已接入启动流程。6 个按键采用 20 ms 消抖，短按在松开时触发，长按在持续达到 1 s 时触发一次；事件通过 `hc165_get_events()` 或 `hc165_take_events()` 读取。接口与使用示例见[驱动说明](main/drivers/hc165/README.md)，各驱动进度见[驱动实现状态](驱动实现状态.md)。

- GPIO35/36/37 由 N16R8 的 PSRAM 占用，不能用于外设。
- GPIO45/46/47/48 在本 PCB 上未接。GPIO45/46 是启动配置脚，不作为普通空闲引脚使用。
- GPIO0 是 BOOT 启动配置脚；GPIO3 同时影响 JTAG 选择。使用电池 ADC 时不要随意改变相关 eFuse。
- GPIO39–42 已用于板上功能，调试使用 USB Serial/JTAG。GPIO19/20 使用 USB 时不能另作 GPIO。
- ESP_EN 是模组复位输入，不是 MCU GPIO，没有放入 PIN.h。

## 显示屏

- 驱动芯片：ST7789。
- 屏幕原始尺寸：长 320、高 172 像素。
- 安装与显示方向：竖屏，显示坐标按宽 172、高 320 像素使用。
- 接口：SPI，使用 PIN.h 中的 LCD_MOSI、LCD_CLK、LCD_CS、LCD_DC、LCD_RES 和 LCD_BLK 引脚。
- 背光低电平点亮。旋转方向和显示窗口偏移待接屏后确认。

### LVGL 接入

依赖固定为 `espressif/esp_lvgl_port 2.9.0` 和 `lvgl/lvgl 9.3.0`，与本机 ESP-IDF SPI 屏幕示例及 GUI Guider 的 LVGL 主次版本一致。组件管理器在配置工程时下载依赖，不复制旧工程的 LVGL 源码。

调用顺序：`app_main()` → `flash_trigger_init()` → `hc165_init()` → `settings_init()` → `lcd_st7789_init()` → `display_init()` → `lcd_test_start()`。闪光输出、按键和显示初始化失败通过 `ESP_ERROR_CHECK` 停止启动；参数存储失败记录错误并继续显示启动。参数锁成功分配后，器件错误时仍可使用 RAM 默认值，持久化提交被禁止。初始化接口只在开机时调用一次；显示和测试定时器持续运行，不提供运行中销毁或重复初始化接口。

使用 ESP-IDF 官方 `esp_lcd_new_panel_st7789()`，SPI2、模式 0、20 MHz、RGB565。板级文件只配置引脚、SPI 和背光，直接输出官方 `panel`、`io` 句柄。LVGL 任务、时钟、刷新、字节交换、DMA 完成回调和硬件旋转由官方移植组件管理。

LVGL 使用两个内部 DMA 绘图缓冲区，每个 5,120 像素、10,240 字节，SPI 最大传输长度与之匹配。使用局部刷新；PSRAM 已在本机配置中启用，但本显示路径不依赖 PSRAM 绘图缓冲区。操作 LVGL 时使用 `lvgl_port_lock()` / `lvgl_port_unlock()`；LVGL 定时器回调已经处于该锁的保护下。

### 方向与偏移测试

上电循环显示 12 组画面，每组保持 6 秒：ROT 0、90、180、270，各测试短边偏移 34、0、68 像素。首组为 ROT 0、X34、Y0；34 是按 240 像素控制器短边居中裁出 172 像素的候选值，尚未实测确认。

网格、边框、角标、文字、箭头和 RGB 色条全部使用 LVGL 对象绘制，切换由 LVGL 定时器执行。ROT 使用 LVGL 的旋转定义；原测试中手动设置的 90/270 镜像组合与官方移植层不同，应按当前画面重新确认方向。

- ROT 0 / 180：逻辑尺寸 172×320，调整 X 偏移。
- ROT 90 / 270：逻辑尺寸 320×172，调整 Y 偏移。
- 正确画面应四边完整，TL/TR/BL/BR 分别位于左上、右上、左下、右下，TOP 与 UP 箭头朝上，文字正向。
- 底部 R/G/B 色条应依次为红、绿、蓝。若红蓝互换，调整 [lcd_st7789.h](main/drivers/st7789/lcd_st7789.h) 中的 `LCD_ST7789_BGR`；若颜色反相，调整 `LCD_ST7789_INVERT_COLORS`。

画面和串口都会给出 CASE 编号、ROT、X/Y 偏移。确认后，在 [lcd_test.c](main/app/lcd_test/lcd_test.c) 中设置 `LCD_TEST_ROTATION_INDEX`、`LCD_TEST_GAP_INDEX`，并将 `LCD_TEST_AUTO_CYCLE` 改为 0 固定显示。偏移索引 0/1/2 分别对应 34/0/68。

切换时关闭背光，将 LVGL 分辨率临时设为控制器 RAM 的 240×320，刷新黑色背景，再恢复 172×320 和候选偏移，绘制新画面。每次同步刷新后调用官方 SPI IO 的 `esp_lcd_panel_io_tx_param(io, -1, NULL, 0)`，等待最后一批 DMA 完成，不发送屏幕命令；完成后才改变窗口或打开背光，避免旧窗口残影影响偏移判断。

RGB565 一帧为 110,080 字节。20 MHz SPI 的纯像素传输理论耗时约 44 ms，不含 LVGL 绘图、命令和调度。全 RAM 清屏只用于偏移测试；业务界面可直接操作 LVGL 控件并局部刷新。74HC165 已独立扫描，测试画面尚未使用按键；触摸、蓝牙、Wi-Fi 和 GUI Guider 业务界面尚未接入。

### 当前验证状态

PCB、PIN.h 和文档中的 29 个 GPIO 对应一致；分区表已通过 ESP-IDF 分区工具校验。

2026-10-07：4 个应用源文件、官方移植层的 LVGL 任务和显示源文件，以及固定 ROT 270 / GAP 68 的测试变体，使用本机 ESP-IDF 6.1 交叉编译器、官方发布头文件和 LVGL Kconfig 默认值通过 `-fsyntax-only` 检查。文档链接和 `git diff --check` 通过。Astra（中）对照实际组件源码完成只读审查，未发现可证实的 P1/P2 缺陷。

本次未运行完整固件构建，未修改本机 sdkconfig，未创建 sdkconfig.defaults。旋转、偏移、颜色和 SPI 时钟稳定性尚未接屏验证，以实机测试为准。

2026-10-07：74HC165 驱动、按键识别逻辑及应用入口通过 ESP-IDF 6.1 交叉编译器语法检查。14 项主机测试执行实际 C 驱动，覆盖全部 256 种输入位序、消抖、1 s 边界、多键操作、事件保持与清除、启动时按住、充电输入隔离和时间戳回绕。真实 RTOS 调度、电气时序和按键手感尚未接板验证。

2026-10-07：U12 按用户确认的 24C64 实现，8 KB、32 字节页、双字节地址、器件地址 0x50。EEPROM 和参数服务通过语法检查及 25 项主机测试，覆盖分页、忙等待、CRC、轮换、中断写入、未知版本保护和旧版导入。采用稳定参数 ID 与明确的小端编码，64 位统计量和背光参数缓存于 RAM，外部通过 `settings_commit()` 保存。启动只读，旧版区域保留。实料地址、实际断电恢复与跨任务行为尚未接板验证；校准字段和业务保存触发尚未接入。

2026-10-07：闪光输出使用 GPIO38 和官方 RMT，默认高电平 20 ms、低电平恢复 1 ms，提供单次触发、1–30 ms 指定脉宽及忙状态接口。初始化只保持低电平，不自动触发；快门联动尚未接入。按用户要求停止后续测试和语法检查，移除本轮新增闪光测试文件。实机波形及外部闪光灯兼容性未验证。

## Flash 分区

分区表地址为 `0x8000`。

| 分区 | 起始地址 | 大小 | 用途 |
| --- | --- | --- | --- |
| nvs | 0x9000 | 64 KB | 参数、Wi-Fi 配置、射频校准数据 |
| otadata | 0x19000 | 8 KB | OTA 启动槽与升级状态 |
| phy_init | 0x1B000 | 4 KB | 预留 PHY 初始化数据，当前未使用 |
| ota_0 | 0x20000 | 4 MB | 固件槽 A，首次烧录位置 |
| ota_1 | 0x420000 | 4 MB | 固件槽 B |

0x820000 起剩余 7.875 MB 暂不分配。不设 factory 分区；OTA 状态区为空时从 ota_0 启动。OTA 接收、校验和新固件确认逻辑尚未实现，自动回滚暂未启用。

## 配置与构建

在 ESP-IDF 终端中进入本目录。目标为 esp32s3；Flash 设为 16 MB、QIO、80 MHz，PSRAM 设为 Octal、80 MHz。

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
- [GUI Guider](../../GuiGuider/MainControl/)：旧界面资源，移植前需适配。
- [软件与 PCB 兼容性表](../../软件与PCB兼容性表.md)。
