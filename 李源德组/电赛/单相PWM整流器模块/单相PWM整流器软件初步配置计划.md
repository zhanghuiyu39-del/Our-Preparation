# STM32G474 单相 PWM 整流器软件初步配置计划

## 1. 目标与适用范围

本计划面向最终仿真模型 [`x2026A2022b.slx`](../开源资料（matlab仿真及PCB）/达尔闻/x2026A2022b.slx) 中的单相 PWM 整流器部分，并以当前实际工程 [`02-1HRTIM_ADC_USART.ioc`](02-1各外设初步配置/HRTIM+ADC+USART/02-1HRTIM_ADC_USART.ioc) 及其生成代码作为外设配置事实来源。

目标是在 `STM32G474VCT6` 上按以下顺序建立可验证的软件链路：

1. 安全启动、硬件 Fault 和统一软件关断；
2. HRTIM 同步触发的双 ADC 规则组循环 DMA；
3. `VAC`、`IPFC`、`VBUS` 标定和 VOFA 观测；
4. HRTIM A/B 无功率及低压开环；
5. 10 kHz PR 电流内环；
6. 1 kHz母线电压PI外环；
7. DMA、ADC、时钟、驱动器和IWDG故障注入。

本阶段不实现三相逆变器，不启用COMP，不引入RTOS、SOGI-PLL、Clarke/Park、SVPWM、UART DMA或动态内存。所有应用代码必须位于CubeMX的`USER CODE BEGIN/END`区域或独立用户模块中。

> 软件过流只能覆盖控制周期级异常，不能替代栅极驱动器DESAT/OCP。若驱动器没有独立短路保护，不允许进行带功率测试。

## 2. 仿真模型与固件对应关系

| 仿真模块 | 固件对应功能 | 执行频率 |
| --- | --- | ---: |
| `Vac` | VAC采样、极性检查、归一化参考和电压前馈 | 10 kHz |
| `Iac` | IPFC采样和PR反馈 | 10 kHz |
| `Digital PR` | 交流电流准PR内环 | 10 kHz |
| `Udc` / `DCVFB` | VBUS反馈 | 10 kHz采样，1 kHz使用 |
| `PID Controller` | 母线电压PI外环 | 1 kHz |
| `倍频SPWM调制器` | HRTIM A/B单相全桥差分调制 | 10 kHz |

仿真求解步长不能直接作为MCU控制周期。`PR_Init()`中的`sample_frequency_hz`必须等于`PR_Calc()`的真实调用频率，即首版固定为10 kHz。

当前HRTIM只在Master Compare 2产生一次ADC Trigger，因此中心对齐PWM每100 us只执行一次快速控制。不得再从HRTIM更新中断、ADC2回调或DMA Half Transfer回调重复运行PR。

## 3. 当前工程基线

### 3.1 已经配置的资源

| 模块 | 当前实际配置 |
| --- | --- |
| MCU | STM32G474VCT6，LQFP100，设备宏`STM32G474xx` |
| 时钟 | 8 MHz HSE，SYSCLK/HCLK/HRTIM1均为170 MHz |
| ADC时钟 | ADC12 Kernel为SYSCLK，异步`/4`，实际42.5 MHz |
| HRTIM Master | MUL2，Period=34000，CMP2=17000，周期10 kHz |
| HRTIM Timer A/B | MUL4，Up-Down，Period=34000，中心对齐10 kHz |
| PWM输出 | PA8/PA9为TA1/TA2，PA10/PA11为TB1/TB2 |
| 死区 | DIV1，Rising/Falling Value=85，初值约500 ns |
| ADC Trigger | HRTIM ADC Trigger 1，Master CMP2，Postscaler=0 |
| ADC1规则组 | Rank 1=PA1/IPFC，Rank 2=PC1/VBUS，24.5 cycles |
| ADC1 DMA | DMA1_Channel1，Circular，Half Word，优先级2 |
| ADC2规则组 | Rank 1=PC0/VAC，24.5 cycles |
| ADC2 DMA | DMA1_Channel2，Circular，Half Word，优先级1 |
| Gate Enable | PE0/PFC_GATE_EN，推挽输出，初始化为低 |
| 诊断故障 | PE5/PFC_NFAULT_DIAG，下降沿EXTI，优先级3 |
| 总故障输入 | PB10/HRTIM1_FLT3，低有效引脚复用 |
| USART2 | PD5/PD6，460800，8N1，无UART DMA和UART中断 |
| IWDG | LSI，Prescaler=64，Reload=249，Window=4095，名义500 ms |
| COMP | 全部关闭 |

