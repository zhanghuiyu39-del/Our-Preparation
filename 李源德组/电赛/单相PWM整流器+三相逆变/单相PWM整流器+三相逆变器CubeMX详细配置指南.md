# STM32G474 单相 PWM 整流器 + 三相逆变器 CubeMX 详细配置指南

## 1. 文档定位

本文以当前 `01仅INV/02仅INV初步开环/02OpenLoop.ioc` 为配置事实来源，用于逐步完成“单相全桥 PWM 整流器 + 三相三桥臂逆变器”的 CubeMX 外设配置。最终负载接口固定为三相三线，不使用外接中性线和第四逆变桥臂。本文只说明 CubeMX 配置、生成代码核对和上板验证，不修改现有 `.ioc`，也不包含 PI、PR/QPR、PLL、DDS 或 SVPWM 的具体代码。

固定环境如下：

| 项目 | 固定值 |
| --- | --- |
| MCU | STM32G474VCT6，LQFP100 |
| 兼容器件 | STM32G474VET6，同封装；固件仍按 VC 的 256 KB Flash 限制构建 |
| STM32CubeMX | 6.17.0 |
| 固件包 | STM32Cube FW_G4 V1.6.2 |
| 外部晶振 | 8 MHz HSE，PF0/PF1 |
| SYSCLK/HCLK/HRTIM1 | 170 MHz |
| ADC 内核输入 | 170 MHz，ADC 内部异步 `/4`，实际 42.5 MHz |
| PWM | 中心对齐 10 kHz |
| 软件结构 | 裸机、规则组 ADC + 循环 DMA、主循环低速任务 |
| COMP/DAC | 默认关闭 |

> 安全边界：PB10/HRTIM1_FLT3 必须连接具有独立 DESAT/OCP 或等效短路保护能力的驱动器故障链。ADC 软件过流只能在采样和软件响应延迟后关断，不能替代驱动器短路保护。

### 1.1 赛题要求1~5对应目标

本项目当前优先完成赛题要求1~5，CubeMX资源选择必须服务于下表，而不是为三相四线等无关拓扑增加复杂度。

| 项目 | 赛题目标 | 对外设配置的含义 |
| --- | --- | --- |
| 输入 | 单相交流，`Ui=36 V RMS`、`fi=50 Hz` | ADC1/2采集IPFC、VBUS和VAC，HRTIM A/B执行PWM整流 |
| 基本输出 | 三相对称三线制，线电压`Uo=32 V RMS ±0.1 V`、`fo=60 Hz ±0.2 Hz`、线电流`Io=2.0 A` | HRTIM C/D/E三桥臂；测量负载侧线电压和三相线电流 |
| 输入功率因数 | `cos(phi_i) >= 0.98` | 需要VAC同步、PFC电流环和母线电压环；不是CubeMX单独可以保证的指标 |
| 整机效率 | `eta >= 95%` | 10 kHz先作为调试基线，最终结合开关损耗、磁性器件和辅助电源功耗实测 |
| 输出THD | 线电压THD不大于`2%` | 需要负载侧LC滤波、线电压反馈、死区优化和最终开关频率验证 |
| 变频输出 | `Uo=32 V RMS ±0.1 V`、`fo=30 Hz ±0.2 Hz`、`Io=2.0 A` | 30/60 Hz由软件DDS和控制器参数切换，HRTIM载波与ADC触发仍保持10 kHz |

额定三相输出有功功率约为：

```text
Pout = sqrt(3) x 32 V x 2 A = 110.9 W
```

按整机效率`95%`和输入功率因数`0.98`估算，36 V输入侧额定电流约为：

```text
Iin = 110.9 W / (36 V x 0.95 x 0.98) = 3.3 A RMS
```

因此IPFC传感器、运放和ADC标定量程至少应覆盖`±5 A`，并根据PFC电感纹波、启动冲击和软件过流阈值进一步留量。2 A是三相输出线电流，不是PFC输入电流量程。

题目中的`Uo`是输出线电压，不是某一相相对中性点的电压。控制算法内部可使用虚拟相电压：

```text
Vphase_rms = 32 / sqrt(3) = 18.48 V
Vphase_peak = 18.48 x sqrt(2) = 26.13 V
```

按当前三桥臂CBSVPWM和桥臂调制度限幅`0.90`估算：

```text
Vdc_min = sqrt(3) x Vphase_peak / 0.90
         = sqrt(3) x 26.13 / 0.90
         = 50.3 V
```

考虑母线纹波、器件压降、死区和闭环余量，初始母线目标建议按`52~55 V`设计，最终值由功率器件、PFC升压能力、效率和波形实测共同确定。

## 2. 分阶段配置档

不要一次启用全部资源。每完成一个配置档，先生成代码并通过该阶段验收，再进入下一阶段。

| 配置档 | 启用内容 | 保持关闭 |
| --- | --- | --- |
| P0-PFC：单相整流器调试 | HRTIM Master/A/B、ADC1/2、DMA1_CH1/2、PB10、PE0/PE5、USART2、IWDG | HRTIM C~F、ADC3~5、PE1/PE6、COMP/DAC |
| P0-INV：当前逆变器调试 | HRTIM Master/C/D/E、ADC3/4/5、DMA2_CH1/2/3、PB10、PE1/PE6、USART2、软件I2C OLED、IWDG | HRTIM A/B/F输出、ADC1/2 DMA、PE0、COMP/DAC |
| P1：整流器 + 三相逆变器 | HRTIM Master/A~E、ADC1~5、五路DMA、PE0~PE3、PE5/PE6、PB10 | HRTIM F、COMP/DAC |
| P2：可选制动桥臂 | P1全部资源，再增加HRTIM F作为制动桥臂 | 中性桥臂功能、COMP/DAC仍默认关闭 |

当前`02OpenLoop.ioc`已经配置P1所需的大部分外设，但当前应用代码只运行P0-INV。配置档描述的是软件实际启动的资源，不表示每次都要删除IOC中暂未启动的外设。P1是赛题要求1~5的联合运行基线；P2只作为直流母线制动扩展，不用于三相四线或中性线控制。

## 3. 现有 IOC 审计

### 3.1 已确认的现有配置

`02OpenLoop.ioc`已经包含：

- STM32G474VCT6、LQFP100、CubeMX 6.17.0；
- 8 MHz HSE，PLL `M=2`、`N=85`、`R=2`，SYSCLK=170 MHz；
- HRTIM Master：MUL2、Period=34000、CMP2=17000；
- HRTIM Timer A/B：MUL4、Up-Down、Period=34000、CMP1=17000；
- HRTIM ADC Trigger 1：Master Compare 2，Postscaler=0；
- PB10/HRTIM1_FLT3：数字输入、低有效、无数字滤波；
- ADC1规则组：PA1/IPFC、PC1/VBUS，DMA1 Channel 1；
- ADC2规则组：PC0/VAC，DMA1 Channel 2；
- USART2：PD5/PD6，460800 baud；
- PA15/PB7软件I2C OLED；
- PE0/PFC_GATE_EN、PE5/PFC_NFAULT_DIAG；
- HRTIM Timer C/D/E、ADC Trigger 2、ADC3/4/5及DMA2 Channel 1/2/3；
- PE1/INV_GATE_EN、PE6/INV_NFAULT_DIAG以及PE2/PE3控制输出；
- HSE CSS已经启用，IOC记录`RCC.EnbaleCSS=true`；
- ADC1/2共享错误中断、ADC3/4/5错误中断及五路DMA完成中断；
- IWDG Prescaler=64、Reload=249，用户代码在外设和安全状态建立后启动。

