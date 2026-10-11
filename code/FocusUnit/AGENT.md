# Focus Unit 软件工程记录

## 当前实现概览（2026-10-11，代码核对）

当前是已集成的基础采集与电机开环控制固件，目标为 STM32F030F4Px，48 MHz、16 KB Flash、4 KB RAM。

| 功能 | 当前实现 | 尚未实现或待确认 |
| --- | --- | --- |
| 调度 | TIM14 1 ms 中断；编码器 1 ms、电机 Step 10 ms、ADC 100 ms，由调度器统一触发；记录积压与延迟，空闲 WFI | 时基精度与实机最长任务延迟待测 |
| ADC | 七通道中断扫描、自校准、VDDA 补偿、引脚 mV、MCU 温度估算；完成帧交接与 HAL 错误终止，不屏蔽全局中断；采集正常（用户确认） | NTC 实际温度、分压前轨电压、电机实际电流换算未实现，参数待核实；尚未使用 ADC DMA |
| 电机 | Stop/Glide/OpenLoop/Error；20 kHz PWM；上电沿宏定义方向寻零，相邻触底位置差严格小于 50 计数才进入原 A/B 测试；最多触底 5 次，失败锁存；用户确认测试没有问题 | 各故障边界的定量结果未记录；速度/位置闭环、行程管理、过温保护及单次 Glide 超时未实现 |
| 编码器 | TIM3 16 位硬件四倍频；软件 int32_t 位置累计、双向回绕、置零、符号映射，不屏蔽全局中断 | 计数到实际位移/速度的换算未实现；线序和滤波适配待实机确认 |
| 串口 | USART1 460800 8N1 与 TX/RX DMA、IRQ 已预配置 | 未启动收发，无控制协议、状态上报或串口升级实现 |
| 调试与故障 | SWD、模块快照和软件置零请求；初始化错误/HardFault 直接令桥输入双低 | NMI 当前只停留，没有调用桥停机；硬件异常与调试恢复覆盖尚未完成定量验收 |

2026-10-11：用户已确认此前 ADC/编码器精简及单次 Glide 运行正常，并在零位校验、重试及 `step_home()` 提取完成后反馈“测试没有问题”。智能体仅执行静态核对，未编译或烧写；未收到测试版本、时长及各故障边界的定量记录。

### 前一轮更改记录（2026-10-11，单次 Glide）

- ADC、编码器删除多余的全局中断屏蔽和边界检查；ADC 保留基本帧交接、HAL 错误终止和除零处理，编码器保留 16 位硬件计数差到 int32_t 软件位置的累计，溢出自然回绕。
- 新增指定方向 Glide：固定 70% 占空比驱动，`raw_motor > FOCUS_MOTOR_STALL_RAW_THRESHOLD` 时停止并将编码器置零；阈值宏暂定为 1200。
- 电机模式统一由 Motor 管理，保留 Stop、Glide、OpenLoop。开环接口只接收方向和占空比，main 只调用 Step，不判断模式或堵转。
- 调度器统一按 10 ms 触发电机 Step；删除 Motor 内部的周期计时、阶段与时间字段、软件唤醒等待和换向计时。
- 原 A/B 开环循环测试移至电机目录的 `test.c/.h`：每个方向加速、保持、减速、制动各 3 s，整圈 24 s。测试使用调度器传入的时间计算占空比，已加入 CMake 源清单。
- 当时上电测试先执行一次正向 Glide，堵转停机置零后自动进入原开环循环；已由下文多次触底校验替代。
- VS Code 调试配置将 `Launch ST-Link GDB Server` 移至首项，用于启动完整调试服务。
- 运行结果：用户确认正常、没有问题；ADC 采集正常。ADC 数据使用连续运行或 Live Watch 观察。

### 电机寻零与测试流程（2026-10-11，当前代码）

