# PowerManage2 - 电源管理固件

STM32L011D4Px 微控制器固件，负责宝丽来相机改装项目的电源管理功能。

## 功能特性

- **电源开关控制**: 长按唤醒按钮切换系统电源状态
- **状态指示灯**: 开机和用户操作时提供视觉反馈
- **低功耗设计**: 1MHz 运行频率，电池供电优化
- **主控制器通信**: 通过 C2P0/C2P1 引脚与主控制器协同工作

## 硬件规格

| 参数 | 值 |
|------|-----|
| **MCU** | STM32L011D4Px (Cortex-M0+) |
| **Flash** | 16KB |
| **RAM** | 2KB |
| **系统时钟** | 16MHz HSI PLL |
| **运行频率** | 1MHz (AHB 分频 16) |

## GPIO 引脚定义

| 引脚 | 名称 | 功能 | 配置 |
|------|------|------|------|
| **PC14** | PWR_LED | 电源状态指示灯 | 输出 |
| **PC15** | PWR_EN | 主电源使能 (低电平有效) | 开漏输出 |
| **PA0** | WKUP | 唤醒/电源按钮 | 输入 (下拉) |
| **PA1** | P2C0 | 电源控制信号 0 | 输出 |
| **PA4** | P2C1 | 电源控制信号 1 | 输出 |
| **PA7** | PWR_KEY | 电源按键输入 | 输入 |
| **PA9** | C2P0 | 主控制器通信 0 | 输入 (上拉) |
| **PA10** | C2P1 | 主控制器通信 1 | 输入 (上拉) |
| **PA13/14** | SWDIO/SWCLK | SWD 调试接口 | - |

## 电源控制逻辑

### 开机/关机流程
1. 初始状态：`PWR_EN = HIGH` (电源关闭)
2. 长按 WKUP 按钮 1 秒 → `PWR_EN = LOW` (电源开启)
3. 再次长按 WKUP 按钮 1 秒 → `PWR_EN = HIGH` (电源关闭)

### 指示灯行为
| 事件 | 指示灯状态 |
|------|-----------|
| 上电初始化 | 亮 200ms |
| 用户长按触发 | 亮 2000ms |
| 待机状态 | 熄灭 |

### 主控制器协同
- 当 `C2P0 = HIGH` 时，电源状态保持不变 (主控制器请求保持供电)
- 关机后通过 `P2C0/P2C1` 输出状态信号

## 编译与烧录

### 使用 Keil MDK-ARM
1. 打开 `MDK-ARM/PowerManage2.uvprojx`
2. 点击 **Build** 编译项目
3. 连接 ST-Link 调试器
4. 点击 **Download** 烧录固件

### 使用 pyOCD 命令行
```bash
# 编译后烧录
pyocd flash -t stm32l011 MDK-ARM/PowerManage2/PowerManage2.hex
```

### 输出文件
- **HEX**: `MDK-ARM/PowerManage2/PowerManage2.hex`
- **BIN**: `MDK-ARM/PowerManage2/PowerManage2.bin`

## 目录结构

```
├── Inc/
│   ├── main.h              # 主头文件，含 GPIO 定义和参数常量
│   ├── gpio.h              # GPIO 初始化声明
│   ├── stm32l0xx_it.h      # 中断处理函数声明
│   └── stm32l0xx_hal_conf.h # HAL 库配置
├── Src/
│   ├── main.c              # 主程序 (电源控制逻辑)
│   ├── gpio.c              # GPIO 初始化配置
│   ├── stm32l0xx_it.c      # 中断处理函数
│   ├── stm32l0xx_hal_msp.c # HAL MSP 初始化
│   └── system_stm32l0xx.c  # 系统初始化
├── Drivers/                # STM32 HAL 库和 CMSIS
└── MDK-ARM/                # Keil 项目文件
```

## 关键参数 (main.h)

```c
#define PWR_UP_DELAY      200U    // 启动延时 (ms)
#define PRESS_HOLD_TIME   1000U   // 长按触发时间 (ms)
#define LED_PWRUP_DUR     200U    // 开机指示灯时长 (ms)
#define LED_USR_DUR       2000U   // 用户触发指示灯时长 (ms)
```

## 与其他模块通信

```
PowerManage2 <----> MainController2
     |                    |
  PWR_EN (电源使能)    <-  监控电源状态
  C2P0/C2P1 (状态)    <-  读取控制信号
  P2C0/P2C1 (输出)    ->  发送控制信号
```

## 注意事项

1. **低功耗**: 避免添加不必要的轮询或高频操作
2. **去抖处理**: 按键检测已使用时序去抖，修改时保持逻辑
3. **电源时序**: 修改 PWR_EN 逻辑时需确保与主控制器协同
4. **开漏输出**: PWR_EN 使用开漏模式，外部需上拉电阻

## 相关文档

- [STM32L011 参考手册](https://www.st.com/en/microcontrollers-microprocessors/stm32l011d4.html)
- [项目主 README](../../ReadMe.md)
- [主控制器固件](../MainController2/)
