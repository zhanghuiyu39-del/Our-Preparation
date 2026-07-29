# STM32G474 单相 PWM 整流器 CubeMX 外设配置指南

> **修改标记说明：** 本次依据固定板卡保护方案修改的旧内容保留在原位置，并使用 `~~删除线~~` 标出；紧随其后的 **【修改为】** 是当前应执行的配置。原文没有对应内容的新增项使用 **【新增】** 标记。

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
| COMP | ~~首版不启用~~ **【修改为】** 默认不作为启动依赖；完成无功率验证后可启用COMP1/IPFC和COMP3/VBUS增强保护 |

本文只描述 CubeMX 中实际存在的设置，以及生成代码后必须补充的启动和保护逻辑。不会把软件状态机、HAL 启动调用或示波器整定项伪装成 CubeMX 参数。

~~当前IOC由`STM32Cube FW_G4 V1.6.3`生成，且`ProjectManager.LastFirmware=true`；本项目规范仍固定为V1.6.2。~~

**【修改为】** 当前 `03-1OpenLoop.ioc` 已记录 `STM32Cube FW_G4 V1.6.2` 且 `ProjectManager.LastFirmware=false`，不需要再次修改固件包。以后重新生成代码仍应保持这两项，并完整比较HRTIM、ADC、DMA、Fault和新增模拟看门狗代码。

> ~~PE0 必须有板级下拉。驱动器必须具备独立 DESAT/OCP 或等效短路保护。PB10/HRTIM1_FLT3 是首版必须保留的硬件关断路径。~~
>
> ~~原理图核对表明PE0没有连接UCC21520的`DISABLE`，不能作为实际Gate Enable；驱动板也没有引出的`nFAULT/DESAT/OCP`。PB10/HRTIM1_FLT3目前依靠板上约10 kOhm上拉，只能作为外接手动急停和故障注入输入。按现有低有效接法，直接连接PB10-GND的按钮必须使用“按下闭合”的常开触点。ADC模拟看门狗与COMP可增强过流/过压保护，但仍不能代替独立DESAT/OCP。~~
>
> ~~本版不使用PB10急停、PE0或PE5，三者均恢复为`Reset_State`。~~
>
> **【修改为】** PE0和PE5不使用，均恢复为`Reset_State`。PB10继续配置为`HRTIM1_FLT3`，但板外不接急停按钮、驱动故障线或其他信号；板上约10 kOhm上拉必须保证PB10稳定为非故障高电平。该配置保留Fault 3硬件关断能力和以后接入故障源的接口，但在外部没有故障源时，它不会主动检测OCP/DESAT，不能计入当前实际保护链。基础保护仍以ADC模拟看门狗和软件Trip为主，建议验证COMP1到内部HRTIM Fault 1作为增强保护。

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
| 裸板开环验证 | ~~当前HRTIM A/B、ADC1/2、DMA、USART2、OLED、IWDG、FLT3；不启用FLT3~~ **【修改为】** 当前HRTIM A/B、ADC1/2、DMA、USART2、OLED、IWDG和FLT3，PB10板外不接信号 | ~~PE0始终为低，只允许示波器测PA8~PA11~~ **【修改为】** PE0不使用；无功率边界是仅开放MCU侧HRTIM引脚，驱动器与母线均断电 |
| 低压PFC验证 | ~~外设配置基本不变，增加VAC同步、软件保护和人工启动状态机~~ **【修改为】** 增加ADC1/2模拟看门狗、统一Trip和人工启动状态机；COMP1/3仅在验证后加入 | ~~采样有效、Fault无效且人工确认后才允许PE0为高~~ **【修改为】** 采样有效、软件故障锁存为无、人工确认后才允许调用HRTIM Output Start开放PWM命令 |
| 题目要求1~5联合运行 | 在后续联合工程增加HRTIM C/D/E、逆变采样和INV_GATE_EN | PFC母线稳定后启动逆变器，支持60 Hz和30 Hz两档 |

当前IOC不需要提前启用HRTIM C/D/E/F或ADC3/4/5，但相应引脚不得分配给无关的永久功能。裸板开环代码中自动开放MCU PWM引脚只属于第一档，不能直接沿用到带功率版本。

### 1.3 当前IOC审查结论

| 项目 | 当前状态 | 处理结论 |
| --- | --- | --- |
| 170 MHz时钟树和ADC异步/4 | 正确 | 保持 |
| HRTIM Master/A/B、10 kHz和500 ns初始死区 | 正确 | 保持，死区以后按实测调整 |
| Master CMP2和ADC Trigger 1 | 正确 | 保持，采样位置以后按示波器调整 |
| ADC1/ADC2规则组与双循环DMA | 正确 | 保持 |
| DMA和ADC1_2中断 | ~~正确，仅处理ADC错误~~ **【修改为】** 资源正确 | 保持当前DMA；ADC1_2共享中断增加AWD1/AWD2越界处理 |
| ADC模拟看门狗 | ~~当前未启用~~ ~~IOC已启用，但生成代码未同步~~ **【修改为】** IOC与当前生成代码已同步 | ADC1 AWD1监视IPFC、AWD2监视VBUS，ADC2 AWD1监视VAC；当前已有三次`HAL_ADC_AnalogWDGConfig()`，但ADC2 AWD1的`0/0`阈值必须修正 |
| PB10 Fault 3数字输入、低有效、无Blanking | ~~改为不使用和`Reset_State`~~ **【修改为】** 保持`HRTIM1_FLT3` | ~~作为驱动器总故障输入或外接急停~~ **【修改为】** 板外不接任何信号；保留Fault配置但不作为当前有效故障来源 |
| ~~PE0/PE5保护接线~~ **【修改为】PE0/PE5不使用** | ~~目标配置已是`Reset_State`~~ **【修改为】** 当前IOC仍为PE0输出、PE5下降沿EXTI | 先清理应用代码引用，再把两脚改为`Reset_State`并重新生成 |
| **【新增】PD0人工启停按键** | GPIO Input、内部上拉、低有效 | 当前配置正确；主循环每1 ms轮询，不启用EXTI0 |
| OLED软件I2C和USART2 | 可用于调试 | 保持在主循环低速任务中使用 |
| 固件包 | ~~当前为G4 V1.6.3且自动最新版~~ **【修改为】** 当前已是G4 V1.6.2且`LastFirmware=false` | 保持，不再重复切换 |
| HSE CSS | ~~当前IOC未记录启用~~ **【修改为】** `RCC.EnbaleCSS=true` | 当前已启用；保留NMI中的HRTIM安全关断 |
| HRTIM C/D/E、ADC3/4/5 | 当前关闭 | 当前PFC工程保持关闭，联合工程再启用 |

~~因此，当前阶段真正需要的小幅CubeMX调整只有固件包固定以及带功率前启用CSS；物理阈值保护属于生成代码后的软件职责。~~

~~PWM频率、ADC时钟、DMA通道和FLT3路由继续保持；当前必须增加ADC1/2模拟看门狗、启用CSS、取消无实际信号的PE5 EXTI，并修正PE0的软件含义。人工启动、故障锁存、动态阈值切换和IWDG复位策略仍属于生成代码后的软件职责。~~

~~PWM频率、ADC时钟和DMA通道继续保持；取消FLT3数字输入、PE0和PE5。~~