ADC1和ADC2均使用`ADC_EXTERNALTRIG_HRTIM_TRG1`，不是注入组。ADC1的完整两通道序列约1.74 us，ADC2单通道会更早完成，因此使用ADC2较高DMA中断优先级发布VAC完成序列，再由ADC1 DMA完成回调统一运行控制。

### 3.2 带功率前必须消除的阻断项

以下问题来自当前`.ioc`和生成代码检查。未处理前只能进行配置审查和无功率调试：

1. 当前工程记录`STM32Cube FW_G4 V1.6.3`且`ProjectManager.LastFirmware=true`。应固定到项目要求的V1.6.2，并关闭自动采用最新版；重新生成后再编译核对。
2. PB10虽然已映射为`HRTIM1_FLT3`，生成的`hrtim.c`仍出现`pFaultCfg.Source = HRTIM_FAULTSOURCE_INTERNAL`。必须在CubeMX中把Fault 3来源改为Digital Input，并确认生成结果为`HRTIM_FAULTSOURCE_DIGITALINPUT`。
3. 当前代码调用`HAL_HRTIM_FaultBlankingConfigAndEnable()`。首次Fault验证不应引入未经测量的blanking窗口，应关闭该功能，避免真实驱动故障被遮蔽。
4. Timer A与Timer B的Fault/Burst Roll-over配置不一致。应按实际输出设计统一配置，随后分别验证A/B桥臂在Fault发生和恢复时均保持无效态。
5. 当前`MX_IWDG_Init()`位于ADC初始化之前。应在CubeMX函数调用顺序中把它移到ADC1、ADC2和USART2之后，作为最后一个外设初始化函数。
6. 当前`Error_Handler()`、NMI和HardFault只进入死循环，没有主动拉低PE0。后续写应用代码时，必须先接入不会阻塞的统一安全关断入口。
7. 当前NMI没有CSS安全处理。启用HSE CSS后，NMI必须先关闭Gate Enable和HRTIM输出，再记录或等待IWDG复位。

这些问题必须通过修改`.ioc`后重新生成解决，不能通过长期手改CubeMX生成区掩盖。

## 4. 可复用模块与应用文件

复用 [`easy_port`](../单相逆变器PR控制与VOFA模块/easy_port/README.md) 中已经核对过的简化模块：

```text
../单相逆变器PR控制与VOFA模块/easy_port/Core/Inc/pr.h
../单相逆变器PR控制与VOFA模块/easy_port/Core/Src/pr.c
../单相逆变器PR控制与VOFA模块/easy_port/Core/Inc/vofa.h
../单相逆变器PR控制与VOFA模块/easy_port/Core/Src/vofa.c
```

- `PR_Init/PR_Reset/PR_Calc`保持纯算法接口，不访问HAL和硬件。
- `VOFA_Send()`保持阻塞式发送，只允许由主循环单点调用。
- 不复制F1/F4工程的HAL、启动文件、链接脚本或外设初始化代码。
- `codex_hal_test`只用于理解用户已有的HAL编码习惯，不作为本工程配置来源。

目标工程保留CubeMX生成文件，新增最少量应用模块：

```text
Core/Inc/pfc_params.h    标定比例、保护阈值、时序和控制参数
Core/Inc/pfc_app.h       公共类型、状态机、回调桥接和故障接口
Core/Src/pfc_app.c       DMA快照、测量换算、控制、PWM更新和监督
Core/Inc/pr.h            复用的PR接口
Core/Src/pr.c            复用的PR实现
Core/Inc/vofa.h          复用的VOFA接口
Core/Src/vofa.c          复用的VOFA实现
```

首版不再拆分BSP、服务层或消息队列。ADC、DMA、HRTIM、Gate Enable和故障处理的应用逻辑集中在`pfc_app.c`，但不得修改CubeMX生成的外设初始化职责。

