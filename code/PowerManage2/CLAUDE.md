# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

STM32L011D4Px 微控制器项目 (Cortex-M0+), 使用 STM32CubeMX 生成配置，通过 Keil MDK-ARM 构建。这是宝丽来相机改装项目的电源管理模块，负责控制主电源开关和状态指示灯。

## Build & Debug

### 构建命令
- 使用 **Keil MDK-ARM** 打开 `MDK-ARM/PowerManage2.uvprojx` 进行编译
- 目标设备：STM32L011D4Px
- 编译器：ARM Compiler 5 (AC5)
- 输出：`MDK-ARM/PowerManage2/PowerManage2.hex`

### 烧录
- 调试器：ST-Link (SWD 接口)
- 使用 Keil MDK-ARM 的 Download 按钮烧录
- 或使用 pyOCD: `pyocd flash -t stm32l011 MDK-ARM/PowerManage2/PowerManage2.hex`

## Code Architecture

### 目录结构
```
├── Inc/            # 头文件 (main.h, gpio.h, stm32l0xx_it.h, stm32l0xx_hal_conf.h)
├── Src/            # 源代码 (main.c, gpio.c, stm32l0xx_it.c, stm32l0xx_hal_msp.c, system_stm32l0xx.c)
├── Drivers/        # STM32L0xx HAL 库和 CMSIS
└── MDK-ARM/        # Keil 项目配置
```

### 核心功能 (main.c)
- **电源控制逻辑**: 通过长按 WKUP 引脚 (PA0) 1 秒切换电源状态
- **PWR_EN (PC15)**: 控制主电源开关 (低电平有效)
- **PWR_LED (PC14)**: 状态指示灯
  - 开机后亮 200ms
  - 用户触发后亮 2000ms
- **低功耗**: 系统时钟 16MHz (HSI PLL), AHB 分频 16 → 1MHz 运行

### GPIO 定义 (gpio.h)
| 引脚 | 功能 | 模式 |
|------|------|------|
| PC14 | PWR_LED | 输出 |
| PC15 | PWR_EN | 开漏输出 |
| PA0  | WKUP | 输入 (唤醒按钮) |
| PA1  | P2C0 | 输出 |
| PA4  | P2C1 | 输出 |
| PA7  | PWR_KEY | 输入 |
| PA9  | C2P0 | 输入 (上拉) |
| PA10 | C2P1 | 输入 (上拉) |
| PA13/14 | SWDIO/SWCLK | 调试接口 |

### 关键参数 (main.h)
- `PWR_UP_DELAY`: 200ms (启动延时)
- `PRESS_HOLD_TIME`: 1000ms (长按触发时间)
- `LED_PWRUP_DUR`: 200ms (开机指示灯时长)
- `LED_USR_DUR`: 2000ms (用户触发指示灯时长)

## 开发注意事项

1. **修改外设后**: 需在 STM32CubeMX (`PowerManage2.ioc` 文件) 中重新生成代码
2. **低功耗敏感**: 电池供电设备，避免不必要的轮询和高频操作
3. **时间基准**: 使用 `HAL_GetTick()` 实现非阻塞延时和计时
4. **代码简洁**: 无操作系统，单线程裸机架构

## 相关项目

- 主控制器固件：`code/MainController2/` (STM32G474)
- 完整项目：https://github.com/ZeshuLiu/PolaMiya