~~PWM频率、ADC时钟、DMA通道和PB10/FLT3数字输入继续保持；仅取消PE0和PE5。当前必须增加ADC1/2模拟看门狗、启用CSS。~~

**【修改为】** PWM频率、ADC时钟、DMA通道、PB10/FLT3、CSS和PD0配置继续保持；PE0和PE5仍需按迁移顺序取消。IOC已加入ADC1/2模拟看门狗，~~但当前`adc.c`尚未生成对应初始化~~ **【修改为】** 当前`adc.c`已经生成三个AWD配置；ADC2 AWD1仍为`High=0、Low=0`，必须先修正阈值并重新生成。人工启动、故障锁存、动态阈值切换和IWDG复位策略仍属于应用软件职责。

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
| ~~PFC 驱动使能~~ **【修改为】不使用** | ~~GPIO Output~~ **【修改为】** 不分配外设/GPIO | PE0 | ~~PFC_GATE_EN~~ **【修改为】** `Reset_State`，不保留软件Label |
| ~~驱动总故障/手动急停~~ **【修改为】保留但外部不接** | HRTIM Fault 3 | PB10 | `HRTIM1_FLT3` |
| ~~PFC 故障诊断~~ **【修改为】不使用** | ~~普通GPIO输入或Analog~~ **【修改为】** 不分配外设/GPIO | PE5 | ~~PFC_NFAULT_DIAG~~ **【修改为】** `Reset_State`，不保留软件Label |
| **【新增】PFC人工启停按键** | GPIO Input | PD0 | `PFC_START_KEY`，内部上拉、低有效 |
| VOFA | USART2 | PD5/PD6 | USART2_TX/RX |
| OLED软件I2C | GPIO Output | PA15/PB7 | OLED_SCL/OLED_SDA |
| 看门狗 | IWDG | 内部 LSI | IWDG |

首版只为 ADC1、ADC2 启用两路独立 DMA。HRTIM C/D/E/F、ADC3/4/5和普通TIM保持关闭。~~COMP1~7、DAC全部保持关闭。~~ **【修改为】** ADC模拟看门狗必须启用；COMP1/COMP3与DAC1/DAC3属于验证通过后才启用的增强档，默认调试档仍保持关闭。三相逆变器预留引脚不要分配给无关功能。

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

### 4.1 ~~PE0：PFC_GATE_EN~~ **【修改为】PE0：不使用**

| CubeMX 位置 | 字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- | --- |
| Pinout | PE0 | ~~GPIO_Output~~ **【修改为】** `Reset_State` | 不生成PE0 GPIO输出初始化 |
| GPIO Settings | User Label | ~~PFC_GATE_EN；可暂保留`PFC_GATE_EN`兼容现有代码，后续统一改为`PFC_GATE_EN_RSVD`~~ **【修改为】** 删除Label | `main.h`中不应再生成PE0相关宏 |
| GPIO Settings | GPIO output level | ~~Low~~ **【修改为】** 不适用 | 不生成PE0写电平代码 |
| GPIO Settings | GPIO mode | ~~Output Push Pull~~ **【修改为】** 不适用（引脚为`Reset_State`） | 不出现`GPIO_MODE_OUTPUT_PP` |
| GPIO Settings | Pull-up/Pull-down | ~~No pull~~ **【修改为】** No Pull | 未使用引脚不参与逻辑判断 |
| GPIO Settings | Maximum output speed | ~~Low~~ **【修改为】** 不适用 | 不生成PE0速度配置 |

~~PE0 的CubeMX初始低电平只是软件措施，不能替代板级下拉。初始化、ADC校准、HRTIM DLL校准和自检完成前不得拉高PE0。~~

~~扩展板上PE0已有约10 kOhm下拉，但该引脚没有连接UCC21520的`DISABLE`，拉高或拉低都不会直接使能/关闭驱动器。当前项目应始终保持PE0为Low，不把它加入“功率级已经关闭”的唯一判据；真正停止栅极命令必须关闭HRTIM TA1/TA2/TB1/TB2输出。~~

**【修改为】** PE0完全退出本项目配置和软件逻辑。真正停止栅极命令必须调用HRTIM Output Stop关闭TA1/TA2/TB1/TB2，并禁止后续Compare更新；任何状态机或IWDG健康检查都不得再读取PE0。

### 4.2 ~~PE5：PFC_NFAULT_DIAG~~ **【修改为】PE5：不使用**

| CubeMX 位置 | 字段 | 设置值 |
| --- | --- | --- |
| Pinout | PE5 | ~~GPIO_EXTI5 / GPIO_Input~~ **【修改为】** `Reset_State` |
| GPIO Settings | User Label | ~~PFC_NFAULT_DIAG / PFC_FAULT_RSVD~~ **【修改为】** 删除Label |
| GPIO mode | ~~External Interrupt Mode with Falling edge trigger detection / Input mode~~ **【修改为】** 不适用（引脚为`Reset_State`） |
| Pull-up/Pull-down | ~~Pull-up~~ **【修改为】** No Pull |
| NVIC | EXTI line[9:5] interrupt | ~~Enable，Priority 3~~ **【修改为】** Disable |

~~PE5只用于记录PFC驱动器故障来源。当前原理图没有把任何驱动故障接到PE5，因此不能用它记录OCP/DESAT。以后若通过扩展接口接入真实故障触点，再根据触点正常电平选择上升沿或下降沿EXTI。~~ **【修改为】** PE5不再保留故障诊断用途，生成代码不得注册PE5 EXTI回调，也不得把PE5电平作为启动或喂狗条件。

### 4.3 **【新增】PD0：PFC_START_KEY人工启停按键**

PD0只提供人工运行命令，不是急停、硬件过流保护或故障复位输入。按键使用常开触点：松开时由上拉保持High，按下时将PD0短接到GND形成Low。

| CubeMX位置 | 字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- | --- |
| Pinout | PD0 | `GPIO_Input` | GPIOD时钟和PD0输入初始化 |
| GPIO Settings | User Label | `PFC_START_KEY` | `PFC_START_KEY_Pin`和`PFC_START_KEY_GPIO_Port` |
| GPIO Settings | GPIO mode | Input mode | `GPIO_MODE_INPUT` |
| GPIO Settings | Pull-up/Pull-down | Pull-up | `GPIO_PULLUP` |
| GPIO Settings | GPIO output level | 不适用 | 不生成PD0写电平代码 |
| GPIO Settings | Maximum output speed | 不适用 | 输入模式不配置Speed |
| Pinout | Pin Reserved/Locked | Enable（保持当前设置） | 防止CubeMX自动改作其他复用功能 |
| NVIC | EXTI line 0 interrupt | Disable | 不生成`EXTI0_IRQn`和PD0 EXTI回调 |

当前软件通过`PFC_AppTick1ms()`每1 ms轮询PD0，采用以下固定行为：

| 条件 | 动作 |
| --- | --- |
| 原始电平连续稳定30 ms | 承认按下或释放状态变化 |
| 按下50~1000 ms后释放 | 在释放沿产生一次短按事件 |
| `PFC_READY`收到短按 | 复核采样、VAC/VBUS、参数和Fault条件后启动HRTIM输出 |
| `PFC_OPEN_LOOP_RAMP`或`PFC_OPEN_LOOP_RUN`收到短按 | 立即执行人工停机并进入`PFC_STOP` |
| 按住超过1000 ms | 首版不执行动作 |
| `PFC_FAULT_LATCH`收到按键事件 | 忽略，不允许按键清除故障或自动重启 |