- main 每 10 ms 调用 `FocusMotor_Step(now_ms)`，Step 分发初始化与正常工作，初始化分支提取为内部 `step_home()`，行为不变。PWM 输出层管理 Stop/Glide/OpenLoop/Error；`focus_motor_home.c/.h` 管理触底校验、重试、退回计时及 ADC 帧时间判定；`test.c/.h` 只生成原 A/B 开环波形。各模块共用调度器，不增加调度周期或阻塞延时。
- `FocusMotor_Glide(direction, seek_zero)` 固定使用 700 千分比（70%）。`seek_zero=0` 为普通 Glide，电流达到停止条件后只制动，不置零、不修改零位有效标志或触底次数；`seek_zero=1` 启动完整寻零流程，清除旧的零位确认并重新计次。正常状态下的显式命令会取消自动测试，手动寻零成功后保持 Stop，等待下一条指令。
- 上电使用 `FOCUS_MOTOR_HOME_DIRECTION`，默认 A，与逻辑正方向映射独立。停止条件沿用 `raw_motor > FOCUS_MOTOR_STALL_RAW_THRESHOLD`，默认 1200。只接受请求时间严格晚于本次驱动开始的 ADC 帧，排除启动前或上一次堵转的旧高值。
- 第一次触底：制动、记录清零前位置并置零，仅建立候选零位，`zero_valid` 保持 0。下一次电机任务开始反向开环退回，以 `FOCUS_MOTOR_HOME_BACKOFF_DUTY_PERMILLE=700` 驱动 `FOCUS_MOTOR_HOME_BACKOFF_MS=1000` ms，再沿原寻零方向 Glide。
- 后续每次触底先记录清零前位置 `p`，以 `-50 < p < 50` 判断通过，再执行编码器置零；等于正负 50 均失败。允差由用户指定为 `FOCUS_MOTOR_HOME_TOLERANCE_COUNTS=50` 个四倍频计数。通过后 `zero_valid=1`、工作状态为正常；不通过则本次触底建立新的候选零位，继续退回和校验。此判定确认回零重复性，不是绝对位置传感器测量。
- `FOCUS_MOTOR_HOME_MAX_ATTEMPTS=5` 包含首次触底，最早第二次、最晚第五次触底确认；第五次通过仍成功，失败则立即制动、取消测试并进入 Error，不再退回或执行第六次。次数只在触底时增加，未增加单次 Glide 超时；若始终没有停止条件，仍停留在初始化 Glide。
- 工作状态 `work_state` 固定为初始化 0、正常 1、错误 2，供后续 UART 上报；寻零 Glide、触底制动和反向退回都属于初始化。快照增加 `seek_zero`、`zero_valid`、`home_attempts`、`home_error_counts`。`initialized` 仍只表示 PWM 初始化成功；`glide_completed` 表示普通 Glide 停止或完整寻零成功，自动测试只以 `zero_valid` 放行。
- 初始化期间 Glide/OpenLoop/Stop/方向修改指令不能覆盖寻零；错误状态下均忽略或返回 HAL_ERROR。重复 Init 在运行中返回 HAL_BUSY，在错误状态返回 HAL_ERROR。SWD 的 `g_focus_encoder_zero_request` 只在正常状态消费为置零操作，初始化/错误时丢弃，防止篡改候选零位。
- 故障与最后触底诊断记录存入 16 B 的 `.noinit`，两个链接脚本均将该段放在启动清零范围 `_sbss.._ebss` 之外、堆栈预留之前。NRST、软件和看门狗复位保留故障，RCC POR/PDR 上电复位清除；不写 Flash。保留故障的启动仍初始化 PWM 为制动并运行采集/调度，允许未来 UART 读取错误状态；重烧固件或调试器主动清 RAM 不属于该保留保证。
- 原测试仍为 A/B 各加速、保持、减速、制动各 3 s，整圈 24 s；上电只有零位有效后才建立测试时间起点。新寻零源文件已加入 CMake；独立构建脚本递归取 Core/Src，无需修改。
- 本轮只做接口、状态转移、计数和允差边界、调用入口、启动清零范围及文件差异的静态核对；未调用编译器、构建、烧写或进行电机实机验证。反向 1 s 使用调度器时间，实际持续时间会受 10 ms 调度粒度及任务延迟影响。UART 尚未启动收发。
- 用户随后确认“测试没有问题”，并授权将软件仓库全部现有改动提交本地 Git。该反馈记录为当前版本的用户测试结果，不扩展为正负 50 计数、第五次失败、各类复位和故障指令拦截均已逐项验证。

### ADC 与编码器精简（2026-10-11）

- 按用户要求删除两个模块中的全局中断屏蔽和通用空指针/初始化状态检查；接口在初始化成功后按约定调用，测量快照仅在主循环读取。
- ADC 删除扫描超时、EOS/帧序判定、VREF 校准字和 VDDA 经验范围校验、温度校准字校验、温度限幅、错误帧及错误统计字段。保留七通道帧交接、忙状态、HAL 校准/启动返回值、HAL 错误终止和 VREFINT 除零处理。原始帧由 ISR 写入，收满后停止写缓冲区，主循环换算并释放；无扫描超时恢复。
- 编码器删除半圈歧义报错、位置饱和、故障锁存及方向参数检查。位置保持 int32_t，采用无符号加法实现 32 位自然回绕，再按 ARM GCC 的整数转换规则表示有符号位置；不再使用 64 位饱和累计。
- STM32F030F4 的 TIM3 为 16 位硬件计数器，不能通过 ARR 或软件字段改成 32 位。模差累计仍要求相邻读取间实际位移小于 32768 计数，这是算法适用条件，未添加运行检查。
- 删除 main 中编码器软件置零请求的中断屏蔽。调度器和电机模块的临界区，以及断点暂停时冻结 TIM3 的设置保持原配置。
- 收到“不编译”约束前，CMake Debug 构建已完成：Flash 9904 / 16384 B，RAM 2464 / 4096 B。收到约束后停止构建，仅做静态核对；未构建 Release、未烧写、未操作电机或进行实机采集。

