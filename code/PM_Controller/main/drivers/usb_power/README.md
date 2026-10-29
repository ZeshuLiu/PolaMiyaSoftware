# USB 有效电源检测

2026-10-20。GPIO21（`PIN_USBPWR_DET`）读取 U18 TPS3808G01 的 /RESET。按当前生产网表，R56=10 kΩ 上拉到主控 +3.3 V，SENSE 由 VBUS 经 R58=10.1 kΩ、R57=1 kΩ 分压。

GPIO21 高表示 SENSE 超过阈值且 U18 释放延时结束；低表示外部电源未达到有效条件。按典型 0.405 V 门槛折算 VBUS 约 4.50 V，此为标称计算，未实测。它表示有效外电，不等同于 USB 数据连接状态。

`usb_power_init()` 一次性配置输入，不开内部上下拉或中断；`usb_power_get(&present)` 为任务上下文读取接口，初始化失败返回错误。`app_main()` 在按键初始化阶段调用，失败只记录告警，充电状态显示待确认。200 ms 软件稳定判断由 [charge_status](../../app/charge_status/README.md) 完成，没有新增任务。

依据：[TI TPS3808 数据手册](https://www.ti.com/lit/ds/symlink/tps3808.pdf) 的 G01 门槛和 /RESET 描述。未编译、测试或烧录；有效电平、插拔阈值、硬件释放延时与弱供电状态待实测。
