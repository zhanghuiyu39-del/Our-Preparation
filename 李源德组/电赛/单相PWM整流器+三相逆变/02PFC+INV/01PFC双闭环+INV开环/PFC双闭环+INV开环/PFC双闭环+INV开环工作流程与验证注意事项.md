# PFC双闭环 + INV开环联合工作流程与验证注意事项

## 1. 当前联合工程边界

本说明对应同目录的`01Final.ioc`、`Core/Src/main.c`和联合用户头文件。当前工程使用STM32G474VCT6、170 MHz HRTIM、10 kHz中心对齐PWM，不修改`.ioc`。本地当前`01Final.ioc`的SHA-256为：

```text
630EBF93CCF859F38662B0829E22FD1390106F91235F05AF360DE6F840BC88F6
```

联合固件的物理边界如下：

| 域 | 外设/引脚 | 固件职责 |
| --- | --- | --- |
| PFC | ADC1 IPFC/VBUS、ADC2 VAC、HRTIM A/B PA8~PA11 | 10 kHz PR电流内环、1 kHz PI母线外环 |
| INV | ADC3 IU/VUV、ADC4 IV/VVW、ADC5 IW/VWU、HRTIM C/D/E | 三相三线CBSVPWM开环 |
| 公共时基 | HRTIM Master CMP2、Trigger 1/2 | 同时触发五组规则ADC |
| 保护 | PB10/HRTIM1_FLT3 | 低有效硬件关闭A~E；当前外部保护源未接 |
| 按键 | PD0/START_KEY | 1 ms轮询、30 ms消抖、短按启停 |
| 调试 | OLED、USART2 460800 | 仅主循环读取快照和发送 |

当前只使用三相三线逆变：HRTIM F、PC6/PC7和第4桥臂保持关闭，负载中性点不得接到未启用桥臂。PB10未接真实DESAT/OCP时只能作为人工Fault测试点，不能替代独立硬件保护。

## 2. 唯一用户配置入口

日常只修改：

```text
Core/Inc/pfc_inv_user_config.h
```

`pfc_user_config.h`和`inv_user_config.h`是兼容包装，不再填写第二套参数。配置头中最常修改的字段：

| 宏 | 默认值 | 含义与修改后果 |
| --- | --- | --- |
| `PFC_INV_STAGE` | `PFC_INV_STAGE_RAW_ADC` | 选择标定、PFC-only、INV-only或联合阶段；编译期生效 |
| `PFC_INV_ACTIVE_PROFILE` | `PFC_INV_PROFILE_5V` | 选择5 V低压档或36 V赛题骨架 |
| `PFC_INV_PFC_CALIBRATION_CONFIRMED` | `0U` | PFC零点/比例/极性完成实测后才改为1 |
| `PFC_INV_INV_CALIBRATION_CONFIRMED` | `0U` | INV六通道完成实测后才改为1 |
| `PFC_INV_PWM_ENABLE` | `0U` | 1才允许状态机申请输出；未确认标定时即使填1也会编译拒绝 |
| `PFC_INV_INV_OUTPUT_FREQUENCY` | `60U` | 只允许30或60 Hz，修改后须重新观察相序和负载 |

推荐阶段值：

```c
/* 默认安全标定 */
#define PFC_INV_STAGE PFC_INV_STAGE_RAW_ADC
#define PFC_INV_PWM_ENABLE 0U

/* PFC低压闭环 */
#define PFC_INV_STAGE PFC_INV_STAGE_PFC_ONLY

/* INV低压开环 */
#define PFC_INV_STAGE PFC_INV_STAGE_INV_ONLY

/* PFC先建母线，再自动投入INV */
#define PFC_INV_STAGE PFC_INV_STAGE_JOINT_LOW_POWER
```

修改输入电压、母线目标、线电压目标、负载或采样板电阻后，必须同步重新填写比例、保护阈值、调制度和限流值；不能只改一个电压宏。

`PFC_INV_STAGE_INV_ONLY`虽然不开放PFC A/B，但C/D/E仍以ADC1的`VBUS`为调制分母。因此，只要`PFC_INV_PWM_ENABLE=1U`，INV-only同样要求PFC和INV两套标定确认位都为`1U`；编译器会拒绝漏填PFC母线标定的配置。确认INV标定后，程序会固定使用六个`OFFSET/SCALE/POLARITY`字段，不会在带功率过程中重新估计零点。

## 3. IOC映射与数据所有权

`01Final.ioc`的ADC/DMA Rank必须与下面顺序一致：