### 中文注释与当时验证（2026-10-07）

- 业务模块的全部函数、测量字段和接口补充中文说明，标明单位、主循环/ISR 调用范围、数据有效性和回绕边界；主入口、外设初始化、中断、C 库支持、构建及测试入口同步补注释。
- CubeMX 带 `USER CODE` 标记的文件中，新增说明放在对应保留区域；原标记、许可及 Drivers 原文保留。系统/C 库模板中的说明在以后重新生成模板时需核对保留情况。
- 固件 C/H 仅改注释。四个 MSVC 测试入口添加 `/utf-8`，PowerShell 测试脚本采用 UTF-8 BOM，解决 CP936 默认代码页及 Windows PowerShell 对中文脚本的识别问题；测试断言和固件逻辑不变。
- 四个原有宿主模块测试全部通过。CMake GNU 14.3.1 Debug/Release 编译链接通过，Flash 均为 11296 / 16384 B，剩余 5088 B；RAM 链接占用均为 2568 / 4096 B，包含预留堆栈。
- 31 个修改后的 C/H 文件去掉注释后的代码 token 与 Git HEAD 一致；从最终 ELF 导出的 Debug/Release BIN 均与修改前 SHA-256 一致，均为 `155b42dc0bd506086f7eb2969a3b1816878125fddd2e62a3bfec05e9fccc9e72`。核对记录在 `.build/comment-verification.json`，仅调试信息中的源码行号随注释移动。
- 当时 `FocusMotor_SetPositiveDirection()` 允许运行中修改逻辑方向，没有 `HAL_BUSY` 限制；本轮寻零新增初始化/故障状态拦截。`period_counts` 字段当时未用于可变周期，后续已删除，实际固定为 2400。
- `tools/regenerate_focusunit.py` 仍按集中式 `main.c` / MSP 结构验证外设配置，未适配现在拆分的 `adc.c`、`tim.c`、`usart.c`。本次只补用途及适用范围注释，没有运行该脚本；当前再生成应通过 IOC/CubeMX，随后核对各拆分文件、USER CODE、顶层 CMake 和构建结果。

### 本地测试清理（2026-10-07）

按用户要求移除宿主模块测试、HAL 桩、统一测试入口和本地测试缓存，清理文档中的运行命令。下文宿主测试结果为清理前的历史记录。保留固件内的电机演示、故障处理、SWD 观察接口及构建/生成工具。

提交前核对：27 个保留的固件 C/H 修改去掉注释后与 Git HEAD 的代码 token 一致；保留的两个 Python 工具语法树一致。主控构建清单引用及相关文档本地链接有效，无已删除测试入口引用。本次未重新构建或执行宿主/实机测试。

## 范围与协作约定

- 本文仅适用于 `E:\Develop\PolaMiyaSoftware\code\FocusUnit`。
- 电机模式、堵转判断、停机置零及测试衔接统一放在电机模块内管理；main 初始化后在调度器的 10 ms 电机任务中只调用 `FocusMotor_Step()` 推进电机，不在电机模块内重复计算调度周期。开环转动测试波形放在电机目录的 `test.c/.h`，保留 Glide 和原开环测试。
- 调度统一通过调度器；PWM 输出层不保存时间戳或另算调度周期。寻零退回 1 s 与 ADC 帧时间判定由 `focus_motor_home.c` 使用传入的 `now_ms` 管理；开环测试的占空比与周期计算留在 test.c。
- 用户于 2026-10-11 明确要求：智能体不要编译本工程。后续修改只做源码、配置和文档的静态核对，不运行 CMake、Ninja、Make、CubeIDE 构建或 `tools/build_focusunit.py`，也不以验证为由调用编译器；工程编译由用户手动执行，除非用户后续明确撤销此约束。下文构建命令和构建结果仅作用户操作参考及历史记录。
- 用户于 2026-09-27 确认本轮实施计划，并指定三个 GPT-6 LUNA、中等推理子智能体分别实现 ADC、电机和编码器；主智能体负责 IOC、调度、集成、构建与验收。
- 保留实施前已有未提交修改；不提交 Git，不修改其他设备工程。
- 外设参数以 `FocusUnit.ioc` 为配置源，优先由 CubeMX 生成。业务接入使用 `USER CODE` 区域，模块按功能放在 `Core/Inc/FocusUnit/`、`Core/Src/FocusUnit/` 子目录。
- 子智能体只编辑各自模块，不改 `main.c`、中断文件、MSP、IOC 或共享 HAL 回调。

## 实施前基线（2026-09-27，现场检查）

