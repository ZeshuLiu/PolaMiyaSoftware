# 设置菜单代码

SETTINGS 主菜单包含 BACKLIGHT、FILM、NETWORK、SEEK ZERO 和 BACK。NETWORK 包含 WIFI、BLE REMOTE、PAIR WIFI、WEB CONFIG 和 BACK。WIFI 和 BLE REMOTE 开关由 EEPROM 保存并在开机恢复。实屏未验证。

## 分工与调用顺序

BLE REMOTE：短按 KEY2 开启或停止手机遥控，开关自动保存到 EEPROM，开机恢复。配对时显示六位 PIN，与 PAIR WIFI 互斥。详见 [BLE 遥测和手动对焦](../ble_remote/README.md)。

| 文件 | 职责 |
| --- | --- |
| `ui_menu.h/.c` | 菜单描述、取值快照、浏览 / 编辑 / 子列表状态、参数修改和动作分派，不调用 LVGL |
| `ui_menu_view.h/.c` | 卡片、数值 / 进度条、分段选项、子列表、当前值勾号、滚动及颜色，不调用业务回调 |
| `ui_settings.h/.c` | 注册实际项目，绑定背光 / 寻零 / 参数保存，生成底部状态 |
| `ui_key_hints.h/.c` | 上下、加减、确认、播放、进入和不可执行状态的实体键提示 |

`ui_start()` 在参数和 FocusUnit 初始化后调用 `ui_settings_init()`。`ui_view_create()` 持有显示锁时调用 `ui_settings_create()`，视图只创建一次，根对象由原页面切换逻辑管理。

UI 任务先处理按键、推进 FocusUnit，再在显示锁外调用 `ui_settings_refresh()` 取得 RAM 参数和动作状态；持锁后仅调用 `ui_settings_render()` 读取缓存并绘制。参数锁、EEPROM 提交和动作回调均不在渲染函数中执行。菜单不新增任务、定时器或整屏缓冲；没有变化时不重复设置文字、卡片样式或数值布局。

## 操作

- 浏览：KEY3 上移、KEY1 下移，KEY2 确认，绿色实底表示当前项目。
- 数值：KEY2 进入橙色编辑状态，KEY3 增加、KEY1 减少，KEY2 完成。背光范围 0–100%，步进 5%，每步立即更新 RAM 和 PWM。长按 KEY2 保存并结束编辑，失败保留 RAM 值及错误提示。
- 执行：KEY2 启动；寻零期间卡片蓝色、显示 RUNNING，中键提示不可重复执行。设备未就绪、故障或参数区不可写时禁用启动。BACK 返回主屏，不取消已经启动的寻零。
- 少量选项：KEY2 进入候选值编辑，上下键切换，KEY2 确认后才写 RAM。两个短选项显示分段按钮；其它短枚举显示当前候选值。长按 KEY2 放弃候选值，不保存。
- 多选项：KEY2 进入独立列表，上下键选择，KEY2 确认并返回；勾号表示当前值，橙色表示候选值。BACK 或长按 KEY2 保留原值返回。确认原值不调用写回调，不新增 dirty。
- NETWORK：短按 KEY2 进入网络卡片列表；BACK 返回 SETTINGS 并结束进行中的配网。WEB CONFIG 退出后返回 NETWORK。
- 父菜单浏览时长按 KEY2 显式保存 EEPROM。候选值未确认时不因长按而意外写入。

## 扩展项目

实际项目集中在 `ui_settings.c` 的 `settings_items[]` 和 `network_items[]`。描述表和选项字符串在菜单存续期间保持不变；读写和动作接口只在 UI 任务中调用。标题沿用当前界面的大写英文约定；16 px 标题字形覆盖大写字母、数字、空格和连字符。

| 类型 | 绑定字段 |
| --- | --- |
| `UI_MENU_NUMBER` | `.number.minimum/maximum/step/unit/read/write`，值为 int32；unit 可为空 |
| `UI_MENU_ACTION` | `.action.run`，可选 `.enabled/busy/status/detail`；detail 用于显示卡片上的开关值 |
| `UI_MENU_CHOICE` | `.choice.labels/count/read/write`，用于 ON/OFF 等少量选项 |
| `UI_MENU_SUBMENU` | 同一组 `.choice` 字段，显示独立滚动列表 |
| `UI_MENU_PAGE` | 仅标题；确认后由 `ui_settings` 切换到 NETWORK |
| `UI_MENU_BACK` | 仅标题，无业务回调 |

参数读回调为 `esp_err_t read(int32_t *value)`，写回调为 `esp_err_t write(int32_t value)`；回调修改 RAM，EEPROM 由父菜单显式保存。选项回调传递零基显示索引，实际持久化的稳定 ID / 参数值应在绑定中转换，不把显示顺序直接当成长期存储含义。新增持久化参数仍须通过原 `settings` 服务定义稳定 ID、范围和默认值。

3 项及以下使用常规卡片布局，4 项及以上使用紧凑布局，更多项目自动滚动。子列表使用 LVGL 原生垂直滚动和滚动条，普通选项每屏约 5 行，长选项按 14 px 字体换行并增加行高，不缩字号。标题使用新增 16 px `ui_font_menu_title`，参数和提示 14 px，数值 24 px；长数值降至 14 px 并在需要时另起单位行。

视觉基准及示例见 [设置菜单提案](../../../../../UI设计/PortraitUI/proposals/2026-11-02-settings-menu/README.md)。示例 KEY TONE / LANGUAGE 没有作为可操作功能加入固件。

FILM 使用选项编辑：选择 0–8 或 `A`（黑卡待吐），KEY2 确认后更新相纸状态；相纸字段由后台自动保存。未编译或实机验证。

WEB CONFIG：KEY2 进入独立信息页并请求启用 HTTP；显示 IP、随机高位端口（49152–65535） 和服务器状态。网络未连接显示 NO NETWORK，获得 IP 后自动启动；页面中再次短按 KEY2 返回并停止服务。服务停止由后台执行，退出后立即拒绝新请求。未编译、测试或烧录。