生成的 `hrtim.c` 已确认包含：

```c
pFaultCfg.Source   = HRTIM_FAULTSOURCE_DIGITALINPUT;
pFaultCfg.Polarity = HRTIM_FAULTPOLARITY_LOW;
pFaultCfg.Filter   = HRTIM_FAULTFILTER_NONE;
```

因此当前 PB10 已经是外部数字 Fault 路径，不需要改成内部 Fault。

### 3.2 当前IOC结论和小幅优化项

当前IOC已经记录`STM32Cube FW_G4 V1.6.2`、`LastFirmware=false`和工程名`02OpenLoop`，这三项不需要修改。为完成要求1~5，也不需要增加第四桥臂、COMP、DAC、硬件I2C或UART DMA。

继续使用当前IOC前应完成以下核对；只有实测不满足时才回到CubeMX小幅调整：

1. 用示波器验证A~E在`Set Source=CMP1`、`Reset Source=CMP1`、Up-Down和Valley ROM组合下形成正确的10 kHz中心对齐互补PWM；
2. 验证Master CMP2对应的Trigger 1/2处在实际功率板的低噪声采样窗口；若采样受开关沿污染，只移动对应Trigger来源或Compare位置，不先改变整个HRTIM时基；
3. 保持PB10为数字Fault 3、低有效、Filter None和Blanking None；数字滤波不能替代驱动器DESAT/OCP的快速保护；
4. 保持当前CSS配置，并在NMI中实现PE0/PE1拉低和全部HRTIM输出关闭，不能只依赖默认死循环；
5. 500 ns死区只作为低压起点。为满足THD和效率，最终应根据驱动传播延迟和上下管波形逐步减小到可靠范围，必要时再增加软件死区补偿；
6. 10 kHz先保持不变。只有LC滤波和闭环调试后仍无法满足THD或动态性能，才评估20 kHz，并同步重算HRTIM Period、ADC触发、控制器系数和实时预算；
7. 重新生成代码后比较`hrtim.c`、`adc.c`、`stm32g4xx_hal_msp.c`和`dma.c`，确认CubeMX没有覆盖用户启动顺序或安全关断代码。

## 4. 最终引脚分配

### 4.1 HRTIM桥臂

| 逻辑桥臂 | 主输出/高侧 | 互补输出/低侧 | 默认用途 | 配置档 |
| --- | --- | --- | --- | --- |
| POWER_LEG_A | PA8 / HRTIM1_CHA1 | PA9 / HRTIM1_CHA2 | PFC桥臂1 | P0-PFC/P1 |
| POWER_LEG_B | PA10 / HRTIM1_CHB1 | PA11 / HRTIM1_CHB2 | PFC桥臂2 | P0-PFC/P1 |
| POWER_LEG_C | PB12 / HRTIM1_CHC1 | PB13 / HRTIM1_CHC2 | 逆变U相 | P0-INV/P1 |
| POWER_LEG_D | PB14 / HRTIM1_CHD1 | PB15 / HRTIM1_CHD2 | 逆变V相 | P0-INV/P1 |
| POWER_LEG_E | PC8 / HRTIM1_CHE1 | PC9 / HRTIM1_CHE2 | 逆变W相 | P0-INV/P1 |
| POWER_LEG_F | PC6 / HRTIM1_CHF1 | PC7 / HRTIM1_CHF2 | 可选制动桥臂，赛题要求1~5不启用 | P2 |

### 4.2 ADC反馈

| 网络名 | 引脚 | ADC通道 | Rank | 物理量 | 配置档 |
| --- | --- | --- | ---: | --- | --- |
| IPFC_ADC | PA1 | ADC1_IN2 | ADC1 Rank 1 | PFC输入电流 | P0-PFC/P1 |
| VBUS_A_ADC | PC1 | ADC1_IN7 | ADC1 Rank 2 | 直流母线电压 | P0-PFC/P1 |
| VAC_ADC | PC0 | ADC2_IN6 | ADC2 Rank 1 | 单相交流输入电压 | P0-PFC/P1 |
| IU_ADC | PB0 | ADC3_IN12 | ADC3 Rank 1 | 逆变U相线电流 | P0-INV/P1 |
| VUV_ADC | PD10 | ADC3_IN7 | ADC3 Rank 2 | 负载侧U-V线电压 | P0-INV/P1 |
| IV_ADC | PD8 | ADC4_IN12 | ADC4 Rank 1 | 逆变V相线电流 | P0-INV/P1 |
| VVW_ADC | PE14 | ADC4_IN1 | ADC4 Rank 2 | 负载侧V-W线电压 | P0-INV/P1 |
| IW_ADC | PD9 | ADC5_IN13 | ADC5 Rank 1 | 逆变W相线电流 | P0-INV/P1 |
| VWU_ADC | PD14 | ADC5_IN11 | ADC5 Rank 2 | 负载侧W-U线电压，兼作冗余检查 | P0-INV/P1 |

三路线电压必须在LC滤波器之后、负载接线端之前取样。`32 V RMS`线电压峰值约为`45.3 V`，模拟前端建议至少覆盖`±60 V`差分输入并保留过冲余量。`PD10/PE14/PD14`接收的是隔离放大器或满足输入共模范围的差分运放调理后的`0~3.3 V`单端信号，不得把任意功率线直接连接STM32 ADC。若前端采用约`1.65 V`中点偏置，则上电零输入标定可以扣除中点；若采用单极性或其他参考方式，软件不得沿用双极性中点标定公式。

VBUS分压器同样必须按母线过压而不是额定值设计。母线目标为`52~55 V`时，建议ADC满量程对应至少`65~70 V`，并配置合适的输入RC、钳位和耐压电阻串联分压。具体比例必须根据实际参考电压和器件误差标定。

三相三线中只有两路线电压是独立量，理论上满足：

```text
VUV + VVW + VWU = 0
```

第三路保留用于冗余、极性和传感器故障检查。控制器可选择两路独立线电压完成Clarke变换，但保护与诊断应继续检查三路一致性。

### 4.3 控制、保护和调试

| 功能 | 引脚 | CubeMX模式 | 初始状态 |
| --- | --- | --- | --- |
| PFC_GATE_EN | PE0 | GPIO Output Push-Pull | Low，必须板级下拉 |
| INV_GATE_EN | PE1 | GPIO Output Push-Pull | Low，必须板级下拉 |
| PRECHARGE_EN | PE2 | GPIO Output Push-Pull | Low |
| DISCHARGE_EN/BRAKE_EN | PE3 | GPIO Output Push-Pull | Low |
| PFC_NFAULT_DIAG | PE5 | GPIO EXTI Falling Edge | 外部上拉，低有效 |
| INV_NFAULT_DIAG | PE6 | GPIO EXTI Falling Edge | 外部上拉，低有效 |
| DRV_NFAULT_ANY | PB10 | HRTIM1_FLT3 | 外部上拉，低有效 |
| VOFA_TX/RX | PD5/PD6 | USART2_TX/RX | 460800、8N1 |
| OLED_SCL | PA15 | GPIO Output Push-Pull | 软件I2C |
| OLED_SDA | PB7 | GPIO Output Open-Drain | 软件I2C，外部上拉 |
| SWDIO/SWCLK | PA13/PA14 | Serial Wire | 始终保留 |
| HSE | PF0/PF1 | RCC OSC_IN/OSC_OUT | 8 MHz晶振 |

