# Focus Unit 软件工程记录

## 当前实现概览（2026-10-07，代码核对）

当前是已集成的基础采集与电机开环演示固件，目标为 STM32F030F4Px，48 MHz、16 KB Flash、4 KB RAM。

| 功能 | 当前实现 | 尚未实现或待确认 |
| --- | --- | --- |
| 调度 | TIM14 1 ms 中断；主循环执行编码器 1 ms、电机 10 ms、ADC 100 ms 任务，记录积压与延迟，空闲 WFI | 时基精度与实机最长任务延迟待测 |
| ADC | 七通道中断扫描、自校准、VDDA 补偿、引脚 mV、MCU 温度估算；帧序、超时、HAL 错误检测与失败帧发布 | NTC 实际温度、分压前轨电压、电机实际电流换算未实现，参数待核实；尚未使用 ADC DMA |
| 电机 | 20 kHz PWM；A 后 B，各自加速/保持/减速/制动各 3 s，24 s 循环；同步 CCR、唤醒、换向制动保护、Stop | 速度/位置闭环、目标位置控制、归零/行程管理、应用层过流/过温阈值保护未实现 |
| 编码器 | 硬件四倍频；有符号位置累计、回绕、置零、符号映射、半圈歧义和位置饱和锁存 | 计数到实际位移/速度的换算未实现；线序和滤波适配待实机确认 |
| 串口 | USART1 460800 8N1 与 TX/RX DMA、IRQ 已预配置 | 未启动收发，无控制协议、状态上报或串口升级实现 |
| 调试与故障 | SWD、模块快照和软件置零请求；初始化错误/HardFault 直接令桥输入双低 | NMI 当前只停留，没有调用桥停机；硬件异常与调试恢复覆盖尚未完成定量验收 |

2026-09-28 的“初步测试OK”为用户反馈，具体固件版本、时长、逐项结果未记录；不扩展为优化构建和异常边界均已实机验收。本次未烧写或操作电机。

### 中文注释与本次验证

- 业务模块的全部函数、测量字段和接口补充中文说明，标明单位、主循环/ISR 调用范围、数据有效性和回绕边界；主入口、外设初始化、中断、C 库支持、构建及测试入口同步补注释。
- CubeMX 带 `USER CODE` 标记的文件中，新增说明放在对应保留区域；原标记、许可及 Drivers 原文保留。系统/C 库模板中的说明在以后重新生成模板时需核对保留情况。
- 固件 C/H 仅改注释。四个 MSVC 测试入口添加 `/utf-8`，PowerShell 测试脚本采用 UTF-8 BOM，解决 CP936 默认代码页及 Windows PowerShell 对中文脚本的识别问题；测试断言和固件逻辑不变。
- 四个原有宿主模块测试全部通过。CMake GNU 14.3.1 Debug/Release 编译链接通过，Flash 均为 11296 / 16384 B，剩余 5088 B；RAM 链接占用均为 2568 / 4096 B，包含预留堆栈。
- 31 个修改后的 C/H 文件去掉注释后的代码 token 与 Git HEAD 一致；从最终 ELF 导出的 Debug/Release BIN 均与修改前 SHA-256 一致，均为 `155b42dc0bd506086f7eb2969a3b1816878125fddd2e62a3bfec05e9fccc9e72`。核对记录在 `.build/comment-verification.json`，仅调试信息中的源码行号随注释移动。
- 当前 `FocusMotor_SetPositiveDirection()` 允许运行中修改逻辑方向，没有 `HAL_BUSY` 限制；`period_counts` 字段暂未用于可变周期，实际固定为 2400。记录按当前代码修正，未修改运行行为。
- `tools/regenerate_focusunit.py` 仍按集中式 `main.c` / MSP 结构验证外设配置，未适配现在拆分的 `adc.c`、`tim.c`、`usart.c`。本次只补用途及适用范围注释，没有运行该脚本；当前再生成应通过 IOC/CubeMX，随后核对各拆分文件、USER CODE、顶层 CMake 和构建结果。

### 本地测试清理（2026-10-07）

按用户要求移除宿主模块测试、HAL 桩、统一测试入口和本地测试缓存，清理文档中的运行命令。下文宿主测试结果为清理前的历史记录。保留固件内的电机演示、故障处理、SWD 观察接口及构建/生成工具。

提交前核对：27 个保留的固件 C/H 修改去掉注释后与 Git HEAD 的代码 token 一致；保留的两个 Python 工具语法树一致。主控构建清单引用及相关文档本地链接有效，无已删除测试入口引用。本次未重新构建或执行宿主/实机测试。

## 范围与协作约定

- 本文仅适用于 `E:\Develop\PolaMiyaSoftware\code\FocusUnit`。
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

