2026-11-04：Wi-Fi 用户开关加入 EEPROM（ID 13），开机恢复上次状态；旧记录默认关闭，BLE 仍只在主动配网时开启。成功配网也保存 Wi-Fi 开启。每份轮换记录最多容纳 17 个参数，当前 13 个，剩余 4 个同格式字段。未编译、测试或烧录，实机未验证。

2026-11-04：HOME 长按下键 KEY1 对焦到当前标定 INF，受理后退出手动编辑；无需测距，仍遵守位置初始化和掉电保护。未编译、测试或烧录，实机未验证。

中断槽的已用、空闲及保留状态见 [中断分配记录](中断分配记录.md)；该表是 BLE 启动前的历史快照。

2026-11-04 网络重构：Wi-Fi 启停、已保存网络连接 / 重试和 HTTP 服务由同一工作任务串行处理；配网任务保留并通过队列请求 Wi-Fi。移除临时请求包装与周期性配网诊断，保留关键中断处理。未编译、测试或烧录，故障修复及实机行为待验证。

2026-11-04 Wi-Fi 启动断言：JTAG 显示系统事件任务处理 STA_START 后释放互斥锁时锁字段异常，紧邻任务栈的空间余量很低；原因尚需实机确认。网络配置读取及连接 / 重试已移到独立任务，增加栈余量日志，未编译、测试或烧录。详见 [诊断说明](main/app/diagnostics/README.md)。

2026-11-04：HOME 增加 Wi-Fi 与蓝牙状态图标，分别显示关闭、开启未连接和已连接状态；蓝牙仅表示配网及 BLE 连接，不代表握手已完成。未编译、测试或烧录，实屏未验证。

2026-11-04：对焦五项标定参数存入 EEPROM，SETTINGS / WEB CONFIG 进入后显示 IP 与随机高位端口（49152–65535） 并开启无认证网页，退出关闭；网页保存整组校准，不自动移动镜头。BLE 配网前在 CPU1 预初始化 AES-GCM，尝试把 AES 中断分配至 CPU1。以上均未编译、测试或烧录，实机未验证。

2026-11-04 配网卡死定位：JTAG 显示 NimBLE 主机任务解析握手时越过 4096 字节栈并损坏相邻堆数据；用户单次授权将本机该任务栈设为 10240 字节。未重新配置、编译或烧录，配网和剩余栈待实机验证。详见 [配网说明](main/app/wifi_control/PROVISIONING.md)。

2026-11-04 配网日志补充：BLE 连接后 NimBLE 最小栈余量 1248 字节，最后记录未显示 Security 2 公钥计算；新增请求入口 / 返回诊断以缩小卡死位置，不输出负载。未编译或实机验证。

2026-11-04 配网卡死排查：实机 BLE 连接和 MTU 256 协商成功，之后整机无响应，尚无握手或 panic 证据。新增配网期 NimBLE 栈和内存诊断；相纸任务因历史最小栈余量仅 300 字节而增栈，二者的因果关系未确认。未编译、测试或烧录。

2026-11-04 吐片页面：接入上方出片、底部计时方案，中部电流圆环与刻度同步下移，相纸向上移出并淡出；资源已重新生成。未编译、测试或烧录，实屏未验证。详见 [UI 说明](main/app/ui/README.md)。

2026-11-04 相纸处理：换盒检测后显示 A 表示黑卡待吐；吐片时长严格超过 2 s 后变为 8，之后按同一阈值扣减。吐片及停止斜坡期间屏蔽快门和相纸检测，结束时清除锁存；设置页 FILM 可选择 0–8 或 A 并自动保存相纸字段。未编译、测试或烧录，实机未验证。详见 [相纸处理](main/app/film_reload/README.md)。

2026-11-10：设置菜单新增 BLE REMOTE，用手机小程序读取实测距离和实际对焦距离，手动设置对焦距离或无穷远。通过相机屏幕 PIN 配对后可发送命令，与 PAIR WIFI 互斥。固件未编译、烧录或实机验证。详见 [BLE 遥测与对焦](main/app/ble_remote/README.md)。

BLE REMOTE 开关存入 EEPROM（ID 14），旧记录默认关闭，原 Wi-Fi ID 13 保持独立。开机恢复后保持广播供小程序重连。本机已启用 `CONFIG_BT_NIMBLE_NVS_PERSIST=y`，绑定密钥由 NimBLE 保存到 NVS。配对失败由后台断开并恢复广播，卡死根因及改动效果待串口日志和实机验证。

# PM_Controller 固件

网络设置集中在 SETTINGS → NETWORK，包含 WIFI、BLE REMOTE、PAIR WIFI 和 WEB CONFIG。操作见 [设置菜单](main/app/ui/MENU.md)。菜单改动未编译、烧录或实屏验证。

2026-11-02 PWM 启动占用警告：GPIO1 电机 IN1 和 GPIO9 背光改为公开 GPIO 接口预置安全高电平，由各自 LEDC 通道统一登记输出占用，消除 `gpio_config()` 与 LEDC 重复申领引起的警告。GPIO2 仍为普通高电平输出；电机制动、背光关闭的启动顺序及原 PWM 参数保留。仅源码静态核对，未编译、测试或烧录，波形实机未验证。见 [吐片电机](main/drivers/film_motor/README.md) 和 [背光](main/drivers/st7789/README.md)。

2026-11-02 GPIO38 启动警告：闪光输出预置与完整初始化改为复用同一次 GPIO 配置，消除同一模块重复预留输出脚导致的冲突警告；仍在屏幕初始化前拉低输出。仅源码静态核对，未编译、测试或烧录，实机未验证。见 [闪光驱动](main/drivers/flash_trigger/README.md)。

