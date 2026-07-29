# STM32G474 单相 PWM 整流器 CubeMX 外设配置指南

## 1. 适用环境与目标

本文以当前 [`03-1OpenLoop.ioc`](<03开环验证/3_1 OpenLoop/03-1OpenLoop.ioc>) 为配置事实来源，用于配置题目要求1~5所需的单相全桥PWM整流器外设，并为后续三相逆变器联合运行保留资源。

| 项目 | 基线 |
| --- | --- |
| STM32CubeMX | 6.17.0 |
| 固件包 | STM32Cube FW_G4 V1.6.2 |
| MCU | STM32G474VCT6，LQFP100 |
| HSE | 8 MHz |
| SYSCLK/HCLK/HRTIM1 | 170 MHz |
| PWM/快速控制频率 | 10 kHz，每个 PWM 周期执行一次控制 |
| 软件结构 | CubeMX + HAL，裸机前后台 |
| COMP | 首版不启用 |

本文只描述 CubeMX 中实际存在的设置，以及生成代码后必须补充的启动和保护逻辑。不会把软件状态机、HAL 启动调用或示波器整定项伪装成 CubeMX 参数。

当前IOC由`STM32Cube FW_G4 V1.6.3`生成，且`ProjectManager.LastFirmware=true`；本项目规范仍固定为V1.6.2。因此，下次正式重新生成代码前，应在CubeMX中选择`STM32Cube FW_G4 V1.6.2`并关闭自动使用最新固件包。切换固件包后必须完整比较HRTIM、ADC、DMA和Fault生成代码，不应只依据CubeMX页面无报错判断迁移成功。

> PE0 必须有板级下拉。驱动器必须具备独立 DESAT/OCP 或等效短路保护。PB10/HRTIM1_FLT3 是首版必须保留的硬件关断路径，ADC 软件过流不能替代它。

### 1.1 题目要求1~5对应的系统目标

| 项目 | 目标 |
| --- | --- |
| 单相输入 | 36 V RMS、50 Hz |
| 三相输出模式1 | 线电压32 V RMS、60 Hz、线电流2.0 A |
| 三相输出模式2 | 线电压32 V RMS、30 Hz、线电流2.0 A |
| 输入功率因数 | 不低于0.98，按60 Hz额定输出工况验收 |
| 整机效率 | 不低于95%，包含直流辅助电源消耗 |
| 输出线电压THD | 不大于2%，按60 Hz额定输出工况验收 |

三相阻性负载额定输出功率约为：

```text
Pout = sqrt(3) x 32 V x 2 A = 110.9 W
```

按95%效率和0.98输入功率因数估算，36 V输入侧额定电流约为3.3 A RMS。IPFC传感器、模拟前端和软件量程应覆盖启动与纹波峰值，初步建议至少覆盖正负5 A，最终范围由电感纹波和硬件保护阈值确定。

36 V输入峰值约为50.9 V。为给升压PFC和后级三相SVPWM留出裕量，母线目标可先在60~65 V范围内进行仿真和低压验证，但最终值必须依据实际拓扑、器件耐压、调制度和损耗重新确定，不能把该范围直接当作最终保护阈值。

### 1.2 分阶段配置档

| 配置档 | CubeMX资源 | 输出允许条件 |
| --- | --- | --- |
| 裸板开环验证 | 当前HRTIM A/B、ADC1/2、DMA、USART2、OLED、IWDG、FLT3 | PE0始终为低，只允许示波器测PA8~PA11 |
| 低压PFC验证 | 外设配置基本不变，增加VAC同步、软件保护和人工启动状态机 | 隔离限流低压、采样有效、Fault无效且人工确认后才允许PE0为高 |
| 题目要求1~5联合运行 | 在后续联合工程增加HRTIM C/D/E、逆变采样和INV_GATE_EN | PFC母线稳定后启动逆变器，支持60 Hz和30 Hz两档 |

当前IOC不需要提前启用HRTIM C/D/E/F或ADC3/4/5，但相应引脚不得分配给无关的永久功能。裸板开环代码中自动开放MCU PWM引脚只属于第一档，不能直接沿用到带功率版本。

### 1.3 当前IOC审查结论

| 项目 | 当前状态 | 处理结论 |
| --- | --- | --- |
| 170 MHz时钟树和ADC异步/4 | 正确 | 保持 |
| HRTIM Master/A/B、10 kHz和500 ns初始死区 | 正确 | 保持，死区以后按实测调整 |
| Master CMP2和ADC Trigger 1 | 正确 | 保持，采样位置以后按示波器调整 |
| ADC1/ADC2规则组与双循环DMA | 正确 | 保持 |
| DMA和ADC1_2中断 | 正确 | 保持当前优先级及共享ADC错误中断 |
| PB10 Fault 3数字输入、低有效、无Blanking | 正确 | 保持，带功率前完成无CPU关断实测 |
| OLED软件I2C和USART2 | 可用于调试 | 保持在主循环低速任务中使用 |
| 固件包 | 当前为G4 V1.6.3且自动最新版 | 正式重新生成前改为V1.6.2并关闭自动最新版 |
| HSE CSS | 当前IOC未记录启用 | 带功率前建议启用，并实现NMI安全关断 |
| HRTIM C/D/E、ADC3/4/5 | 当前关闭 | 当前PFC工程保持关闭，联合工程再启用 |

因此，不建议为了题目要求1~5修改当前PFC的PWM频率、ADC时钟、DMA通道或Fault路由。当前阶段真正需要的小幅CubeMX调整只有固件包固定以及带功率前启用CSS；人工启动、故障锁存、VAC同步、物理阈值保护和IWDG复位策略属于生成代码后的软件职责。

## 2. 引脚与外设总表