## 5. RCC、SYS和工程设置

### 5.1 时钟树

| CubeMX位置 | 字段 | 设置值 | 生成代码核对 |
| --- | --- | --- | --- |
| System Core > RCC | High Speed Clock | Crystal/Ceramic Resonator | PF0/PF1为OSC_IN/OUT |
| System Core > RCC | Low Speed Clock | LSI | IWDG时钟源 |
| Clock Configuration | PLL Source | HSE | `RCC_PLLSOURCE_HSE` |
| Clock Configuration | PLLM | /2 | 8 MHz / 2 = 4 MHz |
| Clock Configuration | PLLN | x85 | VCO=340 MHz |
| Clock Configuration | PLLR | /2 | SYSCLK=170 MHz |
| Clock Configuration | AHB/APB1/APB2 Prescaler | /1 | HCLK和PCLK均170 MHz |
| Clock Configuration | HRTIM1 Kernel Clock | 170 MHz | `RCC.HRTIM1Freq_Value=170000000` |
| Clock Configuration | ADC12/ADC345 Kernel Clock | System Clock | ADC页面再进行异步/4 |

170 MHz下应在`SystemClock_Config()`看到：

```c
HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);
```

Flash等待周期以CubeMX为准，生成代码应使用适用于170 MHz的延迟配置。不要手工降低等待周期。

### 5.2 SYS和NVIC分组

| CubeMX位置 | 字段 | 设置值 |
| --- | --- | --- |
| System Core > SYS | Debug | Serial Wire |
| System Core > NVIC | Priority Group | Group 4 |
| System Core > RCC | CSS | Enable，当前IOC已启用 |

当前IOC已记录`RCC.EnbaleCSS=true`，生成的`SystemClock_Config()`应调用`HAL_RCC_EnableCSS()`。CSS触发NMI后必须先拉低PE0/PE1、关闭全部HRTIM输出、锁存时钟故障，再等待IWDG复位或停留在安全态；不能只使用CubeMX默认空循环。NMI软件收尾不替代Gate Enable板级下拉和HRTIM Fault硬件关断。

### 5.3 Project Manager

| 页面 | 字段 | 设置值 |
| --- | --- | --- |
| Project | Toolchain/IDE | MDK-ARM V5 |
| Project | Firmware Package | STM32Cube FW_G4 V1.6.2 |
| Code Generator | Keep User Code | Enable |
| Code Generator | Delete previously generated files | 谨慎；首次迁移前先备份 |
| Code Generator | Generate peripheral initialization as .c/.h pair | Enable，延续现有结构 |
| Linker | Flash目标 | 按STM32G474VC的256 KB限制 |

## 6. HRTIM1详细配置

### 6.1 频率计算

Master向上计数：

```text
fMaster = 170 MHz x 2 / 34000 = 10 kHz
```

Timer A~E以及可选Timer F上下计数：

```text
fPWM = 170 MHz x 4 / (2 x 34000) = 10 kHz
```

CubeMX中的`MUL2/MUL4`是HRTIM高分辨率时基倍频，不是普通TIM的除法预分频。

### 6.2 Master Timer

| CubeMX分组 | 字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- | --- |
| Time Base Setting | Prescaler Ratio | HRTIM Clock Multiplied by 2 | `HRTIM_PRESCALERRATIO_MUL2` |
| Time Base Setting | Period | 34000 | Master `Period=34000` |
| Time Base Setting | Repetition Counter | 0 | 每周期均可更新 |
| Time Base Setting | Mode | Continuous | `HRTIM_MODE_CONTINUOUS` |
| Compare Unit 2 | Compare Value | 17000 | Master CMP2 |

Master不输出PWM，只负责建立100 us公共周期和ADC触发参考。

### 6.3 Timer A~E公共时基

Timer A、B、C、D、E逐个使用下表。P2启用F时，Timer F也使用同一组值。

| CubeMX分组 | 字段 | 设置值 | HAL枚举/结果 |
| --- | --- | --- | --- |
| Time Base Setting | Prescaler Ratio | HRTIM Clock Multiplied by 4 | `HRTIM_PRESCALERRATIO_MUL4` |
| Time Base Setting | Period | 34000 | 10 kHz中心对齐 |
| Time Base Setting | Repetition Counter | 0 | 每完整PWM周期更新 |
| Waveform Timer Control | Up-Down Mode | Enable | `HRTIM_TIMERUPDOWNMODE_UPDOWN` |
| Waveform Timer Control | Preload Enable | Enable | `HRTIM_PRELOAD_ENABLED` |
| Waveform Timer Control | Repetition Update | Enable | `HRTIM_UPDATEONREPETITION_ENABLED` |
| Waveform Timer Control | Push-Pull Mode | Disable | 互补输出由Dead Time逻辑产生 |
| Timer Configuration | Dead Time Insertion | Enable | `HRTIM_TIMDEADTIMEINSERTION_ENABLED` |
| Timer Configuration | Fault Enable | Fault 3 | A~E全部响应PB10 |
| Timer Configuration | Fault Lock | Read/Write | 调试期不锁死配置 |
| Timer Configuration | DMA Requests | None | 控制更新由ADC DMA回调和预装载完成 |
| Timer Configuration | Update Gating | Independent | 与现有设计一致 |

所有计数器必须用一个掩码同时启动，保持A~E相位对齐。不要依次调用五次Counter Start形成软件启动偏差。

### 6.4 Compare和输出

每个Timer的Compare Unit 1初值均为17000。该值只用于无功率50%波形检查，实际运行时由调制算法写入预装载比较寄存器。

| 输出 | Set Source | Reset Source | Polarity | Idle Level | Fault Level |
| --- | --- | --- | --- | --- | --- |
| TA1/TB1/TC1/TD1/TE1 | Timer Compare 1 | Timer Compare 1 | Active High | Inactive | Inactive |
| TA2/TB2/TC2/TD2/TE2 | None | None | Active High | Inactive | Inactive |

Output 2不单独配置Set/Reset事件，由Dead Time模块根据Output 1生成互补波形。生成代码应表现为主输出使用CMP1、互补输出使用NONE。

> 该Set/Reset组合依赖Up-Down模式和正确的Output Roll-over设置。扩展C/D/E前，必须先用A/B实测确认：主输出能调占空比、互补输出极性正确、CMP接近边界时不产生窄脉冲。若实测不成立，停止复制配置并重新核对CubeMX生成的`HAL_HRTIM_WaveformOutputConfig()`。

### 6.5 Roll-over统一值

在Timer A~E的`Roll-over Mode Configuration`中统一为：

| 字段 | 设置值 | 目的 |
| --- | --- | --- |
| Fault Event Roll-over Mode | Both | 保持当前IOC故障事件边界 |
| Burst Mode Roll-over Mode | Both | 保持当前IOC突发模式边界 |
| ADC Roll-over Mode | Both | 保持当前IOC ADC事件边界 |
| Output Roll-over Mode | Valley | 与中心对齐输出路径一致 |
| General Roll-over Mode | Valley | 在Valley更新预装载和重复计数 |

生成后逐个检查`HAL_HRTIM_RollOverModeConfig()`，A~E必须一致，不再保留当前A/B的Both/Valley差异。