**【新增：软件安全阻断项】** 当前消抖变量初始化为“未按下”，但尚未实现“上电后先观察到一次稳定释放，才允许接受新的按下-释放周期”。如果按住PD0上电并一直保持到`PFC_READY`后再释放，现有代码可能把该释放识别为启动短按。带功率前必须增加按键释放解锁状态；完成前不得把PD0作为正式启动许可。

### 4.4 PA15/PB7：OLED软件I2C

| 引脚 | User Label | GPIO模式 | Pull | Speed |
| --- | --- | --- | --- | --- |
| PA15 | OLED_SCL | Output Push Pull | No pull | High |
| PB7 | OLED_SDA | Output Open Drain | No pull | High |

OLED仅用于低速调试显示，不参与采样、控制或保护。软件I2C刷新只能放在主循环低速任务中，禁止在ADC DMA、HRTIM或Fault中断中调用。SDA必须有外部上拉；若OLED模块已经自带上拉，不要重复并联过小阻值。

### 4.5 PB10：HRTIM1_FLT3（保留功能，板外不接）

~~在Pinout中将PB10改为`Reset_State`，不选择`HRTIM1_FLT3`。~~

**【修改为】** 在Pinout选择`PB10 > HRTIM1_FLT3`，不再配置普通GPIO或GPIO EXTI。板外不接急停按钮、驱动故障线或其他信号。

~~PB10急停和Fault 3功能全部取消。~~ **【修改为】** Fault 3功能保留，但PB10板外不接任何故障源，因此当前不会因真实驱动器OCP/DESAT自动动作。必须确认板上约10 kOhm上拉实际存在且PB10静态为High；如果上拉缺失或引脚悬空，禁止启用HRTIM输出。以后接入故障源时，只允许使用与低有效输入兼容的开漏/开集电极输出，不能直接线与多个推挽输出。

## 5. HRTIM1 配置

### 5.1 启用模式

在 `Timers > HRTIM1` 中启用：

- Master Timer；
- Timer A Output TA1/TA2；
- Timer B Output TB1/TB2；
- ADC Trigger 1；
- ~~基础配置不启用Fault Line 3。~~ **【修改为】** 启用Fault Line 3；PB10板外不接信号。

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

### 5.8 Fault Line 3（保留配置，外部不接）

| CubeMX 分组 | 字段 | 设置值 | HAL 生成结果 |
| --- | --- | --- | --- |
| Fault Configuration > Fault Line 3 | Source | ~~Disabled/不配置~~ **【修改为】** Digital Input | `HRTIM_FAULTSOURCE_DIGITALINPUT` |
| Fault Line 3 | Polarity | ~~不适用~~ **【修改为】** Active Low | PB10被拉低时触发 |
| Fault Line 3 | Filter | ~~不适用~~ **【修改为】** None | 首次验证不增加数字延时 |
| Fault Line 3 | Lock | ~~不适用~~ **【修改为】** Read/Write | 调试阶段允许修改 |
| Fault Line 3 | Enable | ~~Disable~~ **【修改为】** Enable | `HAL_HRTIM_FaultModeCtl()` |
| Timer A Configuration | Fault Enable | ~~None~~ **【修改为】** Fault 3；增强档可再加入Fault 1 | Timer A响应FLT3 |
| Timer B Configuration | Fault Enable | ~~None~~ **【修改为】** Fault 3；增强档可再加入Fault 1 | Timer B响应FLT3 |
| Timer A/B Configuration | Fault Lock | Read/Write | 不在调试期硬锁 |
| TA1/TA2/TB1/TB2 | Fault Level | Inactive | 故障输出无效态 |

~~Fault输入的CubeMX `Lock`是硬件配置写保护，不等于软件的“故障锁存”。首版调试保持Read/Write；HRTIM硬件先把四路PWM置为Inactive，Fault ISR负责再次执行Output Stop、保持PE0为Low、记录故障码。~~

~~基础配置没有Fault 3硬件关断。~~ **【修改为】** HRTIM保留Fault 3硬件关断，PB10一旦被拉低即可在CPU响应前把四路PWM置为Inactive；但由于板外没有连接实际故障源，这项能力当前不会自动响应驱动器OCP/DESAT。ADC AWD、DMA失步、ADC错误、CSS和状态机超时仍必须在各自ISR或监督路径中先执行HRTIM Output Stop，再锁存故障。

~~在`HRTIM Interrupt Configuration`中加入`Fault 3 interrupt enable`，并在NVIC中启用`HRTIM1 fault global interrupt`，对应`HRTIM1_FLT_IRQn`，抢占优先级设为0。~~

