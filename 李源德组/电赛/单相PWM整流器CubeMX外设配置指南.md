# STM32G474 单相 PWM 整流器 CubeMX 外设配置指南

## 1. 适用环境与目标

本文基于 [`01时钟树配置.ioc`](01时钟树配置/01时钟树配置.ioc)，用于在 STM32CubeMX 中继续配置单相全桥 PWM 整流器的首版外设。

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

> PE0 必须有板级下拉。驱动器必须具备独立 DESAT/OCP 或等效短路保护。PB10/HRTIM1_FLT3 是首版必须保留的硬件关断路径，ADC 软件过流不能替代它。

## 2. 引脚与外设总表

| 功能 | 外设 | 引脚 | CubeMX 信号 |
| --- | --- | --- | --- |
| 整流桥臂 A 高侧 | HRTIM Timer A Output 1 | PA8 | HRTIM1_CHA1 |
| 整流桥臂 A 低侧 | HRTIM Timer A Output 2 | PA9 | HRTIM1_CHA2 |
| 整流桥臂 B 高侧 | HRTIM Timer B Output 1 | PA10 | HRTIM1_CHB1 |
| 整流桥臂 B 低侧 | HRTIM Timer B Output 2 | PA11 | HRTIM1_CHB2 |
| 输入电流 IPFC | ADC1 Injected Rank 1 | PA1 | ADC1_IN2 |
| 交流电压 VAC | ADC2 Injected Rank 1 | PC0 | ADC2_IN6 |
| 母线电压 VBUS | ADC1 Regular Rank 1 | PC1 | ADC1_IN7 |
| PFC 驱动使能 | GPIO Output | PE0 | PFC_GATE_EN |
| 驱动总故障 | HRTIM Fault 3 | PB10 | HRTIM1_FLT3 |
| PFC 故障诊断 | GPIO EXTI | PE5 | PFC_NFAULT_DIAG |
| VOFA | USART2 | PD5/PD6 | USART2_TX/RX |
| 看门狗 | IWDG | 内部 LSI | IWDG |

首版保持 HRTIM C/D/E/F、ADC3/4/5、COMP1~7、DAC、普通 TIM 和全部 DMA 关闭。三相逆变器预留引脚不要分配给无关功能。

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
| Pull-up/Pull-down | No pull，使用板级上拉 |
| NVIC | EXTI line[9:5] interrupt | Enable，Priority 2 |

PE5 只用于记录 PFC 驱动器故障来源。真正的快速关断由 PB10/HRTIM1_FLT3 完成。

### 4.3 PB10：HRTIM1_FLT3

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
| Output Configuration | Reset Source 1 | None | None |
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

这里的 Valley Roll-over 是主输出在 Up-Down 周期中形成闭合波形的一部分，所以不能只检查 `ResetSource = NONE`。生成代码后必须同时看到对应的 `HAL_HRTIM_RollOverModeConfig()`；若 ROM/OUTROM 不是 Valley，则必须重新设计明确的 Set/Reset事件，不能直接沿用本表。

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
2. 完成ADC校准和Fault检查；
3. HRTIM DLL Ready；
4. 写入安全比较值；
5. 同时启动Master/A/B计数器；
6. 保持TA1/TA2/TB1/TB2输出关闭；
7. 状态机确认后启动输出；
8. 最后才允许PE0拉高。

停止或故障时顺序相反：先拉低PE0，再关闭HRTIM输出，并清零控制器状态。

## 6. ADC1 和 ADC2

### 6.1 ADC 公共参数