### 6.6 Dead Time

| CubeMX字段 | 设置值 | 换算 |
| --- | --- | --- |
| Prescaler | fHRTIM / 1 | 计数频率170 MHz |
| Rising Value | 85 | 85 / 170 MHz = 500 ns |
| Falling Value | 85 | 85 / 170 MHz = 500 ns |
| Rising/Falling Sign | Positive | 插入正死区 |
| Rising/Falling Lock | Write | 调试期保持可写 |

500 ns仅是低压调试起点。最终死区必须根据驱动器传播延迟、功率器件关断时间、米勒平台和示波器上下管波形确定。

生成代码后，A~E必须分别出现`HAL_HRTIM_TimeBaseConfig()`、`HAL_HRTIM_WaveformTimerControl()`、`HAL_HRTIM_WaveformTimerConfig()`、`HAL_HRTIM_WaveformCompareConfig()`、`HAL_HRTIM_WaveformOutputConfig()`和`HAL_HRTIM_DeadTimeConfig()`。仅看到引脚有波形不能证明Set/Reset、ROM、Fault和预装载边界全部正确。

HRTIM计数器启动前必须完成DLL校准并等待就绪。DLL就绪只代表高分辨率时基可用，不代表PWM引脚已经开放，更不代表PE0/PE1已经使能；这三个状态必须由软件分别管理。

### 6.7 ADC Trigger 1和Trigger 2

| Trigger | Update Source | Active Source | Postscaler | 服务对象 |
| --- | --- | --- | ---: | --- |
| ADC Trigger 1 | Master Timer | Master Compare 2 | 0 | ADC1、ADC2 |
| ADC Trigger 2 | Master Timer | Master Compare 2 | 0 | ADC3、ADC4、ADC5 |

CubeMX字段应设置为：

| CubeMX分组 | 字段 | 设置值 |
| --- | --- | --- |
| ADC Triggers Configuration | Enable ADC Trigger 1 | Enable |
| ADC Trigger 1 | Number of Active Trigger Sources | 1 |
| ADC Trigger 1 | Trigger Source | Master Compare 2 |
| ADC Trigger 1 | Update Source | Master Timer |
| ADC Trigger 1 | Postscaler | 0 |
| ADC Triggers Configuration | Enable ADC Trigger 2 | Enable |
| ADC Trigger 2 | Number of Active Trigger Sources | 1 |
| ADC Trigger 2 | Trigger Source | Master Compare 2 |
| ADC Trigger 2 | Update Source | Master Timer |
| ADC Trigger 2 | Postscaler | 0 |

初版两个Trigger发生在同一时刻，但保留独立Trigger编号。后续若逆变器开关噪声要求不同采样位置，只修改Trigger 2的来源，不影响PFC的Trigger 1。

在STM32G474的HAL触发合法性检查中，`ADC_EXTERNALTRIG_HRTIM_TRG2`属于ADC3/4/5可选触发，不用于ADC1/2。因此ADC1/2继续选择Trigger 1，不要为了界面统一改成Trigger 2。

### 6.8 Fault 3

| CubeMX位置 | 字段 | 设置值 | 生成代码核对 |
| --- | --- | --- | --- |
| Pinout | PB10 | HRTIM1_FLT3 | 不再作为普通EXTI |
| Fault Configuration | Fault Line 3 Source | Digital Input | `HRTIM_FAULTSOURCE_DIGITALINPUT` |
| Fault Configuration | Polarity | Active Low | `HRTIM_FAULTPOLARITY_LOW` |
| Fault Configuration | Filter | None | 首次验证不添加延迟 |
| Fault Configuration | Lock | Read/Write | 调试期可修改 |
| Fault Configuration | Blanking Source | None | 禁止把真实DESAT/OCP屏蔽掉 |
| Fault Configuration | Enable Fault Line 3 | Enable | `HAL_HRTIM_FaultModeCtl()` |
| Timer A~E | Fault Enable | Fault 3 | 所有已启用桥臂同时关闭 |
| Output A1~E2 | Fault Level | Inactive | 故障输出无效态 |

Fault关断由HRTIM硬件完成，不依赖中断。`HRTIM1_FLT_IRQn`只负责拉低PE0/PE1、记录故障并进入软件故障锁存。

### 6.9 HRTIM F制动扩展

赛题负载为三相三线，P2只允许把HRTIM F用作直流母线制动桥臂，不配置中性线控制。需要制动桥臂时：

1. 将PC6/PC7设置为HRTIM1_CHF1/CHF2；
2. Timer F复制C/D/E的时基、Compare、Dead Time、Roll-over和Fault 3配置；
3. 输出初值保持Inactive；
4. 只有直流母线制动状态机明确选择`BRAKE_LEG`后才启动Timer F和TF1/TF2；
5. 未启用P2时，不要在P1的Counter/Output Start掩码中包含Timer F和TF1/TF2。

## 7. ADC1~5规则组配置

### 7.1 ADC公共参数

ADC1~ADC5均使用规则组，不使用注入组或ADC双重模式。

| CubeMX位置 | 字段 | 设置值 | HAL结果 |
| --- | --- | --- | --- |
| Parameter Settings | Clock Prescaler | Asynchronous clock divided by 4 | `ADC_CLOCK_ASYNC_DIV4` |
| Parameter Settings | Resolution | 12-bit | `ADC_RESOLUTION_12B` |
| Parameter Settings | Data Alignment | Right | `ADC_DATAALIGN_RIGHT` |
| Parameter Settings | Gain Compensation | 0 | 不使用数字增益 |
| Parameter Settings | EOC Selection | End of sequence | `ADC_EOC_SEQ_CONV` |
| Parameter Settings | Low Power Auto Wait | Disable | 不阻塞外部触发 |
| Parameter Settings | Continuous Conversion | Disable | 每个HRTIM事件启动一次序列 |
| Parameter Settings | Discontinuous Mode | Disable | 完整执行规则序列 |
| Parameter Settings | DMA Continuous Requests | Enable | 连续发DMA请求 |
| Parameter Settings | Conversion Data Management | DMA Circular | `ADC_CONVERSIONDATA_DMA_CIRCULAR` |
| Parameter Settings | Overrun | Data Overwritten | `ADC_OVR_DATA_OVERWRITTEN` |
| Parameter Settings | Oversampling | Disable | 首版不增加延迟 |
| Regular Conversion | External Trigger Edge | Rising Edge | `ADC_EXTERNALTRIGCONVEDGE_RISING` |

ADC1/2选择`HRTIM1 ADC Trigger 1`上升沿；ADC3/4/5选择`HRTIM1 ADC Trigger 2`上升沿。

### 7.2 各ADC规则序列

| ADC | Scan | 转换数 | Rank 1 | Rank 2 | 采样时间 |
| --- | --- | ---: | --- | --- | --- |
| ADC1 | Enable | 2 | ADC1_IN2 / PA1 / IPFC | ADC1_IN7 / PC1 / VBUS | 两个均24.5 cycles |
| ADC2 | Disable | 1 | ADC2_IN6 / PC0 / VAC | 无 | 24.5 cycles |
| ADC3 | Enable | 2 | ADC3_IN12 / PB0 / IU | ADC3_IN7 / PD10 / VUV | 两个均24.5 cycles |
| ADC4 | Enable | 2 | ADC4_IN12 / PD8 / IV | ADC4_IN1 / PE14 / VVW | 两个均24.5 cycles |
| ADC5 | Enable | 2 | ADC5_IN13 / PD9 / IW | ADC5_IN11 / PD14 / VWU | 两个均24.5 cycles |

