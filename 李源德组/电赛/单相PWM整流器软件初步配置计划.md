# STM32G474 单相 PWM 整流器软件初步配置计划

## 1. 计划目标

本计划面向最终仿真模型 [`x2026A2022b.slx`](../开源资料（matlab仿真及PCB）/达尔闻/x2026A2022b.slx) 中的单相 PWM 整流器部分，目标是在 `STM32G474VCT6` 上先建立一套能够安全上板、方便观察和逐步闭环的软件基础。

本阶段只完成以下内容：

- STM32G474、时钟、HRTIM A/B、ADC1/2、GPIO Fault、USART2 和 IWDG 的配置规划；
- `VAC`、`IPFC`、`VBUS` 三路采样、标定和 VOFA 观测；
- 单相全桥低压开环和倍频 SPWM 验证；
- 交流电流 PR 内环；
- 母线电压 PI 外环的接口和后续接入步骤；
- 故障锁存、软件限值和看门狗监督。

本阶段不实现三相逆变器，不生成新的 `.ioc`，也不引入 RTOS、SOGI-PLL、Clarke/Park、SVPWM、UART DMA 或复杂通信框架。

## 2. 仿真模型对应关系

从最终模型中可确认整流部分包含：

| 仿真模块 | 单片机中的对应功能 |
| --- | --- |
| `PID Controller` / 母线电压反馈 | `VBUS` 电压 PI 外环 |
| `Digital PR` | `IPFC` 交流电流 PR 内环 |
| `Vac` | 电网电压采样、极性和归一化相位参考 |
| `Iac` | 整流输入电流采样和 PR 反馈 |
| `Udc` / `DCVFB` | 直流母线电压反馈 |
| `倍频SPWM调制器` | HRTIM A/B 单相全桥差分调制 |
| `10e3` | 初步按 10 kHz 载波规划，最终以实际 HRTIM 输出为准 |

仿真模型的电力电子求解步长不能直接当作 MCU 控制周期。固件中的 `PR_Init(... sample_frequency_hz ...)` 必须等于 `PR_Calc()` 的真实调用频率。

中心对齐 PWM 可能在一个 PWM 周期内产生两次更新事件。初版应只选择一个 HRTIM ADC Trigger 触发快速控制，确保 10 kHz PWM 对应 10 kHz PR 调用；如果最终确实采用每周期两次控制，则必须把 PR 采样频率改为实测的 20 kHz并重新计算离散系数。

## 3. 当前可复用资源

### 3.1 推荐直接复用

使用 [`easy_port`](easy_port/README.md) 中的简化模块：

```text
easy_port/Core/Inc/pr.h
easy_port/Core/Src/pr.c
easy_port/Core/Inc/vofa.h
easy_port/Core/Src/vofa.c
```

- `pr.c` 只负责 PR 初始化、复位和单步计算，不允许加入 ADC、HRTIM、GPIO 或串口代码；
- `vofa.c` 使用阻塞式 `HAL_UART_Transmit()`，只允许从 `while (1)` 调用；
- 不使用 `portable/` 和 `stm32g4_hal/` 的高级队列、UART DMA 或普通 TIM 适配器；
- HRTIM A/B 的比较值更新由最终应用工程单独实现。

### 3.2 不可直接作为最终工程

`codex_hal_test/codex_test` 当前是 `STM32G474` 测试工程，但其时钟约为 150 MHz，且没有按最终 HRTIM A/B、ADC 和引脚方案配置。

它可以用于参考 CubeMX/Keil 工程结构，但不能直接作为最终整流器配置复制。目标工程必须以实际 `.ioc` 为唯一外设配置来源，并核对：

- `STM32G474VCT6`、LQFP100；
- `STM32G474xx` 设备宏；
- 256 KB Flash 链接限制；
- STM32Cube FW_G4 V1.6.2；
- MDK-ARM 工具链。

## 4. 首版硬件资源基线