ADC1、ADC2均设置：

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| Parameter Settings | Clock Prescaler | Asynchronous clock divided by 4 | `ADC_CLOCK_ASYNC_DIV4` |
| Parameter Settings | Resolution | 12-bit | `ADC_RESOLUTION_12B` |
| Parameter Settings | Data Alignment | Right | `ADC_DATAALIGN_RIGHT` |
| Parameter Settings | Gain Compensation | 0 | `GainCompensation = 0` |
| Parameter Settings | Scan Conversion Mode | Disable | 单通道组 |
| Parameter Settings | EOC Selection | End of single conversion | `ADC_EOC_SINGLE_CONV` |
| Parameter Settings | Low Power Auto Wait | Disable | 无等待 |
| Parameter Settings | Continuous Conversion | Disable | 外部/软件单次触发 |
| Parameter Settings | Discontinuous Conversion | Disable | 禁用 |
| Parameter Settings | DMA Continuous Requests | Disable | 首版无DMA |
| Parameter Settings | Overrun | Data overwritten | `ADC_OVR_DATA_OVERWRITTEN` |
| Parameter Settings | Oversampling | Disable | 禁用 |

ADC1和ADC2上电后分别执行单端校准。校准期间PE0和HRTIM输出必须关闭。

### 6.2 ADC1注入组：IPFC

| CubeMX 分组 | 字段 | 设置值 |
| --- | --- | --- |
| ADC Injected Conversion Mode | Number of Conversions | 1 |
| Injected Rank 1 | Channel | ADC1_IN2 / PA1 |
| Injected Rank 1 | Single-ended/Differential | Single-ended |
| Injected Rank 1 | Sampling Time | 24.5 Cycles |
| Injected Rank 1 | Offset Number | None |
| Injected Conversion | External Trigger | HRTIM1 ADC Trigger 1 |
| Injected Conversion | Trigger Edge | Rising |
| Injected Conversion | Injected Queue | Disable |
| Injected Conversion | Queue Injected Context | Disable |
| Injected Conversion | Automatic Injected Conversion | Disable |
| Injected Conversion | Injected Discontinuous Mode | Disable |

### 6.3 ADC2注入组：VAC

ADC2使用相同参数，唯一差异为：

| 字段 | 设置值 |
| --- | --- |
| Channel | ADC2_IN6 / PC0 |
| Rank | Injected Rank 1 |
| Sampling Time | 24.5 Cycles |

两路由同一个HRTIM1 ADC Trigger 1触发。初始采样时间24.5 cycles约为0.576 us；最终值应根据调理运放输出阻抗、RC滤波和实际建立误差调整。

### 6.4 ADC1规则组：VBUS

| CubeMX 分组 | 字段 | 设置值 |
| --- | --- | --- |
| ADC Regular Conversion Mode | Number of Conversions | 1 |
| Regular Rank 1 | Channel | ADC1_IN7 / PC1 |
| Regular Rank 1 | Single-ended/Differential | Single-ended |
| Regular Rank 1 | Sampling Time | 24.5 Cycles |
| Regular Conversion | External Trigger | Software Start |
| Regular Conversion | Continuous Conversion | Disable |
| DMA Settings | DMA | 不添加 |

“每1 ms启动一次VBUS转换”是主循环调度策略，不是CubeMX字段。规则转换不得在10 kHz快速回调内阻塞等待。

### 6.5 ADC1_2 NVIC与控制入口

ADC1/ADC2共享`ADC1_2_IRQn`。在NVIC中启用`ADC1 and ADC2 global interrupt`，抢占优先级1。

CubeMX只能配置公共NVIC，无法替软件决定哪个JEOC回调运行控制。生成代码后应只选择一次注入完成事件作为控制入口，并确认本周期ADC1和ADC2样本均已更新，避免一个PWM周期运行两次PR。

快速控制回调只允许：采样读取、物理量换算、范围检查、参考生成、PR计算、调制限幅、HRTIM预装载比较值更新和心跳计数。

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

## 9. NVIC汇总

| CubeMX中断 | 抢占优先级 | 子优先级 | 用途 |
| --- | ---: | ---: | --- |
| HRTIM1 fault global interrupt | 0 | 0 | 记录FLT3并拉低PE0 |
| ADC1 and ADC2 global interrupt | 1 | 0 | 10 kHz快速控制入口 |
| EXTI line[9:5] interrupt | 2 | 0 | PE5诊断故障 |
| SysTick | 15 | 0 | 只更新时间标志 |

Fault的PWM关断由HRTIM硬件完成，不依赖`HRTIM1_FLT_IRQn`是否及时响应。ISR只负责软件收尾和记录。