## 5. 应用接口与数据所有权

### 5.1 公共类型

计划使用以下数据类型，实际字段可增加调试量，但不得改变数据所有权：

```c
typedef enum {
    PFC_SAFE = 0,
    PFC_SELF_TEST,
    PFC_ADC_CHECK,
    PFC_PWM_OPEN_LOOP,
    PFC_CURRENT_LOOP,
    PFC_VBUS_LOOP,
    PFC_FAULT_LATCH
} PFC_State;

typedef uint32_t PFC_FaultMask;

typedef struct {
    uint16_t ipfc_raw;
    uint16_t vbus_raw;
    uint16_t vac_raw;
    float ipfc_a;
    float vbus_v;
    float vac_v;
    uint32_t sequence;
} PFC_Measurement;

typedef struct {
    /* ADC比例、偏置、保护阈值、PWM频率、控制参数和限幅。 */
} PFC_Params;
```

`PFC_FaultMask`至少包含：参数错误、ADC校准失败、ADC同步丢失、ADC/DMA错误、样本过期、IPFC过流、VBUS欠压/过压、VAC丢失、驱动器Fault、HRTIM Fault、CSS故障、状态超时和IWDG复位记录。

### 5.2 公共接口

```c
void PFC_AppInit(void);
HAL_StatusTypeDef PFC_StartSampling(void);
void PFC_Adc2Complete(void);
void PFC_Adc1Complete(void);
void PFC_Tick1ms(void);
void PFC_Background(void);
void PFC_Trip(PFC_FaultMask fault_bits);
```

- `PFC_AppInit()`读取复位原因、验证参数并初始化状态、PI、PR和诊断计数。
- `PFC_StartSampling()`执行ADC校准、两路DMA启动和HRTIM采样计数器启动，但不使能PWM输出或PE0。
- `PFC_Adc2Complete()`只发布VAC完成序列，不运行控制算法。
- `PFC_Adc1Complete()`建立完整样本快照，并作为唯一10 kHz控制入口。
- `PFC_Tick1ms()`执行状态机、母线PI、慢速保护和超时检查。
- `PFC_Background()`执行VOFA、100 ms安全监督和IWDG条件刷新。
- `PFC_Trip()`必须可从ISR调用：先拉低PE0，再关闭HRTIM输出，锁存故障并复位PI/PR；禁止等待、延时、格式化和串口发送。

GPIO操作必须使用CubeMX生成的`PFC_GATE_EN_Pin`和`PFC_GATE_EN_GPIO_Port`等宏，不在应用代码中重复写死PE0、PE5或PB10。

### 5.3 DMA缓冲区和快照

DMA目标数组只由DMA写入：

```c
static volatile uint16_t adc1_dma[2];
static volatile uint16_t adc2_dma[1];

static volatile uint32_t adc2_sequence;
static uint32_t adc2_sequence_consumed;
static volatile uint32_t control_heartbeat;
static volatile PFC_Measurement measurement_latest;
```

固定映射为：

```text
adc1_dma[0] = IPFC，ADC1 Regular Rank 1
adc1_dma[1] = VBUS，ADC1 Regular Rank 2
adc2_dma[0] = VAC， ADC2 Regular Rank 1
```

ADC1完成回调必须先确认`adc2_sequence != adc2_sequence_consumed`，再把三个原始值复制到局部变量，完成物理量换算后一次性发布`measurement_latest`和新的`sequence`。主循环使用“读取序列号、复制结构体、再次读取序列号”的方式取得一致快照；两次序列号不同则重新读取，不直接访问DMA数组。

STM32G474为Cortex-M4F，没有D-Cache，不需要Cache Clean/Invalidate，但`volatile`不能保证整个结构体原子更新，仍必须使用序列号校验。

## 6. HAL回调、DMA和启动顺序

### 6.1 HAL回调映射