| 功能 | 外设 | 引脚 | CubeMX 信号 |
| --- | --- | --- | --- |
| 整流桥臂 A 高侧 | HRTIM Timer A Output 1 | PA8 | HRTIM1_CHA1 |
| 整流桥臂 A 低侧 | HRTIM Timer A Output 2 | PA9 | HRTIM1_CHA2 |
| 整流桥臂 B 高侧 | HRTIM Timer B Output 1 | PA10 | HRTIM1_CHB1 |
| 整流桥臂 B 低侧 | HRTIM Timer B Output 2 | PA11 | HRTIM1_CHB2 |
| 输入电流 IPFC | ADC1 Regular Rank 1 | PA1 | ADC1_IN2 |
| 母线电压 VBUS | ADC1 Regular Rank 2 | PC1 | ADC1_IN7 |
| 交流电压 VAC | ADC2 Regular Rank 1 | PC0 | ADC2_IN6 |
| ADC1 采样搬运 | DMA（CubeMX 分配通道） | 内部连接 | ADC1 DMA Request |
| ADC2 采样搬运 | DMA（CubeMX 分配通道） | 内部连接 | ADC2 DMA Request |
| PFC 驱动使能 | GPIO Output | PE0 | PFC_GATE_EN |
| 驱动总故障 | HRTIM Fault 3 | PB10 | HRTIM1_FLT3 |
| PFC 故障诊断 | GPIO EXTI | PE5 | PFC_NFAULT_DIAG |
| VOFA | USART2 | PD5/PD6 | USART2_TX/RX |
| OLED软件I2C | GPIO Output | PA15/PB7 | OLED_SCL/OLED_SDA |
| 看门狗 | IWDG | 内部 LSI | IWDG |

首版只为 ADC1、ADC2 启用两路独立 DMA。HRTIM C/D/E/F、ADC3/4/5、COMP1~7、DAC和普通 TIM 保持关闭。三相逆变器预留引脚不要分配给无关功能。

## 3. RCC、SYS 与工程设置

### 3.1 RCC 和 Clock Configuration

| CubeMX 位置 | 字段 | 设置值 | 生成结果/核对 |
| --- | --- | --- | --- |
| System Core > RCC | High Speed Clock | Crystal/Ceramic Resonator | PF0/PF1 为 HSE |
| System Core > RCC | Low Speed Clock | LSI | IWDG 使用 |
| Clock Configuration | PLL Source | HSE | `RCC_PLLSOURCE_HSE` |
| Clock Configuration | PLLM/PLLN/PLLR | /2、x85、/2 | PLL VCO 340 MHz |
| Clock Configuration | SYSCLK/HCLK | 170 MHz/170 MHz | `HAL_RCC_ClockConfig()` |
| Clock Configuration | HRTIM1 clock | 170 MHz | HRTIM 内核输入时钟 |
| Clock Configuration | ADC12 clock source | System Clock | ADC 内核输入为 170 MHz |

ADC 页面还要选择异步 `/4`。最终 ADC 工作时钟为：

```text
fADC = 170 MHz / 4 = 42.5 MHz
```

Clock Configuration 中显示的 170 MHz 是 ADC 内核输入，不代表转换器直接以 170 MHz 工作。

若 CubeMX 的 RCC 页面存在 `Clock Security System` 选项，则启用 HSE CSS；若当前版本没有显示该项，在生成代码的 `USER CODE` 区域调用 `HAL_RCC_EnableCSS()`。CSS/NMI 处理属于生成代码后的安全逻辑。

### 3.2 SYS、NVIC 和工程

| CubeMX 位置 | 字段 | 设置值 |
| --- | --- | --- |
| System Core > SYS | Debug | Serial Wire |
| System Core > SYS | Timebase Source | SysTick |
| System Core > NVIC | Priority Group | NVIC_PRIORITYGROUP_4 |
| Project Manager | Toolchain/IDE | MDK-ARM V5 |
| Project Manager | Keep User Code | Enable |
| Project Manager | Generate peripheral initialization as `.c/.h` | Enable |
| Project Manager | Firmware Package | STM32Cube FW_G4 V1.6.2 |
| Project Manager | Use latest available firmware | Disable |

工程设备宏必须为 `STM32G474xx`，链接空间按 VCT6 的 256 KB Flash 限制。不要复制 F1/F4 工程的启动文件、HAL 驱动或外设初始化代码。

## 4. GPIO 与故障引脚

### 4.1 PE0：PFC_GATE_EN

| CubeMX 位置 | 字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- | --- |
| Pinout | PE0 | GPIO_Output | GPIO 初始化 |
| GPIO Settings | User Label | PFC_GATE_EN | `PFC_GATE_EN_Pin` |
| GPIO Settings | GPIO output level | Low | 初始化前写低 |
| GPIO Settings | GPIO mode | Output Push Pull | `GPIO_MODE_OUTPUT_PP` |
| GPIO Settings | Pull-up/Pull-down | No pull | `GPIO_NOPULL` |
| GPIO Settings | Maximum output speed | Low | `GPIO_SPEED_FREQ_LOW` |

PE0 的 CubeMX 初始低电平只是软件措施，不能替代板级下拉。初始化、ADC 校准、HRTIM DLL 校准和自检完成前不得拉高 PE0。

### 4.2 PE5：PFC_NFAULT_DIAG

| CubeMX 位置 | 字段 | 设置值 |
| --- | --- | --- |
| Pinout | PE5 | GPIO_EXTI5 |
| GPIO Settings | User Label | PFC_NFAULT_DIAG |
| GPIO mode | External Interrupt Mode with Falling edge trigger detection |
| Pull-up/Pull-down | Pull-up，与当前IOC一致；正式硬件同时使用板级上拉 |
| NVIC | EXTI line[9:5] interrupt | Enable，Priority 3 |

PE5 只用于记录 PFC 驱动器故障来源。真正的快速关断由 PB10/HRTIM1_FLT3 完成。

### 4.3 PA15/PB7：OLED软件I2C

