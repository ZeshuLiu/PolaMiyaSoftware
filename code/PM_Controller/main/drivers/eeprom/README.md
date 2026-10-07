# 24C64 驱动

U12 按用户确认的 24C64 实现：8 KB、32 字节页、双字节存储地址、7 位器件地址 `0x50`。使用 ESP-IDF 新版 `driver/i2c_master.h`，SCL=GPIO15、SDA=GPIO16，100 kHz。板上 R15/R16 提供 4.7 kΩ 上拉，WP 接地。

| 接口 | 内容 |
| --- | --- |
| `eeprom_init()` | 初始化总线、设备和互斥锁，探测器件；重复调用复用已初始化资源 |
| `eeprom_read(address, data, length)` | 范围检查、双字节地址及重复 START 读取；每次最多 256 字节 |
| `eeprom_write(address, data, length)` | 范围检查、按物理页拆分、写后 ACK 轮询；完成后才返回 |

读写串行化由互斥锁保证。单次 I²C 传输超时为 50 ms，写完成轮询限时 20 ms；器件忙时至少等待一个 FreeRTOS tick，避免零延时忙循环。接口返回原始错误，调用者应检查结果。零长度访问允许空指针，越界或非零长度空指针返回错误。

驱动不负责参数布局。参数服务使用 `0x0100–0x1FFF`，该区域通过[参数接口](../../app/settings/README.md)访问；原始驱动供其他预留区域或独立硬件测试使用。`settings_init()` 会调用 `eeprom_init()`，启动消费者前完成初始化；所有接口只用于任务上下文。

原理图仍使用 AT24CS16 符号。24C64 和 AT24CS16 的寻址协议不同，本驱动只适用于确认后的 24C64；若实料更换，应同步驱动容量、页大小和地址协议。

时序与存储组织参考 [Microchip AT24C64D 手册](https://ww1.microchip.com/downloads/en/DeviceDoc/20006271A.pdf)；ESP-IDF 接口参考 [I²C 官方文档](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/i2c.html)。具体供应商的时序和器件地址仍应按实料手册核对。