~~基础配置关闭`Fault 3 interrupt enable`。~~ **【修改为】** 启用`Fault 3 interrupt enable`和`HRTIM1_FLT_IRQn`，抢占优先级0。中断只负责二次Output Stop、记录和锁存；真正的FLT3输出无效化由HRTIM硬件完成。

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
HAL_HRTIM_ADCTriggerConfig()
```

~~基础配置不应为PB10/FLT3生成Fault初始化。~~ **【修改为】** 生成代码应包含PB10/FLT3对应的`HAL_HRTIM_FaultConfig()`和`HAL_HRTIM_FaultModeCtl()`。

软件启动顺序固定为：

1. ~~PE0保持低，但不把它当作已关闭驱动器的证明；~~ **【修改为】** PE0不使用；直接确认TA1/TA2/TB1/TB2输出尚未开放；
2. ~~按第6.7节完成ADC校准、两路DMA启动、模拟看门狗和Fault检查；~~ **【修改为】** 按第6.7节完成ADC校准、两路DMA启动、模拟看门狗、软件故障锁存和HRTIM Fault检查；
3. HRTIM DLL Ready；
4. 写入安全比较值；
5. 同时启动Master/A/B计数器；
6. 保持TA1/TA2/TB1/TB2输出关闭；
7. ~~状态机确认后直接启动输出；~~ **【修改为】** 状态机先确认PD0已经连续稳定释放至少30 ms，完成按键释放解锁；
8. 进入`PFC_READY`后只接受一次新的50~1000 ms按下-释放短按事件；
9. ~~最后才允许PE0拉高；PE0继续保持Low。~~ **【修改为】** PE0不参与流程；短按事件和全部启动条件通过后才调用HRTIM Output Start开放TA1/TA2/TB1/TB2。

~~停止或故障时先拉低PE0；基础配置没有PB10硬件抢先关断能力。~~ **【修改为】** 停止或软件故障时立即关闭HRTIM四路输出，禁止新的Compare更新并清零控制器状态。若FLT3触发，HRTIM先把输出置为Inactive，ISR再完成软件锁存；PE0不参与任何步骤。

PD0短按停机依赖1 ms轮询、消抖和CPU执行，只是正常运行命令，不是紧急关断路径。过流、过压、ADC/DMA错误和HRTIM Fault不得等待按键处理。

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

如果CubeMX 6.17页面只显示`Conversion Data Management Mode`而不单独显示`DMA Continuous Requests`，以生成代码中的`ADC_CONVERSIONDATA_DMA_CIRCULAR`为最终核对依据。ADC1和ADC2上电后分别执行单端校准；~~校准期间PE0、HRTIM计数器和全部HRTIM输出必须关闭。~~ **【修改为】** 校准期间HRTIM计数器和全部HRTIM输出必须关闭，PE0不参与流程。

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

### 6.4 **【新增】ADC1/ADC2模拟看门狗**

模拟看门狗直接比较每次ADC转换结果，不需要等待DMA回调中的物理量换算。它仍然由10 kHz HRTIM触发采样，最坏检测时间约为一个100 us采样周期加ADC转换和ISR延迟，因此属于快速过载保护，不是MOSFET短路保护。

~~**【新增：当前工程阻断项】** `03-1OpenLoop.ioc`已经记录ADC1 AWD1、ADC1 AWD2和ADC2 AWD1，但当前`Core/Src/adc.c`仍未生成任何`HAL_ADC_AnalogWDGConfig()`。这表示IOC与生成代码不同步；在重新生成并核对三个AWD配置以前，不能认为模拟看门狗已经生效。当前IOC阈值也只是无功率调试窗口：ADC1 AWD1为`High=4095、Low=0`，ADC1 AWD2为`High=4080、Low=0`，ADC2 AWD1保持默认宽窗口，均不能直接作为带功率保护值。~~

**【修改为：当前工程阻断项】** 当前`Core/Src/adc.c`已经生成三次`HAL_ADC_AnalogWDGConfig()`：ADC1 AWD1为Channel 2、`High=4095、Low=0`；ADC1 AWD2为Channel 7、`High=4080、Low=0`；ADC2 AWD1为Channel 6，但实际生成值是`High=0、Low=0`。ADC2的`0/0`不是宽窗口，VAC只要不是0码就会越界。无功率调试前必须在CubeMX中明确填写ADC2 AWD1为`High=4095、Low=0`并重新生成；带功率前，三个窗口都必须替换为根据模拟前端和实测VDDA标定得到的保护阈值。

#### 6.4.1 ADC1 AWD1：IPFC双向过流窗口

在 `Analog WatchDog` 或 `Parameter Settings > Analog WatchDog 1` 中配置：

| CubeMX字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- |
| Enable Analog WatchDog 1 Mode | Enable | 生成一次`HAL_ADC_AnalogWDGConfig()` |
| Watchdog Number | Analog Watchdog 1 | `ADC_ANALOGWATCHDOG_1` |
| Watchdog Mode | Single regular channel | `ADC_ANALOGWATCHDOG_SINGLE_REG` |
| Watchdog Channel | ADC1_IN2 / IPFC | `ADC_CHANNEL_2` |
| Interrupt Mode | Enable | `ITMode = ENABLE` |
| Filtering | None | `ADC_AWD_FILTERING_NONE` |
| Low Threshold | `IPFC_AWD_LOW_CODE` | 按实测负向电流阈值计算 |
| High Threshold | `IPFC_AWD_HIGH_CODE` | 按实测正向电流阈值计算 |

CubeMX字段只能输入数字，表中的宏名表示应先计算再填写，不能把文字直接填进CubeMX。无功率验证时可暂填`Low=0、High=4095`确认配置和回调路径；该范围不会形成有效过流保护，带功率前必须替换。

PFC电流采样已知近似具有“电流增大、ADC电压降低”的极性，不能直接认为正向过流一定对应High Threshold。先实测：

```text
code(+Itrip) = round(offset_code + slope_code_per_A x (+Itrip))
code(-Itrip) = round(offset_code + slope_code_per_A x (-Itrip))
IPFC_AWD_LOW_CODE  = min(code(+Itrip), code(-Itrip))
IPFC_AWD_HIGH_CODE = max(code(+Itrip), code(-Itrip))
```

AWD1使用完整12位阈值，阈值范围为0~4095。

#### 6.4.2 ADC1 AWD2：VBUS过压和运行态欠压窗口

先在ADC1的 `Analog WatchDog 2` 分组启用AWD2，再回到PC1/ADC1_IN7规则通道行，把 `Monitored by` 选择为 `Analog Watchdog 2`：

| CubeMX字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- |
| Enable Analog WatchDog 2 Mode | Enable | AWD2配置被生成 |
| Watchdog Mode | ~~Single regular channel~~ **【修改为】** CubeMX页面选择`Single channel` | G4的AWD2不区分Regular/Injected，当前IOC记录为`ADC_ANALOGWATCHDOG_SINGLE_REGINJEC` |
| Regular Rank 2 > Monitored by | Analog Watchdog 2 | `ADC_ANALOGWATCHDOG_2` |
| ~~AWD2分组内的Channel下拉框~~ **【修改为】Regular Rank 2 > Channel** | Channel 7 / VBUS | 通道由Rank行绑定，生成配置中应为`ADC_CHANNEL_7` |
| Interrupt Mode | Enable | `ITMode = ENABLE` |
| Filtering | None | 首次验证不增加连续越界次数 |
| Low Threshold | 启动阶段先为0；运行态改为`VBUS_UV_CODE` | 欠压不能阻止正常启动 |
| High Threshold | `VBUS_OV_CODE` | 母线过压阈值 |

**重要限制：** STM32G474的AWD2和AWD3只有8位有效比较精度。ADC使用12位时最低4位被忽略，因此填写的阈值应按16个ADC码对齐：

```text
code12 = round(Vadc_threshold / VDDA x 4095)
awd2_code = clamp((code12 + 8) & 0xFF0, 0, 4080)
```

阈值计算必须使用实测VDDA和母线分压/隔离采样标定，不要把60~65 V母线建议值直接写成保护阈值。上电时VBUS从0 V开始，若一开始写入非零欠压阈值，AWD2会立即报错；因此CubeMX可先填`Low=0`，软件进入母线稳态后再停转换、更新运行态欠压阈值并重新启用。

AWD2分组内没有独立的Channel 7下拉框属于正常现象。必须在ADC1规则组的Rank 2行设置`Channel 7`，再把同一行的`Monitored by`设为`Analog Watchdog 2`。当前IOC中的`AWDChannel-1#ChannelRegularConversion=ADC_CHANNEL_7`和`MonitoredBy-1#ChannelRegularConversion=ADC_ANALOGWATCHDOG_2`已经证明该绑定正确。

#### 6.4.3 ADC2 AWD1：VAC严重过压窗口

| CubeMX字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- |
| Enable Analog WatchDog 1 Mode | Enable | ADC2生成一次`HAL_ADC_AnalogWDGConfig()` |
| Watchdog Number | Analog Watchdog 1 | `ADC_ANALOGWATCHDOG_1` |
| Watchdog Mode | Single regular channel | `ADC_ANALOGWATCHDOG_SINGLE_REG` |
| Watchdog Channel | ADC2_IN6 / VAC | `ADC_CHANNEL_6` |
| Interrupt Mode | Enable | `ITMode = ENABLE` |
| Filtering | None | 首轮不滤波 |
| Low Threshold | 0或经标定的负峰安全边界 | 不用于判断交流输入丢失 |
| High Threshold | 经标定的正峰安全边界 | 只作严重输入过压/贴轨保护 |

**【新增：必须修改】** 当前CubeMX未保存ADC2 AWD1的High/Low Threshold，生成代码实际得到`HighThreshold = 0`、`LowThreshold = 0`。先在页面中手工填入无功率调试值`High Threshold = 4095`、`Low Threshold = 0`，重新生成后逐行核对；这一步只避免误触发，不提供有效VAC过压保护。

