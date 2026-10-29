# FILM_KEY 锁存

2026-10-20。U3 为 SN74AUP1G74，D 接地、/CLR 接 VAA，时钟取 U11 输出。跨域连接已按当前生产网表核对：

- GPIO6（`PIN_ESP_FILM_TRG`）接 Q1 漏极，R9 上拉至主控 +3.3 V。U3 捕获事件后 Q=0，Q1 截止，GPIO6 为高；未触发/已恢复时为低。
- GPIO7（`PIN_ESP_FILM_CLR`）经 R11 驱动 Q2，R10 将 Q2 漏极上拉到 VAA。GPIO7 为高使 Q2 导通，拉低 U3 /PRE，恢复 Q=1，使 GPIO6 回低；GPIO7 为低时释放。

| 接口 | 行为 |
| --- | --- |
| `film_latch_init()` | 一次性配置 GPIO6 输入、GPIO7 输出，输出初值为低；不清除启动前保留的事件，无内部上下拉或中断 |
| `film_latch_pending()` | GPIO6 为高返回 true；调用前须完成初始化 |
| `film_latch_clear()` | 任务上下文，GPIO7 高电平保持至少 100 ms，释放后读回 GPIO6；仍为高则返回 `ESP_ERR_INVALID_RESPONSE` |

100 ms 是软件选取的宽裕脉宽，不是已测量的硬件最小值。期间使用 `vTaskDelay()` 让出 CPU。当前唯一调用者是 [film_reload 任务](../../app/film_reload/README.md)，驱动本身不更新 UI、不控制电机、不写 EEPROM。

未编译、测试或烧录，实机有效电平、恢复效果、冷上电状态待验证。
