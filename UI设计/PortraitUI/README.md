# 当前竖屏 UI

2026-10-19。当前固件资源输入为 [home.svg](home.svg)，结合 Space Mono 与 Roboto Mono。临时方案、对比图及中间生成脚本已清理；原有已跟踪的历史布局和按键基准保留。

![当前设计预览](home.png)

| 区域 | 字体 | 目的 |
| --- | --- | --- |
| PolaMiya 标题 | Space Mono Bold，12.4 px | 保留白色，强化品牌与设备感 |
| 对焦 / 实测大数字 | Space Mono Bold，34 px | 突出关键读数，继续使用 `x.x m` / `xx m` |
| 单位、小标签、刻度、纵轴、胶片数量与运行时间 | Roboto Mono，按原设计字号 | 保持小字号清晰和数字排列整齐 |
| INFO / SETUP、INF / NEAR / 无效值、电量问号 | Roboto Mono，正文 14 px、问号 10 px | 长文字和异常提示沿用统一正文体系 |

布局沿用 v4：白色标题、左侧矩形五色标记、右移电池、内置 LVGL 折线图、三键功能及整行胶片。大数字容器为 (12,46)/(83,46)、70×44 px，基线 y=82；单位在小数与整数模式下跟随。显示进位、无效历史断线、圆点隐藏及调参行为保持。

两种字体均取自官方固定提交，采用 SIL OFL 1.1。字体原文件、版权声明、完整许可和 SHA-256 随项目保存，见 [字体许可](../../code/PM_Controller/THIRD_PARTY_NOTICES.md)。发布源码或固件包时保留两份声明和许可。

维护时直接编辑 `home.svg`。导出固件资源使用 `node export_lvgl_assets.cjs <node_modules路径> [chrome.exe路径]`；更新 `home.png` 使用 `node render_ui.cjs <node_modules路径> [chrome.exe路径]`。脚本依赖 Playwright，导出另需 Sharp；显式加载两种嵌入字体，失败时停止，不回退到系统字体。TTF 不在设备运行时加载，固件使用转换后的字形。

已渲染并目视核对小数、整数及调参版本，完成字体来源和源码引用核对。未测试、未编译、未烧录，也未运行 LVGL 模拟器；附图是设计预览，实屏一致性、刷新耗时与固件大小待手动确认。