| 引脚 | User Label | GPIO模式 | Pull | Speed |
| --- | --- | --- | --- | --- |
| PA15 | OLED_SCL | Output Push Pull | No pull | High |
| PB7 | OLED_SDA | Output Open Drain | No pull | High |

OLED仅用于低速调试显示，不参与采样、控制或保护。软件I2C刷新只能放在主循环低速任务中，禁止在ADC DMA、HRTIM或Fault中断中调用。SDA必须有外部上拉；若OLED模块已经自带上拉，不要重复并联过小阻值。

### 4.4 PB10：HRTIM1_FLT3

在 Pinout 选择 `PB10 > HRTIM1_FLT3`。PB10 不再配置 GPIO EXTI。

多个驱动器故障脚只有在均为开漏输出时才能线与汇总。推挽输出必须先经过逻辑门或开漏转换。

## 5. HRTIM1 配置

### 5.1 启用模式

在 `Timers > HRTIM1` 中启用：

- Master Timer；
- Timer A Output TA1/TA2；
- Timer B Output TB1/TB2；
- ADC Trigger 1；
- Fault Line 3。

HRTIM C/D/E/F 保持禁用。CubeMX 只负责生成初始化，默认不要在 `MX_HRTIM1_Init()` 后自动启动计数器或输出。

### 5.2 时基计算

CubeMX 的 `MUL4` 是“HRTIM Clock Multiplied by 4”，不是普通定时器的 `/4`。

Timer A/B 采用 Up-Down 计数：

```text
fTA/TB = 170 MHz x 4 = 680 MHz
fPWM   = fTA/TB / (2 x Period)
       = 680 MHz / (2 x 34000)
       = 10 kHz
```

Master 采用向上计数：

```text
fMASTER = 170 MHz x 2 = 340 MHz
fperiod = fMASTER / Period
        = 340 MHz / 34000
        = 10 kHz
```

HRTIM Period 为 16 位字段。若 Timer A/B 使用 `MUL16`，10 kHz Up-Down 所需 Period 为 136000，超过 65535，因此本方案使用 `MUL4 + Period 34000`。

### 5.3 Master Timer

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| Time Base Setting | Prescaler Ratio | HRTIM Clock Multiplied by 2 | `HRTIM_PRESCALERRATIO_MUL2` |
| Time Base Setting | Period | 34000 | `pTimeBaseCfg.Period = 34000` |
| Time Base Setting | Repetition Counter | 0 | `RepetitionCounter = 0` |
| Time Base Setting | Mode | Continuous | `HRTIM_MODE_CONTINUOUS` |
| Compare Unit 2 | Compare Value | 17000 | Master CMP2位于50 us处 |

Master 只提供统一周期参考和 ADC Trigger 位置，不输出 PWM。

### 5.4 Timer A 和 Timer B

A/B 参数完全相同：

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| Time Base Setting | Prescaler Ratio | HRTIM Clock Multiplied by 4 | `HRTIM_PRESCALERRATIO_MUL4` |
| Time Base Setting | Period | 34000 | `Period = 34000` |
| Time Base Setting | Repetition Counter | 0 | `RepetitionCounter = 0` |
| Time Base Setting | Mode | Continuous | `HRTIM_MODE_CONTINUOUS` |
| Waveform Timer Control | Up-Down Mode | Up-Down | `HRTIM_TIMERUPDOWNMODE_UPDOWN` |
| Timer Configuration | Preload Enable | Enable | `HRTIM_PRELOAD_ENABLED` |
| Timer Configuration | Update Gating | Independent | `HRTIM_UPDATEGATING_INDEPENDENT` |
| Timer Configuration | Repetition Update | Enable | `HRTIM_UPDATEONREPETITION_ENABLED` |
| Timer Configuration | Push-Pull | Disable | `HRTIM_TIMPUSHPULLMODE_DISABLED` |
| Timer Configuration | Dead Time Insertion | Enable | `HRTIM_TIMDEADTIMEINSERTION_ENABLED` |
| Timer Configuration | DMA Requests | None | `HRTIM_TIM_DMA_NONE` |

Compare Unit 1 用于占空比，Compare 2/3/4 首版不启用。初始 Compare 1 设为 17000，对应约 50% 占空关系，用于无功率波形验证。

Master、Timer A、Timer B 必须在生成代码后使用同一次 Counter Start 掩码启动，例如同时启动 Master/A/B，不能依次启动后假设三者相位完全一致。

### 5.5 Up-Down 输出、Set/Reset 与 ROM

沿用用户已使用过的 Up-Down + Valley Roll-over 思路，每个桥臂由 Output 1 定义主波形，Output 2 由死区逻辑形成互补输出。

Timer A：

| CubeMX 分组 | 字段 | TA1 | TA2 |
| --- | --- | --- | --- |
| Output Configuration | Polarity | High | High |
| Output Configuration | Set Source 1 | Timer A Compare 1 | None |
| Output Configuration | Reset Source 1 | Timer A Compare 1 | None |
| Output Configuration | Idle Level | Inactive | Inactive |
| Output Configuration | Fault Level | Inactive | Inactive |
| Output Configuration | Chopper Mode | Disable | Disable |

Timer B 使用相同设置，将事件替换为 Timer B Compare 1。

在 Timer A/B 的 Roll-over 设置中选择：

| 字段 | 设置值 |
| --- | --- |
| Output Roll-over Mode | Valley |
| Timer Roll-over Mode | Valley |
| Fault Event Roll-over Mode | Both |
| Burst Mode Roll-over Mode | Both |
| ADC Roll-over Mode | Both |

当前IOC和生成代码中，TA1/TB1的Set与Reset均选择各自Timer Compare 1，并配合`OUTROM_VALLEY`和`ROM_VALLEY`区分Up-Down过程中的动作；TA2/TB2由Dead Time逻辑形成互补输出。生成代码后必须同时核对`HAL_HRTIM_WaveformOutputConfig()`和`HAL_HRTIM_RollOverModeConfig()`，不能只看引脚上已经出现PWM就认为Set/Reset路径正确。