首版不配置普通TIM。1 ms、10 ms和100 ms任务由SysTick产生标志，实际任务在主循环执行。

## 10. 配置与生成代码核对

| CubeMX设置 | 生成代码应出现 |
| --- | --- |
| Master MUL2/Period 34000 | Master `HAL_HRTIM_TimeBaseConfig()` |
| Timer A/B MUL4/Period 34000 | A/B各自的`HAL_HRTIM_TimeBaseConfig()` |
| A/B Up-Down | `HAL_HRTIM_WaveformTimerControl()` + `HRTIM_TIMERUPDOWNMODE_UPDOWN` |
| Valley ROM | `HAL_HRTIM_RollOverModeConfig()`含`OUTROM_VALLEY`和`ROM_VALLEY` |
| Compare 1 | A/B各自`HAL_HRTIM_WaveformCompareConfig()` |
| 500 ns死区 | `Prescaler DIV1`，Rising/Falling Value 85 |
| 四路输出 | 四次`HAL_HRTIM_WaveformOutputConfig()` |
| ADC Trigger 1 | `HAL_HRTIM_ADCTriggerConfig()`，Master CMP2 |
| Fault Line 3 | `HAL_HRTIM_FaultConfig()`和`HAL_HRTIM_FaultModeCtl()` |
| ADC1/2注入组 | 两次`HAL_ADCEx_InjectedConfigChannel()` |
| ADC12异步/4 | `ADC_CLOCK_ASYNC_DIV4` |

若CubeMX生成结果与表中不一致，先回到`.ioc`修正，避免直接编辑生成区掩盖配置错误。

## 11. 配置顺序

1. 固定MCU、G4 V1.6.2和MDK-ARM；
2. 核对8 MHz HSE和170 MHz时钟树；
3. 配置PE0、PE5和PB10；
4. 配置HRTIM Master、Timer A/B、Compare、Output、ROM和Dead Time；
5. 配置Fault Line 3及HRTIM Fault中断；
6. 配置Master CMP2和ADC Trigger 1；
7. 配置ADC1/ADC2注入组、ADC1规则组和ADC1_2 NVIC；
8. 配置USART2；
9. 配置IWDG和调试冻结；
10. 检查Pinout冲突、Clock Configuration和NVIC；
11. 生成代码并按第10节逐项核对；
12. 完成无功率测试后才进入低压功率测试。

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
- [ ] ADC1 PA1和ADC2 PC0均为注入Rank 1、24.5 cycles、HRTIM触发。
- [ ] ADC1 PC1为规则Rank 1、Software Start、无DMA。
- [ ] USART2为PD5/PD6、460800、8N1、无DMA和NVIC。
- [ ] COMP、ADC3/4/5、HRTIM C/D/E/F和普通TIM保持关闭。
- [ ] Pinout无黄色冲突。

### 12.2 无功率上板检查

1. 复位和初始化期间PE0始终为低；
2. PWM实测为10 kHz，Master和A/B物理周期均为100 us；
3. A/B桥臂互补，死区初值约500 ns，无上下管重叠；
4. 25%、50%、75%测试值的实测占空方向与比较值公式一致；
5. ADC Trigger和快速回调均为10 kHz，每周期只执行一次控制；
6. ADC采样位置远离实际开关沿；
7. PB10拉低时不依赖CPU立即关闭四路PWM；
8. PB10恢复后PWM不自动恢复；
9. 停止控制心跳后IWDG在实测LSI误差范围内复位；
10. IWDG复位后PE0和PWM仍保持关闭。

## 13. 首版边界

- PWM和控制频率均为10 kHz，每周期运行一次PR。
- ADC1/ADC2使用独立注入组，不启用ADC双重模式。
- VBUS使用ADC1规则组软件启动，不启用TIM6或DMA。
- USART2使用阻塞发送，不启用UART DMA或中断。
- COMP不作为首版启动依赖，但DESAT/OCP和HRTIM Fault 3不可省略。
- 500 ns死区、24.5 cycles采样时间和Master CMP2采样位置都必须通过实际硬件测量修正。
