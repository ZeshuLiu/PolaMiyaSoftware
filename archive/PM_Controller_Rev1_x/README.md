# PM_Controller_Rev1_x 第一代固件归档

本目录保存第一代 PM 主控系统的旧固件，与第二代 PM_Controller Rev2.0 的 ESP32-S3 固件分开维护。

| 目录 | MCU | 原工程名 | 工具链 |
| --- | --- | --- | --- |
| [MainController](MainController/) | STM32G474RET6 | MainController2 | Keil MDK / Keil Studio，ARM Compiler 6 |
| [PowerManage](PowerManage/) | STM32L011D4P6 | PowerManage2 | Keil MDK，ARM Compiler 5 |

`Rev1_x` 表示第一代固件归档范围，不保证所有 Rev1.x 板卡都兼容。硬件仓库当前冻结版本为 PM_Controller Rev1.3 及其配套 DispKey Rev1.0；使用本固件前仍需逐项核对 PCB、引脚和外设。

Rev1.3 实际主控为 STM32G474RE，固件配置与其一致；硬件文件仍标注原 STM32G431RB。两者引脚兼容，G474RE 是实际采用的升级型号。

## 归档来源

- 整理日期：2026-10-06。
- 整理前快照：`a72e7de`（保存软件目录整理前快照）。
- 原位置：`code/MainController/` 和 `code/PowerManage/`。
- 本次完整移动工程，保留原内部文件名、工程名、源码、库与本地构建产物；仅更新目录相关说明和文档链接。
- 工程名 `MainController2`、`PowerManage2` 中的 `2` 不表示第二代主控硬件。

## 打开与构建

以 `MainController/` 或 `PowerManage/` 为工作目录打开工程。

- 主控 Keil 工程：`MainController/MDK-ARM/MainController2.uvprojx`。
- 主控 CMSIS 工程：`MainController/MDK-ARM/MainController2.csolution.yml`。
- 电源管理 Keil 工程：`PowerManage/MDK-ARM/PowerManage2.uvprojx`。
- CubeMX 配置仍为 `MainController/MainController2.ioc` 和 `PowerManage/PowerManage2.ioc`。

旧 CMake/CMSIS 构建缓存、索引及 IDE 本机设置可能含有原绝对路径。归档后的第一次构建需由工具链重新生成构建目录和索引；不要直接复用旧缓存。本次没有重新编译、烧录或实机验证。

烧录接口为对应 STM32 的 SWD，相关步骤见[主控说明](MainController/README.md)和[电源管理说明](PowerManage/README.md)。旧 HEX/AXF 是历史产物，不保证与当前源码一致，烧录前需确认版本。

## 维护范围

本目录用于查询旧实现、维修旧设备及必要的历史修复。第二代开发在 [code/PM_Controller](../../code/PM_Controller/) 进行。旧算法或界面资源可经适配移植，不直接搬用 STM32 启动代码、HAL、时钟和引脚配置。

GUI Guider 资源继续保留在 [GuiGuider/MainControl](../../GuiGuider/MainControl/)，尚未确认其与第二代固件的兼容性。