| HAL回调 | 应用处理 |
| --- | --- |
| `HAL_ADC_ConvCpltCallback(ADC2)` | 调用`PFC_Adc2Complete()`，递增VAC序列 |
| `HAL_ADC_ConvCpltCallback(ADC1)` | 调用`PFC_Adc1Complete()`，运行一次快速控制 |
| `HAL_ADC_ConvHalfCpltCallback()` | 不使用，DMA启动后关闭HT中断 |
| `HAL_ADC_ErrorCallback()` | 锁存ADC/DMA故障并调用`PFC_Trip()` |
| `HAL_GPIO_EXTI_Callback(PE5)` | 记录驱动诊断故障并调用`PFC_Trip()` |
| `HAL_HRTIM_Fault3Callback()` | 记录FLT3并调用`PFC_Trip()`完成软件收尾 |
| NMI/CSS | 立即安全关断，记录CSS故障，等待IWDG复位 |
| HardFault/Error_Handler | 立即安全关断，不尝试通信或自动恢复 |

DMA1_Channel2/ADC2优先级为1，DMA1_Channel1/ADC1优先级为2。ADC2单通道转换先完成并抢占ADC1回调，保证ADC1控制入口看到本周期VAC序列。

`HAL_ADC_Start_DMA()`会使能Half Transfer中断。两路启动成功后立即执行：

```c
__HAL_DMA_DISABLE_IT(hadc1.DMA_Handle, DMA_IT_HT);
__HAL_DMA_DISABLE_IT(hadc2.DMA_Handle, DMA_IT_HT);
```

### 6.2 固定启动顺序

CubeMX初始化函数顺序调整为：

```text
MX_GPIO_Init()
MX_DMA_Init()
MX_HRTIM1_Init()
MX_ADC1_Init()
MX_ADC2_Init()
MX_USART2_UART_Init()
MX_IWDG_Init()          最后一个外设初始化
```

进入`USER CODE BEGIN 2`后按以下顺序启动应用：

1. 保持PE0为低，保持TA1/TA2/TB1/TB2输出关闭；
2. 读取并保存RCC复位原因，随后清除复位标志；
3. 调用`PFC_AppInit()`，检查参数和Fault输入静态状态；
4. 分别校准ADC1和ADC2的单端模式；
5. 清零DMA数组、序列号、控制心跳和测量快照；
6. 先启动`HAL_ADC_Start_DMA(&hadc2, ..., 1)`；
7. 再启动`HAL_ADC_Start_DMA(&hadc1, ..., 2)`；
8. 两次返回均为`HAL_OK`后关闭两路DMA Half Transfer中断；
9. 确认HRTIM DLL Ready并写入安全的50%初始比较值；
10. 使用同一次掩码启动Master、Timer A和Timer B计数器，但不启动四路输出；
11. 在`PFC_ADC_CHECK`中观察至少100个连续采样周期；
12. 只有人工启动请求、Fault未激活和全部自检通过后，才启动PWM输出并最后拉高PE0。

任一ADC校准、DMA启动或HRTIM启动失败都调用`PFC_Trip()`，不得继续启动后续功率资源。

## 7. 快速控制和慢速任务

### 7.1 10 kHz快速控制数据流

ADC1 DMA完成回调固定执行：

```text
确认ADC2序列已更新
-> 复制IPFC/VBUS/VAC原始值
-> ADC码值转换为ipfc_a、vbus_v、vac_v
-> 检查饱和、过流、过压和样本有效性
-> 生成VAC同步电流参考
-> PR_Calc()计算电流修正量
-> 叠加电网电压前馈
-> 调制量限幅到+/-PFC_MODULATION_LIMIT
-> 换算Timer A/B Compare 1
-> 在预装载寄存器中同步更新A/B
-> 发布测量快照并更新control_heartbeat
```

快速回调禁止调用：

- `HAL_UART_Transmit()`或`VOFA_Send()`；
- `HAL_Delay()`、轮询等待或任何阻塞HAL接口；
- OLED、`sprintf()`、长窗口RMS和文件操作；
- `malloc/free`；
- Fault自动清除、Gate Enable自动重启或IWDG刷新。

使用DWT周期计数器或调试GPIO测量最坏执行时间。首版要求小于30 us，给DMA、Fault和后台中断保留余量。

### 7.2 电流参考、PR和前馈

首版不使用PLL，先由VAC测量形成同相参考：

```c
vac_norm = clamp(vac_v / VAC_PEAK_EST_V, -1.0f, 1.0f);
ipfc_ref_a = pfc_current_amp_cmd_a * vac_norm;
```