- STM32F030F4Px / TSSOP20；HSI/2 × 12 PLL，SYSCLK/HCLK/PCLK/定时器时钟 48 MHz。链接器为 16 KB Flash、4 KB RAM。
- STM32CubeIDE 1.18.1，CubeMX 6.14.1，STM32Cube FW_F0 V1.11.6；本机存在 GNU Tools for STM32 13.3.rel1。
- `main()` 仅初始化 GPIO、DMA、TIM1、USART1、ADC、TIM3、TIM14，主循环为空，尚未启动 ADC/PWM/编码器/调度器。
- ADC：12 位，正向扫描，单通道 EOC 中断、软件触发、不连续转换；通道 0/1/4/5/9/16/17，采样时间 1.5 周期。已配置 ADC NVIC，无测量业务。
- TIM1：CH2/CH3 PWM1，高有效，PSC=0、ARR=65535、CCR=0；已配置 TIM1 NVIC，无业务。
- TIM3：TI12 编码器、PSC=0、ARR=200、无输入滤波，PA6/PA7 内部上拉；已配置 NVIC，无位置业务。
- TIM14：PSC=0、ARR=65535，无 NVIC 中断配置，无调度业务。
- USART1：460800、8N1、PA2/PA3，RX DMA1_CH3 / TX DMA1_CH2，Normal；保留配置但本轮不启动传输，不实现控制协议/升级功能。
- SWD：PA13/PA14 Serial Wire 保留。
- 已有未提交修改：`FocusUnit.ioc`、`Core/Inc/stm32f0xx_it.h`、`Core/Src/main.c`、`Core/Src/stm32f0xx_hal_msp.c`、`Core/Src/stm32f0xx_it.c`。

## 引脚与硬件来源

| 功能 | 引脚 / 外设 | 说明 |
| --- | --- | --- |
| ADC_NTC1 | PA0 / ADC_IN0 | 外部 NTC，型号参数待核实 |
| ADC_3V3 | PA1 / ADC_IN1 | 电源采样，分压参数按硬件核实 |
| ADC_NTC2 | PA4 / ADC_IN4 | 外部 NTC，型号参数待核实 |
| ADC_6V | PA5 / ADC_IN5 | 电源采样，分压参数按硬件核实 |
| ADC_MT | PB1 / ADC_IN9 | DRV8251A IPROPI 电流采样 |
| 温度 / VREFINT | ADC_IN16 / ADC_IN17 | MCU 内部通道 |
| PWMA / IN1 | PA10 / TIM1_CH3 | DRV8251A IN1 |
| PWMB / IN2 | PA9 / TIM1_CH2 | DRV8251A IN2 |
| 编码器 A / B | PA6 / TIM3_CH1，PA7 / TIM3_CH2 | 内部上拉，TI12 |
| USART1 TX / RX | PA2 / PA3 | 预留控制与串口升级 |
| SWDIO / SWCLK | PA13 / PA14 | 专用调试接口 |

硬件参考：`E:\Develop\PolaMiyaHardware\电路\对焦组件\对焦组件.kicad_sch`、同目录 PCB 和 `对焦组件_Rev_1.0_PIN.md`。驱动手册：`D:\School\电子技术表\电机&电机驱动\drv8251a.pdf` 第 11、17 页。芯片实际为 DRV8251A（原理图 DRV8251ADDAR），不要混用 DRV8251 的电流采样公式。

## 已批准的实现目标与最终配置

- TIM14：1 ms 更新中断（PSC=47、ARR=999），ISR 只计时并设置任务标志。main 原子取出标志后执行；编码器 1 ms、电机 Step 10 ms、ADC 100 ms，均由调度器触发。绝对时间驱动状态机，记录延迟/合并任务。
- ADC：上电硬件自校准；采用 PCLK/4 = 12 MHz（`ADC_CLOCK_SYNC_PCLK_DIV4`）、239.5 周期采样，七通道每 100 ms 一组，各通道 10 Hz。单通道采样约 19.96 us、完整转换约 21 us，七通道硬件转换合计约 147 us（均为标称时钟下的计算值，未测实机中断延迟）。ISR 收集原始值，main 换算并发布测量结构体，用工厂 VREFINT_CAL 补偿 VDDA；保留有效性、序号和时间戳。
- 电机：TIM1 20 kHz（PSC=0、ARR=2399），手册推荐的驱动/制动 PWM，停止用 IN1=IN2=1 制动，正确处理上电唤醒和 0/100% 占空比。A 3s 加速/3s 运行/3s 减速/3s 停止，随后 B 同样四阶段，24s 循环。最大占空比默认 100%，正方向映射可配置。加减速是开环占空比变化，不声称实际转速闭环控制。
- 编码器：TIM3 ARR=65535，硬件四倍频，1ms 任务采样 CNT 并以有符号模差累计 int32_t 位置，支持负数和回绕；提供独立测量结构体、一致性读取、外部置零及符号映射接口。相邻采样实际位移必须小于 32768 计数；记录延迟，避免每个边沿 IRQ。
- PWM/编码器边沿由硬件处理；ADC 使用中断，TIM14 驱动任务，运行阶段不采用阻塞延时。USART1 不启动业务。SWD 保留；调试暂停时冻结 TIM14/TIM3，不冻结 TIM1，避免把桥输入冻结在持续驱动电平。断点不是电机停止命令；正常状态下可在主循环调用 `FocusMotor_Stop()`，初始化期间 Stop 不会取消寻零。暂停 TIM14 会暂停退回计时而 PWM 继续运行，不能用寻零中的断点验证 1 s 时长；调试暂停时的编码器数据不用于实际位移验收。