所有通道设置为Single-ended、Offset None。Rank顺序同时定义DMA缓冲区顺序，后续不得在只改CubeMX后继续沿用旧数组下标。

ADC3、ADC4、ADC5的Rank 1会由三个独立ADC核近同步采集IU/IV/IW；各ADC的Rank 2必须等待本核Rank 1转换完成，因此VUV/VVW/VWU分别比对应相电流晚约一次转换时间。本文不把六个逆变量描述为严格同时采样，最终必须确认该时差满足电压闭环和保护算法要求。

推荐缓冲区映射：

```c
uint16_t adc1_dma[2]; /* [0]=IPFC, [1]=VBUS */
uint16_t adc2_dma[1]; /* [0]=VAC */
uint16_t adc3_dma[2]; /* [0]=IU,   [1]=VUV */
uint16_t adc4_dma[2]; /* [0]=IV,   [1]=VVW */
uint16_t adc5_dma[2]; /* [0]=IW,   [1]=VWU */
```

当前IOC的ADC通道、Rank和DMA分配无需改变；`VU/VV/VW`改为`VUV/VVW/VWU`首先是PCB网络含义和软件数据结构含义的修正。CubeMX用户标签可以在下一次正常维护IOC时同步改名，但本指南修改不要求立即重新生成工程。

### 7.3 转换时间检查

42.5 MHz ADC时钟下，24.5 cycles采样时间约为：

```text
24.5 / 42.5 MHz = 0.576 us
```

加上约12.5 cycles的12位转换阶段，单通道约0.87 us，两通道序列约1.74 us，远小于100 us PWM周期。该余量不代表可以忽略模拟前端建立时间；最终采样时间要根据运放输出阻抗和输入RC实测调整。

### 7.4 ADC校准

每次上电在启动DMA和HRTIM计数器前，依次对ADC1~5执行单端校准。校准期间PE0/PE1必须Low，所有HRTIM输出关闭。

生成代码只负责ADC配置，校准和启动顺序属于用户代码。若任一路校准失败，禁止启动任何功率输出。

## 8. DMA和DMAMUX

### 8.1 通道分配

| ADC | DMA请求 | DMA实例 | 模式 | 配置档 |
| --- | --- | --- | --- | --- |
| ADC1 | ADC1 | DMA1 Channel 1 | Circular | P0-PFC/P1 |
| ADC2 | ADC2 | DMA1 Channel 2 | Circular | P0-PFC/P1 |
| ADC3 | ADC3 | DMA2 Channel 1 | Circular | P0-INV/P1 |
| ADC4 | ADC4 | DMA2 Channel 2 | Circular | P0-INV/P1 |
| ADC5 | ADC5 | DMA2 Channel 3 | Circular | P0-INV/P1 |

STM32G4使用DMAMUX映射请求。若CubeMX提示上述DMA2通道已冲突，可在DMA2内重新分配空闲通道，但必须同步修改本文通道记录和NVIC表，不能让两个ADC共用一个DMA通道。

### 8.2 每路DMA参数

| DMA字段 | 设置值 |
| --- | --- |
| Direction | Peripheral to Memory |
| Mode | Circular |
| Peripheral Increment | Disable |
| Memory Increment | Enable |
| Peripheral Data Width | Half Word |
| Memory Data Width | Half Word |
| Priority | High |
| DMAMUX Synchronization | Disable |
| Request Generator | Disable |

CubeMX/HAL启动循环DMA时通常会使能Half Transfer中断。长度为2的ADC缓冲区在Half Transfer时只有Rank 1有效，Rank 2仍可能属于上一周期，因此Half Transfer不能运行控制算法。五路DMA启动成功后关闭HT中断：

```c
__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc3.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc4.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc5.DMA_Handle, DMA_IT_HT);
```

### 8.3 数据同步边界

ADC Trigger相同不等于五路DMA回调严格按固定顺序发生。联合工程应建立两个独立的快速测量域，不能要求五个ADC全部完成后才运行任一控制器。

#### PFC测量域：ADC1 + ADC2

- ADC2只有VAC一个Rank，DMA完成通常早于ADC1的IPFC+VBUS两Rank；
- ADC2 DMA完成回调只递增`pfc_adc2_sequence`，不运行PR控制；
- ADC1 DMA完成回调是PFC唯一10 kHz控制入口；
- ADC1回调确认ADC2序列已前进后，一次性复制IPFC、VBUS和VAC快照，再运行PFC电流环；
- VBUS虽以10 kHz采集，母线PI仍建议每10次PFC快回调运行一次，即1 kHz；
- ADC1完成时ADC2序列未前进、任一路DMA停止或发生Overrun，立即锁存PFC采样故障。

#### 逆变测量域：ADC3 + ADC4 + ADC5

- 三个DMA回调分别发布单调递增序列；
- 只有ADC3/4/5本周期数据均到齐时才发布IU/IV/IW和VUV/VVW/VWU一致性快照；
- 每个逆变快照只运行一次三相控制和CBSVPWM，不能由三个DMA回调重复运行；
- 不要仅靠DMA/NVIC优先级推断三路完成顺序，必须使用序列号或等效帧标志。

PFC与逆变快照均来自同一个100 us HRTIM周期，但二者无需形成“五ADC全局屏障”。逆变器使用最近一次有效VBUS做调制前馈；母线越限、PFC失稳和跨模块停机由1 kHz状态机协调。这样某一控制器不会因为另一组DMA回调短暂延迟而无谓丢失本周期控制。

五路DMA全部保留Transfer Complete中断时，每个PWM周期会产生五次DMA中断，即10 kHz下合计50 k次/s。首轮Bring-up保留这些中断以验证各ADC序列；进入联合闭环前必须用DWT或调试GPIO测量两套快速路径的最坏执行时间和嵌套关系。任何优化都必须保留DMA错误检测、序列监督以及“每个控制器每周期最多运行一次”的约束。

## 9. GPIO和EXTI

### 9.1 安全输出

| 引脚 | Mode | Pull | Speed | User Label | 初始电平 |
| --- | --- | --- | --- | --- | --- |
| PE0 | Output Push-Pull | No Pull | Low | PFC_GATE_EN | Low |
| PE1 | Output Push-Pull | No Pull | Low | INV_GATE_EN | Low |
| PE2 | Output Push-Pull | No Pull | Low | PRECHARGE_EN | Low |
| PE3 | Output Push-Pull | No Pull | Low | DISCHARGE_EN | Low |

CubeMX生成的`MX_GPIO_Init()`必须先写Low，再把引脚切换为输出。PE0/PE1的板级下拉是复位阶段安全保证，不能用MCU内部初值代替。

### 9.2 故障诊断EXTI

| 引脚 | Mode | Pull | User Label | 用途 |
| --- | --- | --- | --- | --- |
| PE5 | External Interrupt Mode with Falling Edge | Pull-up或板级上拉 | PFC_NFAULT_DIAG | 记录PFC驱动故障来源 |
| PE6 | External Interrupt Mode with Falling Edge | Pull-up或板级上拉 | INV_NFAULT_DIAG | 记录逆变驱动故障来源 |