`PR_Calc()`误差方向固定为`reference - feedback`。VAC和IPFC极性必须在测量层修正，不能在PR内部隐藏反号。

电压前馈按实际全桥方向引入：

```text
m_ff  = PFC_VAC_FF_SIGN * vac_v / max(vbus_v, VBUS_FF_MIN_V)
m_cmd = clamp(m_ff + m_pr, -PFC_MODULATION_LIMIT,
                             +PFC_MODULATION_LIMIT)
```

`PFC_VAC_FF_SIGN`只能在阶段D的低压开环极性试验后固定。VBUS低于`VBUS_FF_MIN_V`时禁止计算前馈和进入电流闭环，避免除数过小。

差分调制映射保持：

```text
duty_A = 0.5 * (1 + m_cmd)
duty_B = 0.5 * (1 - m_cmd)
CMP_A  = Period * (1 - duty_A)
CMP_B  = Period * (1 - duty_B)
```

应用层只更新A/B两个主比较值，互补输出和死区由HRTIM生成，不分别自由控制四个开关。

### 7.3 1 kHz母线外环和后台任务

每10次ADC1快速完成事件设置一次1 ms任务标志。PI外环只在`PFC_VBUS_LOOP`状态执行：

```text
vbus_error = vbus_ref_ramp_v - vbus_v
pfc_current_amp_cmd_a = PI(vbus_error)
```

PI要求：

- 输出限制为`0~PFC_CURRENT_AMP_MAX_A`；
- 使用条件积分或积分反算实现抗饱和；
- `vbus_ref_ramp_v`缓慢上升，禁止参考阶跃；
- 达到电流、调制或母线限制时停止继续积分；
- 离开双环、停机或故障时同时复位PI和PR。

主循环任务：

| 周期 | 内容 |
| ---: | --- |
| 1 ms | 状态机、PI外环、VAC存在判断、慢速保护 |
| 10 ms | 从一致快照发送VAC/IPFC/VBUS到VOFA |
| 100 ms | 安全监督、故障统计和IWDG条件刷新 |

VOFA固定使用460800 baud和三个JustFloat通道。通信超时只丢弃本帧并计数，不应因调试串口断开而直接触发功率故障。

## 8. 状态机与安全监督

### 8.1 状态流程

```text
PFC_SAFE
-> PFC_SELF_TEST
-> PFC_ADC_CHECK
-> PFC_PWM_OPEN_LOOP
-> PFC_CURRENT_LOOP
-> PFC_VBUS_LOOP

任意状态发生故障 -> PFC_FAULT_LATCH
```

| 状态 | HRTIM计数器/ADC触发 | PWM输出 | PE0 | 控制器 |
| --- | --- | --- | --- | --- |
| `PFC_SAFE` | 停止 | 关闭 | 低 | PI/PR Reset |
| `PFC_SELF_TEST` | 停止 | 关闭 | 低 | 参数与Fault检查 |
| `PFC_ADC_CHECK` | 运行 | 关闭 | 低 | 只采样和VOFA |
| `PFC_PWM_OPEN_LOOP` | 运行 | 低调制度 | 人工允许后高 | PR/PI Reset |
| `PFC_CURRENT_LOOP` | 运行 | PR内环 | 高 | PR有效，PI关闭 |
| `PFC_VBUS_LOOP` | 运行 | 双环 | 高 | PR和PI有效 |
| `PFC_FAULT_LATCH` | 视诊断需要 | 强制关闭 | 低 | PI/PR Reset |

所有运行状态转换都必须由人工命令触发，并设置进入条件和超时。Fault信号恢复后不能自动退出`PFC_FAULT_LATCH`；人工清故障前必须再次确认PB10、PE5、VAC、VBUS和IPFC均处于安全范围。

### 8.2 统一故障动作

`PFC_Trip()`固定按以下顺序执行：

1. 使用CubeMX引脚宏立即拉低PE0；
2. 请求HRTIM四路输出进入无效态；
3. 原子置位故障位并切换到`PFC_FAULT_LATCH`；
4. 清零电流幅值、调制指令和软启动状态；
5. 调用`PR_Reset()`并清除PI积分；
6. 保留首个故障原因和必要的最后测量快照；
7. 退出，不发送串口、不延时、不自动清除硬件Fault。

