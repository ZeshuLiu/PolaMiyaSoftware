# 对焦物距接口

`focus_distance_get(const sdm50_sample_t *actual)` 在任务上下文返回对焦位置对应的物距。返回值包括毫米数、有限/无穷远/不可用状态，以及是否借用实测距离的标志；毫米数仅在 `FOCUS_DISTANCE_FINITE` 时有效。

当前占位实现使用传入的 SDM50 同一快照，并设置 `using_actual=true`：VALID 返回有限距离，NO_TARGET 按显示约定返回 INFINITY，TOO_CLOSE 及通信异常/过期返回 UNAVAILABLE。FocusUnit 接口不增加 NEAR 状态；实测太近时左侧占位值为 `--.--`，右侧为 NEAR。不主动读取 UART，不修改 FocusUnit 固件，没有实现通信或位置标定。

后续将本函数内部改为：读取 FocusUnit 最近位置 → 检查通信与寻零有效性 → 按标定关系换算为物距；无穷远以独立状态返回，失联或未标定返回不可用，不能把无效数据转成 INF。原始编码器计数不是毫米，当前不假定换算公式或比例。

2026-10-17：未测试、未编译、未烧录；FocusUnit 原工程未改动。