2026-11-02 异常定位：用户提供静止约 657 s 后 StoreProhibited / CORRUPTED 回溯；旧固件 SHA 为 `8ad557bc5`，对应 ELF 未保留，当前 ELF 为另一版，暂不能准确符号定位或确认根因。已增加低频内部堆 / 任务栈日志及启动 ELF SHA，后续用户构建将在链接后按 SHA 保存 ELF / MAP；当前已有构建的符号归档已完成。此为定位措施，未声明故障已修复；未编译、运行固件测试或烧录。详见 [异常定位说明](main/app/diagnostics/README.md)。

2026-11-02 设置页代码生成：确认的卡片界面已接入固件，新增数值 / 执行 / 少量选项 / 多选项子菜单的统一模型、LVGL 渲染及按键处理。当前只注册背光、寻零和返回；ON/OFF 和语言示例仍为设计说明。背光即时调节及长按保存保留，寻零忙状态和执行可用性已接入；子菜单支持滚动、候选确认和保留原值返回。参数与 EEPROM 操作均在显示锁外。详见 [菜单模块](main/app/ui/MENU.md)。已同步 CMake、字形与文档，只做静态核对和资源生成，未编译、测试或烧录。

2026-11-02：充电电池改为按当前估算电量静态填充，左侧显示 8×14 px 大闪电，取消循环动画和动画时基；HOME / EJECT 共用实现，电池本体位置不变、不显示百分比数字。图形资源及 [充电状态预览](../../UI设计/PortraitUI/proposals/2026-11-02-home-readable/charging-preview.png) 已同步。仅设计渲染和静态核对，未编译、测试或烧录，实屏未验证。

2026-11-02：HOME 两处距离单位 m 放大至 20 px，独立字形和控件在长数值缩小显示时也保持此字号；调整间距并与距离基线对齐。已更新资源和设计预览，未编译、测试或烧录，实屏未验证。

2026-11-02 HOME 可读性：主界面小字下限为 14 px，标尺及 FILM 放大，对焦短提示移入 FOCUS 卡片；移除主屏电池百分比，仅保留图标。新增设置页状态和保存提示也改为 14 px。正式 SVG、固件静态图块及 [三状态预览](../../UI设计/PortraitUI/proposals/2026-11-02-home-readable/preview.png) 已同步。已做设计渲染、视觉及源码静态核对，未编译、测试或烧录，实屏未验证。

2026-11-02 构建修复：针对 ESP-IDF 6.1.0、NimBLE 动态模式且 CoC / EATT 均为零时的 `ble_l2cap_coc_deinit` 缺失，提供受配置限制的空函数兼容定义；待重新编译验证。

2026-11-03 配网断线排查：HTTP 服务启停移出默认事件循环，BLE 成功后的停止由配网后台任务在 30 秒结果读取窗口后执行；未成功的清理仍由配网任务处理。针对同步等待风险，既有卡死根因仍未确认；未编译或上板。

2026-11-03 配网兼容：Security 2 用户名改为乐鑫 Android App 默认的 `wifiprov`；手机断开原因仍待实机确认。未编译、测试或烧录。

2026-11-02 配网失败：用户反馈手机输入 PIN 后提示设备断开。增加 BLE 连接 / 断开原因、Security 2 握手及清理阶段日志；Wi-Fi 凭据失败后暂保留 BLE 供手机读取。启动 BLE 前对控制器核心进行 RWBLE 中断可用性预检（非预留，不能排除竞争）。根因尚未确认，未编译或上板。

2026-11-02 BLE 中断：用户日志显示 CPU0 RWBLE 三级 IRAM 中断无可用输入并触发看门狗。FocusUnit UART2 初始化改由 CPU1 固定任务执行，BLE 启动前打印双核中断分配表；是否释放出三级中断及串口可靠性待实机验证。未重新编译或烧录。

2026-11-02 LVGL PSRAM：LVGL 内建 TLSF 池通过分配钩子改由 PSRAM 提供，原静态片内池在当前配置为 64 KiB；保持 172×64 内部 DMA 双缓冲及任务栈。未编译或上板，内存布局、BLE 和显示性能待确认。

2026-11-02 BLE 内存：实机已启用 Wi-Fi/LwIP 及 NimBLE PSRAM 选项，但 BLE controller 初始化仍因 INTERNAL|DMA 分配失败；配网管理器前内部 free=16495 / largest=7680。新增启动前后 DMA 内存和任务栈日志，建议针对单手机配网在 menuconfig 收紧 BLE 活动数；不改屏幕 64 行缓冲，未重新编译。

2026-11-02 Wi-Fi / 蓝牙 PSRAM：按用户要求保持 64 行屏幕双缓冲；为 Wi-Fi PSRAM 优先选项补充静态 TX 上限并记录 NimBLE 外部内存配置方案。配网 8192 字节任务栈仍使用内部 RAM，效果待用户配置及实测。

2026-11-02 Wi-Fi 内存修正：实机日志显示静态 RX 缓冲 10 个仅分配 9 个，`esp_wifi_init` 返回 `ESP_ERR_NO_MEM`；本应用初始化时将静态 RX 上限调为 4 并匹配 RX BA 窗口，动态 RX / TX 上限 16，不改 sdkconfig。未重新编译或上板，连接与 BLE 内存余量待确认。

2026-11-02 Wi-Fi 卡死排查：WIFI 开关的初始化 / 启停移至独立后台任务，配网启动不再在 UI 任务初始化 Wi-Fi；增加菜单确认、任务创建、启动阶段及内部堆日志。根因仍待串口和实机确认；未重新编译或烧录。

2026-11-02 Wi-Fi：SETTINGS 可启停客户端并主动开启蓝牙配网，Security 2 验证码显示在配网项；成功连接后保存最多三组 NVS 网络，进入 WEB CONFIG 后启动局域网标定网页。首次使用、手机 App 兼容、存储和 Web 均未编译、测试或烧录，实机未验证；详细限制见 [Wi-Fi](main/app/wifi_control/PROVISIONING.md)。