VAC是带中点偏置的50 Hz双极性波形，不能用普通的非零Low Threshold判断“交流输入丢失”，否则波形经过中点或零交越时会周期性触发。输入丢失应由软件在完整周期上计算RMS、频率或PLL状态后判定；AWD只用于采样输入越过模拟前端安全范围。

#### 6.4.4 AWD中断职责

ADC1和ADC2共享`ADC1_2_IRQn`，优先级保持1。~~该中断只处理ADC Overrun。~~ **【修改为】** 该中断同时处理AWD1、AWD2和ADC错误：

```text
AWD越界
-> 立即停止TA1/TA2/TB1/TB2 HRTIM输出
-> 锁存首个故障来源和ADC原始码
-> 禁止新的Compare更新
-> 禁止自动恢复并停止刷新IWDG
```

~~AWD越界后保持PE0为Low。~~ **【修改为】** AWD路径不访问PE0；停止四路HRTIM输出后直接锁存故障。

HAL生成代码应包含`HAL_ADC_AnalogWDGConfig()`；回调根据实际HAL生成版本核对`HAL_ADC_LevelOutOfWindowCallback()`以及AWD2对应回调/标志。ISR中禁止OLED、USART、浮点RMS和阻塞等待。

### ~~6.4 DMA Settings~~ **【顺延为】6.5 DMA Settings**

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

### ~~6.5 NVIC与双ADC同步~~ **【顺延为】6.6 NVIC与双ADC同步**

规则组DMA方案不使用ADC1/2的EOC/JEOC中断作为控制入口。~~ADC1 and ADC2 global interrupt只用于ADC Overrun。~~ **【修改为】** 共享的`ADC1 and ADC2 global interrupt`必须开启，用于AWD1/AWD2越界和ADC Overrun；快速控制入口仍然只能来自DMA完整传输回调。

| 中断 | 抢占优先级 | 子优先级 | 用途 |
| --- | ---: | ---: | --- |
| ADC1 and ADC2 global interrupt | 1 | 0 | **【修改】** 处理AWD1/AWD2越界及ADC Overrun，不运行控制算法 |
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

若ADC1完成时VAC序列号没有前进、任一路DMA停止、ADC发生Overrun或样本持续异常，~~应立即拉低PE0、关闭HRTIM输出并保持PE0为Low~~ **【修改为】** 应立即关闭HRTIM四路输出并锁存故障，软件不得读写PE0。快速回调只允许采样快照、物理量换算、范围检查、参考生成、PR计算、调制限幅、HRTIM预装载比较值更新和心跳计数；禁止串口发送和阻塞等待。

### ~~6.6 校准和启动顺序~~ **【顺延为】6.7 校准和启动顺序**

必须在HRTIM计数器产生第一个ADC Trigger之前准备好两个DMA通道：

1. ~~保持PE0为Low并保持HRTIM输出关闭；PE0仅为预留GPIO；~~ **【修改为】** 保持HRTIM输出关闭，PE0不配置也不访问；
2. ~~只确认软件故障锁存为0。~~ **【修改为】** 确认软件故障锁存为0且HRTIM Fault 3未激活；增强档还需确认Fault 1未激活；
3. 对ADC1和ADC2分别执行`HAL_ADCEx_Calibration_Start(..., ADC_SINGLE_ENDED)`；
4. 清零DMA缓冲区、ADC状态标志、序列计数器和控制心跳；
5. 先调用`HAL_ADC_Start_DMA(&hadc2, (uint32_t *)adc2_dma, 1)`；
6. 再调用`HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc1_dma, 2)`；
7. 确认两个启动调用均返回`HAL_OK`，随后关闭两路DMA Half Transfer中断；
8. 按第6.4节配置AWD；无功率阶段可先使用宽窗口验证，带功率前写入标定阈值；
9. 完成HRTIM DLL校准并写入安全比较值；
10. 使用同一次掩码启动HRTIM Master、Timer A和Timer B计数器，暂不启动四路输出；
11. ~~连续观察ADC1/ADC2序列正常更新并确认AWD/Fault均未触发；~~ **【修改为】** 连续观察ADC1/ADC2序列正常更新并确认AWD、软件故障锁存和已启用的内部Fault均未触发；
12. ~~状态机检查通过后才启动HRTIM输出；最后拉高PE0，或PE0全程保持Low。~~ **【修改为】** 状态机检查通过后才启动HRTIM输出；不读写PE0。

HRTIM未运行时，规则组只处于等待外部触发状态，不会因`HAL_ADC_Start_DMA()`自行连续转换。任一ADC/DMA启动失败时不得启动HRTIM计数器或PWM输出。~~不得启动Gate Enable；当前硬件不存在由MCU控制的Gate Enable，不能用PE0状态替代HRTIM输出状态检查。~~ **【修改为】** PE0已经退出配置，唯一的软件输出边界是HRTIM计数器与TA1/TA2/TB1/TB2 Output Start/Stop状态。

ADC硬件自校准与传感器零点标定是两件事。IPFC可在Gate关闭且确认无电流时统计零点；VAC若在MCU启动前已经接入50 Hz交流，不能使用任意长度的短窗口直接取平均作为中点。当前软件使用256点/25.6 ms只适合无交流输入的裸板测试。带功率版本应使用经过板级验证的中点标定值，或至少对完整的50 Hz整数周期取平均，并检查输入是否削顶和偏置是否处于合理范围。

## 6A. **【新增】可选COMP/DAC增强保护**

本节不是基础启动依赖。必须先完成第6.4节ADC模拟看门狗，再逐个启用COMP1和COMP3。COMP输入复用同一ADC模拟节点时，CubeMX 6.17可能显示物理引脚冲突；若无法同时分配，保留ADC引脚配置，不得为了COMP取消IPFC或VBUS采样。此时COMP只能在生成代码的USER CODE区手动初始化，并核对引脚仍为Analog/No Pull。

### 6A.1 COMP1：IPFC单方向快速阈值

PA1同时具备`ADC1_IN2`和`COMP1_INP`能力。推荐先保护正常PFC工作时最危险的正向过流方向；由于现有电流采样近似“电流增大、采样电压降低”，比较器在越过下限时输出由High变Low。

| CubeMX位置 | 字段 | 设置值 | 生成代码中应看到 |
| --- | --- | --- | --- |
| Analog > COMP1 | Mode | Enable | `MX_COMP1_Init()` |
| COMP1 Parameter Settings | Non Inverting Input | IO1 / PA1 | `COMP_INPUT_PLUS_IO1` |
| COMP1 Parameter Settings | Inverting Input | DAC1 Channel 1 | `COMP_INPUT_MINUS_DAC1_CH1` |
| COMP1 Parameter Settings | Output Polarity | Non Inverted | IPFC低于阈值时COMP输出Low |
| COMP1 Parameter Settings | Hysteresis | 10 mV | `COMP_HYSTERESIS_10MV` |
| COMP1 Parameter Settings | Blanking Source | None | `COMP_BLANKINGSRC_NONE` |
| COMP1 Parameter Settings | Trigger Mode | None（内部Fault负责第一响应） | 不依赖COMP ISR完成第一关断 |

DAC1 Channel 1只作内部阈值：

