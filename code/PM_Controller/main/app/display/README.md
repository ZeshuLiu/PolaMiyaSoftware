# LVGL 显示接入

2026-11-02：用户确认 100 ms 慢帧只在切页时偶发；同日另提供静止时发生的 StoreProhibited / CORRUPTED 回溯，二者尚无已证实的因果关系。新增 ELF 归档和运行定位日志见 [异常定位](../diagnostics/README.md)。未改变慢帧阈值、SPI / DMA 回调或看门狗设置；panic 根因尚未确认。

2026-11-02：LVGL 9 的内建 TLSF 内存池通过 `LV_MEM_POOL_ALLOC` 钩子从 PSRAM 分配，替代原片内 64 KiB 静态池；原池大小仍由 `CONFIG_LV_MEM_SIZE_KILOBYTES` 控制。失败时显式终止启动，不回退到片内内存；串口输出池大小及片内空闲 / 最大块。应用初始化位于 `display.c`，LVGL 组件的钩子由 `main/CMakeLists.txt` 注入，不修改第三方源码或 sdkconfig。屏幕内部 DMA 双缓冲仍为 172×64，LVGL 与 UI 任务栈不迁移。未编译、测试或烧录；BLE 配网、Flash/NVS 操作期间的显示和性能、池内存完整性待实机验证。

`display_init()` 配置官方 esp_lvgl_port、RGB565、172×64 行内部 DMA 双缓冲，设置 CASE 7 及 gap=(34,0)，背光关闭时清理整个控制器 RAM。官方移植层保留 LVGL 任务、时钟、SPI 刷新和 DMA 完成回调。

2026-10-19：显示初始化前移到大部分外设之前。GPIO38 先预置低电平，随后完成 ST7789/LVGL，并由 [boot_screen](../boot_screen/README.md) 显示狗图、逐行启动状态和出厂 MAC。各模块初始化释放显示锁，最后由原 UI 接管；方向、gap、缓冲与 DMA 回调未改变。本轮未编译、未上板验证。

## 2026-10-18 看门狗排查

用户报告距离主屏运行时 IDLE1 超时，CPU1 正在执行 taskLVGL。补充堆栈定位于 `lv_line_event → lv_draw_line → lv_draw_sw_line` 的绘制链路；该堆栈不是 SPI 等待现场，尚不能据此确定具体是哪条线或完整故障原因。

应用层已处理两处明确的持续负载来源：

- UI 数码段和小数点仅在显示掩码变化时设置透明度。LVGL 9.3 的本地样式 setter 会刷新样式并使对象失效，即使调用方重复传入相同值；此前 56 个数码段每 50 ms 都被重复设置。
- 隐藏页面不再更新文字与样式；页面、电池百分比和编辑颜色按变化更新。UI 轮询每轮结束后阻塞 50 ms，不在等待慢帧或 EEPROM 后连续补跑过期周期；距离历史仍按单调时钟补空缺。

显示事件记录 RENDER_START/RENDER_READY 间隔，超过 100 ms 时输出 `LVGL render cycle took ... ms`，最多每秒一次。该间隔包含框架渲染过程，不是单独某个线条函数的 CPU 时间；若渲染无法返回，完成事件不会出现，仍需结合看门狗堆栈定位。

依据补充堆栈，未保留先前对 SPI 忙等的试探性改动；没有改 SPI/DMA 完成回调、LVGL 任务优先级/亲和性或看门狗配置，没有修改托管组件及 sdkconfig。2026-10-18：用户对本次修改复测并明确确认“复测 OK”。本轮助手未代为编译、测试或烧录；未提供长时间运行及慢帧统计，不扩展为所有故障边界的验收。
