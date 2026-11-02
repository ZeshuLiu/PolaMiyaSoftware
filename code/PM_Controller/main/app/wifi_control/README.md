# Wi-Fi 与配网

SETTINGS 的 WIFI 子菜单启停 Wi-Fi 客户端；PAIR WIFI 主动进入或退出蓝牙配网。网络凭据管理、手机操作、IP 和状态页见 [配网说明](PROVISIONING.md)。重启默认 Wi-Fi 关闭。

网络获取 IP 后启动只读 HTTP 状态页，失去 IP 或停止 Wi-Fi 后关闭服务。三组凭据保存在 NVS 应用命名空间，新增 / 更新在成功获取 IP 后提交；Wi-Fi 启动时从新到旧尝试各组一次。当前 NVS 未启用静态加密，官方配网组件还可能保留驱动命名空间副本。未编译、测试或烧录，网络与实屏均未验证。

WIFI 开关确认后由独立低优先级任务初始化 / 启停 Wi-Fi，UI 显示 `WIFI WAIT`，结束后才显示实际 ON/OFF，失败显示 `WIFI ERROR`。PAIR WIFI 的 Wi-Fi 初始化也在配网任务进行。串口以 `wifi` 标签输出 UI 菜单确认、请求、后台任务创建 / 进入，以及 NVS、netif、事件循环、driver init、设置模式、加载网络及 driver start 等阶段；driver init 前输出可用内部内存和最大连续块。尚未编译或实机验证；如果底层或系统本身卡死，这项隔离不能保证解决根因。

针对实机 `Expected to init 10 rx buffer, actual is 9` / `ESP_ERR_NO_MEM`：不修改 sdkconfig，Wi-Fi 初始化结构在 SDK 默认值基础上将静态 RX 上限设为 4、动态 RX / 动态 TX 上限设为 16，并将启用的 RX BA 窗口限制到静态 RX 数量。与原先的 10 个静态 RX 相比，理论上少分配约 9.6 KB 内部 RAM；吞吐和实际余量待实测，动态缓冲上限并不等于预留内存。仍有可能在 Wi-Fi 或后续 BLE 初始化阶段内存不足。未重新编译或上板。

实机 `PAIR WIFI: ESP_ERR_NO_MEM` 前的片内总空闲为 21619 字节、最大连续块为 7680 字节；配网任务请求 8192 字节栈，符合任务分配失败的条件。按用户要求保持显示 DMA 64 行双缓冲；新增配网任务创建、SRP、配网管理器、BLE 启动和失败阶段日志。尚未重新编译或上板，不能保证 BLE 后续内存足够。

PSRAM 方案：ESP-IDF 的 `SPIRAM_TRY_ALLOCATE_WIFI_LWIP` 可让可迁移的 Wi-Fi/LwIP 分配优先使用外部 RAM；此选项会要求 Wi-Fi 静态 TX 缓冲，因此源码也将静态 TX 上限限制为 4。NimBLE 的 `BT_NIMBLE_MEM_ALLOC_MODE_EXTERNAL` 可把 NimBLE 动态分配放到 PSRAM，不能保证蓝牙所有内存均在外部。两项目前需要在 menuconfig 中由用户修改；工程约定不改本机 sdkconfig。普通 FreeRTOS `xTaskCreate` 栈仍在内部，配网任务会访问 NVS，不能简单把栈迁至 Flash cache 不可用时无法访问的 PSRAM。保留 64 行显示双缓冲。编译与实机验证均未执行。

实机已启用 Wi-Fi/LwIP 优先 PSRAM 与 NimBLE 外部内存：配网任务创建前片内 free=25463 / largest=11264，任务创建并生成 SRP 后配网管理器前 free=16495 / largest=7680；BLE controller 初始化仍报 malloc failed。本机 ESP-IDF 将控制器动态分配限制为 INTERNAL | DMA，NimBLE 选 PSRAM 不能迁移这部分。当前 `BT_CTRL_BLE_MAX_ACT=6`，Kconfig 注释每个活动约需 828 字节，可由用户在 menuconfig 将 BLE Max Instances 试调为 2（连接与广播各需活动预算）并按单手机用途把 NimBLE Max connections 设为 1；未来多设备控制需重新评估。减少幅度与实机是否足够待验证，屏幕 64 行双缓冲不调整。新增启动 BLE 前、失败后的 INTERNAL/DMA 空闲及最大块和配网任务栈历史余量日志；按工程约定不修改 sdkconfig、不执行构建或测试。

为给 BLE controller 腾出片内 RAM，LVGL builtin TLSF 内存池改由 PSRAM 分配（原链接映射为片内静态 64 KiB）；屏幕 64 行 DMA 双缓冲不变。该节省为链接布局预期，并非实测值；BLE controller 仍要求 INTERNAL | DMA，配网任务栈仍在片内。未编译、测试或烧录。

实机 BLE controller 在 CPU0 分配三级 IRAM RWBLE 中断失败（flags 0x408）后断言并触发 IDLE0 看门狗；启动顺序中 FocusUnit UART2 移到 CPU1 安装中断，尝试释放 CPU0 中断资源。每次启动 BLE 前 `esp_intr_dump` 输出两核中断表。尚不确认 UART2 原先是否占用三级中断；本轮未重新编译、测试或烧录。

配网失败处理：Wi-Fi 连接失败保留 BLE 和失败状态供手机读取，用户可主动停止并重新开始；增加 BLE 连接 / 断开原因、Security 2 握手和清理阶段日志，禁止打印凭据。BLE 启动前检测控制器核心可用的 RWBLE 三级中断，检测不是预留，不能保证后续无竞争。未编译、测试或烧录，手机报告断开及卡死原因尚待日志确认。