| CubeMX位置 | 字段 | 设置值 |
| --- | --- | --- |
| Analog > DAC1 > Channel 1 | Output connection | On-chip peripherals only；若页面名称略有不同，选择不占外部PA4的内部连接模式 |
| DAC1 Channel 1 | Trigger | None / Software |
| DAC1 Channel 1 | Output Buffer | Enable作为首轮设置 |
| DAC1 Channel 1 | Sample and Hold | Disable |

阈值代码按实测VDDA换算：

```text
DAC1_CH1_CODE = round(Vadc_at_positive_Itrip / VDDA x 4095)
```

一个COMP只有一个阈值，本配置只能覆盖IPFC的一侧。另一个方向仍由ADC1 AWD1双边窗口保护；不得将COMP1描述为完整双向过流保护。

### 6A.2 COMP1到HRTIM Fault 1

仅当CubeMX的Fault 1页面明确允许内部COMP1来源时，增加：

| HRTIM分组 | 字段 | 设置值 | 生成代码核对 |
| --- | --- | --- | --- |
| Fault Configuration > Fault Line 1 | Source | Internal | `HRTIM_FAULTSOURCE_INTERNAL` |
| Fault Line 1 | Polarity | Active Low | 与COMP1越过下限时输出Low匹配 |
| Fault Line 1 | Filter | None | 内部比较器不加数字滤波 |
| Fault Line 1 | Blanking | None | 首次测试不屏蔽开关沿 |
| Fault Line 1 | Lock | Read/Write | 调试期可修改 |
| Timer A/B Configuration | Fault Enable | ~~仅Fault 1~~ **【修改为】** Fault 1 + Fault 3 |
| TA1/TA2/TB1/TB2 | Fault Level | Inactive | 保持不变 |
| HRTIM Interrupt | Fault 1 interrupt | Enable | 仅记录和联合停机 |

必须通过以下测试证明它是无CPU路径：暂停CPU后缓慢改变PA1输入跨过DAC阈值，TA1/TA2/TB1/TB2仍立即变为Inactive。若只能进入COMP回调而PWM没有硬件关断，则把本项降级为COMP IRQ软件Trip，不得登记为HRTIM硬件Fault。

### 6A.3 COMP3：VBUS过压增强

PC1同时具备`ADC1_IN7`和`COMP3_INP`能力：

| CubeMX位置 | 字段 | 设置值 | 说明 |
| --- | --- | --- | --- |
| Analog > COMP3 | Non Inverting Input | IO2 / PC1 | `COMP_INPUT_PLUS_IO2` |
| COMP3 | Inverting Input | DAC3 Channel 1 | `COMP_INPUT_MINUS_DAC3_CH1` |
| COMP3 | Output Polarity | Non Inverted | VBUS超过阈值时输出High |
| COMP3 | Hysteresis | 10 mV | 抑制阈值附近抖动 |
| COMP3 | Blanking Source | None | 不遮蔽真实过压 |
| COMP3 | Trigger Mode | Interrupt on Rising Edge | COMP3 IRQ执行统一Trip |
| NVIC | COMP1/2/3 interrupt | Enable，Priority 0 | 只做关断和故障锁存 |

DAC3 Channel 1同样选择内部连接、无外部引脚、Software/None Trigger。阈值为：

```text
DAC3_CH1_CODE = round(Vadc_at_vbus_overvoltage / VDDA x 4095)
```

~~PB10数字FLT3取消后，可把COMP3尝试路由为HRTIM内部Fault 3。~~ **【修改为】** PB10继续占用Fault 3的Digital Input来源，同一Fault Line 3不能再改为Internal。COMP3通过`COMP1_2_3_IRQn`执行统一软件Trip；该路径比10 kHz ADC采样快，但仍依赖CPU和采样前端。

### 6A.4 COMP启用验收

1. 只启用一个COMP，确认ADC1对应通道的零点、增益和噪声没有明显变化；
2. 使用受限0~3.3 V信号跨越DAC阈值，核对COMP输出极性；
3. COMP1只有通过CPU暂停测试后才可登记为L1硬件保护；
4. ~~COMP3可通过内部Fault 3关断。~~ **【修改为】** COMP3 IRQ必须先关闭四路HRTIM输出并锁存故障；禁止在回调中发送USART/OLED；
5. 验证无误触发后才评估增加Hysteresis或窄Blanking，不能用宽Blanking掩盖真实过流；
6. COMP保护仍经过电流/电压采样运放和RC，不等同于UCC21520 DESAT/OCP。

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

**【新增：当前工程核对】** IOC外设列表中IWDG的排序位置不代表它一定会在该位置自动调用。当前`main.c`没有在CubeMX自动初始化区调用`MX_IWDG_Init()`，而是在ADC校准、DMA启动、HRTIM采样时基和`PFC_AppInit()`完成后于USER CODE区手动调用，实际顺序符合要求，继续保持。

若SYS Debug页面提供IWDG Freeze选项，调试构建启用；若没有，在`USER CODE`区调用`__HAL_DBGMCU_FREEZE_IWDG()`。量产构建不要依赖调试冻结。

~~每100 ms检查控制心跳、ADC更新、状态机超时、PB10 GPIO电平和PE0状态。~~ **【修改为】** 每100 ms检查ADC1/2 DMA序列、控制心跳、AWD/COMP状态、HRTIM Fault标志和状态机超时；全部正常才刷新IWDG。PE0、PE5不配置；PB10使用HRTIM复用功能，不在软件中作为普通GPIO轮询。

**【新增】** PD0只改变启停请求，不是系统健康信号。按键松开、按下、损坏或一直保持某一电平都不应直接决定是否刷新IWDG；IWDG仍只监督采样、控制心跳、Fault和状态机是否正常。

当前裸板程序在故障时停止HRTIM和ADC，随后因心跳停止而不再喂狗；IWDG复位后程序又可能按开环启动流程重新开放MCU PWM引脚。~~PE0保持低时仍属于无功率试波，PE0状态可参与判断；只检查软件/内部Fault。~~ **【修改为】** 正式版本复位后必须读取并记录`IWDGRSTF`，默认保持HRTIM输出关闭。只有人工启动、自检通过、采样正常且软件故障与HRTIM Fault均恢复后才允许重新开放PWM；任何故障都不得自动恢复。

## 9. NVIC汇总

| CubeMX中断 | 抢占优先级 | 子优先级 | 用途 |
| --- | ---: | ---: | --- |
| HRTIM1 fault global interrupt | ~~基础档Disable~~ **【修改为】** 0 | 0 | ~~记录FLT3并拉低PE0~~ **【修改为】** 记录数字FLT3和可选内部FLT1，再次停止A/B输出并锁存故障 |
| COMP1/2/3 global interrupt（增强档） | 0 | 0 | COMP3母线过压和COMP软件后备Trip |
| ADC1 and ADC2 global interrupt | 1 | 0 | **【修改】** AWD1/AWD2越界及ADC Overrun错误处理 |
| ADC2 DMA通道全局中断 | 1 | 0 | VAC规则序列完成标志 |
| ADC1 DMA通道全局中断 | 2 | 0 | 唯一10 kHz快速控制入口 |
| EXTI line[9:5] interrupt | ~~3~~ **【修改为】** Disable | ~~PE5诊断故障~~ **【修改为】** PE5不配置，关闭该中断（若其他引脚使用EXTI9_5则另行评估） |
| **【新增】EXTI line 0 interrupt** | Disable | - | PD0使用1 ms轮询消抖，不启用EXTI0 |
| SysTick | 15 | 0 | 只更新时间标志 |

