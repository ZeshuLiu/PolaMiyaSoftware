# PM_Controller 固件

对应 PCB：PM_Controller Rev2.x，当前为 Rev2.0。模组：ESP32-S3-WROOM-1-N16R8，16 MB Flash、8 MB Octal PSRAM。开发环境为 ESP-IDF 6.1。

`app_main()` 当前运行屏幕方向与偏移测试。引脚和双 OTA 分区已整理，电机、蓝牙、Wi-Fi 和 OTA 业务尚未接入。

## 工程文件

| 文件 | 用途 |
| --- | --- |
| [main/main.c](main/main.c) | 程序入口 |
| [main/PIN.h](main/PIN.h) | PCB 引脚定义，只记录连接，不初始化 GPIO |
| [main/drivers/st7789/](main/drivers/st7789/) | 官方 esp_lcd ST7789 驱动的板级封装 |
| [main/app/lcd_test/](main/app/lcd_test/) | 屏幕测试画面与切换逻辑 |
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

### 方向与偏移测试

使用 ESP-IDF 官方 `esp_lcd_new_panel_st7789()`，SPI2、模式 0、20 MHz、RGB565。板级驱动只封装引脚、背光、视窗设置和同步绘制，不另写 ST7789 初始化指令。

上电循环显示 12 组画面，每组保持 6 秒：ROT 0、90、180、270，各测试短边偏移 34、0、68 像素。首组为 ROT 0、X34、Y0；34 是按 240 像素控制器短边居中裁出 172 像素的候选值，尚未实测确认。

- ROT 0 / 180：逻辑尺寸 172×320，调整 X 偏移。
- ROT 90 / 270：逻辑尺寸 320×172，调整 Y 偏移。
- 正确画面应四边完整，TL/TR/BL/BR 分别位于左上、右上、左下、右下，TOP 与 UP 箭头朝上，文字正向。
- 底部 R/G/B 色条应依次为红、绿、蓝。若红蓝互换，调整 `LCD_TEST_BGR`；若颜色反相，调整 `LCD_TEST_INVERT`。

画面和串口都会给出 CASE 编号、ROT、X/Y 偏移。确认后，在 [lcd_test.c](main/app/lcd_test/lcd_test.c) 中设置 `LCD_TEST_ROTATION_INDEX`、`LCD_TEST_GAP_INDEX`，并将 `LCD_TEST_AUTO_CYCLE` 改为 0 固定显示。偏移索引 0/1/2 分别对应 34/0/68。

驱动通过内部 DMA 条带传输，等待传输完成后才复用缓冲区；同一屏幕句柄需由一个任务顺序调用。切换画面前清空控制器 RAM，避免旧窗口残影影响偏移判断。本测试没有启动蓝牙或 Wi-Fi。

RGB565 一帧为 110,080 字节。20 MHz SPI 的纯像素传输理论耗时约 44 ms，不含绘图、字节转换和调度；当前逐条带等待，不是双缓冲流水传输。普通界面可用 `lcd_st7789_draw_rgb565()` 更新矩形区域，无需每次清空显存。

### 当前验证状态

PCB、PIN.h 和文档中的 29 个 GPIO 对应一致；分区表已通过 ESP-IDF 分区工具校验。屏幕代码已核对本机 ESP-IDF 接口和文件引用，尚未记录本版代码的编译及接屏结果。旋转、偏移、颜色和 SPI 时钟稳定性以实机测试为准。

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