| 功能 | MCU 资源 | 首版软件用途 |
| --- | --- | --- |
| 整流桥臂 1 | HRTIM A：PA8/PA9 | 全桥 A 桥臂互补 PWM |
| 整流桥臂 2 | HRTIM B：PA10/PA11 | 全桥 B 桥臂互补 PWM |
| 输入电流 | PA1 / ADC1_IN2 | `IPFC` 快速注入采样 |
| 交流电压 | PC0 / ADC2_IN6 | `VAC` 快速注入采样 |
| 母线电压 | PC1 / ADC1_IN7 | `VBUS` 慢速规则采样 |
| PFC 驱动使能 | PE0 | 初始低，软件允许后才拉高 |
| 驱动总故障 | PB10 / HRTIM1_FLT3 | 低有效、硬件锁存关断 |
| PFC 诊断故障 | PE5 / GPIO EXTI | 低有效，仅用于故障定位 |
| VOFA | USART2 PD5/PD6 | 460800 或 921600 baud，阻塞发送 |
| 外部晶振 | PF0/PF1 | 8 MHz HSE |
| 看门狗 | IWDG / LSI | 名义约 500 ms 超时 |

`PE0` 必须具有板级下拉。MCU 复位、下载程序、进入 HardFault 或软件未完成初始化时，驱动器都应保持关闭。

## 5. 时钟与控制周期配置

目标时钟采用现有时钟树方案：

```text
HSE             = 8 MHz
PLLM            = /2
PLLN            = x85
PLLR            = /2
SYSCLK          = 170 MHz
HRTIM1          = 170 MHz
ADC12 kernel    = SYSCLK
ADC prescaler   = /4
ADC actual clock= 42.5 MHz
LSI             = 32 kHz nominal
```

同时确认：

- 170 MHz 下启用电源 Boost 和 `FLASH_LATENCY_4`；
- 开启 HSE CSS；CSS/NMI 发生时立即关闭 PE0 并锁存故障；
- HRTIM DLL 完成校准后才允许启动计数器；
- 不使用 USB/RNG 时关闭 HSI48；
- PA8 不配置为 MCO，必须保留给 HRTIM A。

首版周期规划：

| 任务 | 初始频率 | 调用位置 |
| --- | ---: | --- |
| HRTIM PWM | 10 kHz | HRTIM A/B 同步运行 |
| IPFC/VAC 快速采样 | 10 kHz | HRTIM ADC Trigger |
| PR 电流环 | 10 kHz | 快速 ADC 完成回调 |
| VBUS PI 外环 | 1 kHz | 每 10 次快速控制执行一次 |
| 状态机与保护复核 | 1 kHz | 主循环节拍或轻量定时标志 |
| VOFA | 100 Hz | `while (1)` |
| IWDG 安全监督 | 100 ms | 主循环监督任务 |

## 6. CubeMX 初步配置步骤

### 6.1 GPIO 安全状态

1. 将 `PFC_GATE_EN/PE0` 配置为推挽输出、初始低电平，并确认 PCB 外部下拉；
2. 将 `PFC_NFAULT_DIAG/PE5` 配置为低有效 EXTI 输入，仅用于记录故障来源；
3. 将 `PB10` 配置为 `HRTIM1_FLT3`，低有效；
4. 初始化 GPIO 时必须先写输出锁存器为低，再切换为输出模式；
5. `Error_Handler()`、HardFault、CSS/NMI 和任何故障入口都必须执行安全关断。

### 6.2 HRTIM A/B

1. 使用 HRTIM Waveform 模式；
2. Timer A/B 使用同一 Master 周期并同步更新；
3. 配置 PA8/PA9、PA10/PA11 为两对互补输出；
4. 首版采用中心对齐 10 kHz，周期值按 CubeMX 实际计数模式计算，不直接照搬普通 TIM 的 ARR；
5. 配置板级要求的死区，初始建议只记录参数接口，不在软件计划中写死纳秒值；
6. 比较寄存器使用预装载，在周期边界同时更新 A/B；
7. Fault 3 触发时 A/B 四路输出全部进入无效态并锁存；
8. PWM 默认不启动，完成 ADC、Fault、自检和参数合法性检查后才允许使能。