~~基础档没有FLT3。~~ **【修改为】** 数字FLT3和验证通过的COMP1/内部FLT1可由HRTIM硬件关断；但PB10板外未接信号时，FLT3只是保留能力而非当前实际保护来源。ADC AWD和COMP3 IRQ依赖CPU，对应ISR必须先停止HRTIM输出，再做故障记录。

当前DMA中断分别为`DMA1_Channel1_IRQn`和`DMA1_Channel2_IRQn`。后续联合工程若由CubeMX重新分配通道，必须同步修改中断映射。~~`ADC1_2_IRQn`只处理ADC错误。~~ **【修改为】** `ADC1_2_IRQn`处理AWD和ADC错误，但不得在该中断路径中重复执行采样快照或控制算法。

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
| ~~Fault Line 3基础档关闭~~ **【修改为】保留数字FLT3** | `HAL_HRTIM_FaultConfig()`使用`HRTIM_FAULTSOURCE_DIGITALINPUT`，并启用Fault 3 |
| ADC1规则组Rank 1/2 | 两次`HAL_ADC_ConfigChannel()`，分别为IN2和IN7 |
| ADC2规则组Rank 1 | 一次`HAL_ADC_ConfigChannel()`，通道为IN6 |
| ADC1/2规则组外部触发 | `ADC_EXTERNALTRIG_HRTIM_TRG1`和上升沿触发 |
| ADC1/2循环DMA | 两个独立DMA句柄、`DMA_CIRCULAR`和Half Word数据宽度 |
| ADC12异步/4 | `ADC_CLOCK_ASYNC_DIV4` |
| ~~ADC错误中断~~ **【修改为】ADC看门狗和错误中断** | `ADC1_2_IRQn`已启用；ADC1生成AWD1/IPFC与AWD2/VBUS，ADC2生成AWD1/VAC的`HAL_ADC_AnalogWDGConfig()` |
| **【新增】PD0人工启停按键** | `main.h`生成`PFC_START_KEY`宏；`gpio.c`为`GPIO_MODE_INPUT + GPIO_PULLUP`，不出现`EXTI0_IRQn` |
| 可选COMP1 | `HAL_COMP_Init()`，PA1/IO1、DAC1_CH1、10 mV Hysteresis、No Blanking |
| 可选HRTIM Fault 1 | Source Internal，Timer A/B Fault Enable包含Fault 1 + Fault 3 |
| 可选COMP3 | PC1/IO2、DAC3_CH1、Rising Edge IRQ；FLT3已由PB10 Digital Input占用 |

若CubeMX生成结果与表中不一致，先回到`.ioc`修正，避免直接编辑生成区掩盖配置错误。

还必须核对：生成结果中不再出现本方案ADC通道对应的`HAL_ADCEx_InjectedConfigChannel()`；ADC1的`NbrOfConversion`为2，ADC2为1；两个ADC均通过`__HAL_LINKDMA()`链接各自DMA句柄；AWD1使用12位阈值，AWD2阈值按16码对齐。应用启动代码中的两个`HAL_ADC_Start_DMA()`必须早于HRTIM Counter Start。

**【新增：重新生成前的迁移顺序】** 当前IOC和生成代码仍保留PE0/`PFC_GATE_EN`及PE5/`PFC_NFAULT_DIAG`，而本指南要求二者退出。必须先在应用代码中移除PE0写入、Gate状态监督、PE5电平检查、PE5 EXTI回调和对应宏引用，再把PE0/PE5设为`Reset_State`并重新生成。否则`main.h`删除宏后，现有`main.c`和`pfc_app.c`会编译失败。重新生成后还必须确认三个`HAL_ADC_AnalogWDGConfig()`真正出现，不能只检查IOC页面复选框。

## 11. 配置顺序

1. ~~固定MCU、切换到G4 V1.6.2~~ **【修改为】** 核对当前已为G4 V1.6.2、`LastFirmware=false`和MDK-ARM；
2. 核对8 MHz HSE和170 MHz时钟树；
3. ~~PE0设为输出、PE5设为EXTI，PB10改为`Reset_State`。~~ **【修改为】** 先清理应用代码中的PE0/PE5引用，再将PE0、PE5设为`Reset_State`并删除User Label；PB10保持`HRTIM1_FLT3`，板外不接信号；
4. **【新增】** 将PD0设为`GPIO_Input`、`PFC_START_KEY`、`Pull-up`并锁定引脚；不启用EXTI0；
5. 配置HRTIM Master、Timer A/B、Compare、Output、ROM和Dead Time；
6. ~~关闭Fault Line 3和HRTIM Fault中断。~~ **【修改为】** 配置Fault Line 3数字输入及HRTIM Fault中断；
7. 配置Master CMP2和ADC Trigger 1；
8. 配置ADC1/ADC2规则组，二者均选择HRTIM1 ADC Trigger 1上升沿触发；
9. 分别添加ADC1和ADC2循环DMA，设置Half Word宽度和DMA中断优先级；
10. **【新增】** 配置ADC1 AWD1/IPFC、ADC1 AWD2/VBUS和ADC2 AWD1/VAC，启用`ADC1_2_IRQn`；
11. 配置USART2和软件I2C OLED GPIO；
12. 配置IWDG和调试冻结；
13. **【可选增强】** 逐个配置COMP1/DAC1、Fault 1以及COMP3/DAC3，不一次性启用；
14. 检查Pinout冲突、Clock Configuration、DMA和NVIC；
15. 生成代码并按第10节逐项核对；
16. ~~取消FLT3故障注入。~~ **【修改为】** 完成无功率、按键、AWD、FLT3、DMA/ADC错误、CSS和可选COMP故障注入后，才进入隔离限流的低压功率测试；FLT3可在无功率条件下临时把PB10测试点短接到GND验证，测试后恢复板外不接状态。

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
- [ ] ~~PB10改为`Reset_State`，不生成FLT3初始化。~~ **【修改为】** PB10为Fault Line 3 Digital Input、Active Low、Filter None，板外不接信号。
- [ ] ~~基础档Timer A/B不包含Fault 3。~~ **【修改为】** Timer A/B均启用Fault 3，四路Fault Level为Inactive。
- [ ] ADC1规则组Rank 1为PA1/IPFC，Rank 2为PC1/VBUS，均为24.5 cycles。
- [ ] ADC2规则组Rank 1为PC0/VAC，采样时间为24.5 cycles。
- [ ] ADC1和ADC2规则组均使用HRTIM1 ADC Trigger 1上升沿触发。
- [ ] ADC1转换数为2且Scan Enable；ADC2转换数为1。
- [ ] 两个ADC均为Circular DMA、Half Word、Memory Increment Enable。
- [ ] **【新增】** ADC1 AWD1监视ADC1_IN2/IPFC，Single Regular、IT Enable、Filtering None。
- [ ] **【新增】** ADC1 AWD2监视ADC1_IN7/VBUS，阈值按16个ADC码对齐。
- [ ] **【新增】** ADC2 AWD1监视ADC2_IN6/VAC；VAC丢失不使用单点Low Threshold判断。
- [ ] ADC1_2共享中断开启且优先级1，处理AWD和ADC错误，不运行控制算法。
- [ ] DMA1_Channel2/ADC2中断优先级1，DMA1_Channel1/ADC1中断优先级2。
- [ ] ~~Fault 3数字输入关闭。~~ **【修改为】** Master、Timer A/B、ADC Trigger 1、Dead Time和Fault 3参数均保持；仅PB10板外不接信号。
- [ ] USART2为PD5/PD6、460800、8N1、无DMA和NVIC。
- [ ] OLED软件I2C为PA15/SCL推挽输出和PB7/SDA开漏输出，不占用控制中断。
- [ ] ~~PE5配置下降沿EXTI或保持普通输入。~~ **【修改为】** PE5为`Reset_State`、无User Label，EXTI9_5 Disable。
- [ ] ~~PE0配置为初始Low的预留输出。~~ **【修改为】** PE0为`Reset_State`、无User Label，生成代码不读写PE0。
- [ ] **【新增】** PD0为`GPIO_Input`，User Label为`PFC_START_KEY`，Pull-up启用并保持Locked；不选择`GPIO_EXTI0`，NVIC中不启用`EXTI line 0 interrupt`。
- [ ] **【新增】** 重新生成后的`gpio.c`使用`GPIO_MODE_INPUT`和`GPIO_PULLUP`初始化PD0，且不包含PD0的`EXTI0_IRQn`配置或EXTI回调入口。
- [ ] **【新增】** 重新生成后的ADC初始化中实际出现三次对应配置：ADC1 AWD1/IPFC、ADC1 AWD2/VBUS和ADC2 AWD1/VAC的`HAL_ADC_AnalogWDGConfig()`；ADC1 AWD2绑定`ADC_CHANNEL_7`。
- [ ] **【新增】** 当前ADC1 AWD1 `4095/0`、ADC1 AWD2 `4080/0`只作为无功率调试窗口；~~ADC2 AWD1为默认宽窗口~~ **【修改为】** 当前ADC2 AWD1实际为错误的`0/0`，应先改为无功率调试用`4095/0`并重新生成；带功率前再为三者填写标定后的实际阈值。
- [ ] ~~COMP全部保持关闭。~~ **【修改为】** 基础档关闭COMP；增强档只允许逐个启用COMP1/COMP3及对应DAC，其他COMP保持关闭。
- [ ] ADC3/4/5、HRTIM C/D/E/F和普通TIM保持关闭。
- [ ] Pinout无黄色冲突。