PE5/PE6共享`EXTI9_5_IRQn`。EXTI仅用于诊断和软件锁存，快速关断仍由PB10/HRTIM1_FLT3完成。

## 10. USART2和软件I2C OLED

### 10.1 USART2

| CubeMX位置 | 字段 | 设置值 |
| --- | --- | --- |
| Connectivity > USART2 | Mode | Asynchronous |
| Pinout | TX/RX | PD5/PD6 |
| Parameter Settings | Baud Rate | 460800 |
| Parameter Settings | Word Length | 8 Bits |
| Parameter Settings | Parity | None |
| Parameter Settings | Stop Bits | 1 |
| Parameter Settings | TX/RX Mode | Enable |
| Parameter Settings | Hardware Flow Control | None |
| Parameter Settings | Oversampling | 16 |
| DMA Settings | DMA Request | 不添加 |
| NVIC Settings | USART2 Global Interrupt | Disable |

VOFA阻塞发送只能放在主循环低速任务中。禁止在ADC DMA、HRTIM或Fault中断中调用`HAL_UART_Transmit()`。

### 10.2 软件I2C OLED

不启用CubeMX的I2C1外设。GPIO配置为：

| 引脚 | Mode | Pull | Speed | 初始值 |
| --- | --- | --- | --- | --- |
| PA15/OLED_SCL | Output Push-Pull | No Pull | High | Low |
| PB7/OLED_SDA | Output Open-Drain | No Pull | High | Low |

SDA必须有2.2~4.7 kOhm外部上拉；若OLED模块已经自带上拉，不重复并联过小阻值。软件I2C和OLED刷新只允许在主循环执行。

允许在全部功率输出关闭的上电初始化阶段使用一次：

```c
HAL_Delay(100);
OLED_Init();
```

运行后不要用`HAL_Delay()`形成OLED刷新周期，改用SysTick时间戳或后台标志。任何`HAL_Delay()`都不得进入10 kHz控制回调和Fault处理。

## 11. IWDG

| CubeMX位置 | 字段 | 设置值 |
| --- | --- | --- |
| System Core > IWDG | Prescaler | 64 |
| System Core > IWDG | Reload Counter | 249 |
| System Core > IWDG | Window | 4095 |

名义超时：

```text
T = (249 + 1) x 64 / 32000 = 0.5 s
```

LSI误差较大，必须在实板上测量实际复位时间。

在`Project Manager > Advanced Settings`中不要让`MX_IWDG_Init()`随普通外设过早执行。保持现有做法：完成GPIO安全态、ADC校准、DMA启动、HRTIM采样验证和状态初始化后，最后手动调用`MX_IWDG_Init()`。

每100 ms只有在以下条件全部满足时刷新：

- 已启用ADC的DMA序列持续更新；
- 快速控制心跳持续变化；
- HRTIM没有未处理Fault；
- 状态机没有超时；
- PE0/PE1实际状态与状态机一致；
- 母线和电流没有越限。

调试构建可在SYS页面启用IWDG Debug Freeze；若页面没有该字段，在用户代码调用`__HAL_DBGMCU_FREEZE_IWDG()`。量产构建不要依赖调试冻结。

复位后必须读取并记录`RCC_FLAG_IWDGRST`。发生IWDG复位时，PE0/PE1、预充、放电和全部PWM默认保持关闭，不允许沿用裸板试波程序的自动输出启动流程。只有人工启动命令、自检通过、Fault输入恢复、母线处于安全范围且两套采样心跳均正常后，才能重新开放功率级。

## 12. COMP和DAC

首版保持：

- COMP1~7全部关闭；
- DAC1/DAC3/DAC4内部阈值通道全部关闭；
- HRTIM内部Fault 1/2/4/5/6不启用；
- 不把COMP作为PE0/PE1允许启动的必要条件。

即使COMP关闭，以下链路不能删除：

```text
驱动器DESAT/OCP
    -> 开漏nFAULT汇总
    -> PB10/HRTIM1_FLT3
    -> HRTIM A~E（以及已启用的F）硬件无效态
    -> Fault ISR拉低PE0/PE1并记录故障
```

以后启用COMP时，应单独验证输入极性、DAC阈值、迟滞、消隐和COMP到HRTIM的内部路由，不能直接沿用未经验证的Fault编号。

## 13. NVIC优先级

使用`NVIC_PRIORITYGROUP_4`，全部位用于抢占优先级。

| 中断 | 抢占优先级 | 子优先级 | 说明 |
| --- | ---: | ---: | --- |
| HRTIM1 Fault Global | 0 | 0 | 软件收尾；硬件已先关PWM |
| ADC1_2 Global | 1 | 0 | 只处理ADC1/2 Overrun等错误 |
| ADC3 Global | 1 | 0 | 只处理ADC3错误 |
| ADC4 Global | 1 | 0 | 只处理ADC4错误 |
| ADC5 Global | 1 | 0 | 只处理ADC5错误 |
| DMA1 Channel 2 / ADC2 | 1 | 0 | VAC序列发布 |
| DMA2 Channel 1 / ADC3 | 1 | 0 | U相序列发布 |
| DMA2 Channel 2 / ADC4 | 1 | 0 | V相序列发布 |
| DMA2 Channel 3 / ADC5 | 1 | 0 | W相序列发布 |
| DMA1 Channel 1 / ADC1 | 2 | 0 | PFC测量快照和控制入口 |
| EXTI9_5 | 3 | 0 | PE5/PE6分区故障诊断 |
| SysTick | 15 | 0 | HAL时基和低速任务标志 |

`HAL_ADC_Start_DMA()`会使能ADC Overrun中断，即使ADC参数选择了`Data Overwritten`。因此必须在CubeMX中启用`ADC1_2_IRQn`、`ADC3_IRQn`、`ADC4_IRQn`和`ADC5_IRQn`，并确认对应IRQHandler调用`HAL_ADC_IRQHandler()`。这些ADC全局中断只用于进入`HAL_ADC_ErrorCallback()`处理Overrun等错误，不用于EOC控制；不要在ADC全局IRQ中再次运行控制算法。

联合控制不能假定ADC1回调发生时ADC3~5必然已完成，也不需要等待它们。ADC1回调只在ADC2的VAC序列已更新时运行PFC控制；逆变控制只等待ADC3/4/5三路快照。跨模块故障、VBUS有效性和Gate Enable许可由慢速状态机协调。

## 14. 生成代码后的启动顺序

CubeMX只生成初始化函数，不会自动建立安全启动状态机。用户代码固定按以下顺序执行：

1. `HAL_Init()`和`SystemClock_Config()`；
2. `MX_GPIO_Init()`，确认PE0/PE1/PE2/PE3均为Low；
3. `MX_DMA_Init()`；
4. `MX_HRTIM1_Init()`，保持所有输出关闭；
5. `MX_ADC1_Init()`至`MX_ADC5_Init()`；
6. `MX_USART2_UART_Init()`；
7. 在功率输出关闭时执行`HAL_Delay(100)`和`OLED_Init()`；
8. 依次校准ADC1~5；
9. 清零DMA缓冲区、样本序列号、控制心跳和故障状态；
10. 依次启动ADC2、ADC5、ADC4、ADC3，最后启动ADC1的循环DMA，使两个测量域的辅助ADC先进入等待触发状态；
11. 检查五个`HAL_ADC_Start_DMA()`均返回`HAL_OK`并关闭HT中断；
12. 确认PB10 Fault 3未激活；
13. 完成HRTIM DLL校准；
14. 用同一掩码启动Master和A~E计数器，但保持十路输出和PE0/PE1关闭；
15. 连续检查Trigger 1/2和五路DMA更新；
16. 最后调用`MX_IWDG_Init()`；
17. 状态机允许时，先启动所需HRTIM输出，再拉高对应Gate Enable。