## 实施与验证状态

2026-09-28：调度、ADC、电机、编码器模块与 main/HAL 回调集成已完成。三个子智能体使用 GPT-6 LUNA、中等推理，分别负责 ADC、电机和编码器；主智能体完成 IOC、生成流程、调度、集成和验收。实现与构建阶段未提交 Git，智能体未执行硬件烧写；随后用户反馈初步测试通过。

### 初步测试反馈（2026-09-28）

- 结果：用户确认“初步测试OK”，当前版本已通过用户初步测试。
- 证据来源：用户反馈；本次未新增智能体执行的实机测量或测试日志。
- 用户尚未提供测试固件版本、测试时长、逐项覆盖范围和定量测量数据，因此不将该反馈扩展为所有外设、精度和异常边界均已完成实机验收。
- 本次仅更新工程状态记录，保留现有实现、IOC 参数及之前的构建/宿主测试结果。

### 代码与接口

| 模块 | 文件位置（头文件/源文件分别位于 Core/Inc、Core/Src） | 主要接口 |
| --- | --- | --- |
| 调度 | `FocusUnit/Scheduler/focus_scheduler.h/.c` | Init、OnTick、Take、Now、GetSnapshot、Idle |
| ADC | `FocusUnit/Adc/focus_adc.h/.c` | Init、Request、Process、OnConversionComplete、OnError、GetSnapshot |
| 电机 | `FocusUnit/Motor/focus_motor.h/.c` | Init、Step、Glide、OpenLoop、Stop、SetPositiveDirection、GetSnapshot |
| 电机寻零 | `FocusUnit/Motor/focus_motor_home.h/.c` | Motor 内部触底比较、置零、重试、退回计时、ADC 帧判定和跨复位故障锁存 |
| 电机开环测试 | `FocusUnit/Motor/test.h/.c` | TestStep、TestOpenLoop：按调度器时间生成原 A/B 开环占空比 |
| 编码器 | `FocusUnit/Encoder/focus_encoder.h/.c` | Init、Update、Zero、SetSign、GetSnapshot |

- TIM14 ISR 经 `HAL_TIM_PeriodElapsedCallback()` 进入调度器；main 原子领取标志。ADC 经 ADC1 IRQ / HAL 单 EOC 回调立即读取 DR，按固定七通道数量交接整帧。HAL 错误由 main 停止采集；不再做超时、EOS/帧序判定或错误帧发布。
- ADC 对外结构体为 `FocusAdcMeasurement`。工厂参考计算为 `VDDA_mV = 3300 * VREFINT_CAL / ADC_VREFINT`，引脚电压为 `raw * VDDA / 4095`，采用整数及舍入。外部 NTC、电源轨、电机 IPROPI 当前只发布校准后的引脚 mV，不提供未经核实的 NTC 温度、轨电压或电机电流单位换算。
- MCU 温度使用本型号的 TS_CAL1（30 C）和典型斜率 4.3 mV/C，属于估算，不使用该型号未声明的 TS_CAL2；不再检查温度校准字或限幅。`valid` 仅表示 VREFINT 非零，可进行换算，不代表温度精度。
- ADC 完整结构体的发布与 GetSnapshot 均由主循环执行，直接复制，不屏蔽中断。原始缓冲区和事件标记使用 volatile，完成标志前后使用内存屏障；ISR 收满后停止写缓冲区，主循环处理后才允许下一帧启动。
- 电机 `FocusMotor_ConfigData.max_duty_permille` 默认 1000，控制原 A/B Demo 的最大占空比；Glide 独立使用固定 700。`positive_direction` 默认 A，只定义逻辑正向，上电寻零方向独立由 `FOCUS_MOTOR_HOME_DIRECTION` 指定，原 Demo 物理顺序仍为 A 后 B。`FocusMotor_SetPositiveDirection()` 只允许正常状态修改；非法方向或故障返回 HAL_ERROR，初始化返回 HAL_BUSY，快照符号在下一次发布时更新。
- 电机两个 CCR 开启预装载，成对更新期间用 CR1.UDIS 抑制更新事件，恢复后于同一次 PWM 更新事件生效。0% 为双高制动，100% 使用 ARR+1=2400；驱动内无软件唤醒、换向等待或周期计时。
- 只有 test.c 根据调度器时基计算开环测试波形，并每圈重建测试基准。电机 Init/Step/Glide/OpenLoop/Stop/SetPositiveDirection 在主循环上下文调用，GetSnapshot 保留 ISR 一致读取接口。
- 编码器 `FocusEncoderMeasurement.position` 为 int32_t。相邻计数器差值按 16 位模差解释，再累计到软件位置；32 位位置溢出后自然回绕，不饱和、不锁存。Update/Zero/SetSign/GetSnapshot 均仅由主循环调用，无临界区；相邻实际位移小于 32768 计数是模差算法的适用条件。`elapsed_ms` 与调度积压统计用于观察主循环延迟。
- 编码器符号由 `FocusEncoder_SetSign(+1/-1)` 独立配置，不假设电机 A 的位移必然是编码器正向。低层 C 接口 `FocusEncoder_Zero()` 保留，调用者不能在寻零流程中自行改基准；SWD 可置 `g_focus_encoder_zero_request=1`，由下一次 encoder 任务消费，仅正常状态执行置零。Zero 重取硬件 CNT 为基准，不关闭硬件计数。
- SWD 观察变量：`g_focus_adc_measurement`、`g_focus_encoder_measurement`、`g_focus_motor_state`、`g_focus_scheduler_status`、`g_focus_fault`。模块测量与应用观察快照使用相同结构体类型；快照由 main 更新。
- 调度状态记录三类任务标志合并次数及最长等待时间，HAL SysTick 保留。空闲采用屏蔽中断、检查标志、WFI、恢复 PRIMASK 的顺序，避免检查与休眠之间丢失调度唤醒。
- 初始化错误与 HardFault 会通过 `FocusUnit_FaultStop()` 将 PA9/PA10 直接改为低电平 GPIO，桥进入滑行/睡眠；该错误停止路径不等待 PWM 更新或调度器。
- USART1 保留 460800 8N1、既有收发 DMA 和 NVIC 配置，无收发启动、命令处理或 Bootloader。TIM1/TIM3 的多余 NVIC 已关闭，硬件 PWM/编码器计数正常运行；TIM14 IRQ 优先级 1、ADC 优先级 0。