初始比较值与占空比的关系应根据示波器实测确认。按上述 Valley 模式，应用层先采用：

```text
duty_A = 0.5 x (1 + m)
duty_B = 0.5 x (1 - m)
CMP_A  = Period x (1 - duty_A)
CMP_B  = Period x (1 - duty_B)
m      = clamp(m, -0.90, +0.90)
```

在控制代码接入前，分别写入 25%、50%、75%目标值，确认实测占空方向与公式一致。若相反，只修正应用层映射，不在PR控制器内部隐藏反号。

### 5.6 Dead Time

Timer A/B 使用相同的 Dead Time：

| CubeMX 分组 | 字段 | 设置值 |
| --- | --- | --- |
| Dead Time | Prescaler | fHRTIM |
| Dead Time | Rising Value | 85 |
| Dead Time | Rising Sign | Positive |
| Dead Time | Rising Lock | Write/可修改 |
| Dead Time | Rising Sign Lock | Write/可修改 |
| Dead Time | Falling Value | 85 |
| Dead Time | Falling Sign | Positive |
| Dead Time | Falling Lock | Write/可修改 |
| Dead Time | Falling Sign Lock | Write/可修改 |

当 `fDTG = fHRTIM = 170 MHz`：

```text
tdead = 85 / 170 MHz = 500 ns
```

不要把“500 ns”直接填为计数值 500。调试阶段不要锁死死区寄存器，完成驱动器和功率器件实测后再决定是否启用锁。

500 ns只是低压调试起点，最终值必须依据驱动器传播延迟、MOSFET/IGBT关断时间、栅极电阻、温度和示波器波形确定。

### 5.7 ADC Trigger 1

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| ADC Triggers Configuration | ADC Trigger 1 | Enable ADC Trigger 1 | `HRTIM_ADCTRIGGER_1` |
| ADC Trigger 1 | Update Source | Master Timer | `HRTIM_ADCTRIGGERUPDATE_MASTER` |
| ADC Trigger 1 | Number of Active Trigger Sources | 1 | 单一触发源 |
| ADC Trigger 1 | Trigger Source | Master Compare 2 | `HRTIM_ADCTRIGGEREVENT13_MASTER_CMP2` |
| ADC Trigger 1 | Postscaler | 0 | 不分频 |

Master CMP2=17000 对应每100 us周期的50 us位置，每周期只产生一次 ADC Trigger。它是首次调试点，不是永久固定的“安静窗口”。后续应同时观察PWM开关沿、调理运放输出和ADC原始码，必要时移动CMP2。

### 5.8 Fault Line 3

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| Fault Configuration > Fault Line 3 | Source | Digital Input | `HRTIM_FAULTSOURCE_DIGITALINPUT` |
| Fault Line 3 | Polarity | Active Low | 低电平触发 |
| Fault Line 3 | Filter | None | 首次验证不加数字延时 |
| Fault Line 3 | Lock | Read/Write | 调试阶段允许修改 |
| Fault Line 3 | Enable | Enable | `HAL_HRTIM_FaultModeCtl()` |
| Timer A Configuration | Fault Enable | Fault 3 | Timer A响应FLT3 |
| Timer B Configuration | Fault Enable | Fault 3 | Timer B响应FLT3 |
| Timer A/B Configuration | Fault Lock | Read/Write | 不在调试期硬锁 |
| TA1/TA2/TB1/TB2 | Fault Level | Inactive | 故障输出无效态 |

Fault 输入的 CubeMX `Lock` 是硬件配置写保护，不等于软件的“故障锁存”。首版调试保持 Read/Write；软件仍必须在 Fault ISR 中拉低PE0、记录故障码，并要求人工确认后才能重新启动。

在 `HRTIM Interrupt Configuration` 中加入 `Fault 3 interrupt enable`，并在 NVIC 中启用 `HRTIM1 fault global interrupt`，对应 `HRTIM1_FLT_IRQn`，抢占优先级设为0。

### 5.9 DLL 与生成代码后的启动顺序

`MX_HRTIM1_Init()` 中应包含：

```text
HAL_HRTIM_Init()
HAL_HRTIM_DLLCalibrationStart()
HAL_HRTIM_PollForDLLCalibration()
HAL_HRTIM_TimeBaseConfig()
HAL_HRTIM_WaveformTimerControl()
HAL_HRTIM_WaveformTimerConfig()
HAL_HRTIM_WaveformCompareConfig()
HAL_HRTIM_DeadTimeConfig()
HAL_HRTIM_WaveformOutputConfig()
HAL_HRTIM_FaultConfig()
HAL_HRTIM_ADCTriggerConfig()
```

软件启动顺序固定为：

1. PE0保持低；
2. 按第6.6节完成ADC校准、两路DMA启动和Fault检查；
3. HRTIM DLL Ready；
4. 写入安全比较值；
5. 同时启动Master/A/B计数器；
6. 保持TA1/TA2/TB1/TB2输出关闭；
7. 状态机确认后启动输出；
8. 最后才允许PE0拉高。

停止或故障时顺序相反：先拉低PE0，再关闭HRTIM输出，并清零控制器状态。

## 6. ADC1、ADC2 与 DMA

本方案不再使用ADC注入组。ADC1和ADC2均使用规则组，由现有`HRTIM1 ADC Trigger 1`同时触发，并分别通过独立DMA通道循环搬运。ADC1先采IPFC、再采VBUS；ADC2只采VAC，因此IPFC和VAC由两个ADC核近同步开始采样。

### 6.1 ADC公共参数

ADC1、ADC2共同设置：