### 6.3 倍频 SPWM 映射

控制器输出归一化调制量 `m`，范围初步限制为 `-0.90~+0.90`：

```text
duty_A = 0.5 * (1 + m)
duty_B = 0.5 * (1 - m)
```

HRTIM A/B 的高低管由互补输出和死区产生。应用层只更新两个桥臂的主占空比，不允许分别自由写四个开关，避免破坏互锁关系。

在带功率测试前必须用示波器确认：

- A/B 调制方向正确；
- 四路互补关系正确；
- 死区存在且上下管无重叠；
- 占空比更新只发生在允许的更新点；
- PB10 拉低时不依赖 CPU 即可关闭四路输出。

### 6.4 ADC1/ADC2

快速组：

- ADC1 注入通道：PA1 / `IPFC`；
- ADC2 注入通道：PC0 / `VAC`；
- 两路由同一个 HRTIM ADC Trigger 触发；
- 控制算法只在两路样本都更新后执行一次。

慢速组：

- ADC1 规则通道：PC1 / `VBUS`；
- 以约 1 kHz 触发，可用规则 DMA 循环采样；
- VBUS 的 DMA 处理不得阻塞或改变快速注入组的控制节拍。

初步采样链路统一输出物理量：

```text
vac_v    单位 V，交流瞬时电压，已经去除 ADC 偏置
ipfc_a   单位 A，交流瞬时电流，已经去除 ADC 偏置
vbus_v   单位 V，直流母线电压
```

ADC 采样时间不能只追求最短。应根据调理运放输出阻抗和 RC 滤波选择采样周期，并用示波器调整 HRTIM Trigger，使采样点远离开关沿。

### 6.5 USART2 与 VOFA

CubeMX 配置：

- USART2 Asynchronous；
- PD5 TX、PD6 RX；
- 8N1、无硬件流控；
- 460800 baud 起步，链路稳定后可改为 921600；
- 不配置 UART DMA，不要求 UART NVIC。

初版只发送三通道：

```text
CH0 = VAC
CH1 = IPFC
CH2 = VBUS
```

三通道 JustFloat 每帧 16 字节，100 Hz 只需约 16 kbit/s，460800 baud 有足够余量。`VOFA_Send()` 只能在主循环每 10 ms 调用一次，禁止放入 ADC/HRTIM/Fault 中断。

### 6.6 IWDG

IWDG 使用 LSI，名义配置可采用：

```text
Prescaler = 64
Reload    = 249
Nominal timeout = 500 ms
```

实际超时需按 LSI 数据手册误差核算。初步调试使用软件启动；量产是否使用 Option Byte 硬件 IWDG，在完成上板调试后再决定。

## 7. 软件模块与数据流

### 7.1 建议文件

目标工程保留 CubeMX 生成的 `main.c`、`adc.c`、`hrtim.c`、`gpio.c`、`usart.c` 和 `iwdg.c`，新增最少量应用文件：

```text
Core/Inc/pfc_params.h    采样比例、保护阈值、频率和控制参数
Core/Inc/pfc_app.h       PFC 初始化、快速步进、慢速任务和故障接口
Core/Src/pfc_app.c       测量换算、参考生成、PR/PI 调用和状态控制
Core/Inc/pr.h            从 easy_port 复制
Core/Src/pr.c            从 easy_port 复制
Core/Inc/vofa.h          从 easy_port 复制
Core/Src/vofa.c          从 easy_port 复制
```

首版不再拆分更多层。`pr.c` 和 `vofa.c` 保持职责单一，ADC、HRTIM、Gate Enable、保护和状态逻辑集中在 `pfc_app.c` 的应用层。

### 7.2 快速控制数据流

快速 ADC 回调固定按以下顺序执行：