### 12.2 无功率上板检查

1. ~~复位和初始化期间PE0始终为Low，同时确认HRTIM四路输出关闭。~~ **【修改为】** 复位和初始化期间直接确认HRTIM四路输出关闭，软件不访问PE0；
2. PWM实测为10 kHz，Master和A/B物理周期均为100 us；
3. A/B桥臂互补，死区初值约500 ns，无上下管重叠；
4. 25%、50%、75%测试值的实测占空方向与比较值公式一致；
5. ADC Trigger和ADC1 DMA完成回调均为10 kHz，每周期只执行一次控制；
6. ADC1缓冲区顺序始终为IPFC、VBUS，ADC2缓冲区为VAC；
7. IPFC和VAC属于同一PWM周期，连续运行时无跨周期混用；
8. VBUS以10 kHz采样，但母线外环仍严格按1 kHz执行；
9. ADC采样位置远离实际开关沿；
10. ~~人为停止任一路DMA后，软件检测失步、关闭HRTIM四路输出、保持PE0为Low并锁存故障。~~ **【修改为】** 人为停止任一路DMA后，软件检测失步、关闭HRTIM四路输出并锁存故障；
11. ~~PB10不参与测试。~~ **【修改为】** 无功率条件下临时将PB10测试点拉低，确认不依赖CPU立即关闭四路PWM；正式运行时板外不接；
12. ~~PB10无需验证恢复行为。~~ **【修改为】** PB10恢复为高电平后PWM不自动恢复；AWD、DMA、ADC、CSS或内部Fault恢复后同样不自动恢复；
13. 停止控制心跳后IWDG在实测LSI误差范围内复位；
14. ~~IWDG复位后PE0和PWM仍保持关闭。~~ **【修改为】** IWDG复位后PWM保持关闭，软件不访问PE0；未收到人工启动命令时不重新开放输出；
15. 逐个越过IPFC、VBUS和VAC的AWD阈值，确认只触发对应故障且四路PWM停止；
16. ADC1 AWD2阈值按16码变化，实测保护边界与8位有效精度一致；
17. 可选COMP1只有在CPU暂停时仍能通过FLT1关闭A/B，才能登记为无CPU快速保护；
18. ~~COMP3可使用内部FLT3。~~ **【修改为】** COMP3越过母线阈值时经COMP IRQ关闭PWM；PB10继续占用数字FLT3，不能同时把Fault Line 3改为Internal。
19. **【新增】** 万用表或Keil Watch确认PD0松开为High、按下为Low，按键接法为常开触点接GND；
20. **【新增】** 小于30 ms的触点抖动不产生启停事件；按下并保持50~1000 ms后释放，只产生一次短按事件；
21. **【新增】** `PFC_READY`短按启动，`PFC_OPEN_LOOP_RAMP`或`PFC_OPEN_LOOP_RUN`短按停机；超过1 s的长按不执行动作，`PFC_FAULT_LATCH`中按键不能清除故障；
22. **【新增】** 完成按键释放解锁代码后，按住PD0复位并保持至`PFC_READY`，随后释放不得启动；必须再次完成一次新的“按下-释放”周期才允许启动。

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
- **【新增】** PD0只用于正常人工启停，采用1 ms轮询且不启用EXTI0，不能替代急停、过流、过压或HRTIM Fault保护，也不参与IWDG健康判断。
- **【新增】** 当前软件尚缺“上电后先稳定释放再解锁”的按键启动许可。完成该逻辑并通过“按住按键复位后释放不启动”的测试前，禁止将PD0用于带功率启动。
- ~~PB10/FLT3取消。~~ **【修改为】** PB10/FLT3配置保留，但板外不接故障源，不能计入实际OCP/DESAT保护；ADC模拟看门狗是基础档必选保护，COMP1/内部FLT1是建议增强项。现有驱动板没有DESAT/OCP输出，这是无法由CubeMX补齐的安全边界。
- 500 ns死区、24.5 cycles采样时间和Master CMP2采样位置都必须通过实际硬件测量修正。
- 当前IOC只覆盖单相PWM整流器，不等于题目要求1~5的完整CubeMX工程。~~三相逆变阶段仍需启用独立Gate Enable。~~ **【修改为】** 三相逆变阶段仍需启用HRTIM C/D/E和三相电压/电流采样；现有板卡的PE1同样未接驱动器，不能作为独立Gate Enable。
- 当前自由运行50 Hz SPWM只用于裸板观察。接入36 V交流源前必须改为VAC同步调制，并验证VAC/IPFC极性、输入丢失停机和软件限值。
- 带功率前必须通过AWD和软件共同实现IPFC过流、VBUS过压/运行态欠压、VAC严重越界/输入丢失、ADC饱和/过期和状态超时保护；ADC码值大于4095的检查不能代替这些物理阈值。
- 要求6的负载调整率和要求7的31~41 V输入调整率不属于本阶段目标，但硬件耐压、电流量程和保护范围不应因此只覆盖单一理想工作点。
