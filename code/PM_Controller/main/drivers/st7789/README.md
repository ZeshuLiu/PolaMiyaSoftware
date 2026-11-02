# ST7789 屏幕与背光

屏幕使用 SPI2 官方面板驱动；GPIO9 背光为低电平有效，LEDC 低速 Timer 0 / Channel 0，20 kHz、10 bit，与吐片电机的 Timer 1 / Channel 1 分开。启动先保持背光高电平关闭，初始化屏幕及显示内容后，按既有 300 ms 目标渐亮。

2026-11-02：修正 `ledc: GPIO 9 is not usable, maybe conflict with others`。原先 `gpio_config()` 已登记 GPIO9 输出占用，LEDC 初始化又申请相同路径，产生 ESP-IDF 6.1 警告。现在先预置高电平，用公开 GPIO 方向及上下拉接口保持关闭，输出占用只在 LEDC 绑定时登记；驱动先设置关闭占空比，再连接输出。没有使用私有 GPIO 释放接口或屏蔽日志。

亮度范围、渐变、SPI / DMA、屏幕方向和面板初始化不变。仅源码静态核对，未编译、测试或烧录；GPIO9 交接时的实际波形尚未验证。
