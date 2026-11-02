# Wi-Fi 与蓝牙配网

当前源码接入 ESP-IDF `network_provisioning` 1.4.0、NimBLE 和 Security 2。未编译、测试或烧录，蓝牙 App 兼容性、启动次数、断线与实机配网尚未验证。

## 操作

1. SETTINGS 中选择 `PAIR WIFI`，按 KEY2 主动启动。Wi-Fi 初始化在后台任务执行；开机或仅开启 WIFI 不自动开启蓝牙配网。WIFI ON/OFF 也在后台执行，等待期间显示 `WIFI WAIT`。
2. 页面显示 `PIN XXXXXXXX` 后，在支持 Security 2 的乐鑫 BLE Provisioning App 中选择 `PROV_` 加 Wi-Fi MAC 末 6 位。Security 2 用户名为 `pola`，密码为屏幕当前一次性 PIN；在手机选择 Wi-Fi 并输入该网络密码。
3. 获得 IP 并成功保存时显示 `CONNECTED`；保存失败为 `SAVE ERROR`。Wi-Fi 凭据连接失败显示 `PAIR FAILED`，保留 BLE 服务和失败状态让手机读取；再次选择 `PAIR WIFI` 可取消，离开 SETTINGS 也取消，最长 3 分钟。要重新输入 Wi-Fi，请停止后再启动配网。配网服务结束后释放配网服务资源，但不永久释放蓝牙控制器，给未来控制功能保留可能。
4. 选中 SETTINGS 的 `WIFI` 可查看本机 IPv4 地址；过长地址会滚动。连接同一局域网的手机或电脑访问 `http://设备IP/`，打开当前静态状态页。地址由路由器 DHCP 分配，可能变化。

## 存储和网络

- 三组 SSID / 密码保存在 NVS 的 `pm_wifi` blob；成功获得 IP 后更新，同名更新并变成最新，第四组覆盖最早的组。启动 Wi-Fi 时按从新到旧顺序尝试各已保存网络一次；断线后如仍有未尝试记录则继续尝试，不持续无限重试。
- SSID 最多 32 字节，密码为空（开放网络）或 8–63 个可打印 ASCII 字节；64 位十六进制 PSK 暂不支持。用户输入仅供配网，不写 UI 或项目业务日志。
- 当前 `sdkconfig` 未开启 NVS 加密，静态凭据不能视为已加密；Security 2 保护手机与设备间的会话。官方配网管理器会把收到的当前候选配置写入驱动的 NVS 命名空间，可能在自有三组记录外留下副本，且写入发生在连接验证前；不能将驱动副本当作成功配置。之后普通连接从三组记录读取并以 RAM 模式设置驱动。
- Web 首页为无认证 HTTP 静态页面，不提供控制、密码和个人信息。仅适用于可信局域网；IP 与网页不保证对启用客户端隔离的访客网络或所有手机热点可达。后续设备控制必须另行设计认证和访问权限。
- 需在允许网络的构建环境由 ESP-IDF 组件管理器获取 `espressif/network_provisioning` 1.4.0。工程不修改 sdkconfig，当前本机已启用 NimBLE 和 Security 2；正式部署还需确认手机 App 的 Security 2 模式及用户名输入方式。

ESP-IDF 6.1.0、NimBLE 动态内存且 CoC / EATT 通道数均为 0 时，SDK 的空 `inline` 清理函数可能产生未定义符号；`wifi_pair.c` 在此配置提供条件编译的空函数兼容定义。此修复仅源码核对，需重新编译验证；不修改本机 sdkconfig 或 SDK 文件。

诊断：固件现在记录 BLE connected / disconnected（含原因码）、Security 2 会话是否建立 / 参数错误 / 凭据不匹配、Wi-Fi 配网失败原因和资源清理阶段，不输出 PIN、SSID 或 Wi-Fi 密码。BLE 启动前会在控制器所在核心尝试分配并释放一个被禁用的 RWBLE 三级中断；资源不足显示 `IRQ ERROR` 而不主动启动 BLE。此检查不是保留资源，不能保证消除之后其他中断分配竞争或所有卡死。此轮未编译、测试或烧录。