2026-11-02 对焦显示修正：无穷远目标 RAW 1016 附近 ±50 RAW 按 INF 显示，避免反算渐近线产生数百米；RAW 0 等超出标定范围的位置显示 `--`，不因此驱动电机。未编译、测试或烧录，实机未验证。

2026-11-02 对焦标定：设备 90DA724F2FBC 的拟合参数保存在独立头文件；FOCUS 短按 KEY2 根据测距执行一次连续公式对焦，蓝字显示；不足 1 m 限制到 1 m 并红字闪烁，测距无目标按 INF 对焦；长按进入手动模式，手动模式下短按退出，手动使用 1–10 m 的 17 个圆整对数距离，以及 15 m、20 m 和无穷远档位。HOME 的 FOCUS 为目标距离，INFO 显示根据当前位置反算的对焦距离。9 m 以上为外推；未编译、测试或烧录，实机和跨设备适用性未验证。详见 [对焦物距](main/app/focus_distance/README.md)。

2026-11-02 对焦响应提速：活动状态查询间隔由 200 ms 改为 100 ms，空闲为 1 s；控制帧立即提交 UART，收到到位状态后立即发 Stop，连续调整保留最新目标并跳过中间位置保存。运动前失效写入、最终 RAW 稳定确认及 EEPROM 完整校验保留；组件内 1 s 到位稳定时间未改，EEPROM 仍为同步提交。仅源码静态核对，未编译、测试或烧录，实际耗时和改善待实测。

2026-11-02 历史对焦步进（已被标定距离档位取代）：每次按键增减 500 RAW，显示 0.01 m 对应 500 RAW，即 RAW / 50000；数字与标尺同步。串口及 EEPROM 仍保存原始 RAW，不迁移已有值。仅静态核对，未编译、测试或烧录。

2026-11-02 调焦显示修正：发送 MoveRaw / Stop 等控制时保留尚未过期的有效状态，避免正常等待响应和新查询时误闪 C；完成判断仍要求命令后的新状态，通信失败和过期提示保留。仅静态核对，未编译、测试或烧录，实机未验证。

2026-11-02 对焦：适配新的外部初始化协议，开机有效 EEPROM RAW 用 SetCurrentRaw（0x14）恢复；旧记录、无效位置或恢复失败提示初始化。运动前先提交并验证位置失效记录，到位后发 Stop 取消持续纠偏，两次新状态确认 STOP / RAW 稳定后保存实际位置；复用 CRC32、轮换记录、写后读回与最后提交标记。SETTINGS 加入 SEEK ZERO，与 BACKLIGHT 同级，上下键选项、中键编辑或执行，BACK 返回主屏。未编译、测试或烧录，实机未验证。详见 [对焦位置](main/app/focus_position/README.md)。

2026-10-20 充电显示：新增 USB GPIO21 读取和 CHG/STB 组合状态；HC165 的 5 ms 扫描保留 CHG 脉冲历史，UI 持续观察并做稳定判断。HOME / EJECT 共用普通电量、充电动画、充满对勾、无电池斜杠与待确认问号；充满对勾按预览上移 1 px。INFO 显示对应状态。双高不直接判定无电池，重复脉冲满足条件才显示无电池；整板负载下的实际特征待实测。不新增任务、定时器或整屏图片，未编译、测试或烧录。见 [充电状态](main/app/charge_status/README.md)。

2026-10-20 相纸余量：新增 EEPROM 参数 ID 5（0–8），开机读取保存值，旧记录缺项默认 6。FILM_KEY 延时恢复成功后设为 A（黑卡待吐）并保存；每次吐片从首次启动到最终停止斜坡目标结束严格超过 2 s 时从 A 变 8 或扣 1 张，最低 0，完成后写 EEPROM。中途反向切换仍视为同一次动作。相纸自动保存只提交此字段，不保存尚未确认的背光修改；失败由低优先级任务每秒重试当前值，不重复扣减。保留记录格式、CRC 和轮换机制，未编译、测试或烧录，实机未验证。

FILM_KEY：优先级 1 的低优先级任务每秒读取 GPIO6，发现事件后等待约 1 s；GPIO7 发出至少 100 ms 高脉冲恢复锁存，读回成功后将相纸状态设为 A（黑卡待吐）。初始化不清除旧事件；失败记录日志，余量不变，并继续轮询重试。任务通过参数服务更新并保存相纸数量，UI 线程负责显示；不控制吐片电机。GPIO 驱动及应用分别见 [film_latch](main/drivers/film_latch/README.md) / [film_reload](main/app/film_reload/README.md)。未编译、测试或烧录，实机未验证。

统计：重启计数由 `app_main()` 在参数加载成功后加 1 并提交一次。快门首次下降沿立即触发并计数，随后 1 s 内抑制；主任务约每秒更新计数并单独保存，失败时重试。`settings_init()` 保持只读，EEPROM 布局不变。INFO 电机累计工作时长按整秒加 `s` 显示，存储仍为毫秒；电机时长的业务累计尚未接入。未编译、测试或烧录，实机未验证。

2026-10-20：EJECT 已改为独立页面，启动成功后隐藏 HOME，显示绿色主体、黑色顶栏、本次吐片计时和实时电流圆环。小于 1 A 显示 `.xxx`，上方为循环吐片动画，底部显示计时，无 EJECTING / PRESS TO STOP。2026-10-25 已取消页面渐变：启动成功直接进入，停止成功直接返回 HOME；电机仍保留 1 s 启停斜坡，相纸在停止斜坡目标结束后结算；上下键不操作。只刷新当前页，沿用 50 ms UI 周期，无新任务、GIF 或整屏位图；SDK 配置和电机驱动未改动。源码、资源及设计预览已更新，未编译、测试或烧录，实屏未验证。详见 [UI 说明](main/app/ui/README.md)。

