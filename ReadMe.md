# PolaMiyaSoftware - 宝丽来相机改装项目

宝丽来相机自动化改装项目软件部分。

> 完整项目（含机械/电路设计）：https://github.com/ZeshuLiu/PolaMiya

## 仓库结构

| 子目录 | 说明 |
|--------|------|
| [code/MainController2](code/MainController2) | 主控器固件 (STM32G474) |
| [code/PowerManage2](code/PowerManage2) | 电源管理固件 |

## 技术栈

| 项目 | 主控器 (MainController2) | 电源管理 (PowerManage2) |
|------|-------------------------|------------------------|
| **MCU** | STM32G474RETx (Cortex-M4F) | STM32L011D4Px (Cortex-M0+) |
| **IDE** | Keil MDK-ARM V5 / CMSIS 6 | Keil MDK-ARM V5 |
| **编译器** | ARM Compiler 6 (AC6) | ARM Compiler 5 (AC5) |
| **框架** | STM32CubeMX + HAL 库 | STM32CubeMX + HAL 库 |
| **调试器** | ST-Link / pyOCD (SWD, 4MHz) | ST-Link (SWD) |

## 烧录方法

### 硬件连接
```
ST-Link V2    目标板
---------    -------
3.3V         VCC
GND          GND
SWCLK        SWCLK
SWDIO        SWDIO
NRST         NRST
```

### 使用 Keil Studio (VS Code)
1. 在 VS Code 中打开 `code/MainController2` 文件夹
2. 连接 ST-Link 调试器
3. 按 `F5` 或点击运行/调试按钮烧录并启动调试

### 使用 Keil MDK-ARM
1. 打开 `MDK-ARM/MainController2.uvprojx`
2. 点击 **Download** 按钮烧录

### 使用命令行 (pyOCD)
```bash
# 主控器固件
pyocd flash -t stm32g474 MDK-ARM/Objects/MainController2.hex

# 电源管理固件
pyocd flash -t stm32l011 code/PowerManage2/MDK-ARM/PowerManage2/PowerManage2.hex
```

## 使用方法

### 相机操作
- **开机/关机**：长按机身左下角按钮 2 秒
- **吐片**：三向按钮长按中键 2 秒启动吐片电机，短按中键停止
- **测距**：开机即运行

### 子项目文档
- [主控器固件详解](code/MainController2/README.md)
- [电源管理固件](code/PowerManage2/)
