# UI 设计

2026-10-29：当前 HOME / INFO 沿用明亮配色；HOME 上 ACTUAL、下 FOCUS，两标题字体统一，侧栏仅图标，电池靠右，UPTIME 位于 INFO。设计源、预览和实现边界见 [竖屏 UI 说明](PortraitUI/README.md)，历史三版提案保留。独立 [EJECT 设计](PortraitUI/eject.svg) 使用窄侧栏，停止成功直接返回 HOME；电机斜坡及相纸结算见 [UI 模块](../code/PM_Controller/main/app/ui/README.md)。本轮未编译、测试或烧录，实屏待验证。

2026-10-20 充电状态图标已接入：HOME / EJECT 共用充电动画、上移后的充满对勾、无电池斜杠和待确认问号。资源为 [battery-icons.svg](PortraitUI/battery-icons.svg)，CHG/STB 与 USB 状态判断见 [充电状态模块](../code/PM_Controller/main/app/charge_status/README.md)。新功能未编译、测试或上板验证。

2026-10-25 吐片页取消渐变：启动成功直接进入完整亮度的绿色页面，停止成功直接返回 HOME；电机仍保留 1 s 启停斜坡，余量在最终停止斜坡目标结束后结算。静态预览不变。未编译、测试或上板验证。

2026-10-20 新增独立 [EJECT 吐片页](PortraitUI/eject.svg) 和 [预览](PortraitUI/eject.png)：绿色主体、黑色顶栏、本次用时、电流圆环及放大的吐片动画；小于 1 A 显示 `.xxx`。无 EJECTING / PRESS TO STOP。运行时隐藏 HOME，上下键不操作，中键停止成功后直接返回；原电机斜坡逻辑保留。设计、生成资源和代码已接入，未编译、测试或烧录，实屏未验证。详见 [当前竖屏 UI](PortraitUI/README.md)。

2026-10-19 新增 [开机画面](PortraitUI/boot.svg) 与 [预览](PortraitUI/boot.png)：白底黑狗圆角图、六行初始化日志，各行显示 `模块 .... [OK]` 或 `[FAIL]`，初始化中显示 `[...]`；底部为 `MAC : [出厂MAC]`。已接入主控启动；未编译、未上板验证。

2026-10-19 当前字体：标题和大距离数字使用 Space Mono Bold，小字、单位及 INFO/SETUP 使用 Roboto Mono。当前方案、预览和导出入口见 [竖屏 UI 说明](PortraitUI/README.md)。临时方案、对比图和中间生成脚本已清理；原有已跟踪历史文件保留，不作为当前固件字体来源。两种官方字体、固定来源及 OFL 1.1 许可随软件工程保存。

历史记录，2026-10-18 图标与底纹更新：已按用户确认的 [v11 SVG](PortraitUI/key-icons-v11-proposal.svg) 修改正式 UI 源码，图标保持大小、主笔画加粗至 2 px，位置后续调整见文末 2026-10-19 记录；深色连续底纹宽 17 px，上下各留 20 px，保留上下渐变、直边及两条按键分隔线。本轮未编译、未烧录，实屏效果待确认。以下为历史实现与验证记录。

2026-10-18 后续确认：用户反馈当前版本“测试 OK”，已记录到固件验证状态；下文未验证说明保留实现阶段边界，本轮提交未额外构建或测试。

2026-10-18：已接入细线图标提示、右缘上下渐隐及紧凑顶部区域。实体键从上到下更正为 KEY3、KEY2、KEY1；功能统一为 KEY3 上/右/增加、KEY1 下/左/减少、KEY2 确认/执行。当前布局预览为 [key-icons-v4-preview.png](PortraitUI/key-icons-v4-preview.png)，原 SVG 保留作前版参考；预览不是固件截图，代码未编译、未烧录，实机未验证。

原 `GuiGuider/` 目录已更名为 `UI设计/`。

| 目录 / 文件 | 内容 |
| --- | --- |
| [MainControl/](MainControl/) | 旧版 GUI Guider 1.10.1 工程及原有导出文件，完整保留供参考 |
| [PortraitUI/设计说明.md](PortraitUI/设计说明.md) | 172×320 竖屏布局、控件及接入方案 |
| [PortraitUI/portrait-ui.svg](PortraitUI/portrait-ui.svg) | 三页视觉示意，数值为设计样例 |
| [正式 UI 源码](../code/PM_Controller/main/app/ui/) | 直接实现的 LVGL 9.3 C 代码 |

2026-10-17：固件已改为启动 HOME / INFO / SETUP 三页界面，使用方向 7，KEY1–3 导航与编辑；具体按键规则见 [UI 模块说明](../code/PM_Controller/main/app/ui/README.md)。本次未通过 GUI Guider、SquareLine 或 EEZ 导出，不包含可在这些编辑器中继续编辑的新工程。布局示意中的普通大字在代码中采用七段数码字，设置页采用现有正文字体；信息页计数分行并支持滚动。

旧工程文件未改写；其中历史生成代码、图片和字体均不加入当前固件。未编译、未运行模拟器、未烧录，正式界面的字体、圆角边界和按键体验待实机确认。

主屏现为“对焦距离 / 实测距离”，下方 0–20 m 采用包含零点的对数映射，并单列 INF 端点。绿色上指针为对焦距离，蓝色下指针为实测距离；对焦接口当前借用实测快照，内部以 using_actual 标记占位来源，屏幕不显示状态行。布局图已同步，实际取值接口见 [focus_distance](../code/PM_Controller/main/app/focus_distance/README.md)。

模拟器、第三方库及临时目录随文件夹一起保留，`.gitignore` 已同步新路径。旧模拟器构建缓存包含原目录的绝对路径，今后重新运行旧工程时需由 GUI Guider 重新生成构建内容，不直接复用旧缓存。

主屏已移除距离状态文字行；近距离越界显示 NEAR，超量程/测不到按约定显示 INF。右上角电池图标已接 ADC 电压估算，未知电量以 `?` 区分空电池。INFO 显示电池电压、估算百分比及电机电流/IPROPI 电压；电量不是库仑计测量结果，电流尚不参与保护控制。

2026-10-19 调整：两条分隔线由 y=122/198 移至 y=84/236，中间区域高度由 76 px 扩大为 152 px，横向尺寸不变。所有图标保持 14×14 px；KEY2 中心 y=160，KEY3 中心 y=54，KEY1 中心 y=266；上下图标中心到各自分隔线的距离由 10 px 增至 30 px（各再向外移动 20 px）。底纹不变。v11 SVG 保留为此前确认的基准；本轮未编译、未烧录，实屏效果待确认。