2026-10-19 全量保存工作区：同步 HOME 上下双距离布局、移除时域曲线、字形与图标资源、设计源及预览；一并收录已有 `sdkconfig.defaults` 的 Montserrat 8/10/12 字号开关，不改动本机 `sdkconfig`。本轮未代为编译、测试或烧录，当前布局实屏效果仍待确认。

2026-10-19 吐片电机：HOME 增加 EJECT，KEY2 选中运转、再短按取消选中并停止；运行时保持此选项，上下键无操作。GPIO1 反向 PWM、GPIO2 恒高，20 kHz，运行目标为 GPIO1 实际高电平占比 0%（全驱动）；启停均为目标 1 s 的非阻塞硬件斜坡，停止斜坡结束后双高制动。启动先初始化为制动，使用独立 LEDC Timer 1 / Channel 1。用户实机确认启动冲转及随后约半速的问题来自驱动芯片限流，解除电流限制后运行正常；保留斜坡逻辑和启停占空比、频率日志，具体解除方式及采样有效性待记录。仅接入手动启停，自动吐片流程、事件锁存及计数尚未接入；本轮未代为编译、测试或烧录。见 [吐片电机](main/drivers/film_motor/README.md)。

2026-10-19 开机显示：先预置闪光输出低电平，再初始化 ST7789/LVGL 并显示白底黑狗圆角图、逐行启动日志与 `MAC : [出厂MAC]`。狗图上移到 y=48，日志采用 9 px Roboto Mono Medium、20 px 行距和对齐的状态标记。开机图像仅保留 RGB565 小标题与 A8 狗图，其余形状由 LVGL 绘制；启动字体按需导出字符，图像和字形像素比整屏位图版减少约 103.7 KiB，最终固件变化待手动编译确认。随后初始化触发输入输出、按键、测距、存储和 ADC，完成后进入原 UI。可继续启动的失败显示警告，详情见 [开机画面说明](main/app/boot_screen/README.md)。本轮未编译、未测试、未烧录。

2026-10-29 历史记录：FOCUS 当时按 1000 RAW 步进发送 FocusUnit MoveRaw，显示 RAW / 100000 两位小数，m 仅为未标定占位；INFO 当时显示设备状态和 RAW 位置。2026-11-02 后续已改为标定距离及手动档位，见本文开头。

2026-10-29：HOME 上方为深蓝 ACTUAL 实测卡，下方为鲜绿 FOCUS 对焦卡，距离使用 Space Mono Bold 50 px；保留距离状态、即时对数标尺和相纸余量，UPTIME 移至 INFO。两标题共用 Roboto Mono Bold 16 px，HOME 电池主体位于 x=143；HOME / INFO / SETUP 采用 36 px 纯图标侧栏，INFO / SETUP 横向切页提示使用左右箭头；独立 EJECT 保留窄侧栏。设计及动态资源已更新，未编译、测试或烧录，实屏待验证。详见 [UI 说明](main/app/ui/README.md)。

2026-10-19 历史资源精简记录（本次重排前）：追溯到 `448b0f7` 引入的整屏背景与九张胶片条，改为保留原像素的窄条和亮 / 暗单张素材，布局及 0–8 张胶片显示保持。继开机画面优化后，主界面背景与胶片像素再减少 132048 字节（约 129.0 KiB）；所有生成图像和字形像素合计约 67.4 KiB，不含描述表及代码。未修改编译优化、SDK 配置或字体现有外观；本轮未编译、未上板验证。详细统计见 [UI 说明](main/app/ui/README.md)。

2026-10-18：用户确认当前竖屏 UI、三键映射和 ADC 接入版本“测试 OK”，据此保存本地 Git 版本。本轮未代为构建、测试或烧录；ADC 实际采样率、计量精度和其它专项指标未提供量化记录。具体按键功能统一在 [ui.c 的 handle_keys()](main/app/ui/ui.c) 中实现，扫描识别与图标显示分别由 HC165 驱动和 UI 视图负责。

同日后续：用户报告距离主屏 IDLE1 看门狗，补充堆栈停在 `lv_draw_sw_line()`。本轮已减少无变化数码段/样式重绘和隐藏页更新、取消 UI 过期周期补跑，并增加慢帧日志；没有调整看门狗或 SDK 配置。用户已复测并确认 OK；本轮未代为编译、测试或烧录，详见 [显示排查记录](main/app/display/README.md)。

对应 PCB：PM_Controller Rev2.x，当前为 Rev2.0。模组：ESP32-S3-WROOM-1-N8R8，8 MB Flash、8 MB Octal PSRAM。开发环境为 ESP-IDF 6.1。当前 FreeRTOS tick 为 1 kHz（`CONFIG_FREERTOS_HZ=1000`）；按键扫描目标周期 5 ms，EEPROM 忙等待延时为 1 tick。消抖、长按、LVGL 定时与 GPTimer 目标脉宽仍按各自的时间单位配置。

`app_main()` 先将闪光输出预置低电平、初始化吐片电机为制动，再完成屏幕和开机画面。随后创建闪光定时器、接入 GPIO42 快门下降沿中断、启动 74HC165 扫描、初始化测距、加载 24C64 参数并启动 ADC，逐阶段追加状态行，最后进入三页 UI。HOME 使用 KEY1–KEY3 滚动功能列表和执行/调参；INFO 使用横向切页；SETUP 使用上下菜单选择、中键编辑背光或执行寻零，BACK 返回 HOME，长按中键显式保存。INFO 数值不滚动，较长时缩小字号完整显示。KEY4–KEY6 暂未接入业务。主程序每 1 s 打印测距及 ADC/电源快照。参数修改与持久化提交分开。引脚和双 OTA 分区已整理；电机已接手动启停，自动吐片和 OTA 尚未接入；Wi-Fi 与蓝牙配网源码已接入、尚未实机验证。