| ADC | Rank 1 | Rank 2 | DMA |
| --- | --- | --- | --- |
| ADC1 | IPFC | VBUS | DMA1 Channel 1 |
| ADC2 | VAC | 无 | DMA1 Channel 2 |
| ADC3 | IU | VUV | DMA2 Channel 1 |
| ADC4 | IV | VVW | DMA2 Channel 2 |
| ADC5 | IW | VWU | DMA2 Channel 3 |

HRTIM Master CMP2同时产生Trigger 1和Trigger 2。DMA原始数组只允许HAL写入：

```c
PFC_Adc1Dma[0]=IPFC, PFC_Adc1Dma[1]=VBUS, PFC_Adc2Dma[0]=VAC
INV_Adc3Dma[0]=IU,   INV_Adc3Dma[1]=VUV
INV_Adc4Dma[0]=IV,   INV_Adc4Dma[1]=VVW
INV_Adc5Dma[0]=IW,   INV_Adc5Dma[1]=VWU
```

OLED、VOFA和控制器都必须使用`PFC_Measure_GetSnapshot()`、`INV_Measure_GetSnapshot()`，不能直接读取正在由DMA改写的数组。

PFC快照采用双缓冲发布。ADC1 ISR先在工作副本中完成本周期全部字段，再把结构体写入非活动缓冲区，执行数据屏障，最后用一次索引翻转发布。DMA2的INV回调优先级高于DMA1的PFC回调，即使INV在PFC发布过程中抢占，也只能读取上一个已经完整发布的VBUS快照，不会读到只更新了一半的结构体。`volatile`只保证实际内存访问，本身不提供这种一致性；一致性来自双缓冲、发布索引和短临界区。

运行期模拟看门狗的实际覆盖关系如下：PFC的ADC1 AWD1监视IPFC、AWD2监视VBUS，ADC2 AWD1监视VAC；INV的ADC3/4/5各用AWD1监视Rank 1电流、AWD2监视Rank 2线电压。INV窗口由`PFC_INV_INV_ADC_RAIL_LOW_COUNT/HIGH_COUNT`给出，默认是16～4079码，只用于识别连续贴轨，不等价于完成采样比例后的安培/伏特过流阈值。RAW_ADC标定阶段不改窄窗口，避免用户故意施加标定量时被看门狗打断。

## 4. 联合启动流程

```text
HAL_Init/170 MHz时钟/GPIO
 -> 关闭A~E输出
 -> 初始化联合配置、PFC、INV、OLED
 -> ADC5、4、3、2、1单端校准
  -> 配置PFC运行期AWD
  -> 配置INV ADC3/4/5六路防贴轨AWD（标定模式保留IOC宽窗口）
 -> 启动ADC5、4、3、2、1循环DMA
 -> 关闭五路DMA Half Transfer中断
 -> 一次启动Master+A+B+C+D+E计数器
 -> 按当前阶段等待所需DMA序列稳定
 -> 启动IWDG
 -> 根据阶段停在标定/READY，或等待PD0
```

联合阶段按以下顺序投入：

```text
PD0短按
 -> 只开放PFC A/B，PR电流环从0开始
 -> 电流跟踪合格后切入PI母线环
 -> VBUS在目标容差内连续稳定500 ms
 -> 写入INV 50%初值并开放C/D/E
 -> INV 1 s软启动，调制度按实测VBUS计算
 -> JOINT_RUN
```

PFC和INV不能各自启动/停止Master。普通停机只关闭对应域输出并保留采样触发；Fault、CSS、HardFault等不可恢复异常关闭A~E全部输出并停止刷新IWDG。

DMA启动仍固定覆盖ADC1~5，但启动准入和故障仲裁按阶段处理：RAW_ADC、INV-only和联合阶段要求PFC与INV采样都更新；PFC-only只要求ADC1/2正常，未接逆变采样板造成的ADC3/4/5贴轨或诊断故障只保留记录，不得阻断PFC闭环。INV-only仍必须监督ADC1，因为实测VBUS直接参与INV调制度计算。

## 5. ADC标定流程

### 5.1 PFC标定

1. 断开功率母线和驱动输入，只给系统板、ADC采样板供电。
2. 设置`PFC_INV_STAGE_RAW_ADC`、两项确认位为0、`PFC_INV_PWM_ENABLE=0U`，重新编译下载。
3. 零输入运行至少1秒，记录PFC统计快照`ipfc_mean/min/max`、`vac_mean/min/max`、`vbus_mean/min/max`。
4. IPFC、VAC双极性零点填入`PFC_INV_PFC_5V_IPFC_ZERO_COUNT`和`...VAC_ZERO_COUNT`；VBUS单极性比例使用已知直流电压除以平均码计算。
5. 给VAC输入已知RMS值，记录峰值码幅：`V/count = 已知峰值电压 / ((max-min)/2)`。给输入电感已知电流，计算`A/count`。
6. 通过示波器确认正向输入电压对应软件`vac>0`、正向输入电流对应`ipfc>0`；接反只改对应极性为`-1`，不要在PR/PI中隐藏反号。
7. 重新编译，切换`PFC_INV_STAGE_PFC_ONLY`但保持PWM=0，观察工程量、RMS、频率、AWD窗口，确认无越窗后再把PFC确认位置1。

