# FocusUnit 串口主机

U15 使用 UART2：TX GPIO18、RX GPIO17，460800 baud、8N1；采用 ESP-IDF 普通 UART 驱动，不占用 SDM50 的 UART1 / UHCI。初始化失败只记录错误，界面显示 C；UI 任务独占处理和快照读取，无新增任务。

协议见 [FocusUnit UART 协议](../../../../FocusUnit/docs/UART_PROTOCOL.md)。每次只允许一个在途请求；状态查询约 1 s 使用新序号。用户修改目标时优先发送 MoveRaw（0x13），新目标只替换尚未发送的值，不覆盖在途帧。100 ms 超时最多重发一次完全相同的帧，仍无响应标记通信无效，再定期查询状态；不自动重放结果不确定的运动。校验帧起始、版本、类型、长度、序号、命令和 CRC，坏帧不更新状态。接收缓冲先于超时处理，每轮读取有预算；发送通过非阻塞 FIFO 接口，部分发送时记录偏移，发送期限和响应期限均为 100 ms，事务期限不代表线上字节间隔。

上电先查询，不主动运动、寻零、停止或开环；无用户编辑时首次合法状态可采纳设备非负目标，不产生 MoveRaw。每次用户目标变化发一次，ACK 仅表示受理；BUSY / ERROR / 序号冲突等不当作成功，不自动重试新序号。拒绝状态保留至新目标获成功 ACK，状态查询不会将拒绝视为成功；通信失败时优先显示 C。状态超过 2.5 s 或事务失败显示通信无效。快照保存 work_state、mode、direction、duty、initialized、zero_valid、seek_zero、home_attempts、home_error_counts、position、target_raw、move_state、glide_completed；期望目标与设备最近受理目标分开保存。

未编译、测试或烧录；连线、电平、时序、丢包恢复及硬件运动均未验证。设备必须先完成寻零且零位有效，才能受理 MoveRaw；本主控尚不提供寻零命令界面。