| CubeMX 分组 | 字段 | 设置值 | HAL生成结果 |
| --- | --- | --- | --- |
| Parameter Settings | Clock Prescaler | Asynchronous clock divided by 4 | `ADC_CLOCK_ASYNC_DIV4` |
| Parameter Settings | Resolution | 12-bit | `ADC_RESOLUTION_12B` |
| Parameter Settings | Data Alignment | Right | `ADC_DATAALIGN_RIGHT` |
| Parameter Settings | Gain Compensation | 0 | `GainCompensation = 0` |
| Parameter Settings | Low Power Auto Wait | Disable | `LowPowerAutoWait = DISABLE` |
| Parameter Settings | Continuous Conversion Mode | Disable | 每次HRTIM事件只启动一个规则序列 |
| Parameter Settings | Discontinuous Conversion Mode | Disable | 规则序列不中断 |
| Parameter Settings | DMA Continuous Requests | Enable | `DMAContinuousRequests = ENABLE` |
| Parameter Settings | Conversion Data Management Mode | DMA Circular Mode | `ADC_CONVERSIONDATA_DMA_CIRCULAR` |
| Parameter Settings | Overrun | Data overwritten | `ADC_OVR_DATA_OVERWRITTEN` |
| Parameter Settings | Oversampling | Disable | 不使用硬件过采样 |
| Regular Conversion | External Trigger Conversion Source | HRTIM1 ADC Trigger 1 | `ADC_EXTERNALTRIG_HRTIM_TRG1` |
| Regular Conversion | External Trigger Conversion Edge | Rising Edge | `ADC_EXTERNALTRIGCONVEDGE_RISING` |

如果CubeMX 6.17页面只显示`Conversion Data Management Mode`而不单独显示`DMA Continuous Requests`，以生成代码中的`ADC_CONVERSIONDATA_DMA_CIRCULAR`为最终核对依据。ADC1和ADC2上电后分别执行单端校准；校准期间PE0、HRTIM计数器和全部HRTIM输出必须关闭。

### 6.2 ADC1规则序列：IPFC和VBUS

| CubeMX 分组 | 字段 | 设置值 |
| --- | --- | --- |
| Parameter Settings | Scan Conversion Mode | Enable |
| ADC Regular Conversion Mode | Number of Conversions | 2 |
| Parameter Settings | EOC Selection | End of sequence of conversion |
| Regular Rank 1 | Channel | ADC1_IN2 / PA1 / IPFC |
| Regular Rank 1 | Single-ended/Differential | Single-ended |
| Regular Rank 1 | Sampling Time | 24.5 Cycles |
| Regular Rank 1 | Offset Number | None |
| Regular Rank 2 | Channel | ADC1_IN7 / PC1 / VBUS |
| Regular Rank 2 | Single-ended/Differential | Single-ended |
| Regular Rank 2 | Sampling Time | 24.5 Cycles |
| Regular Rank 2 | Offset Number | None |

DMA缓冲区顺序必须与Rank顺序一致：

```c
static uint16_t adc1_dma[2];
/* adc1_dma[0] = IPFC, adc1_dma[1] = VBUS */
```

VBUS随ADC1规则序列以10 kHz采样，但母线电压PI仍每10个PWM周期运行一次，即保持1 kHz外环频率。不要因采样频率提高而把外环执行频率改成10 kHz。

### 6.3 ADC2规则序列：VAC

| CubeMX 分组 | 字段 | 设置值 |
| --- | --- | --- |
| Parameter Settings | Scan Conversion Mode | Disable |
| ADC Regular Conversion Mode | Number of Conversions | 1 |
| Parameter Settings | EOC Selection | End of single conversion |
| Regular Rank 1 | Channel | ADC2_IN6 / PC0 / VAC |
| Regular Rank 1 | Single-ended/Differential | Single-ended |
| Regular Rank 1 | Sampling Time | 24.5 Cycles |
| Regular Rank 1 | Offset Number | None |

DMA缓冲区为：

```c
static uint16_t adc2_dma[1];
/* adc2_dma[0] = VAC */
```

24.5 cycles的采样阶段约为`24.5 / 42.5 MHz = 0.576 us`。按12位转换阶段约12.5 cycles估算，ADC1两个通道的完整序列约为1.74 us，明显小于100 us的PWM周期。最终采样时间仍须根据调理运放输出阻抗、RC滤波和实际建立误差调整。

### 6.4 DMA Settings

分别在ADC1和ADC2的`DMA Settings`中点击`Add`。当前IOC已经无冲突地分配ADC1到`DMA1_Channel1`、ADC2到`DMA1_Channel2`；继续基于当前工程生成时应保持该映射。若后续联合工程因新增外设发生冲突，再让CubeMX重新分配并同步修改NVIC和软件说明。

两路DMA均设置：

| CubeMX字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- |
| DMA Request | ADC1或ADC2 | 与对应ADC匹配 |
| Direction | Peripheral To Memory | `DMA_PERIPH_TO_MEMORY` |
| Mode | Circular | `DMA_CIRCULAR` |
| Peripheral Increment Address | Disable | `DMA_PINC_DISABLE` |
| Memory Increment Address | Enable | `DMA_MINC_ENABLE` |
| Peripheral Data Width | Half Word | `DMA_PDATAALIGN_HALFWORD` |
| Memory Data Width | Half Word | `DMA_MDATAALIGN_HALFWORD` |
| Priority | High | 与当前IOC一致，不得低于普通遥测外设DMA |

本方案使用两个独立DMA通道，不使用ADC双重模式，也不使用DMA双缓冲。STM32G474的Cortex-M4没有D-Cache，因此不需要额外执行Cache Clean/Invalidate。

HAL在`HAL_ADC_Start_DMA()`中会打开DMA Half Transfer中断。ADC1长度为2时，Half Transfer发生在仅有IPFC写入、VBUS尚未写入的时刻；它不能作为控制入口。两路DMA启动成功后关闭Half Transfer中断：

