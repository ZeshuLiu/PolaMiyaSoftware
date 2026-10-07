# 软件与 PCB 兼容性表

更新：2026-10-06。PCB 版本见硬件仓库的[结构与 PCB 兼容性表](../PolaMiyaHardware/结构与PCB兼容性表.md)。

| PCB 硬件系列 | 当前对应版本 | MCU | 软件工程 | 状态 |
| --- | --- | --- | --- | --- |
| PM_Controller Rev1.x：主控 | Rev1.3 | STM32G474RE | [MainController](archive/PM_Controller_Rev1_x/MainController/) | 已归档，本次未复测 |
| PM_Controller Rev1.x：电源管理 | Rev1.3 | STM32L011D4 | [PowerManage](archive/PM_Controller_Rev1_x/PowerManage/) | 已归档，本次未复测 |
| PM_Controller Rev2.x | Rev2.0 | ESP32-S3-WROOM-1-N16R8 | [PM_Controller](code/PM_Controller/) | 官方 LVGL 移植组件与 ST7789 屏幕测试，已配置双 OTA 分区 |
| Focus Unit Drive Rev1.x | Rev1.0 | STM32F030F4 | [FocusUnit](code/FocusUnit/) | 初步测试通过，与新主控的通信尚未实现 |

- 旧主控实际使用 STM32G474RE，与原 STM32G431RB 引脚兼容、性能更高。硬件文件仍标注 G431RB，固件按 G474RE 配置。
- `PM_Controller_Rev1_x` 是第一代归档目录。内部工程名 `MainController2`、`PowerManage2` 中的 `2` 不代表第二代硬件；其他 Rev1.x 板卡需另行核对。
- DispKey、电机驱动和闪光灯转接板无独立固件工程，由配套主控控制。旧 STM32 固件不能直接用于主控器 2.0。
- [GuiGuider/MainControl](GuiGuider/MainControl/) 是界面资源，尚未适配第二代主控。