2026-10-07：按用户要求移除本地主机测试、桩及测试缓存，保留固件内的屏幕方向与偏移测试。下文主机测试结果为清理前的验证记录。

2026-10-15：参数加载合并重复校验，编码使用入口已验证的 RAM 值，三阶段保存共用写入回读函数；保留稳定 ID、版本兼容、轮换记录和旧版导入。按键扫描与背光开关移除固定 GPIO 操作的逐次错误检查，初始化检查保留。HC165 改为 SPI3、Mode 2、500 kHz 硬件读取并启用 DMA，GPIO 单独控制 /PL，按键事件规则不变。本轮用户确认编译通过；未运行测试，未上板验证。

2026-10-15 提交状态：用户确认当前固件可编译，未上板验证；本轮未代为构建或运行测试。`.clangd` 保存编辑器参数过滤和 Picolibc 头文件路径；本机 `.vscode/settings.json` 按现有规则忽略，其中使用 Espressif clangd 并关闭重复 IntelliSense。工具链升级或换机后需调整路径。

2026-10-17：新增启动阶段、快门 GPIO 初始化及按键日志。用户上板确认固件可以运行，按键日志正常，硬件初步工作；不代表各模块已完成验收。本机 IPC 任务栈为 4096 字节，主任务栈为 8192 字节，按键扫描任务栈为 4096 字节；前两项保存在未跟踪的 `sdkconfig` 中，换环境需手动核对。本轮未代为构建或运行测试。

## 工程文件

2026-10-15：新增 [SDM50 测距驱动](main/drivers/sdm50/README.md)，UART1（GPIO4/5）、460800 baud、UHCI DMA 收发。DMA 中断直接解析并更新距离，同核任务负责 EOF 后重新挂载；提供启停应答和带时间戳的结果快照。开机初始化并尝试开启测距，失败记录警告后继续启动。用户确认编译通过；未运行测试，未上板验证。

| 文件 | 用途 |
| --- | --- |
| [AGENT.md](AGENT.md) | 开发约定、接口约束、执行限制及验证边界 |
| [main/main.c](main/main.c) | 程序入口 |
| [main/PIN.h](main/PIN.h) | PCB 引脚定义，只记录连接，不初始化 GPIO |
| [main/drivers/st7789/](main/drivers/st7789/) | SPI、官方 ST7789 驱动初始化和背光 |
| [main/drivers/hc165/](main/drivers/hc165/README.md) | 74HC165 读取、6 个按键消抖及长短按标志位 |
| [main/drivers/eeprom/](main/drivers/eeprom/README.md) | 官方 I²C、24C64 分页读写及写完成轮询 |
| [main/drivers/flash_trigger/](main/drivers/flash_trigger/README.md) | GPIO 直接触发、GPTimer 中断结束、忙状态及默认低电平 |
| [main/drivers/sdm50/](main/drivers/sdm50/README.md) | UART1/UHCI DMA 测距、中断更新结果和启停应答 |
| [main/drivers/adc_measure/](main/drivers/adc_measure/README.md) | ADC1 最低速连续 DMA、两路校准与滤波快照 |
| [main/app/shutter/](main/app/shutter/) | GPIO42 快门下降沿 ISR 直接触发闪光 |
| [main/drivers/film_latch/](main/drivers/film_latch/README.md) | FILM_KEY 锁存输入、恢复脉冲及读回确认 |
| [main/app/film_reload/](main/app/film_reload/README.md) | 优先级 1、约 1 s 后恢复锁存并将相纸状态设为 A（黑卡待吐） |
| [main/app/settings/](main/app/settings/README.md) | 参数定义、RAM 缓存、CRC、轮换保存与旧版导入 |
| [main/app/display/](main/app/display/) | 官方 esp_lvgl_port 初始化和显示配置 |
| [main/app/boot_screen/](main/app/boot_screen/) | 狗图、逐行启动状态和出厂 MAC；完成后交给原 UI |
| [main/app/ui/](main/app/ui/README.md) | CASE 7 竖屏 UI、测距曲线、KEY1–3 菜单及背光设置 |
| [main/drivers/focus_unit/](main/drivers/focus_unit/README.md) | FocusUnit UART2 状态、MoveRaw、SetCurrentRaw、寻零和 Stop |
| [main/app/focus_position/](main/app/focus_position/README.md) | EEPROM 位置恢复、运动前失效、完成保存与初始化状态 |
| [main/app/ble_remote/](main/app/ble_remote/README.md) | BLE 遥测、手机手动对焦和命令回执 |
| [main/app/focus_distance/](main/app/focus_distance/README.md) | 标定物距、手动档位、自动对焦目标与状态显示 |
| [main/app/power_monitor/](main/app/power_monitor/README.md) | 电池电压、单节锂电池电量估算和 IPROPI 电流换算 |
| [main/drivers/usb_power/](main/drivers/usb_power/README.md) | GPIO21 / TPS3808 有效外电检测 |
| [main/app/charge_status/](main/app/charge_status/README.md) | CHG/STB、USB 和脉冲历史组合判定充电、充满、无电池和待确认 |
| [main/app/lcd_test/](main/app/lcd_test/) | LVGL 测试控件和定时切换 |
| [main/idf_component.yml](main/idf_component.yml) | 固定官方移植组件和 LVGL 版本 |
| [partitions.csv](partitions.csv) | 8 MB Flash 自定义分区表 |
| CMakeLists.txt、main/CMakeLists.txt | ESP-IDF 构建配置 |
| sdkconfig | 本机配置，不纳入 Git；当前 UI 字形由项目资源提供，无需新增 SDK 内置字体开关 |