```text
检查 ADC1/2 样本均已更新
-> 原始 ADC 转换为 vac_v、ipfc_a
-> 检查采样范围和数据新鲜度
-> vac_v 去偏置并归一化到 -1~1
-> 生成 ipfc_ref_a
-> PR_Calc(PR_PFC_I, ipfc_ref_a, ipfc_a)
-> 调制限幅
-> 同步更新 HRTIM A/B 比较值
-> 更新快速 ISR 心跳
```

快速回调禁止执行：

- `HAL_UART_Transmit()`；
- `HAL_Delay()`；
- OLED 刷新和 `sprintf()`；
- 浮点 RMS 长窗口计算；
- 动态内存分配；
- 自动清除 Fault 或重新启动 Gate Enable。

### 7.3 电流参考生成

本阶段不使用 SOGI-PLL。先对 `VAC` 去偏置并限制：

```c
vac_norm = vac_v / vac_peak_est_v;
vac_norm = clamp(vac_norm, -1.0f, 1.0f);
ipfc_ref_a = pfc_current_amp_cmd_a * vac_norm;
```

其中：

- `vac_peak_est_v` 初步使用额定交流峰值，之后可改为慢速峰值估计；
- `pfc_current_amp_cmd_a` 在仅电流环阶段由调试参数给定；
- 接入母线外环后，`pfc_current_amp_cmd_a` 由 VBUS PI 输出；
- 若 VAC 极性错误，必须在测量层修正，不能在 PR 控制器内部隐藏反号。

## 8. 最小状态流程

为避免首版过度复杂，应用层只使用以下状态：

```text
PFC_SAFE
-> PFC_ADC_CHECK
-> PFC_PWM_OPEN_LOOP
-> PFC_CURRENT_LOOP
-> PFC_VBUS_LOOP
-> PFC_FAULT_LATCH
```

状态要求：

| 状态 | PE0 | HRTIM 输出 | 控制器状态 |
| --- | --- | --- | --- |
| `PFC_SAFE` | 低 | 关闭 | PR/PI Reset |
| `PFC_ADC_CHECK` | 低 | 关闭 | 只采样和 VOFA |
| `PFC_PWM_OPEN_LOOP` | 受人工允许 | 低调制度 | PR Reset |
| `PFC_CURRENT_LOOP` | 高 | PR 电流闭环 | PI 外环未启用 |
| `PFC_VBUS_LOOP` | 高 | 双环闭环 | PR + PI 有效 |
| `PFC_FAULT_LATCH` | 低 | Fault/软件关闭 | PR/PI Reset |

所有启动请求必须由人工操作或调试命令触发。任何故障都进入 `PFC_FAULT_LATCH`，信号恢复后不能自动重启。

## 9. 分阶段实施与验收

### 阶段 A：安全启动与基础外设

工作内容：

1. 完成 170 MHz 时钟、GPIO、HRTIM、ADC、USART2、IWDG 配置；
2. 上电后 PE0 保持低，HRTIM 输出无效；
3. 读取 RCC 复位原因；
4. 初始化 PR、应用状态和 VOFA；
5. 最后启动 IWDG。

验收：

- 上电、复位和未烧录状态下 Gate Enable 为低；
- 不执行显式启动函数时无 PWM 输出；
- HSE、SYSCLK、HRTIM 和 ADC 实际频率与配置一致；
- IWDG 复位后仍保持 Gate 关闭。

### 阶段 B：ADC 与 VOFA

工作内容：

1. PE0 始终保持低；
2. 完成 ADC 自校准；
3. 采集 VAC、IPFC、VBUS 原始值；
4. 测量零点偏置和比例；
5. 通过 VOFA 100 Hz 输出三路物理量。

验收：

- 断电零输入时 IPFC 接近 0 A；
- VAC 正负半周极性正确且无明显削顶；
- VBUS 与万用表读数在允许误差内；
- 主循环发送 VOFA 不改变快速 ADC 采样周期；
- ADC 断线、饱和或数据过期会进入故障锁存。