### IOC 重新生成与构建

以下是 2026-09-28 集中式工程的生成流程历史记录，不能直接用于当前拆分的 CMake 工程：`tools/regenerate_focusunit.py` 尚未适配拆分结构，见本文当前实现概览。此前本机 `project generate` 路径会误进入固件下载/登录流程；已有 F0 V1.11.6 固件无需下载。旧脚本调用已安装 CubeMX 的 `generate code` 模式，在独立临时目录中生成，再安装六个指定 Core 文件。

生成前将当前 Core 中 main、MSP、中断及对应头文件复制到临时 Src/Inc，让 CubeMX 保留 USER CODE；安装前逐一验证非空用户代码区、ADC/TIM 参数、TIM14 IRQ 和上拉。模块目录、Drivers、Startup、IDE 元数据不会由该脚本替换。所有检查通过后才更新 Core，保留临时目录和生成日志。不能仅凭 CubeMX 进程退出码认定生成成功。

IOC 固定 FW_F0 V1.11.6、关闭自动选择最新固件和删除旧文件；本机固件位置写入 `ProjectManager.CustomerFirmwarePackage`。在其他电脑需调整该路径。脚本支持 `--mx-dir`、`--java`，构建脚本支持 `--toolchain`。

在 PowerShell 中运行：

```powershell
Set-Location 'E:\Develop\PolaMiyaSoftware\code\FocusUnit'
# 仅旧集中式工程适用；当前拆分工程暂不运行此脚本
# python tools/regenerate_focusunit.py
python tools/build_focusunit.py --configuration Debug
python tools/build_focusunit.py --configuration Release
```

构建脚本使用本机 CubeIDE 的 GNU Tools for STM32 13.3.rel1，将完整 Cortex-M0 程序链接并输出 `.build/Debug`、`.build/Release` 下的 `FocusUnit.elf/.hex/.bin/.map` 及 `build_report.json`。两个配置均使用 -Os；Debug 保留 -g3。16KB Flash 容量较小，CubeIDE Debug 配置也已设置 -Os，部分局部变量在调试中可能被优化。Core 目录递归参与 CubeIDE 构建，新增模块无需修改自动生成的 Debug makefile。

### 已完成验证（2026-09-28）

- 四个实际 C 模块的 MSVC C11 `/W4 /WX` 宿主测试通过；测试代码及入口已于 2026-10-07 移除。
- ADC：完整七通道扫描、VDDA/mV/温度估算、提前 EOS、跨 tick 回绕超时、overrun、启动失败、零 VREF、无效温度校准字以及错误后下一帧恢复。
- 电机：两路启动、0/50/100% 输出、A/B 阶段、第二圈阶段时间、正映射 B 但物理 A 先运行、周期重基准/时钟回绕、UDIS 成对更新、跨过制动段后的换向保护、错误路径与配置边界。
- 编码器：正负位移、双向硬件回绕、符号映射、置零、tick 回绕及延迟、半圈歧义、int32 两端饱和与置零恢复。
- 调度：1/10/100ms 分频、任务标志合并/领取、最长等待时间、非 TIM14 回调忽略、时钟回绕、PRIMASK 恢复与空闲休眠条件。
- 集成后的 IOC 再生成成功，并验证全部非空 USER CODE 区域保留。ADC 最终时钟为同步 PCLK/4，没有使用本型号不存在的异步 /2 分频枚举。
- Debug/Release 完整构建与 CubeIDE 原生 Debug 构建结果见下方最终验收记录。应用代码编译启用 -Wall/-Wextra/-Werror；第三方 HAL 的未使用参数告警单独抑制，未修改 Drivers。

