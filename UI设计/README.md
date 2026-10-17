# UI 设计资源

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