### 阶段 C：HRTIM A/B 低压开环

工作内容：

1. 断开高压电网，使用隔离、限流的低压直流源；
2. 先不接功率器件或保持 PE0 低，只测 MCU PWM；
3. 验证 HRTIM A/B 互补输出、死区和同步更新；
4. 使用很小的固定 `m` 验证差分调制方向；
5. 注入 PB10 Fault。

验收：

- PWM 实测为 10 kHz；
- A/B 桥臂占空比满足 `duty_A + duty_B = 1` 的初步差分关系；
- 无上下管交叠；
- Fault 3 无需软件参与即可关闭四路 PWM；
- Fault 清除后 PWM 不自动恢复。

### 阶段 D：VAC 同步开环

工作内容：

1. 只使用 `VAC` 归一化波形生成低幅值调制；
2. 保持 PR 和母线 PI 关闭；
3. 观察调制过零、极性和桥臂输出；
4. 调整 HRTIM ADC Trigger 到噪声较低的位置。

验收：

- 调制极性与 VAC 一致；
- VAC 过零附近无错误翻转和大占空跳变；
- `vac_norm` 始终在 `-1~1`；
- 交流输入丢失后软件关闭 PE0 和 PWM。

### 阶段 E：PR 电流内环

初步控制器实例：

```c
PR_t PR_PFC_I;

PR_Init(&PR_PFC_I,
        0.25f,       /* Kp：只作为低压调试起点 */
        5.0f,        /* Kr：只作为低压调试起点 */
        50.0f,       /* 基波频率 Hz */
        3.14159f,    /* 准 PR 带宽 rad/s */
        F_CONTROL_HZ,/* 必须等于 PR_Calc 实际调用频率 */
        -0.90f,
        0.90f);
```

工作内容：

1. `pfc_current_amp_cmd_a` 从很小的安全值开始；
2. 生成 `ipfc_ref_a = pfc_current_amp_cmd_a * vac_norm`；
3. 调用 `PR_Calc()` 并更新 HRTIM A/B；
4. 调整反馈极性，确认不存在正反馈；
5. 逐步整定 Kp、Kr 和输出限幅；
6. 停机、欠压或 Fault 后调用 `PR_Reset()`。

验收：

- IPFC 基本跟踪参考，且与 VAC 同相；
- 电流峰值不超过软件和硬件安全限值；
- 过零附近无持续振荡；
- 调制未长时间顶在 `+/-0.90`；
- 快速 ISR 最坏执行时间小于控制周期的 30%。

### 阶段 F：母线电压 PI 外环

母线 PI 不放入 `pr.c`，在 `pfc_app.c` 中单独实现，初步 1 kHz 执行一次：

```text
vbus_error = vbus_ref_v - vbus_v
pfc_current_amp_cmd_a = PI(vbus_error)
```

要求：

- 输出范围限制为 `0~PFC_CURRENT_AMP_MAX_A`；
- 实现最基本的积分限幅或条件积分；
- 母线参考使用斜坡上升，不允许一步跳到目标值；
- VBUS 欠压、过压或采样异常时立即退出双环；
- PI 增益不能直接照搬 Simulink 连续参数，必须按照 1 kHz 离散周期和实际标幺/物理量重新换算。

验收：

- 母线能缓慢进入目标区间，无明显过冲；
- 负载变化后母线能够恢复；
- 外环不会导致电流幅值指令突变；
- 达到电流限幅时 PI 不持续积分；
- 停机后 PI 和 PR 状态都被清零。

### 阶段 G：保护与 IWDG 完整性

必选故障：

- PB10/HRTIM FLT3；
- PE5 驱动诊断故障；
- IPFC 软件过流；
- VBUS 软件过压和欠压；
- VAC 丢失；
- ADC 数据过期或饱和；
- 快速控制 ISR 心跳停止；
- HSE CSS 故障；
- 状态过渡超时。

安全监督任务每 100 ms 只在以下条件全部成立时刷新 IWDG：

