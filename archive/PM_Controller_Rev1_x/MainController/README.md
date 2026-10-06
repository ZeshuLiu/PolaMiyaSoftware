# MainController2 - 宝丽来相机主控器固件

STM32G474RETx 微控制器固件，用于宝丽来改装相机的自动控制系统。

## 功能

- 激光测距
- 电动吐片控制
- LCD 显示屏 (280x240)
- USB CDC 虚拟串口
- 电量计量 (SDM18)
- 温度监测 (TMP102)
- 自研 ZUI 图形界面

## 项目结构

```
MainController2/
├── Core/           # STM32CubeMX 生成的核心代码
├── Drivers/        # STM32G4xx HAL 库和 CMSIS
├── HARDWARE/LCD/   # LCD 显示屏驱动
├── Libs/           # 自定义功能库
│   ├── ZUI/        # 自研 UI 框架
│   ├── TMP102/     # 温度传感器驱动
│   ├── SDM18/      # 电量计量芯片驱动
│   ├── Motors/     # 电机控制
│   └── EEPROM/     # 数据存储
├── USB_Device/     # USB CDC 虚拟串口
└── MDK-ARM/        # Keil 项目配置
```

## 构建与调试

### 编译
- 使用 **Keil MDK-ARM** 打开 `MDK-ARM/MainController2.uvprojx`
- 目标设备：STM32G474RETx
- 编译器：ARM Compiler 6 (AC6)

### 调试
- 调试器：ST-Link via pyOCD
- 接口：SWD，时钟 4MHz

## 使用方法

1. 长按机身左下角按钮 2 秒开机/关机
2. 三向按钮长按中键 2 秒启动吐片电机
3. 短按中键停止吐片电机
4. 推动侧面推杆完成吐片
5. 测距功能开机即运行

## 相关仓库

本固件为宝丽来改装相机项目的一部分：
- **Hardware**: [电路设计仓库链接]
- **Mechanical**: [机械结构仓库链接]

## 已知问题

详见 [TODO.md](TODO.md)

## 许可证

[License]