### 5.2 INV标定

1. 保持功率母线断开，采样板供电并运行RAW_ADC；记录`INV_Adc3/4/5Dma`六路中点和1秒变化范围。
2. 逐通道施加已知电流或线电压，计算：`物理量/count = 已知物理量 / (实测码-零点码)`。
3. 依据已知正向量对应的符号填写六路极性。线电压必须按`U-V、V-W、W-U`实际接线确认。
4. 将六个offset、scale、polarity写回联合用户头，切换`PFC_INV_STAGE_INV_ONLY`且PWM=0，确认六路工程量和`IU+IV+IW`、`VUV+VVW+VWU`趋势合理。`SCALE`必须为正数；已知正向量显示为负时只把对应`POLARITY`改为`-1`。确认位为`1U`后，运行代码直接采用这些固定零点，不能再把交流波形平均为零点。
5. 确认INV标定位置1后，先做MCU波形和驱动无母线测试，再允许低压功率输出。

标定期间ADC输入不得悬空。采样板的1V65REF是模拟偏置，不是MCU的VREF+；零输入双极性通道通常接近2048码，但最终以实测为准。

## 6. 分阶段验证

### P0：RAW_ADC无功率

- 五路DMA序列均约10 kHz递增，Half Transfer不进入业务回调。
- OLED/VOFA显示原始码和统计窗口，`pfc_inv_state=ADC_CALIBRATION`。
- A/B/C/D/E全部输出关闭；PD0无效。

### P1：PFC-only低压闭环

- 先使用隔离限流5 V RMS、50 Hz输入和30 ohm、至少5 W负载。
- 标定确认后把阶段设为`PFC_ONLY`，PWM=1；示波器先测PA8~PA11互补PWM和约500 ns死区。
- PD0短按后观察PR电流环，再观察PI母线参考从实测值以1 V/s爬升到9 V。
- 任何PR持续限幅、VBUS过压、VAC丢锁、ADC贴轨或DMA失步都必须锁存停机。
- 这里的ADC贴轨和DMA失步指活动的PFC域ADC1/2；未活动的INV域只记录诊断。切换到INV-only或联合阶段前，必须排除ADC3/4/5故障并重新完成对应准入检查。

### P2：INV-only低压开环

- 使用已确认PFC VBUS采样，INV开环不使用固定母线作为最终调制分母，而使用ADC1发布的实测VBUS。
- 先断开功率板，测PB12/PB13、PB14/PB15、PC8/PC9的10 kHz中心对齐互补波形。
- 驱动器仅接逻辑电源和隔离电源时，使用差分/隔离探头测Gate-Source；不得用普通接地探头测高侧栅极。
- 通过后使用5 V限流母线和三相对称阻性负载，观察30/60 Hz相序、线电压和电源电流。

### P3：联合低功率

1. 阶段设为`JOINT_LOW_POWER`，PFC和INV确认位均为1，PWM=1。
2. 上电后先确认PFC A/B未异常，INV C/D/E仍关闭。
3. PD0短按只启动PFC；母线进入目标容差并稳定500 ms后，C/D/E才自动开放。
4. INV软启动期间线电压幅值逐渐展开；停止PD0或任一故障时A~E一起关闭，不能自动恢复。

### P4：36 V赛题档

先完成P0~P3再切换`PFC_INV_ACTIVE_PROFILE=PFC_INV_PROFILE_36V`。重新标定全部PFC/INV通道，核对器件耐压、驱动隔离电源、母线电容、死区和独立OCP/DESAT；36 V宏中的比例和增益只是骨架，确认位必须保持0直到实测完成。

## 7. OLED、VOFA与Keil Watch

OLED每100 ms由主循环刷新：

```text
P:VAC原始码 I:IPFC原始码
B:VBUS原始码 U:IU原始码
S:联合状态 D:INV调制度千倍值
F:联合/PFC/INV故障 H:INV控制心跳
```

RAW_ADC下VOFA每10 ms交替两页：

- 页0：`CH0=0, IPFC mean/min/max, VAC mean/min/max, VBUS mean`
- 页1：`CH0=1, IU, IV, IW, VUV, VVW, VWU, INV fault`

运行阶段VOFA为：`VAC、IPFC、VBUS、VUV、U相正弦、INV调制度、联合状态、故障编码`。VOFA只用于趋势观察，不能代替示波器测量50/60 Hz波形、PF、效率或THD。