- 快速控制心跳持续更新；
- ADC1/2 数据持续更新；
- 无 HRTIM Fault；
- 状态机没有卡住；
- PE0 实际状态与软件状态一致。

验收：

- 停止快速 ISR 后约 500 ms 内产生 IWDG 复位；
- IWDG 复位原因可以读取；
- 复位后 Gate Enable 和 PWM 仍关闭；
- 所有故障均锁存，必须人工确认后才能重新启动。

## 10. 参数清单

以下参数在带功率运行前必须根据 PCB、传感器和功率器件填写，不能沿用仿真标幺值：

```text
ADC_VREF_V
ADC_FULL_SCALE
IPFC_ZERO_CODE
IPFC_A_PER_CODE
VAC_ZERO_CODE
VAC_V_PER_CODE
VBUS_V_PER_CODE
VAC_PEAK_EST_V
VBUS_REF_V
VBUS_UV_TRIP_V
VBUS_OV_TRIP_V
IPFC_SW_LIMIT_A
PFC_CURRENT_AMP_MAX_A
PFC_MODULATION_LIMIT
PFC_PWM_HZ
PFC_CONTROL_HZ
PFC_DEADTIME_NS
PFC_PRECHARGE_TIMEOUT_MS
```

参数缺失、为零、超出合理范围或 ADC 标定失败时，软件必须停留在 `PFC_SAFE`。

## 11. 中断优先级建议

| 优先级 | 中断/事件 | 要求 |
| ---: | --- | --- |
| 硬件路径 | HRTIM FLT3 | 不依赖 CPU，立即关闭 A/B |
| 0~1 | ADC 快速转换/HRTIM 控制事件 | 只执行快速采样和 PR 控制 |
| 2~3 | PE5 EXTI、HRTIM Fault ISR | 记录故障并关闭 PE0 |
| 5~7 | 慢速 ADC DMA、1 kHz 调度 | 不得阻塞快速控制 |
| 最低 | SysTick、VOFA/OLED 相关任务 | 仅后台执行 |

STM32G474 为 Cortex-M4F，不需要 Cortex-M7 的 D-Cache DMA 维护，但 ISR 与主循环共享的测量快照仍需使用短临界区或序号校验，避免读取一半更新的数据。

## 12. 首版完成标准

当以下条件全部满足时，认为“单相 PWM 整流器软件初步配置”完成：

1. 170 MHz 系统时钟、HRTIM 和 ADC 时钟实测正确；
2. PE0 在复位和异常时始终保持低；
3. VAC、IPFC、VBUS 三路采样比例、零点和极性正确；
4. VOFA 能稳定显示三路波形且不在 ISR 中发送；
5. HRTIM A/B 输出 10 kHz 互补 PWM，死区正确；
6. PB10 Fault 可不依赖 CPU 关闭全部整流 PWM；
7. VAC 同步低压开环方向正确；
8. PR 电流环能够低电流稳定运行；
9. 母线 PI 外环能够软启动并限制电流幅值；
10. 停止控制 ISR 会触发 IWDG，复位后功率输出仍关闭；
11. Keil 编译达到 `0 Error(s), 0 Warning(s)`，并生成 HEX；
12. 固件链接空间不超过 STM32G474VC 的 256 KB Flash。

## 13. 明确边界

- 软件电流限幅不能代替驱动器 DESAT/OCP、硬件急停和 HRTIM Fault；
- 如果栅极驱动器没有可靠的独立短路保护，不允许进行高压带功率测试；
- COMP 首版可以不使用，相关引脚和内部 Fault 1/2 不作为整流器启动依赖；
- 不把三相逆变器代码、三相 ADC3/4/5 或 HRTIM C/D/E 混入本阶段；
- 不直接复制 STM32F1/F4 工程的 HAL、启动文件、链接脚本或定时器初始化；
- 高压测试必须排在 ADC/VOFA、无功率 PWM、硬件 Fault、低压开环和低电流闭环全部通过之后。