```c
__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
```

以上语句必须放在CubeMX保留的`USER CODE`区域。不要修改HAL库文件，也不要在CubeMX生成区手工删除中断使能代码。

### 6.5 NVIC与双ADC同步

规则组DMA方案不使用ADC1/2的EOC/JEOC中断作为控制入口，但`HAL_ADC_Start_DMA()`会启用ADC Overrun中断。因此当前IOC保持共享的`ADC1 and ADC2 global interrupt`开启是正确的，否则ADC发生Overrun后可能无法进入`HAL_ADC_ErrorCallback()`。快速控制入口仍然只能来自DMA完整传输回调。

| 中断 | 抢占优先级 | 子优先级 | 用途 |
| --- | ---: | ---: | --- |
| ADC1 and ADC2 global interrupt | 1 | 0 | 处理ADC Overrun等错误，不运行控制算法 |
| ADC2 DMA通道全局中断 | 1 | 0 | 发布VAC序列完成标志 |
| ADC1 DMA通道全局中断 | 2 | 0 | 唯一10 kHz控制入口 |

ADC2只有一次转换，硬件上会早于ADC1的两通道序列完成。再配合ADC2 DMA中断优先级高于ADC1，可在ADC1控制回调运行前先发布VAC序列号。不要仅依据两个DMA缓冲区“看起来都有数据”判断同步；必须用单调递增的序列计数检查本周期VAC是否已经完成。

推荐的软件数据：

```c
static volatile uint32_t adc2_sequence;
static uint32_t adc2_sequence_consumed;
static uint32_t control_heartbeat;
```

`HAL_ADC_ConvCpltCallback()`按`hadc->Instance`分流：ADC2分支只递增`adc2_sequence`；ADC1分支确认它不同于`adc2_sequence_consumed`，随后锁定本周期三个样本、更新已消费序列号并执行一次控制。ADC1回调之外不得再次运行PR控制器。

若ADC1完成时VAC序列号没有前进、任一路DMA停止、ADC发生Overrun或样本持续异常，应立即拉低PE0、关闭HRTIM输出并锁存故障。快速回调只允许采样快照、物理量换算、范围检查、参考生成、PR计算、调制限幅、HRTIM预装载比较值更新和心跳计数；禁止串口发送和阻塞等待。

### 6.6 校准和启动顺序

必须在HRTIM计数器产生第一个ADC Trigger之前准备好两个DMA通道：

1. 保持PE0为低并保持HRTIM输出关闭；
2. 确认HRTIM Fault 3未激活；
3. 对ADC1和ADC2分别执行`HAL_ADCEx_Calibration_Start(..., ADC_SINGLE_ENDED)`；
4. 清零DMA缓冲区、ADC状态标志、序列计数器和控制心跳；
5. 先调用`HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma, 1)`；
6. 再调用`HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma, 2)`；
7. 确认两个启动调用均返回`HAL_OK`，随后关闭两路DMA Half Transfer中断；
8. 完成HRTIM DLL校准并写入安全比较值；
9. 使用同一次掩码启动HRTIM Master、Timer A和Timer B计数器，暂不启动四路输出；
10. 连续观察ADC1/ADC2序列正常更新若干周期；
11. 状态机检查通过后才启动HRTIM输出，最后拉高PE0。

HRTIM未运行时，规则组只处于等待外部触发状态，不会因`HAL_ADC_Start_DMA()`自行连续转换。任一ADC/DMA启动失败时不得启动HRTIM计数器、PWM输出或Gate Enable。

ADC硬件自校准与传感器零点标定是两件事。IPFC可在Gate关闭且确认无电流时统计零点；VAC若在MCU启动前已经接入50 Hz交流，不能使用任意长度的短窗口直接取平均作为中点。当前软件使用256点/25.6 ms只适合无交流输入的裸板测试。带功率版本应使用经过板级验证的中点标定值，或至少对完整的50 Hz整数周期取平均，并检查输入是否削顶和偏置是否处于合理范围。

## 7. USART2 与 VOFA

| CubeMX 位置 | 字段 | 设置值 | 生成结果 |
| --- | --- | --- | --- |
| Connectivity > USART2 | Mode | Asynchronous | USART2启用 |
| Pinout | TX/RX | PD5/PD6 | USART2_TX/RX |
| Parameter Settings | Baud Rate | 460800 Bits/s | `BaudRate = 460800` |
| Parameter Settings | Word Length | 8 Bits | 8N1 |
| Parameter Settings | Parity | None | 8N1 |
| Parameter Settings | Stop Bits | 1 | 8N1 |
| Parameter Settings | Data Direction | Receive and Transmit | TX/RX |
| Parameter Settings | Hardware Flow Control | None | 无CTS/RTS |
| Parameter Settings | Oversampling | 16 | `UART_OVERSAMPLING_16` |
| DMA Settings | DMA | 不添加 | 阻塞发送 |
| NVIC Settings | USART2 global interrupt | Disable | 无UART中断 |

VOFA每10 ms在`while (1)`中发送VAC、IPFC和VBUS。禁止在ADC、HRTIM或Fault中断中调用`HAL_UART_Transmit()`。

## 8. IWDG

| CubeMX 位置 | 字段 | 设置值 | 说明 |
| --- | --- | --- | --- |
| System Core > IWDG | Prescaler | 64 | LSI分频 |
| System Core > IWDG | Reload Counter | 249 | 名义500 ms |
| System Core > IWDG | Window | 4095 | 不使用窗口限制 |

```text
Tnominal = (249 + 1) x 64 / 32000 = 0.5 s
```

LSI误差较大，上板必须测量实际超时。IWDG应在其他外设和状态机初始化完成后最后启动。

若SYS Debug页面提供IWDG Freeze选项，调试构建启用；若没有，在`USER CODE`区调用`__HAL_DBGMCU_FREEZE_IWDG()`。量产构建不要依赖调试冻结。