测距边界显示：合法返回值小于 50 mm 时实测显示 NEAR，下指针停在零端点；NO_TARGET（50000 及以上返回值）按约定显示 INF，指针移到无穷远。20～49.999 m 仍显示真实数值，只把指针限在 20 m。过期与通信错误仍显示短横线；FocusUnit 接口不增加 NEAR 状态。主屏移除距离状态文字行，BAT 改为电池图标，电量数据不可用时显示 `?`。此次改动未编译、未上板验证。

## 引脚

下表按当前 [PCB](../../../PolaMiyaHardware/电路/主控器/PM_Controller/PM_Controller.kicad_pcb) 的 U1 焊盘网络核对。数字是 ESP32 GPIO 编号，不是模组焊盘号。

| PIN.h 定义 | GPIO | PCB 网络 / 连接 |
| --- | --- | --- |
| PIN_MOTORPWM_A | 1 | MotorPWM_A，DRV8251A IN1 |
| PIN_MOTOR_IN2 | 2 | MotorIN2，DRV8251A IN2 |
| PIN_ADC_MT | 8 | ADC_MT，电机电流采样 |
| PIN_ADC_BAT | 3 | ADC_BAT，电池电压采样 |
| PIN_USBPWR_DET | 21 | USBPWR_DET，USB 供电检测 |
| PIN_ESP_FILM_TRG | 6 | ESP_FILM_TRG，吐片事件锁存读取，经 Q1 反相 |
| PIN_ESP_FILM_CLR | 7 | ESP_FILM_CLR，经 Q2 控制锁存恢复 |
| PIN_LCD_BLK | 9 | LCD_BLK，背光控制，低电平点亮 |
| PIN_LCD_RES | 10 | LCD_RES，屏幕复位 |
| PIN_LCD_DC | 11 | LCD_DC，数据 / 命令 |
| PIN_LCD_MOSI | 12 | LCD_MOSI，屏幕 SPI 数据 |
| PIN_LCD_CLK | 13 | LCD_CLK，屏幕 SPI 时钟 |
| PIN_LCD_CS | 14 | LCD_CS，屏幕片选 |
| PIN_ESP_SCL | 15 | ESP_SCL，EEPROM I2C 时钟 |
| PIN_ESP_SDA | 16 | ESP_SDA，EEPROM I2C 数据 |
| PIN_ESP_TXD0 | 43 | 经 R47 接 ESP_TXD0，J1.2 |
| PIN_ESP_RXD0 | 44 | ESP_RXD0，J1.3 |
| PIN_ESP_TXD1 | 4 | 经 R49 接 ESP_TXD1，U9.2 |
| PIN_ESP_RXD1 | 5 | ESP_RXD1，U9.3 |
| PIN_ESP_TXD2 | 18 | 经 R48 接 ESP_TXD2，U15.2 |
| PIN_ESP_RXD2 | 17 | ESP_RXD2，U15.3 |
| PIN_MASPI_CS | 39 | MASPI_CS，74HC165 的 /PL，低电平装载 |
| PIN_MASPI_CLK | 40 | MASPI_CLK，74HC165 时钟 |
| PIN_MASPI_MISO | 41 | MASPI_MISO，74HC165 Q7 |
| PIN_FLASH_TRG | 38 | FLASH_TRG，闪光同步光耦 |
| PIN_SHUT_TRG | 42 | SHUT_TRG，快门接口 H2.2 |
| PIN_USB_DM | 19 | USB D−，经 R28 接 USB 接口 |
| PIN_USB_DP | 20 | USB D+，经 R29 接 USB 接口 |
| PIN_ESP_BOOT | 0 | ESP_BOOT，BOOT 按键及启动配置 |

74HC165 的并行输入 D0–D7 依次为 KEY1、KEY2、KEY3、KEY4、KEY5、KEY6、BAT_CHG_OD、BAT_STB_OD。这些信号不直接占用 MCU GPIO。MASPI_CS 实际是 /PL，不能按普通 SPI 片选处理。

74HC165 驱动已接入启动流程，使用 SPI3（Mode 2、500 kHz、8 bit、DMA 接收、轮询等待完成），与屏幕 SPI2 独立。6 个按键采用 20 ms 消抖，短按在松开时触发，长按在持续达到 1 s 时触发一次；事件通过 `hc165_get_events()` 或 `hc165_take_events()` 读取。接口与使用示例见[驱动说明](main/drivers/hc165/README.md)，各驱动进度见[驱动实现状态](驱动实现状态.md)。

- GPIO35/36/37 由 N8R8 的 PSRAM 占用，不能用于外设。
- GPIO45/46/47/48 在本 PCB 上未接。GPIO45/46 是启动配置脚，不作为普通空闲引脚使用。
- GPIO0 是 BOOT 启动配置脚；GPIO3 同时影响 JTAG 选择。使用电池 ADC 时不要随意改变相关 eFuse。
- GPIO39–42 已用于板上功能，调试使用 USB Serial/JTAG。GPIO19/20 使用 USB 时不能另作 GPIO。
- ESP_EN 是模组复位输入，不是 MCU GPIO，没有放入 PIN.h。

## 显示屏

历史记录，2026-10-18：右侧按上 KEY3、中 KEY2、下 KEY1 显示图标，随浏览/编辑切换；16 px 提示区和淡化渐变背景代替文字按键提示，顶部区域收至 30 px，电池主体 19×10 px。按键统一为 KEY3 上/右/增加、KEY1 下/左/减少、KEY2 确认/执行；图标高度为示意位置，字体、渐变与实物对齐待用户上板确认，本轮未编译、未测试、未烧录。