P0-PFC只启动Master/A/B和ADC1/2，P0-INV只启动Master/C/D/E和ADC3~5。P1可以让Master和A~E计数器共同运行以维持采样时基，但功率输出必须按状态机分别开放：先完成预充和PFC母线建立，再允许C/D/E输出和PE1。P2未进入制动状态时始终不启动F。

联合带功率顺序固定为：

```text
安全GPIO和Fault自检
 -> 启动五路ADC DMA与HRTIM采样时基（PWM引脚仍关闭）
 -> 交流输入检测与预充
 -> 开放A/B和PE0，PFC建立52~55 V母线
 -> 母线稳定、PFC电流环正常、输入无故障
 -> 逆变参考从0软启动
 -> 开放C/D/E和PE1
 -> 60 Hz或30 Hz三相三线运行
```

任何PFC、母线、逆变驱动或采样故障均应关闭A~E输出及PE0/PE1；Master是否继续计数用于故障记录可以由状态机决定，但不得因此重新开放功率输出。

任何失败统一执行：

```text
PE0 = Low
PE1 = Low
关闭所有HRTIM输出
复位PI/PR/QPR和调制命令
锁存故障，等待人工确认
```

## 15. 生成代码核对表

| CubeMX配置 | 生成代码中应看到 |
| --- | --- |
| Master MUL2/34000 | Master的`HAL_HRTIM_TimeBaseConfig()` |
| Timer A~E MUL4/Up-Down/34000 | 五个子定时器的TimeBase和WaveformTimerControl配置 |
| A~E统一Roll-over | 五次一致的`HAL_HRTIM_RollOverModeConfig()` |
| A~E Compare 1 | 五次`HAL_HRTIM_WaveformCompareConfig()` |
| A~E HRTIM DMA Request | 各Timer均为`HRTIM_TIM_DMA_NONE` |
| 500 ns死区 | A~E的`HAL_HRTIM_DeadTimeConfig()`，DIV1和85/85 |
| 十路输出 | A1/A2至E1/E2的`HAL_HRTIM_WaveformOutputConfig()` |
| Fault 3 | Digital Input、Low、None Filter以及A~E Fault Enable |
| Trigger 1 | Master CMP2、Postscaler 0 |
| Trigger 2 | Master CMP2、Postscaler 0 |
| ADC1/3/4/5两Rank | 每个ADC各两次`HAL_ADC_ConfigChannel()` |
| ADC2单Rank | 一次`HAL_ADC_ConfigChannel()` |
| ADC1/2 Trigger 1 | `ADC_EXTERNALTRIG_HRTIM_TRG1`和Rising Edge |
| ADC3/4/5 Trigger 2 | `ADC_EXTERNALTRIG_HRTIM_TRG2`和Rising Edge |
| 五路DMA | 五个DMA句柄和五次`__HAL_LINKDMA()` |
| ADC Overrun中断 | `ADC1_2_IRQHandler()`、`ADC3_IRQHandler()`、`ADC4_IRQHandler()`、`ADC5_IRQHandler()`均调用`HAL_ADC_IRQHandler()` |
| 软件I2C | 只有PA15/PB7 GPIO，不生成`i2c.c` |
| USART2阻塞通信 | 无USART2 DMA，无USART2 NVIC |
| HSE CSS | `SystemClock_Config()`调用`HAL_RCC_EnableCSS()`，NMI用户代码进入安全关断 |
| IWDG最后启动 | 自动外设初始化区不提前调用，用户初始化末尾调用 |

## 16. CubeMX配置顺序

当前`02OpenLoop.ioc`已经包含P1外设，不应再次按“新增外设”流程重复配置。后续维护按以下顺序复核，可以减少CubeMX重新生成造成的回退：

1. 复制当前工程作为新的联合控制版本，保留仅INV和仅PFC验证工程；
2. 确认FW_G4仍为V1.6.2、`LastFirmware=false`、工程输出目录正确；
3. 保持170 MHz时钟树、CSS和NVIC Group 4不变；
4. 核对PA8~PA11、PB12~PB15、PC8/PC9仍为HRTIM A~E输出；
5. 核对A~E的Time Base、Compare、Output、ROM、Dead Time和Fault 3完全一致；
6. 核对Trigger 1服务ADC1/2，Trigger 2服务ADC3/4/5，二者仍由Master CMP2产生；
7. 核对ADC1~5的Rank、采样时间和五路循环DMA映射未变化；
8. 核对DMA和ADC错误中断优先级与第13节一致；
9. 核对PE0~PE3初始Low、PE5/PE6下降沿EXTI以及PB10硬件Fault路径；
10. 检查USART2、软件I2C、SWD和HSE未被挤占；
11. 检查COMP/DAC和HRTIM F仍关闭；
12. 生成代码并按第15节逐项比对，尤其确认CSS、HRTIM Fault、DMA链接和用户安全代码未被覆盖。

## 17. 验收清单

### 17.1 CubeMX静态检查

- [ ] 器件为STM32G474VCT6、LQFP100。
- [ ] 固件包固定为G4 V1.6.2，未自动跟随最新版。
- [ ] SYSCLK/HCLK/HRTIM1均为170 MHz。
- [ ] ADC12和ADC345实际转换时钟均为42.5 MHz。
- [ ] PA8~PA11、PB12~PB15、PC8/PC9均显示正确HRTIM输出。
- [ ] PC6/PC7在P1保持未启用，只有采用制动桥臂的P2才配置HRTIM F。
- [ ] Timer A~E均为MUL4、Up-Down、Period 34000。
- [ ] A~E的Roll-over、Dead Time、Fault和Output字段一致。
- [ ] Trigger 1来源为Master CMP2并服务ADC1/2。
- [ ] Trigger 2来源为Master CMP2并服务ADC3/4/5。
- [ ] ADC Rank与第7.2节完全一致。
- [ ] 五路DMA均为Circular、Half Word、Memory Increment Enable。
- [ ] ADC1_2、ADC3、ADC4、ADC5全局中断已启用，仅用于ADC错误处理。
- [ ] DMA1_CH2/ADC2优先级1、DMA1_CH1/ADC1优先级2，PFC控制只从ADC1完整DMA路径进入。
- [ ] ADC3/4/5使用独立序列形成逆变快照，逆变控制每个PWM周期只运行一次。
- [ ] PB10为数字Fault 3、低有效、Filter None、Blanking None。
- [ ] HSE CSS已启用，NMI用户代码会关闭PWM并拉低PE0/PE1。
- [ ] PE0/PE1/PE2/PE3初始Low。
- [ ] USART2无DMA和全局中断。
- [ ] 没有启用硬件I2C外设。
- [ ] COMP和DAC保持关闭。

### 17.2 无功率上板