Keil Watch建议观察：

```c
PFC_Adc1Dma, PFC_Adc2Dma
INV_Adc3Dma, INV_Adc4Dma, INV_Adc5Dma
pfc_inv_state, pfc_inv_fault_bits
pfc_inv_svpwm.valid, pfc_inv_svpwm.limited
pfc_inv_svpwm.duty_u/v/w, pfc_inv_control_heartbeat
```

调试器暂停CPU会同时暂停10 kHz控制和IWDG刷新；带功率测试时禁止断点、单步和长时间暂停。

INV快速回调更新CBSVPWM结果时，联合层在短临界区内复制完整遥测快照，再交给OLED和VOFA。显示允许存在一个10 ms或100 ms刷新周期的延迟，但不会把不同控制周期的`duty_u/v/w`、调制度和限幅状态混在同一帧中。

## 8. 故障与禁止升压条件

| 现象 | 可能原因 | 处理 |
| --- | --- | --- |
| 五路序列停止/不一致 | DMA未启动、Rank或触发错误 | 断电，核对DMA长度、Trigger和回调 |
| 原始码连续贴轨 | 输入悬空、采样板过压或比例错误 | 立即断开功率，检查量程和地 |
| PFC电流反相 | IPFC极性或桥臂极性错误 | 只改配置极性，低压重新验证 |
| VBUS不上升 | VAC未锁定、PR限幅、负载过重 | 降低电流指令，先空载/轻载整定 |
| INV无法投入 | VBUS未稳定、标定未确认或PWM=0 | 先查联合状态和确认位，不要强制跳过 |
| `PFC_INV_FAULT_VBUS_STALE` | ADC1的VBUS快照停滞、欠压、过压或INV先于可用VBUS投入 | 检查ADC1 DMA、VBUS比例、母线电容和PFC状态；故障后必须复位，不能手动重开C/D/E |
| PWM重叠/桥臂发热 | 死区/驱动接线错误 | 立即断母线，测Gate-Source和上下管对应关系 |
| Fault后周期性重启 | IWDG未刷新，属于预期锁存保护 | 读取故障码，复位前排除原因 |

以下任一条件成立时禁止升高母线或接入大功率负载：标定确认位为0、没有独立OCP/DESAT、PB10保护源未接、ADC输入贴轨、PWM互补/死区未实测、采样比例未核对、普通接地探头接触高侧浮地、30 ohm负载功率等级不足。

## 9. 赛题第1~5项的现实边界

联合代码可以提供PFC母线闭环和三相INV开环的分阶段验证，但INV开环本身不能保证赛题要求的`32 V±0.1 V`、输出调节率、`PF≥0.98`、`效率≥95%`和`THD≤2%`。最终验收还需要：

1. 36 V RMS输入、60 Hz下完成PFC输入电流闭环和母线稳定；
2. 逆变输出电压闭环或等效补偿，不能只依赖开环调制度；
3. 独立功率保护、限流和热设计；
4. 用示波器/功率分析仪按规定带宽测量线电压、频率、PF、效率和THD；
5. 30 Hz输出时重新验证DDS、采样相位、滤波器和控制器离散参数。

本目录的默认交付目标是“先可观测、可标定、可低功率安全运行”，而不是宣称已经满足最终高压赛题指标。

## 10. 当前验证记录

- `01Final.ioc`未修改，SHA-256保持为本文第1节记录的值。
- Keil工程`MDK-ARM/01.uvprojx`已完成本机构建，结果为`0 Error(s), 0 Warning(s)`。
- PFC、INV、联合协调、OLED和VOFA源文件在Keil工程中各收录一次。
- 已完成回调签名、头文件依赖、分阶段故障仲裁、PFC双缓冲快照和联合遥测快照的静态核对。
- 尚未完成示波器波形、DWT最坏执行时间、ADC实物标定、Fault无CPU关断和带功率验收；这些项目必须按P0到P4顺序上板验证，不能用编译通过代替。

## 10. 本次静态与构建验证记录

- `01Final.ioc`保持未修改，SHA-256为`630EBF93CCF859F38662B0829E22FD1390106F91235F05AF360DE6F840BC88F6`。
- Keil工程`MDK-ARM/01.uvprojx`已收录PFC、INV、联合协调层、OLED和VOFA模块，各源文件只收录一次。
- 使用Keil命令行完整构建，结果为`0 Error(s), 0 Warning(s)`并生成AXF/HEX。
- 默认配置仍为`RAW_ADC + 5 V档 + 两套标定未确认 + PWM禁止`，因此本次构建不能直接开放功率输出。
- 当前只完成代码静态检查和编译验证，尚未代替上板检查、DWT执行时间测试、Fault注入或5 V限流带功率验收。