每100 ms检查控制心跳、ADC更新、HRTIM Fault、状态机超时和PE0状态，全部正常才刷新IWDG。

当前裸板程序在故障时停止HRTIM和ADC，随后因心跳停止而不再喂狗；IWDG复位后程序又可能按开环启动流程重新开放MCU PWM引脚。PE0保持低时这仍属于无功率试波，但正式带功率版本不得采用这种自动重启行为。应在复位后读取并记录`IWDGRSTF`，默认保持PWM输出和PE0关闭，只有人工启动命令、自检通过和Fault输入恢复后才重新使能；或者在已确认Gate/PWM均关闭的故障锁存状态继续条件喂狗，以保留故障信息。

## 9. NVIC汇总

| CubeMX中断 | 抢占优先级 | 子优先级 | 用途 |
| --- | ---: | ---: | --- |
| HRTIM1 fault global interrupt | 0 | 0 | 记录FLT3并拉低PE0 |
| ADC1 and ADC2 global interrupt | 1 | 0 | ADC Overrun等错误处理 |
| ADC2 DMA通道全局中断 | 1 | 0 | VAC规则序列完成标志 |
| ADC1 DMA通道全局中断 | 2 | 0 | 唯一10 kHz快速控制入口 |
| EXTI line[9:5] interrupt | 3 | 0 | PE5诊断故障 |
| SysTick | 15 | 0 | 只更新时间标志 |

Fault的PWM关断由HRTIM硬件完成，不依赖`HRTIM1_FLT_IRQn`是否及时响应。ISR只负责软件收尾和记录。

当前DMA中断分别为`DMA1_Channel1_IRQn`和`DMA1_Channel2_IRQn`。后续联合工程若由CubeMX重新分配通道，必须同步修改中断映射。`ADC1_2_IRQn`只处理ADC错误，不得在该中断路径中重复执行采样快照或控制算法。

首版不配置普通TIM。1 ms、10 ms和100 ms任务由SysTick产生标志，实际任务在主循环执行。

## 10. 配置与生成代码核对

| CubeMX设置 | 生成代码应出现 |
| --- | --- |
| Master MUL2/Period 34000 | Master `HAL_HRTIM_TimeBaseConfig()` |
| Timer A/B MUL4/Period 34000 | A/B各自的`HAL_HRTIM_TimeBaseConfig()` |
| A/B Up-Down | `HAL_HRTIM_WaveformTimerControl()` + `HRTIM_TIMERUPDOWNMODE_UPDOWN` |
| Valley ROM | `HAL_HRTIM_RollOverModeConfig()`含`OUTROM_VALLEY`和`ROM_VALLEY` |
| Compare 1 | A/B各自`HAL_HRTIM_WaveformCompareConfig()` |
| TA1/TB1 Set和Reset | 两者均为各自Timer Compare 1，TA2/TB2由Dead Time互补生成 |
| 500 ns死区 | `Prescaler DIV1`，Rising/Falling Value 85 |
| 四路输出 | 四次`HAL_HRTIM_WaveformOutputConfig()` |
| ADC Trigger 1 | `HAL_HRTIM_ADCTriggerConfig()`，Master CMP2 |
| Fault Line 3 | `HAL_HRTIM_FaultConfig()`和`HAL_HRTIM_FaultModeCtl()` |
| ADC1规则组Rank 1/2 | 两次`HAL_ADC_ConfigChannel()`，分别为IN2和IN7 |
| ADC2规则组Rank 1 | 一次`HAL_ADC_ConfigChannel()`，通道为IN6 |
| ADC1/2规则组外部触发 | `ADC_EXTERNALTRIG_HRTIM_TRG1`和上升沿触发 |
| ADC1/2循环DMA | 两个独立DMA句柄、`DMA_CIRCULAR`和Half Word数据宽度 |
| ADC12异步/4 | `ADC_CLOCK_ASYNC_DIV4` |
| ADC错误中断 | `ADC1_2_IRQn`已启用，`HAL_ADC_Start_DMA()`启用`ADC_IT_OVR` |

若CubeMX生成结果与表中不一致，先回到`.ioc`修正，避免直接编辑生成区掩盖配置错误。

还必须核对：生成结果中不再出现本方案ADC通道对应的`HAL_ADCEx_InjectedConfigChannel()`；ADC1的`NbrOfConversion`为2，ADC2为1；两个ADC均通过`__HAL_LINKDMA()`链接各自DMA句柄。应用启动代码中的两个`HAL_ADC_Start_DMA()`必须早于HRTIM Counter Start。

## 11. 配置顺序

1. 固定MCU、G4 V1.6.2和MDK-ARM，关闭自动使用最新固件包；
2. 核对8 MHz HSE和170 MHz时钟树；
3. 配置PE0、PE5和PB10；
4. 配置HRTIM Master、Timer A/B、Compare、Output、ROM和Dead Time；
5. 配置Fault Line 3及HRTIM Fault中断；
6. 配置Master CMP2和ADC Trigger 1；
7. 配置ADC1/ADC2规则组，二者均选择HRTIM1 ADC Trigger 1上升沿触发；
8. 分别添加ADC1和ADC2循环DMA，设置Half Word宽度和DMA中断优先级；
9. 启用ADC1_2共享中断用于ADC Overrun错误处理；
10. 配置USART2和软件I2C OLED GPIO；
11. 配置IWDG和调试冻结；
12. 检查Pinout冲突、Clock Configuration、DMA和NVIC；
13. 生成代码并按第10节逐项核对；
14. 完成无功率测试和Fault注入后才进入隔离限流的低压功率测试。

## 12. 验收清单

### 12.1 CubeMX静态检查

