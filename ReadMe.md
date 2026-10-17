# PolaMiyaSoftware - 宝丽来相机改装项目

宝丽来相机自动化改装项目的软件仓库。当前主控器为 PM_Controller Rev2.0，使用 ESP32-S3-WROOM-1-N8R8；第一代 STM32 固件保存在历史归档中。

完整项目（含机械/电路设计）：https://github.com/ZeshuLiu/PolaMiya

版本对应与待核对项见[软件与 PCB 兼容性表](软件与PCB兼容性表.md)。

## 仓库结构与硬件对应

| 目录 | 对应硬件 / MCU | 状态 |
| --- | --- | --- |
| [code/PM_Controller](code/PM_Controller/) | 第二代主控器 PM_Controller Rev2.0 / ESP32-S3-WROOM-1-N8R8 | 官方 LVGL 移植组件、三页竖屏 UI，已配置双 OTA 分区；新 UI 未上板验证 |
| [code/FocusUnit](code/FocusUnit/) | 独立对焦组件 / STM32F030F4Px | 寻零校验、重试及开环控制已实现，用户确认测试没有问题 |
| [code/Scripts](code/Scripts/) | 辅助计算脚本 | 按脚本用途使用 |
| [archive/PM_Controller_Rev1_x](archive/PM_Controller_Rev1_x/) | 第一代主控系统 / STM32G474 + STM32L011 | 历史固件归档，不适用于第二代主控器 |
| [UI设计](UI设计/) | 旧 GUI Guider 工程、竖屏设计图与说明 | 旧工程保留；正式 UI 使用原生 LVGL 9.3 C 代码 |

## 开发入口

第二代主控器在 [code/PM_Controller](code/PM_Controller/README.md) 开发，使用 ESP-IDF 6.1、官方 esp_lcd ST7789 驱动和 esp_lvgl_port。默认启动原生 LVGL 三页竖屏 UI，方向 7，使用目前可用的 KEY1–3，屏幕测试源码保留。74HC165 已实现 6 个按键的消抖、长短按及事件标志位读取；24C64 已实现分页访问和带版本、CRC 的参数轮换保存；闪光输出已实现默认 20 ms 单次高电平脉冲。引脚和双 OTA 分区已配置；电机、BLE、手动开启 Wi-Fi 和 OTA 业务后续实现。2026-10-17 新 UI 未编译、未测试、未烧录。

对焦组件的配置、模块接口、构建与验证记录见 [FocusUnit 工程说明](code/FocusUnit/AGENT.md)。它是独立组件，不随旧主控固件一起归档。

2026-10-07：移除主控器和对焦组件的本地主机测试、桩、测试入口及缓存，保留固件内的屏幕测试和电机演示。各工程文档保留清理前的验证结果。

## 第一代固件

第一代的 `MainController` 和 `PowerManage` 完整工程放在 `archive/PM_Controller_Rev1_x/`。内部保留 `MainController2`、`PowerManage2` 的原工程名，以保留原构建配置；名称中的 `2` 不表示第二代主控硬件。适用范围与使用方法见[归档说明](archive/PM_Controller_Rev1_x/README.md)。

硬件 PCB 版本与软件版本分别记录，不能仅凭目录名推断所有第一代板卡的兼容性。历史固件使用前应核对实际 PCB、引脚、电源管理和屏幕按键模块。

## 目录整理记录

2026-10-06：整理前快照为 `a72e7de`。旧主控和电源管理工程归档至 `archive/PM_Controller_Rev1_x/`，新建 ESP32-S3 工程、PCB 引脚定义、双 OTA 分区和 ST7789 屏幕测试。旧工程已跟踪的文件保留，新增缓存和本机设置不纳入 Git。当前屏幕方向、偏移及刷新速度尚未实测确认。

## 许可证

见 [LICENSE.md](LICENSE.md)。