外部PB10 Fault的PWM关断必须由HRTIM硬件先完成，`HAL_HRTIM_Fault3Callback()`只负责上述软件收尾。

### 8.3 状态相关的IWDG条件

IWDG每100 ms最多刷新一次，但监督条件必须随状态变化：

- `PFC_SAFE/PFC_SELF_TEST`：要求安全监督心跳正常、PE0为低、PWM输出关闭和状态未超时，不要求DMA心跳。
- `PFC_ADC_CHECK`：增加ADC1/ADC2 DMA序列持续更新和无ADC错误要求。
- `PFC_PWM_OPEN_LOOP/PFC_CURRENT_LOOP/PFC_VBUS_LOOP`：进一步要求快速控制心跳、HRTIM无Fault、测量新鲜、Gate状态与状态机一致。
- `PFC_FAULT_LATCH`：只在PE0和PWM确认关闭、状态机仍运行时继续喂狗，以保留故障信息；若安全关断状态不一致则停止喂狗。

不得在ADC、DMA、HRTIM或SysTick中断中直接刷新IWDG。控制ISR或监督任务停止时，应在实际LSI误差范围内约500 ms后复位。

## 9. 分阶段实施与验收

### 阶段0：配置阻断项

工作内容：固定G4 V1.6.2、修正FLT3数字输入、关闭未经验证的blanking、统一A/B Roll-over、调整IWDG初始化顺序并重新生成。

验收：生成代码出现`HRTIM_FAULTSOURCE_DIGITALINPUT`；不再启用Fault blanking；ADC、DMA、HRTIM数值保持本计划基线；Keil首次构建为0错误0警告。

### 阶段A：安全启动

工作内容：实现公共类型、参数校验、复位原因记录、`PFC_Trip()`及Error/NMI/HardFault安全关断。

验收：上电和复位期间PE0始终为低；没有显式人工启动时无PWM输出；异常入口首先关闭Gate；IWDG复位后仍保持安全态。

### 阶段B：双ADC规则组DMA与VOFA

工作内容：校准ADC，先启动ADC2 DMA再启动ADC1 DMA，关闭HT中断，启动HRTIM计数器但保持输出关闭，完成快照、标定和100 Hz VOFA。

验收：两个DMA完成频率均为10 kHz；ADC1回调每周期只运行一次；缓冲区Rank顺序正确；IPFC和VAC属于同一PWM周期；停止任一路DMA会锁存故障；VOFA不扰动采样周期。

### 阶段C：HRTIM无功率和低压固定调制

工作内容：验证10 kHz互补PWM、500 ns初始死区、25%/50%/75%比较值方向、A/B同步预装载和PB10故障关断。

验收：四路波形无上下管重叠；比较值只在周期边界生效；PB10拉低无需CPU即可关断；PB10恢复后输出不自动恢复。

### 阶段D：VAC同步低压开环

工作内容：在隔离限流低压条件下验证VAC极性、`PFC_VAC_FF_SIGN`、差分调制方向、VAC丢失停机和ADC Trigger安静窗口。

验收：`vac_norm`限制在`-1~1`；过零无错误翻转；采样点远离开关沿；VAC丢失进入故障锁存。

### 阶段E：PR电流内环

工作内容：从很小电流幅值开始，按10 kHz初始化PR，逐步验证反馈极性、前馈叠加、Kp/Kr、限幅和停机复位。

验收：IPFC跟踪参考并与VAC同相；无正反馈和持续过零振荡；调制不长期饱和；软件/硬件电流限值均有效；ISR最坏执行时间小于30 us。

### 阶段F：1 kHz母线PI外环

工作内容：实现参考斜坡、条件积分、电流幅值限制和状态切换。PI参数按1 kHz离散周期与实际物理单位重新整定，不照搬连续仿真参数。

验收：在隔离、限流且具备可靠预充的条件下，母线缓慢进入目标区间；负载变化后恢复；达到电流限幅时积分不继续增长；停机后PI/PR状态清零。

当前工程尚未配置预充控制。预充继电器、限流电阻、母线判据和放电路径未明确前，阶段F只能完成软件接口和低压验证，不能进入高压母线闭环测试。