- [ ] STM32G474VCT6、LQFP100、G4 V1.6.2、256 KB Flash。
- [ ] SYSCLK/HCLK/HRTIM1均为170 MHz。
- [ ] ADC1/2为ASYNC DIV4，实际42.5 MHz。
- [ ] Master为MUL2、Period 34000、CMP2 17000。
- [ ] Timer A/B为MUL4、UPDOWN、Period 34000。
- [ ] Timer A/B启用Preload、Repetition Update和Dead Time。
- [ ] TA1/TA2/TB1/TB2的Output、ROM和Fault Level配置完整。
- [ ] Dead Time Prescaler为fHRTIM，Rising/Falling均为85。
- [ ] ADC Trigger 1来源为Master Compare 2，Postscaler 0。
- [ ] PB10为Fault Line 3 Digital Input、Active Low、Filter None。
- [ ] Timer A/B均启用Fault 3，四路Fault Level为Inactive。
- [ ] ADC1规则组Rank 1为PA1/IPFC，Rank 2为PC1/VBUS，均为24.5 cycles。
- [ ] ADC2规则组Rank 1为PC0/VAC，采样时间为24.5 cycles。
- [ ] ADC1和ADC2规则组均使用HRTIM1 ADC Trigger 1上升沿触发。
- [ ] ADC1转换数为2且Scan Enable；ADC2转换数为1。
- [ ] 两个ADC均为Circular DMA、Half Word、Memory Increment Enable。
- [ ] ADC1_2共享中断开启且优先级1，只用于ADC错误处理。
- [ ] DMA1_Channel2/ADC2中断优先级1，DMA1_Channel1/ADC1中断优先级2。
- [ ] Master、Timer A/B、ADC Trigger 1、Dead Time和Fault 3配置未因ADC改造发生变化。
- [ ] USART2为PD5/PD6、460800、8N1、无DMA和NVIC。
- [ ] OLED软件I2C为PA15/SCL推挽输出和PB7/SDA开漏输出，不占用控制中断。
- [ ] COMP、ADC3/4/5、HRTIM C/D/E/F和普通TIM保持关闭。
- [ ] Pinout无黄色冲突。

### 12.2 无功率上板检查

1. 复位和初始化期间PE0始终为低；
2. PWM实测为10 kHz，Master和A/B物理周期均为100 us；
3. A/B桥臂互补，死区初值约500 ns，无上下管重叠；
4. 25%、50%、75%测试值的实测占空方向与比较值公式一致；
5. ADC Trigger和ADC1 DMA完成回调均为10 kHz，每周期只执行一次控制；
6. ADC1缓冲区顺序始终为IPFC、VBUS，ADC2缓冲区为VAC；
7. IPFC和VAC属于同一PWM周期，连续运行时无跨周期混用；
8. VBUS以10 kHz采样，但母线外环仍严格按1 kHz执行；
9. ADC采样位置远离实际开关沿；
10. 人为停止任一路DMA后，软件检测失步、拉低PE0并锁存故障；
11. PB10拉低时不依赖CPU立即关闭四路PWM；
12. PB10恢复后PWM不自动恢复；
13. 停止控制心跳后IWDG在实测LSI误差范围内复位；
14. IWDG复位后PE0和PWM仍保持关闭，未收到人工启动命令时不重新开放输出。

### 12.3 题目要求1~5相关的后续验收

以下内容不是当前裸板IOC能够单独完成的验收，但当前PFC配置必须为其提供稳定基础：

1. 在36 V RMS、50 Hz输入下，PFC闭环建立目标母线，IPFC与VAC同相且无持续过零振荡；
2. 使用功率分析仪测得输入功率因数不低于0.98；
3. PFC和三相逆变器联合运行，在2.0 A阻性负载下输出线电压为32 V RMS；
4. 三相输出支持60 Hz和30 Hz参数档，频率切换由状态机完成，不在运行中直接跳变相位；
5. 60 Hz额定工况下输出线电压THD不大于2%；
6. 60 Hz额定工况下整机效率不低于95%，直流辅助电源消耗计入输入功率；
7. 记录母线纹波、IPFC峰值、器件温升和快速ISR最坏执行时间，确认没有以牺牲保护裕量换取效率或THD。

输出THD主要由后续三相逆变器的PWM、LC滤波器、死区及其补偿、电压闭环和母线纹波共同决定。当前PFC的10 kHz配置先保持不变；是否提高逆变开关频率，应在后续联合仿真和损耗实测后单独决定。

## 13. 首版边界

- PWM和控制频率均为10 kHz，每周期运行一次PR。
- ADC1/ADC2使用独立规则组和两个独立循环DMA通道，不启用注入组或ADC双重模式。
- IPFC、VBUS和VAC均由现有HRTIM1 ADC Trigger 1以10 kHz触发，不增加TIM6。
- ADC1 DMA完成回调是唯一快速控制入口，ADC2 DMA完成回调只发布VAC序列号。
- USART2使用阻塞发送，不启用UART DMA或中断。
- COMP不作为首版启动依赖，但DESAT/OCP和HRTIM Fault 3不可省略。
- 500 ns死区、24.5 cycles采样时间和Master CMP2采样位置都必须通过实际硬件测量修正。
- 当前IOC只覆盖单相PWM整流器，不等于题目要求1~5的完整CubeMX工程；三相逆变阶段仍需启用HRTIM C/D/E、三相电压/电流采样和独立Gate Enable。
- 当前自由运行50 Hz SPWM只用于裸板观察。接入36 V交流源前必须改为VAC同步调制，并验证VAC/IPFC极性、输入丢失停机和软件限值。
- 带功率前必须实现IPFC过流、VBUS过压/欠压、VAC丢失、ADC饱和/过期和状态超时保护；ADC码值大于4095的检查不能代替这些物理阈值。
- 要求6的负载调整率和要求7的31~41 V输入调整率不属于本阶段目标，但硬件耐压、电流量程和保护范围不应因此只覆盖单一理想工作点。