ADC 已接入：GPIO3 电池电压、GPIO8 电机 IPROPI，使用官方连续 DMA 和 curve-fitting 校准，按 SDK 最低总采样率 611 Hz、两路交替约各 305.5 Hz；每路 31 样本生成约 100 ms 快照。主屏图标与 INFO 电压/估算电量、电机 mA/IPROPI mV 已接实际 ADC 快照；百分比按用户确认的单节 4.2 V 锂电池通用电压节点估算，未做电芯标定。未实现低压关机、过流或堵转保护。初始化失败时保留界面并显示未知。实现及误差边界见 [电源读数](main/app/power_monitor/README.md)，本轮未编译、未测试、未烧录。

- 驱动芯片：ST7789。
- 屏幕原始尺寸：长 320、高 172 像素。
- 安装与显示方向：竖屏，显示坐标按宽 172、高 320 像素使用。
- 接口：SPI，使用 PIN.h 中的 LCD_MOSI、LCD_CLK、LCD_CS、LCD_DC、LCD_RES 和 LCD_BLK 引脚。
- 背光低电平点亮，使用 LEDC 低速 Timer 0 / Channel 0、20 kHz、10 bit PWM。开启时约 300 ms 渐亮至设置亮度，关闭时停止渐变并立即熄灭；UI 切页不重新渐亮。设置页修改 RAM 亮度并立即应用，长按 KEY2 保存。显示方向与窗口固定为 CASE 7（ROT 180、X34/Y0）。本次背光百分比接入未上板验证。

### LVGL 接入

依赖固定为 `espressif/esp_lvgl_port 2.9.0` 和 `lvgl/lvgl 9.3.0`，与本机 ESP-IDF SPI 屏幕示例及 GUI Guider 的 LVGL 主次版本一致。组件管理器在配置工程时下载依赖，不复制旧工程的 LVGL 源码。

调用顺序：`app_main()` → `flash_trigger_init()` → `shutter_init()` → `hc165_init()` → `sdm50_init()` / `sdm50_start()` → `settings_init()` → `adc_measure_init()` → `lcd_st7789_init()` → `display_init()` → `ui_start()`。闪光输出、按键和显示初始化失败通过 `ESP_ERROR_CHECK` 停止启动；测距初始化或开启失败、参数存储失败、ADC 初始化失败记录警告并继续显示启动。参数锁成功分配后，器件错误时仍可使用 RAM 默认值，持久化提交被禁止。初始化接口只在开机时调用一次，不提供运行中销毁或重复初始化接口。

使用 ESP-IDF 官方 `esp_lcd_new_panel_st7789()`，SPI2、模式 0、40 MHz、RGB565。板级文件只配置引脚、SPI 和背光，直接输出官方 `panel`、`io` 句柄。LVGL 任务、时钟、刷新、字节交换、DMA 完成回调和硬件旋转由官方移植组件管理。

LVGL 使用两个内部 DMA 绘图缓冲区，每个 172×64 像素、22,016 字节，共 44,032 字节（43 KiB），SPI 最大传输长度使用同一宏。buff_dma=true、buff_spiram=false，不启用 psram_dma_direct。使用局部刷新，帧率测试主动触发整屏刷新。操作 LVGL 时使用 lvgl_port_lock()/lvgl_port_unlock()；定时器回调已经持锁。

### 帧率与圆角测试

2026-10-17：启动入口已改为 [三页业务 UI](main/app/ui/README.md)，方向/偏移和完整显存清屏移至 `display_init()`；正式 UI 使用默认 LVGL 刷新定时器。以下屏幕测试源码仍保留，默认不启动，不与业务 UI 同时运行。新界面未编译、未测试、未烧录。

用户已确认 CASE 1、4、7、10 的方向/偏移正确。当前固定 CASE 7：ROT 180、172×320、X34/Y0；SPI 保持 40 MHz 和 DMA。

测试每 10 秒交替显示两页，启动先显示帧率页：

- 帧率页：红绿蓝背景上两条高对比移动条，显示帧号、FPS、平均帧间隔 AVG 和最大帧间隔 MAX。移动按实际时间计算。每帧强制刷新完整 172×320，调用 `lv_refr_now()` 后等待最后一批 SPI DMA 完成，再计为一帧；每约 1 秒更新屏幕统计并输出串口日志。
- 圆角页：显示距边缘 0/4/8/12/16 像素的白/红/黄/绿/青色矩形边框，四角 TL/TR/BL/BR 标记，以及品红色 R24 候选圆角边框。R24 只用于观察，不代表已测得屏幕圆角半径。对照可见边框选择内容安全边距。

FPS 为这套全屏画面的主控绘制、传输及调度综合吞吐，不是面板扫描频率，也不是小区域更新帧率。统计按相邻完整传输完成时间计算；切页构建和首帧不计入，返回帧率页时重新统计。圆角页静态显示，不计 FPS。

排查程序接管刷新调度，暂停默认刷新定时器，并在手动刷新及更新统计后再次暂停（LVGL 的失效请求会恢复它）；使用 1 ms LVGL 定时器驱动页面，不额外替换官方 DMA 完成回调。仅在启动时清空 240×320 显存和开启背光，页面切换不重新渐亮。业务界面接入时应移除此专用调度或恢复默认刷新定时器。

本轮按用户要求未编译、未烧录，帧率数值和圆角显示效果待实机确认；现有 build 固件不包含此次页面修改。

### 当前验证状态

PCB、PIN.h 和文档中的 29 个 GPIO 对应一致；分区表已通过 ESP-IDF 分区工具校验。

2026-10-07：4 个应用源文件、官方移植层的 LVGL 任务和显示源文件，以及固定 ROT 270 / GAP 68 的测试变体，使用本机 ESP-IDF 6.1 交叉编译器、官方发布头文件和 LVGL Kconfig 默认值通过 `-fsyntax-only` 检查。文档链接和 `git diff --check` 通过。Astra（中）对照实际组件源码完成只读审查，未发现可证实的 P1/P2 缺陷。