### 最终构建验收

| 构建 | 结果 | Flash | RAM 链接占用 |
| --- | --- | ---: | ---: |
| 脚本 Debug（-Os、-g3） | 完整编译链接成功 | 14956 / 16384 B（91.28%） | 2568 / 4096 B（62.70%） |
| 脚本 Release（-Os） | 完整编译链接成功 | 14956 / 16384 B（91.28%） | 2568 / 4096 B（62.70%） |
| CubeIDE 原生 Debug | 0 errors、0 warnings | text=14936 B、data=20 B | bss=2548 B、data=20 B |

RAM 链接占用包含原链接器预留的 heap 512 B 和 stack 1024 B，不代表实机峰值栈深已经测量。Flash 剩余 1428 B，后续 USART 控制或升级功能需重新评估容量。CubeIDE 生成 `FocusUnit.launch` 供 SWD 调试使用；上述构建验收阶段智能体没有执行调试连接/烧写，后续用户初步测试反馈见前文。启动后应用观察结构体会随任务更新，可将 `g_focus_encoder_zero_request` 置 1 验证软件置零接口。

生成验收日志保留在 `C:\Users\LZS\AppData\Local\Temp\focusunit-regenerate-d52prfdi\cubemx.log`；CubeIDE 构建日志位于 `.build/cubeide-debug.log`。`.build`、Release 和测试中间产物由本工程 `.gitignore` 忽略。

### 后续定量与边界验证

用户初步测试已通过。以下项目的逐项结果尚未记录，后续补充定量数据与边界验证：TIM14 周期、ADC 10Hz 帧率/实际通道值与校准结果、ADC overrun 计数、20kHz PWM 波形/方向/各3s阶段、编码器实际 AB 线序与置零，以及 SWD 调试恢复。电机速度、机械停止时间、滤波适配和 ADC 输入电路精度仍需以具体实机测量结果确认。