- TIM14：1 ms 更新中断（PSC=47、ARR=999），ISR 只计时并设置任务标志。main 原子取出标志后执行；编码器 1 ms、电机 10 ms、ADC 100 ms。绝对时间驱动状态机，记录延迟/合并任务。
- ADC：上电硬件自校准；最终采用 PCLK/4 = 12 MHz（`ADC_CLOCK_SYNC_PCLK_DIV4`）、239.5 周期采样，七通道每 100 ms 一组，各通道 10 Hz。单通道采样约 19.96 us、完整转换约 21 us，七通道硬件转换合计约 147 us（均为标称时钟下的计算值，未测实机中断延迟）。ISR 收集原始值，main 计算并发布单独测量结构体，用工厂 VREFINT_CAL 补偿 VDDA；保留错误、有效性、序号和时间戳。保留 DMA 改造边界。
- 电机：TIM1 20 kHz（PSC=0、ARR=2399），手册推荐的驱动/制动 PWM，停止用 IN1=IN2=1 制动，正确处理上电唤醒和 0/100% 占空比。A 3s 加速/3s 运行/3s 减速/3s 停止，随后 B 同样四阶段，24s 循环。最大占空比默认 100%，正方向映射可配置。加减速是开环占空比变化，不声称实际转速闭环控制。
- 编码器：TIM3 ARR=65535，硬件四倍频，1ms 任务采样 CNT 并以有符号模差累计 int32_t 位置，支持负数和回绕；提供独立测量结构体、一致性读取、外部置零及符号映射接口。相邻采样实际位移必须小于 32768 计数；记录延迟，避免每个边沿 IRQ。
- PWM/编码器边沿由硬件处理；ADC 使用中断，TIM14 驱动任务，运行阶段不采用阻塞延时。USART1 不启动业务。SWD 保留；调试暂停时冻结 TIM14/TIM3，不冻结 TIM1，避免把桥输入冻结在持续驱动电平。断点不是电机停止命令；调试停机先在主循环调用 `FocusMotor_Stop()`。调试暂停时的编码器数据不用于实际位移验收。

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
| 电机 | `FocusUnit/Motor/focus_motor.h/.c` | Init、Update、Stop、SetPositiveDirection、GetSnapshot |
| 编码器 | `FocusUnit/Encoder/focus_encoder.h/.c` | Init、Update、Zero、SetSign、GetSnapshot |

- TIM14 ISR 经 `HAL_TIM_PeriodElapsedCallback()` 进入调度器；main 原子领取标志。ADC 经 ADC1 IRQ / HAL 单 EOC 回调立即读取 DR，完成或错误通知 main。超时、过早 EOS、缺失 EOS、HAL 启动失败、overrun 和 VREF 无效均有状态记录，不发布混合帧。
- ADC 对外结构体为 `FocusAdcMeasurement`。工厂参考计算为 `VDDA_mV = 3300 * VREFINT_CAL / ADC_VREFINT`，引脚电压为 `raw * VDDA / 4095`，采用整数及舍入。外部 NTC、电源轨、电机 IPROPI 当前只发布校准后的引脚 mV，不提供未经核实的 NTC 温度、轨电压或电机电流单位换算。
- MCU 温度使用本型号的 TS_CAL1（30 C）和典型斜率 4.3 mV/C，属于估算，不使用该型号未声明的 TS_CAL2。`temperature_valid` 单独表示校准字有效，不能解读为温度精度保证；无效温度校准不会破坏其他引脚电压结果。
- ADC 完整结构体在短临界区内发布，GetSnapshot 提供一致性读取；原始帧在完成标志前后使用内存屏障。采集后端和主循环换算分离，未来可用 DMA1_CH1 整帧采集替代 EOC 收集而保留对外结果接口（届时须按本 MCU DMA 映射重新配置 IOC 并验证）。
- 电机 `FocusMotor_ConfigData.max_duty_permille` 默认 1000，范围 0..1000；`positive_direction` 默认 A，仅影响快照 `logical_sign`，Demo 的物理顺序始终 A 后 B。按 2026-10-07 当前代码，`FocusMotor_SetPositiveDirection()` 接受 A/B 并允许运行中修改，非法值返回 HAL_ERROR；快照符号在下一次状态发布时更新。
- 电机两个 CCR 开启预装载，成对更新期间用 CR1.UDIS 抑制硬件更新事件，恢复后于同一 PWM 更新事件生效。对外 0% 对应 IN1=IN2=1 制动；100% 对应 10 或 01 持续驱动，使用 ARR+1=2400 实现常高。启动先低输入，再制动唤醒至少 1ms；10ms 调度使上电启动阶段通常占约 20ms。正常 Demo 每阶段 3000ms、整圈 24000ms；输出更新分辨率 10ms，时基精度取决于实际系统时钟。
- 电机相位按绝对时间计算，每圈更新基准，避免长期运行后的 uint32_t 时钟回绕造成周期跳变。如果 main 延迟跳过了原定制动阶段，实际换向前额外插入至少 1ms 制动保护；这是电气状态切换间隔，不能保证机械转子已经停止。电机 Init/Update/Stop/SetPositiveDirection 只在 main 上下文调用；GetSnapshot 可在 ISR 调用。
- 编码器 `FocusEncoderMeasurement.position` 为 int32_t。相邻计数器差值按 16 位模差解释，负增量从 65535 转换而非直接把 CNT 当位置。恰好半圈差值 32768 或位置溢出会锁存无效状态，直到 Zero 重建基准；更大实际位移或多圈不能仅由 CNT 自动检出。`elapsed_ms` 与调度积压统计用于观察主循环延迟。
- 编码器符号由 `FocusEncoder_SetSign(+1/-1)` 独立配置，不假设电机 A 的位移必然是编码器正向。外部 C 调用使用 `FocusEncoder_Zero()`；SWD 可置 `g_focus_encoder_zero_request=1`，由下一次 encoder 任务消费并置零。Zero 重取硬件 CNT 为基准，不关闭硬件计数。
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
