2026-11-04 Wi-Fi 启动断言：JTAG 读取运行 ELF SHA256 `7165e8d87c4343f2f596da7701e8ae75b78527c519942f29938d229a62098104`，CPU0 的 `sys_evt` 在处理 `WIFI_EVENT_STA_START` 返回后释放事件循环递归互斥锁时触发 `spinlock_acquire` 断言。该锁的计数及队列字段异常，锁位于 `sys_evt` 栈下界附近；从栈填充观察到余量很低，但不能据此证明覆盖来源。`0xCDCD` 是 CPU0 的锁拥有者值，不是释放标记。为缩短事件回调，将保存网络的 NVS 读取、连接 / 重试及 Wi-Fi 和 HTTP 启停收拢到 `wifi_control` 任务；增加 `sys_evt` 和 `wifi_control` 栈余量诊断。本机事件任务栈配置未改，未编译、测试或烧录，需实机核对栈余量及是否仍触发断言。

# 运行异常定位

2026-11-02：用户提供静止界面运行约 657 s 后 Core 0 的 StoreProhibited，写入地址 `0x0039cbe0`，回溯标记 CORRUPTED，随后自动重启。崩溃固件 ELF SHA 前缀为 `8ad557bc5`，对应 ELF 已未保留；当前 ELF 为 `cad37faea257b297c3108116b3ec79bb63aaf1cf8852bca6721937a3fdca7e73`。当前 ELF 与当前 BIN 内嵌 SHA 一致，但不能代替崩溃版本进行准确符号定位。根因仍未确认，新增内容为定位措施，不是已完成故障修复的声明。

## 固件日志

主任务在应用任务初始化完毕后调用 `runtime_diagnostics_init()`，记录完整运行 ELF SHA 和应用任务句柄。随后使用既有主任务循环调用 `runtime_diagnostics_poll()`，首次及每 30 s 输出：

- `stack_min_bytes`：main、ui、LVGL、ADC、SDM50、按键和相纸恢复任务的历史最小剩余栈，单位为 ESP-IDF 定义的字节；`-1` 表示可选模块任务未创建。低于 512 B 且余量继续下降时额外警告，不能仅凭该阈值认定已经溢出。
- `internal_heap_bytes`：内部 8 位可访问堆的空闲量、历史最小量、最大空闲块与完整性状态。

完整性检查发现内部堆分配器元数据损坏时，先由 ESP-IDF 打印损坏信息，再 `abort()`，尝试保留更早的异常证据。该检查不扫描全部 PSRAM，不修改任务栈、看门狗、SPI 或 SDK 配置，也不新增任务或定时器。它不能发现所有已分配块内的非法写入，栈水位也不能替代硬件写入监视点，因此正常诊断日志不等于内存绝对安全。

诊断由主任务在显示锁外进行；它读取运行状态，不执行外设验证或测试序列。本轮只生成源码并静态核对，没有编译、运行固件或接板验证。

## 符号归档

根 CMake 在用户后续构建的链接后阶段调用 `tools/archive_firmware.py`，将 ELF 和 MAP 复制到 `.firmware_archive/<完整 ELF SHA256>/`。目录不在 build 内，执行 clean / 重建不会自动清除这些归档；没有自动删除历史版本的规则，目录已加入 Git 忽略。

脚本仅复制现有文件和记录 SHA，不构建或烧录。ELF 复制到临时文件并复核 SHA 后才改为正式归档名，既有归档不覆盖。相应 BIN 由 ESP-IDF 后续生成，不在该链接后步骤复制，避免混入上一版 BIN。

下一次异常应结合启动行 `Running ELF SHA256`、最近的 `runtime_diag` 日志和完整 panic；按 SHA 找到归档 ELF 后再做地址解析。当前归档的 `cad37faea…` 版本无法追溯修复已经丢失的 `8ad557bc5…` 文件。当前归档复制操作已经完成；新增 CMake 构建挂接尚未运行验证。

## 慢帧与本次异常

用户确认 `LVGL render cycle took 100 ms` 只在切页时偶发。它来自 RENDER_START 到 RENDER_READY 的墙钟间隔，包含渲染流程及部分传输等待，不是 ADC 转换或 EEPROM 提交耗时。单条慢帧不能解释本次内存访问异常。保留现有慢帧阈值，详细记录见 [显示说明](../display/README.md)。