依据：[STM32F030 数据手册 DS9773](https://www.st.com/resource/en/datasheet/stm32f030c6.pdf)、本机 STM32F0 HAL V1.11.6 源码、用户提供的 DRV8251A 手册第 11/17 页及 [TI 原始手册](https://www.ti.com/lit/ds/symlink/drv8251a.pdf)。

## 2026-09-29：CMake 构建容量修正

- 用户将工程生成目标改为 CMake，外设初始化拆分为 `adc.c`、`dma.c`、`gpio.c`、`tim.c`、`usart.c`；保留这些已有调整，不恢复旧 CubeIDE 工程结构。
- 当前 `FocusUnit.ioc` 已设置 `ProjectManager.CompilerOptimize=6`，但新生成的 `cmake/gcc-arm-none-eabi.cmake` 将 Debug 固定为 `-O0 -g3`。现场失败 map 显示 Flash 装载末地址为 `0x080042C4`，即 17092 B，超出 16 KB；这份源文件清单还遗漏了四个 FocusUnit 业务模块，因此不是完整固件的有效容量基线。
- 在不会随 CubeMX 重生成覆盖的顶层 `CMakeLists.txt` 中显式加入 ADC、电机、编码器和调度模块，并经 `stm32cubemx` 共享接口将 Debug 的 `-Os` 应用于应用和 HAL。保留 `-g3` 调试符号；未修改业务代码、IOC 外设配置、HAL 驱动、16 KB Flash 限制或 RAM 预留。
- `/build/` 加入工程 `.gitignore`，CMake 构建产物不纳入源码；原 `.build/` 忽略规则保留。此轮不提交 Git，不烧写硬件。

### 当前 CMake 验证结果

使用当前 Cube 工具链 GNU 14.3.1、CMake 4.4.0、Ninja 1.13.2 完整编译，编译及链接无告警或错误：

| 构建 | Flash（text + data） | RAM（data + bss，含链接器预留） | Flash 剩余 |
| --- | ---: | ---: | ---: |
| CMake Debug（有效 `-Os`，保留 `-g3`） | 15424 / 16384 B（94.14%） | 2568 / 4096 B | 960 B |
| CMake Release（`-Os`） | 15424 / 16384 B（94.14%） | 2568 / 4096 B | 960 B |

`size` 的 `dec=17972` 是 `text + data + bss`，其中 `bss=2548` 位于 RAM，不占 Flash；不能据此认定 Flash 超出 16 KB。ELF 文件的磁盘大小还包括调试信息，也不能作为烧录大小。

四个模块的现有宿主测试再次全部通过。测试和 Ninja 构建遇到 Windows 沙箱临时目录/子进程权限问题后，使用正常权限完成；没有因此修改测试逻辑。日志在 `.build/cmake-debug-build.log` 与 `.build/cmake-release-build.log`，实际 ELF/map 在 `build/Debug/` 与 `build/Release/`。

在已配置 Cube 工具链环境的终端中重新构建：

```powershell
cmake --preset Debug
cmake --build --preset Debug
cmake --preset Release
cmake --build --preset Release
```

后续增加串口协议或升级功能时，仍须重新评估仅剩的 960 B Flash；当前验证不替代优化构建后的实机复测。

## 2026-09-29：继续缩减 Flash，启用 LTO

上述 15424 B 是仅启用 `-Os`、尚未启用链接时优化的完整 CMake 固件基线。用户要求为后续功能继续释放空间，本轮保持业务功能和 IOC 参数不变。

### 体积来源

基于 GNU 14.3.1 Release map 的 Flash 输入段统计（不包含调试段）：

| 来源 | 基线 Flash 输入段占用 |
| --- | ---: |
| HAL 定时器及扩展驱动 | 2078 B |
| HAL 串口驱动 | 1723 B |
| HAL 时钟及扩展驱动 | 1692 B |
| HAL ADC 及扩展驱动 | 1514 B |
| 四个 FocusUnit 业务模块 | 3918 B |

全部被链接的 HAL 驱动合计 8047 B，约占基线 Flash 的一半；四个业务模块约占四分之一。其余为生成的初始化代码、中断入口、启动代码、软件整数除法和其他支持代码及段对齐。Cortex-M0 使用的软件有符号/无符号除法实现合计 744 B；未发现浮点数学或 printf 实现被链接。USART 尚无业务，但其初始化、IRQ 和 DMA 支持已占用空间。驱动源文件参与编译不等于最终占 Flash，未使用的 I2C 等函数已由 `--gc-sections` 丢弃。

### 修改与最新验收

- 顶层 `CMakeLists.txt` 对应用和 `STM32_Drivers` 启用 `INTERPROCEDURAL_OPTIMIZATION`，使编译器跨业务与 HAL 文件裁剪通用分支；链接阶段也显式使用 `-Os`，避免 Debug 的生成默认 `-O0` 限制 LTO 收益。
- 独立 `tools/build_focusunit.py` 同步加入编译/链接 `-flto` 和链接 `-Os`，构建报告记录 `lto=true`，保留其原有 GNU 13.3 工具链入口。
- 未删除错误处理、ADC 校准、电机换向保护、编码器或调度逻辑；未改 HAL 驱动源码、外设配置、16 KB Flash 上限及堆栈预留。Debug 仍保留 `-g3`，但跨文件内联会影响单步调试和局部变量可见性。

以下为最新结果，取代前一节的剩余容量判断：

| 构建 | Flash | RAM（含链接器预留） | Flash 剩余 |
| --- | ---: | ---: | ---: |
| CMake Debug，GNU 14.3.1，`-Os` + LTO | 11296 / 16384 B（68.95%） | 2568 / 4096 B | 5088 B |
| CMake Release，GNU 14.3.1，`-Os` + LTO | 11296 / 16384 B（68.95%） | 2568 / 4096 B | 5088 B |
| 独立脚本 Debug，GNU 13.3，`-Os` + LTO | 11104 / 16384 B（67.77%） | 2960 / 4096 B | 5280 B |
| 独立脚本 Release，GNU 13.3，`-Os` + LTO | 11104 / 16384 B（67.77%） | 2960 / 4096 B | 5280 B |

当前使用的 CMake 固件节省 4128 B（26.76%），余量由 960 B 增至 5088 B。独立脚本的编译器、启动文件和驱动源清单与 CMake 不同，因此不能混用两者的 RAM/Flash 数字；日常 CMake 开发以当前 GNU 14.3.1 结果为准。

- 两套入口的 Debug/Release 完整构建成功；四个原有模块宿主测试再次全部通过。
- 通过最终 ELF 和 BIN 检查 ADC、TIM14、DMA、USART、HardFault 中断向量正确指向实际处理函数，未落入默认处理函数；六个 `g_focus_*` SWD 观察/控制变量均保留。
- CMake 的两份 BIN 均为 11296 B，编译命令验证所有 C 源文件有效使用 `-Os` 和 LTO。
- 日志：`.build/cmake-debug-lto.log`、`.build/cmake-release-lto.log`、`.build/script-debug-lto.log`、`.build/script-release-lto.log`；优化前分项统计为 `.build/flash-before-lto.json`。
- 本轮未提交 Git、未烧写硬件。优化构建仍需用户实机复测 ADC、PWM/换向、编码器和调度；构建与宿主测试不能替代该检查。

若后续 5088 B 仍不足，优先根据新增业务的 map 结果评估轻量化 UART/中断驱动或 LL 实现；这将涉及运行行为和硬件验证，不与本轮编译优化混在一起。新增功能的容量应以重新链接的结果为准。