本次未运行完整固件构建，未修改本机 sdkconfig，未创建 sdkconfig.defaults。以上为历史验证范围。2026-10-17 用户已确认 CASE 1/4/7/10，选定 CASE 7；新设 40 MHz 的稳定性尚待上板确认。

2026-10-07：74HC165 驱动、按键识别逻辑及应用入口通过 ESP-IDF 6.1 交叉编译器语法检查。14 项主机测试执行实际 C 驱动，覆盖全部 256 种输入位序、消抖、1 s 边界、多键操作、事件保持与清除、启动时按住、充电输入隔离和时间戳回绕。真实 RTOS 调度、电气时序和按键手感尚未接板验证。

2026-10-07：U12 按用户确认的 24C64 实现，8 KB、32 字节页、双字节地址、器件地址 0x50。EEPROM 和参数服务通过语法检查及 25 项主机测试，覆盖分页、忙等待、CRC、轮换、中断写入、未知版本保护和旧版导入。采用稳定参数 ID 与明确的小端编码，64 位统计量和背光参数缓存于 RAM，外部通过 `settings_commit()` 保存。启动只读，旧版区域保留。实料地址、实际断电恢复与跨任务行为尚未接板验证；校准字段和业务保存触发尚未接入。

2026-10-07 历史实现：闪光输出使用 GPIO38 和官方 RMT，默认高电平 20 ms、低电平恢复 1 ms，提供单次触发、1–30 ms 指定脉宽及忙状态接口。初始化只保持低电平，不自动触发；快门联动尚未接入。按用户要求停止后续测试和语法检查，移除本轮新增闪光测试文件。实机波形及外部闪光灯兼容性未验证。

2026-10-15：闪光改为 GPIO42 下降沿 ISR 直接拉高 GPIO38，GPTimer 报警 ISR 结束脉冲，不经过任务通知或 RMT。默认目标 20 ms、恢复期至少 1 ms；支持任务和 ISR 调用。后续加入快门触发后 1 s 抑制，未做回充判定；关中断和 Flash cache 暂停仍可能延迟响应。详见[闪光说明](main/drivers/flash_trigger/README.md)。历史版本用户确认编译通过；本次改动未测试或上板验证。

## Flash 分区

分区表地址为 `0x8000`。

| 分区 | 起始地址 | 大小 | 用途 |
| --- | --- | --- | --- |
| nvs | 0x9000 | 64 KB | 参数、Wi-Fi 配置、射频校准数据 |
| otadata | 0x19000 | 8 KB | OTA 启动槽与升级状态 |
| phy_init | 0x1B000 | 4 KB | 预留 PHY 初始化数据，当前未使用 |
| ota_0 | 0x20000 | 3 MB | 固件槽 A，首次烧录位置 |
| ota_1 | 0x320000 | 3 MB | 固件槽 B |

0x620000 起剩余 1.875 MB 暂不分配。不设 factory 分区；OTA 状态区为空时从 ota_0 启动。OTA 接收、校验和新固件确认逻辑尚未实现，自动回滚暂未启用。

## 配置与构建

在 ESP-IDF 终端中进入本目录。目标为 esp32s3；Flash 设为 8 MB、QIO、80 MHz，PSRAM 设为 Octal、80 MHz。2026-10-08 按采购的 N8R8 调整分区表，本机 sdkconfig 的 Flash 容量仍须手动从 16 MB 改为 8 MB。

在 `menuconfig → Partition Table` 选择 `Custom partition table CSV`，文件名为 `partitions.csv`，偏移为 `0x8000`。本机 sdkconfig 已设置。

首次接入 LVGL 后先执行 `idf.py reconfigure`，由组件管理器下载固定版本依赖并更新本机配置。LVGL 保持默认字体和软件绘图配置；显示缓冲区格式在代码中明确设为 RGB565。生成的 `managed_components/` 不纳入 Git，`dependencies.lock` 应随后续已验证构建保存。本次不整理 `sdkconfig.defaults`。

```powershell
idf.py build
idf.py -p COM端口 flash monitor
```

第一次使用本分区表时按生成的完整烧录参数烧录；旧分区表地址不同，不能只替换应用镜像。

计划的运行方式：开机启动 BLE，Wi-Fi 由按键、菜单或 BLE 指令手动开启，需要升级时再执行 OTA。目前已接入设置页 Wi-Fi 客户端与主动 BLE 配网；蓝牙设备控制和 OTA 尚未实现。

## 相关工程

- [第一代主控归档](../../archive/PM_Controller_Rev1_x/)：旧 STM32 固件，不适用于本 PCB。
- [FocusUnit](../FocusUnit/)：独立对焦组件。
- [GUI Guider](../../UI设计/MainControl/)：旧界面资源，移植前需适配。
- [软件与 PCB 兼容性表](../../软件与PCB兼容性表.md)。

屏幕 SPI 上限依据：ST7789V3 V0.1，第 38 页四线串行接口写周期最小 16 ns，即 62.5 MHz。ESP32-S3 当前 80 MHz APB 分频最高合规档为 40 MHz；不能直接填 62.5 MHz（当前 IDF 会选成 80 MHz）。CS 建立和保持各设置 1 个 SPI 周期，即 25 ns，满足手册各 15 ns 要求。

缓冲对比：用户实测内部 RAM 64 行双缓冲为 23.5 FPS、平均 42.5 ms、最大 43.5 ms；整屏内部 RAM 版本 buf2 分配失败；整屏 PSRAM 版本为 19.3 FPS、平均 51.7 ms、最大 55.3 ms。当前保持内部 RAM 64 行双缓冲。