### 阶段G：故障与IWDG

分别注入PB10 Fault、PE5故障、ADC软件过流、VBUS过压/欠压、VAC丢失、ADC饱和、DMA停止、ADC同步丢失、控制心跳停止、CSS故障和状态超时。

验收：所有故障均先关闭Gate和PWM再记录；故障不会自动恢复；停止监督任务后IWDG复位；复位原因可读取；复位后功率输出仍关闭。

## 10. 参数清单

带功率前必须填写并验证：

```text
ADC_VREF_V
ADC_FULL_SCALE
IPFC_ZERO_CODE
IPFC_A_PER_CODE
VAC_ZERO_CODE
VAC_V_PER_CODE
VBUS_V_PER_CODE
VAC_PEAK_EST_V
VAC_PRESENT_MIN_V
VAC_LOSS_TIMEOUT_MS
VBUS_REF_V
VBUS_FF_MIN_V
VBUS_UV_TRIP_V
VBUS_OV_TRIP_V
IPFC_SW_LIMIT_A
PFC_CURRENT_AMP_MAX_A
PFC_MODULATION_LIMIT
PFC_VAC_FF_SIGN
PFC_PWM_HZ
PFC_CONTROL_HZ
PFC_VBUS_LOOP_HZ
PFC_DEADTIME_NS
ADC_STALE_LIMIT_CYCLES
STATE_TIMEOUT_MS
```

参数缺失、为零、非有限值或超出合理范围时，`PFC_AppInit()`必须置位参数故障并停留在`PFC_SAFE`。预充相关参数在硬件链路确定后单独加入，当前不得伪造默认值。

## 11. 中断优先级与实时预算

| 路径 | 抢占优先级 | 职责 |
| --- | ---: | --- |
| HRTIM FLT3硬件 | 不依赖NVIC | 立即把A/B四路输出置为无效态 |
| HRTIM1 Fault ISR | 0 | 软件关断收尾和故障记录 |
| DMA1_Channel2 / ADC2 | 1 | 发布VAC完成序列 |
| DMA1_Channel1 / ADC1 | 2 | 唯一10 kHz快速控制入口 |
| EXTI9_5 / PE5 | 3 | 驱动器故障诊断和软件关断 |
| SysTick | 15 | 只更新时间和设置任务标志 |

两个DMA中断合计20 kHz。ADC2回调只做序列递增，ADC1回调最坏执行时间目标小于30 us；所有后台任务都不得关闭中断进行长时间拷贝或计算。

## 12. 首版完成标准与边界

满足以下条件才认为初步软件链路完成：

1. 当前配置阻断项全部消除，G4固件包和生成代码可重复生成；
2. 170 MHz系统/HRTIM和42.5 MHz ADC时钟核对正确；
3. PE0在复位、Fault、NMI、HardFault和Error_Handler中保持或立即回到低电平；
4. ADC1/ADC2循环DMA均为10 kHz，控制每周期只执行一次且无Half Transfer控制回调；
5. VAC、IPFC、VBUS比例、偏置、极性、Rank顺序和数据新鲜度正确；
6. VOFA稳定显示一致快照，且串口不在ISR中发送；
7. HRTIM A/B输出10 kHz互补PWM，死区和比较值方向实测正确；
8. PB10无需CPU即可关闭四路PWM，恢复后不会自动启动；
9. VAC同步低压开环和小电流PR内环稳定；
10. 快速ISR最坏执行时间小于30 us；
11. DMA、ADC、Fault、CSS和IWDG故障注入全部通过；
12. Keil构建为`0 Error(s), 0 Warning(s)`，生成HEX且链接空间不超过256 KB Flash。

明确边界：

- COMP、三相ADC3/4/5、HRTIM C/D/E/F和三相逆变器不进入本阶段；
- UART仍使用460800 baud阻塞发送，不增加DMA或UART中断；
- 软件限值不能替代DESAT/OCP、硬件急停和HRTIM Fault；
- 未配置可靠预充、限流和放电链路前，不执行高压母线闭环；
- 测试顺序固定为ADC/VOFA、无功率PWM、硬件Fault、低压开环、小电流内环，最后才评估母线外环。