1. 测量SYSCLK和HRTIM输出频率；
2. 分别验证A/B和C/D/E为10 kHz中心对齐PWM；
3. 检查每对上下管互补关系和约500 ns死区；
4. 改变CMP1，确认占空比连续变化且只在预期边界更新；
5. 确认Trigger 1/2及五路DMA完成频率均为10 kHz；
6. 核对所有DMA数组Rank顺序；
7. 使用调试GPIO或示波器确认采样点远离实际开关沿；
8. 拉低PB10，确认不依赖CPU即可关闭A~E全部输出；
9. 保持PB10恢复，确认软件不会自动重新启动PWM；
10. 确认复位、下载、HardFault和IWDG复位后PE0/PE1仍为Low；
11. 确认OLED和VOFA工作时不会改变10 kHz控制节拍。

### 17.3 低压联合验证

1. PFC A/B低压开环；
2. PFC ADC1/2采样和电流闭环；
3. C/D/E三相低调制比开环，确认U/V/W桥臂参考相差120度；
4. 核对IU/IV/IW及负载侧VUV/VVW/VWU的极性、比例和Rank顺序；
5. 验证`IU+IV+IW`和`VUV+VVW+VWU`在传感器误差允许范围内接近0；
6. 接三相三线阻性负载，先验证线电压开环，再验证线电压闭环；
7. 分别停止任一路DMA，确认输出关断并锁存故障；
8. 注入PFC或逆变驱动故障，确认两部分同时关断；
9. 通过全部低压Fault测试后才提高母线电压和负载。

### 17.4 赛题要求1~5验收

1. 使用功率分析仪测量，不能用OLED或ADC换算结果代替评分仪器；
2. 输入保持`36 V RMS/50 Hz`，负载线电流达到`2.0 A`后，验证输出线电压`32 V RMS ±0.1 V`；
3. 在60 Hz模式测量频率误差、输入功率因数、整机效率和输出线电压THD；
4. 效率计算必须包含题目允许的直流辅助电源消耗。正式测量时降低OLED/VOFA刷新率或关闭不必要的调试负载；
5. 切换到30 Hz时只改变软件输出频率指令和对应控制器系数，不修改HRTIM载波、ADC触发或DMA配置；
6. 30 Hz模式重新验证`32 V RMS ±0.1 V`和`2.0 A`，禁止直接复用未经重算的60 Hz PR/QPR谐振系数；
7. 分别保存60 Hz和30 Hz下的输入、母线、三路线电压、三相线电流及故障状态，作为设计报告测试数据。

## 18. 资源冲突

| 引脚/资源 | 本方案用途 | 主要冲突 |
| --- | --- | --- |
| PA8~PA11 | HRTIM A/B | USART1、TIM1、USB_DM等 |
| PB12~PB15 | HRTIM C/D | SPI2、USART3及部分模拟功能 |
| PC8/PC9 | HRTIM E | TIM8、I2C3等 |
| PC6/PC7 | 可选HRTIM F制动桥臂 | TIM8、I2C4、部分HRTIM EEV/Fault |
| PD8/PD9 | ADC4/5相电流 | USART3_TX/RX |
| PB10 | HRTIM1_FLT3 | USART3_TX、TIM1_BKIN等 |
| PA15/PB7 | 软件I2C OLED | HRTIM Fault/EEV和其他TIM复用 |
| PA11 | HRTIM1_CHB2 | USB_DM，因此本方案不使用原生USB |
| PF0/PF1 | HSE | ADC、COMP、SPI/I2C复用 |

每次增加可选功能或更换引脚后，都必须重新运行CubeMX Pinout冲突检查，不能只依据本文表格判断。

## 19. 控制周期和参数边界

当前HRTIM载波和ADC采样基线保持10 kHz，即控制采样周期：

```text
Ts = 100 us
```

10 kHz是低压Bring-up和首轮闭环的统一基线，不等于已经证明可满足`THD <= 2%`和`eta >= 95%`。应先完成LC滤波器、死区和闭环调试，再根据功率分析仪结果决定是否提高载波频率。

Simulink模型中部分QPR模块使用`Ts=1/20000=50 us`。移植时必须以100 us重新离散化QPR/PR系数，不能直接复制50 us系数。若以后统一改为20 kHz，需要同时重算：

- Master、Timer A~E以及已选用的Timer F Period；
- ADC Trigger位置；
- ADC序列完成余量；
- PR/QPR/PI离散系数；
- DMA和ISR最坏执行时间；
- VOFA和OLED后台带宽。

本文不提供20 kHz的并行配置，避免现场误混两套时基。未经完整重算和示波器复验，不允许只把HRTIM Period减半。

输出基波频率与10 kHz载波解耦。不要继续用“每周期固定200点并清零”的50 Hz专用方式生成赛题参考；60 Hz和30 Hz都不能由10 kHz整除。软件应使用相位累加器/DDS或等效数控振荡器：

```text
phase_step = round(fout / 10000 x 2^32)
```

`fout`在要求1~4时为60 Hz，在要求5时为30 Hz。该切换不需要修改CubeMX，只需要切换DDS频率指令和对应的电压控制器参数。

ADC1 Rank 2的VBUS必须作为实际母线反馈使用，不能仅把`52~55 V`写成软件常量。每个逆变控制周期至少使用最新有效VBUS完成调制前馈，并由慢速保护判断母线欠压、过压和纹波；线电压闭环则使用负载侧VUV/VVW/VWU，二者职责不能混用。

两套快速控制的调用关系固定为：

```text
ADC2 DMA完成 -> 发布VAC序列
ADC1 DMA完成 -> 核对VAC -> PFC电流环（10 kHz）
                         -> 母线PI分频执行（1 kHz）

ADC3/4/5 DMA完成 -> 发布三相一致性快照
                 -> 三相电压/电流控制 + CBSVPWM（10 kHz，每帧一次）

1 ms状态机 -> 交换VBUS、Fault和Gate Enable许可，不运行第二次快速控制
```

两个10 kHz控制路径可能在同一个100 us周期内先后进入中断，必须用DWT周期计数器或调试GPIO测量联合最坏执行时间。初步要求每个快速路径单独小于`30 us`，两者加上DMA/HAL开销后仍应保留明确余量；OLED、VOFA、浮点格式化和任何阻塞HAL调用只能留在主循环。

## 20. 最终边界

- 赛题逆变器固定使用C/D/E三桥臂和三相三线输出，不配置中性桥臂；HRTIM F默认关闭，仅可选作制动桥臂。
- VUV/VVW/VWU和IU/IV/IW均作为负载侧控制、保护或冗余反馈，不只是显示量。
- 当前`02OpenLoop.ioc`不需要为了30/60 Hz、32 V线电压或三相三线而改变HRTIM频率、ADC引脚、Rank或DMA通道。
- PFC和逆变使用两个独立同步快照及两个明确的快速控制入口，不建立五ADC全局等待屏障。
- 真正需要新增或确认的是板级线电压差分/隔离调理、52~55 V母线能力、LC滤波器和校准参数；这些不能由CubeMX代替。
- 10 kHz为当前统一基线，只有THD、效率或动态性能实测证明需要时才整体迁移到20 kHz。
- 500 ns死区和24.5 cycles采样时间只是低压起点。
- COMP/DAC是后续可选增强，不是首版启动依赖。
- 没有可靠DESAT/OCP时，禁止高压带功率运行。
- 任何无法确认ADC同步、DMA更新、Fault状态或控制心跳的情况，都按故障处理。
- 故障恢复后禁止自动重启，必须先保持PWM和PE0/PE1关闭，再由人工确认。
