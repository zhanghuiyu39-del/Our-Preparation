# 单相PWM整流器闭环工作流程与验证注意事项

## 1. 当前工程与默认运行档

当前`4_1 FinalLoop`工程用于分阶段完成单相PWM整流器ADC标定、工程量复核和PI+PR双闭环调试。用户参数只允许在以下文件修改：

```text
Core/Inc/pfc_user_config.h
```

不要再到`main.c`、`pfc_params.c`、`pfc_measure.c`或控制模块中查找和分散修改参数。默认交付设置为：

```c
#define PFC_USER_ACTIVE_PROFILE              PFC_USER_PROFILE_36V_LIGHT_100R
#define PFC_USER_RUN_MODE                    PFC_USER_MODE_CLOSED_LOOP
#define PFC_USER_36V_CALIBRATION_CONFIRMED   1U
```

当前固件直接用于36 V RMS、50 Hz输入和100 Ω轻载闭环测试，不再要求先切换RAW_ADC完成另一轮标定，也不再存在`PRECHARGE_CONFIRMED`或`FAST_OCP_CONFIRMED`编译/运行许可。上电后ADC、VAC同步和自然整流母线满足实时条件，状态机进入`PFC_READY`；随后短按并释放PD0才开放PA8～PA11四路PWM。

当前36 V轻载目标参数为：

- 交流输入：36 V RMS、50 Hz，READY允许约28.8～43.2 V RMS和45～55 Hz。
- 自然整流母线启动下限43 V，运行下限40 V，PI母线目标60 V。
- 负载：100 Ω；60 V时约36 W，除5 A额定电流外还必须确认连续功率和温升。
- 预计输入电流约1.1～1.3 A RMS；PI最大指令约1.344 A RMS。
- PWM、ADC同步采样和PR电流内环为10 kHz，PI外环为1 kHz。
- 保留FLT3、66 V母线过压、2.50 A峰值过流、ADC/DMA失步和计算异常关断。

5 V低压档仍保存在配置文件中，但不是当前默认运行档，其目标参数为：

- 隔离交流输入：5 V RMS、50 Hz。
- 直流母线目标：9.0 V。
- 负载：最终30 Ω，第一次闭环先使用80 Ω与50 Ω串联得到约130 Ω。
- PWM、ADC同步采样和PR电流内环：10 kHz。
- 100 Hz母线陷波器和PI电压外环：1 kHz。
- HRTIM Timer A/B：PA8～PA11中心对齐互补PWM。
- ADC1 DMA：Rank 1 IPFC、Rank 2 VBUS；ADC2 DMA：Rank 1 VAC。

9 V接30 Ω时负载功率约为2.7 W，理想输入电流约0.54 A RMS，按85%效率估算约0.635 A RMS。本次把PI最大电流指令提高为`1.00/sqrt(2)=0.707 A RMS`，具备覆盖30 Ω目标的理论余量，但软件上限增加不等于硬件已经通过，仍须按130 Ω、80 Ω、50 Ω、30 Ω逐级验证。

## 2. 只改一个文件的使用流程

本节2.1～2.3保留给以后更换采样板或主动重新校准时使用。当前36 V/100 Ω测试已经选择`CLOSED_LOOP`且确认值为1，不需要在本次测试前重复执行ADC标定步骤；直接按2.5和第22节操作。

### 2.1 第一步：原始ADC标定

需要重新标定时，先把运行模式改为`PFC_USER_MODE_RAW_ADC`并把当前档`CALIBRATION_CONFIRMED`改为`0U`，再编译并上板。此阶段：

- 状态进入`PFC_RAW_ADC_CALIBRATION`。
- 每1秒发布IPFC、VAC、VBUS原始码的`mean/min/max`。
- 不计算可信工程量，不配置基于占位比例的运行期AWD窗口。
- PD0无效，HRTIM输出始终关闭。

依次完成以下实测：

1. IPFC零电流时记录一秒窗口`mean`。
2. VAC零输入时记录一秒窗口`mean`。
3. 给IPFC通道施加已知正、负电流，记录原始码摆幅和方向。
4. 给VAC通道施加已知正弦电压，记录`min/max`和物理峰值。
5. 给VBUS通道施加万用表确认的直流电压，记录`mean`。

计算公式：

```text
双极性通道零点count = 零输入时mean
双极性通道峰值码幅 = (max - min) / 2
IPFC A/count = 已知电流峰值 / IPFC峰值码幅
VAC V/count = 已知电压峰值 / VAC峰值码幅
VBUS V/count = 万用表直流电压 / VBUS mean
```

如果已知正向物理量换算后为负，修改对应采样极性`+1/-1`，不要在PR或PI公式中临时加负号。

### 2.2 第二步：填写标定值

5 V档修改`PFC_USER_5V_*`标定宏；两个36 V档共用以下`PFC_USER_36V_*`标定宏：

```c
PFC_USER_36V_IPFC_ZERO_COUNT
PFC_USER_36V_VAC_ZERO_COUNT
PFC_USER_36V_IPFC_A_PER_COUNT
PFC_USER_36V_VAC_V_PER_COUNT
PFC_USER_36V_VBUS_V_PER_COUNT
PFC_USER_36V_IPFC_POLARITY
PFC_USER_36V_VAC_POLARITY
PFC_USER_36V_BRIDGE_POLARITY
```

完成万用表、示波器和接线方向复核后，将：

```c
#define PFC_USER_36V_CALIBRATION_CONFIRMED  1U
```

极性只能为`+1`或`-1`，零点必须位于0～4095。工程量或闭环模式下标定确认仍为0会在编译期直接报错，防止误下载带功率固件。

### 2.3 第三步：工程量复核

改为：

```c
#define PFC_USER_RUN_MODE  PFC_USER_MODE_ENGINEERING_CHECK
```

该模式使用已确认标定值计算IPFC、VAC、VBUS、RMS和频率，并配置运行期AWD，但仍强制关闭PWM且忽略PD0。必须确认：

- 5 V RMS输入时VAC峰值约为正负7.07 V，`vac_rms`接近5 V。
- `vac_frequency_hz`接近50 Hz并能建立`vac_locked`。
- VBUS与万用表一致，输入增加时原始码和工程量都单调增加。
- 已知正向IPFC和VAC的符号符合控制定义。
- 原始码不贴近0或4095，软件保护和AWD阈值有合理裕量。
- `PFC_Params_GetValidationError()`返回0。

当前`PFC_USER_5V_VAC_V_PER_COUNT`暂时保留为`0.02730 V/count`，但此前5.06 V RMS、原始码`min=1701/max=2375`的单点结果推算约为：

```text
码峰值幅度 = (2375 - 1701) / 2 = 337 count
实际电压峰值 = 5.06 * sqrt(2) = 7.156 V
低压单点比例 = 7.156 / 337 = 0.02123 V/count
```

两者差异过大，不能直接任选一个进入闭环。应在ENGINEERING_CHECK且PWM禁止的条件下依次施加约1 V、3 V、5 V RMS，每一点同时记录仪表RMS、`vac_min`、`vac_max`和`vac_mean`，分别计算`V/count`。只有三个结果基本一致、换算RMS误差可接受且原始码未贴轨，才能把新的统一比例写回配置头文件。比例未确认时保持工程量复核模式。

### 2.4 第四步：闭环调试

所有工程量、极性和保护阈值复核通过后才改为：

```c
#define PFC_USER_RUN_MODE  PFC_USER_MODE_CLOSED_LOOP
```

先把80 Ω和50 Ω串联成约130 Ω验证PR电流环，再观察PI外环，随后按80 Ω、50 Ω、30 Ω逐步降低负载。每次更换负载前必须断开输入并确认母线已经放电。

### 2.5 当前36 V、100 Ω轻载档

36 V现在分成两个参数档：

- `PFC_USER_PROFILE_36V_LIGHT_100R`：60 V母线、100 Ω、约36 W轻载，PR/PI和电流保护采用保守起点。
- `PFC_USER_PROFILE_36V_CONTEST`：约111 W满功率赛题骨架，保留较大的电流目标和独立控制参数。

两个档位共用同一组36 V ADC换算值，但控制增益、电流目标和限幅阈值分开保存。当前默认选择轻载档；`PFC_Params_PowerAllowed()`只检查参数有效、标定确认和`CLOSED_LOOP`模式，不再检查额外硬件确认宏。满功率赛题档仍不是本次100 Ω测试使用的档位。

## 3. 三种运行模式

| 模式 | 工程量 | AWD运行阈值 | PD0 | PWM输出 |
| --- | --- | --- | --- | --- |
| `PFC_USER_MODE_RAW_ADC` | 无，只发布原始统计 | 保留CubeMX宽窗口 | 忽略 | 强制关闭 |
| `PFC_USER_MODE_ENGINEERING_CHECK` | IPFC/VAC/VBUS、RMS、频率 | 按实测参数配置 | 忽略 | 强制关闭 |
| `PFC_USER_MODE_CLOSED_LOOP` | 全部有效工程量与控制遥测 | 按实测参数配置 | 短按启停 | 条件满足后允许开放 |

参数接口的含义已经分开：

- `PFC_Params_Validate()`：参数数值是否自洽。
- `PFC_Params_CalibrationReady()`：ADC工程量是否可用。
- `PFC_Params_PowerAllowed()`：是否为已确认标定的闭环模式，不再附加预充/OCP确认字段。
- `PFC_Params_GetValidationError()`：返回错误类别位图。

`PowerAllowed()`为1仍不代表立即开放PWM，VAC、频率、VBUS、Fault、采样同步和PD0短按等实时条件仍必须全部满足。

## 4. 上电初始化与数据流

```text
HAL和170 MHz时钟
-> GPIO、DMA、HRTIM、ADC1/2、USART2
-> 强制关闭HRTIM四路输出
-> 读取pfc_user_config.h生成的活动参数
-> OLED等待100 ms并初始化
-> 初始化测量统计和模式状态机
-> ADC2、ADC1单端校准
-> 标定可用时配置IPFC/VBUS/VAC模拟看门狗
-> 先启动ADC2 DMA，再启动ADC1 DMA
-> 关闭两路DMA Half Transfer中断
-> 启动Master、Timer A/B计数器，仅产生10 kHz ADC触发
-> 最后启动约500 ms IWDG
```

采样数据流：

```text
Master CMP2 / HRTIM Trigger 1
-> ADC2采集VAC，DMA写PFC_Adc2Dma[0]
-> ADC2完整回调只推进序列号
-> ADC1采集IPFC、VBUS，DMA写PFC_Adc1Dma[0..1]
-> ADC1完整回调检查ADC2序列
-> 发布同步原始快照并更新一秒统计
-> 标定可用时换算工程量、RMS和频率
-> 仅闭环带功率状态执行一次10 kHz PR路径
```

DMA数组由硬件持续改写，只用于核对Rank。主循环、OLED、VOFA和控制模块都通过快照接口读取，不直接组合DMA数组。

## 5. 状态机

| 数值 | 状态 | 行为 |
| ---: | --- | --- |
| 0 | `PFC_SAFE` | 普通复位等待100 ms，IWDG复位等待1 s，PWM关闭 |
| 1 | `PFC_RAW_ADC_CALIBRATION` | 原始统计，工程量和PD0禁用 |
| 2 | `PFC_ENGINEERING_CHECK` | 工程量与保护复核，PD0和PWM禁用 |
| 3 | `PFC_ADC_CHECK` | 闭环模式检查同步快照至少100 ms |
| 4 | `PFC_PASSIVE_PRECHARGE` | 仅等待VAC和自然整流VBUS条件连续200 ms，不表示存在物理预充电路 |
| 5 | `PFC_READY` | PWM关闭，等待一次有效PD0短按 |
| 6 | `PFC_CURRENT_LOOP_RAMP` | PR内环执行0.20 A RMS探测斜坡 |
| 7 | `PFC_VBUS_LOOP_RAMP` | PI无扰投入，36 V轻载档VBUS参考按2 V/s上升 |
| 8 | `PFC_VBUS_LOOP_RUN` | 60 V目标附近稳态双闭环 |
| 9 | `PFC_STOP` | 人工停机，PWM关闭后重新判断条件 |
| 10 | `PFC_FAULT_LATCH` | 故障锁存，禁止自动恢复 |

当前36 V轻载档进入READY前要求：工程量有效、VAC RMS位于28.8～43.2 V、频率45～55 Hz、VAC同步锁定、VBUS不低于43 V且低于64 V、无锁存故障。所有条件连续成立200 ms后进入READY，PWM仍保持关闭，必须再完成一次PD0有效短按才启动。

`PFC_AppGetReadyBlockReason()`给OLED和VOFA返回统一阻断位，避免显示代码与状态机使用不同判据：

| 位值 | 含义 |
| ---: | --- |
| `001` | 当前不是闭环运行模式 |
| `002` | 参数或标定确认无效 |
| `004` | 工程量快照无效 |
| `008` | VAC同步未锁定 |
| `010` | VAC RMS不在额定值正负20% |
| `020` | VAC频率超出允许范围 |
| `040` | VBUS低于启动下限 |
| `080` | VBUS达到告警值，不允许重新投入 |
| `100` | 已存在锁存故障 |

PD0为内部上拉、低有效、1 ms轮询。必须先稳定释放30 ms，再完成50～1000 ms按下并释放才产生一次事件。长按、抖动、原始模式、工程量模式和故障状态都不会启动PWM。

## 6. 一秒ADC统计

`PFC_AdcCalibrationStats`包含三路原始码的`latest/mean/min/max`、窗口序号、样本数和有效标志。默认10 kHz下每10000个同步ADC帧发布一次：

- 上电第一秒内`valid=0`且显示值可能为0。
- 完成首个窗口后`valid=1`，`window_sequence`每秒增加1。
- 统计由ADC1完整DMA ISR更新，不使用任何占位比例。
- 主循环通过`PFC_Measure_GetCalibrationStats()`读取一致性副本。
- `uint32_t`累加上限足以覆盖`4095 * 10000`。

零点标定时输入必须为确定的安全电平，ADC不得悬空。VAC正弦比例优先使用一秒`min/max`，而不是100 Hz刷新率的VOFA瞬时曲线。

## 7. OLED显示

OLED每100 ms在主循环更新，不参与故障判断和IWDG喂狗。

原始ADC模式：

```text
I:mean/min/max
V:mean/min/max
D:mean W:窗口号
S:状态 F:故障码
```

工程量复核模式：

```text
VM:VAC mean原始码   VR:VAC RMS(0.01V)
VN:VAC min原始码    VX:VAC max原始码
VB:VBUS(0.01V)      B:READY阻断位
E:参数错误           F:故障码
```

工程量页把VAC的原始`mean/min/max`与换算RMS同时放在屏幕上，专门用于定位“原始码变化正常，但比例换算错误”的情况。VAC频率、IPFC工程量和RMS继续在VOFA与Keil Watch查看。

闭环模式：

```text
IR:输入电流RMS(mA)     VR:输入电压RMS(0.01V)
VB:母线实测(0.01V)     BR:母线参考(0.01V)
M:调制度*10000         S:状态
F:故障码                PF:软件估算功率因数*100
```

闭环第3行末尾`P`显示PI限幅标志，`0`表示未限幅，`1`表示PI输出达到上下限。`PF`在完成一个工频窗口后显示软件估算值乘100，例如`+098`约为+0.98；它只用于检查极性和趋势，正式验收必须使用功率分析仪。周期削顶占比继续通过Keil Watch观察。

闭环页不再显示瞬时IPFC和VAC。原因是OLED每100 ms刷新一次，而50 Hz交流的周期为20 ms，
每次刷新正好相隔5个完整工频周期，瞬时量容易总在近似相同相位被读取，看起来几乎不变化。
OLED因此改为显示一个工频周期更新一次的RMS值；需要检查VAC、IPFC、瞬时电流参考的波形、
正负极性和相位关系时，仍应观察VOFA的CH0～CH2或使用示波器，不能根据OLED的RMS值判断波形质量。

OLED数字没有小数点，必须按上述缩放解释。例如`VR=0506`表示5.06 V RMS，`VB=0900`表示9.00 V，
`IR=0200`表示0.200 A RMS，`M=-1250`表示调制度为-0.1250。闭环页面所有字段均限制在16列内，
带符号调制度固定使用“1位符号+4位数字”，不会覆盖后面的状态标签。

## 8. VOFA通道

USART2为460800 baud、8N1、JustFloat。主循环每10 ms发送一次。

原始ADC模式：

| 通道 | 数据 |
| ---: | --- |
| CH0 | IPFC mean |
| CH1 | IPFC min |
| CH2 | IPFC max |
| CH3 | VAC mean |
| CH4 | VAC min |
| CH5 | VAC max |
| CH6 | VBUS mean |
| CH7 | 状态和故障编码 |
| CH8 | 一秒统计窗口序号；有效时附加0.5 |
| CH9 | 保留，固定0 |

工程量复核模式：

| 通道 | 数据 |
| ---: | --- |
| CH0 | VAC，V |
| CH1 | VAC RMS，V |
| CH2 | IPFC，A |
| CH3 | IPFC RMS，A |
| CH4 | VBUS，V |
| CH5 | VAC频率，Hz |
| CH6 | 参数错误位`*1000` + READY阻断位 |
| CH7 | 状态和故障编码 |
| CH8 | 输入有功功率软件估算，W |
| CH9 | 输入功率因数软件估算，-1～+1 |

闭环模式：

| 通道 | 数据 |
| ---: | --- |
| CH0 | VAC，V |
| CH1 | IPFC，A |
| CH2 | 瞬时电流参考，A |
| CH3 | VBUS，V |
| CH4 | VBUS参考，V |
| CH5 | RMS电流指令，A |
| CH6 | 调制度 |
| CH7 | 状态和故障编码 |
| CH8 | 输入有功功率软件估算，W |
| CH9 | 输入功率因数软件估算，-1～+1 |

CH7计算为：

```text
CH7 = PFC_State * 1000000 + fault_bits
```

工程量复核模式的CH6使用整数编码：`param_error = floor(CH6/1000)`，`ready_block = CH6 - param_error*1000`。默认ENGINEERING_CHECK时阻断位至少包含`001`，这是“当前模式主动禁止PWM”的正常结果；应继续检查是否还叠加`010`等VAC比例异常位。

VOFA现在每帧发送10个float加4字节帧尾，共44字节；460800 baud下约0.96 ms。10 ms发送周期等于100 Hz，对50 Hz波形每周期只有约两个显示点，不能用于判断完整正弦度、THD或精确相位。波形质量必须使用示波器/电流探头，原始峰值标定使用一秒统计。不得把阻塞式`VOFA_Send()`移入ADC ISR。

## 9. PI+PR闭环流程

PD0启动后先进入PR电流探测：

```text
Irms指令从0以5 A/s升到0.20 A
-> Iref = Irms_cmd * VAC / 5 V RMS
-> PR处理Iref-IPFC
-> v_bridge_ref = VAC - PR输出
-> modulation = bridge_polarity * v_bridge_ref / VBUS
-> SPWM限幅并写Timer A/B Compare预装载
```

电流环至少观察60 ms并完成一个50 Hz误差窗口。误差RMS不超过0.10 A才进入PI外环，500 ms内未通过则锁存故障。0.10 A是0.20 A探测指令的50%，若低压实测噪声较大，应先查采样和比例，不要直接放宽到原来的0.15 A。

PI外环每10个ADC帧运行一次：

```text
VBUS -> 100 Hz陷波 -> 1 kHz PI -> RMS电流指令
```

切入时预置PI输出以减小跳变，VBUS参考从当前值以1 V/s升至9 V。8 s内未进入8.5～9.5 V则锁存目标超时。保护仍使用未陷波VBUS，避免陷波延迟掩盖过压。

正常运行时，控制快速心跳约10 kHz，外环心跳约1 kHz；PR、PI和调制度不应持续限幅。当前36 V轻载档允许连续限幅最多50 ms；此外每200个10 kHz样本统计一个50 Hz周期，RUN中限幅占比超过40%且连续5个周期时锁存故障。进入稳态后若PI已限幅且VBUS偏离目标容差，持续4000 ms会锁存`PFC_FAULT_VBUS_REGULATION`。IPFC与VAC反相或电流越控越大时，立即停机检查三个极性与全桥接线，不能靠继续放宽限幅掩盖错误。

## 10. 分阶段上板准入

### A. 原始ADC，无功率

1. 手动选择RAW模式，并把当前档标定确认值设为0。
2. 功率驱动和交流功率输入保持断开。
3. 验证Rank、序列号、10 kHz心跳和一秒统计。
4. 操作PD0时确认PA8～PA11始终无PWM。
5. 计算并填写零点、比例和采样极性。

### B. 工程量复核，无功率

1. 标定确认改为1，模式改为ENGINEERING_CHECK。
2. 用万用表和示波器复核VAC、VBUS、IPFC、RMS和频率。
3. 核对AWD原始码窗口与软件保护阈值。
4. 操作PD0时仍必须无PWM。

### C. MCU和驱动波形，无母线

1. 改为CLOSED_LOOP，使用受控采样条件进入READY。
2. 短按PD0，确认10 kHz中心对齐互补PWM和约500 ns死区。
3. 确认停机、ADC错误和Fault事件能关闭四路输出。
4. 先测驱动器输出波形，再连接功率母线。

### D. 5 V隔离输入轻载

1. 使用隔离、限流5 V RMS电源。
2. 先把80 Ω和50 Ω串联为约130 Ω足额功率负载。
3. 观察PR探测阶段IPFC与VAC同相且不持续限幅。
4. 观察VBUS参考和实测值平滑上升。
5. 检查MOSFET、驱动器、电感、负载温升和输入电流。

### E. 逐步降到30 Ω

按130 Ω、80 Ω、50 Ω、30 Ω逐步验证。新的PI上限为0.707 A RMS；若PI长期达到上限、VBUS失调或调制度周期性削顶，不得继续提高阈值，应先检查效率、输入跌落、采样量程、电感饱和和独立过流保护。

## 11. 故障码

| 十六进制 | 故障 | 常见原因 |
| ---: | --- | --- |
| `00001` | ADC同步 | DMA停止或ADC1/2失步 |
| `00002` | ADC/DMA HAL错误 | Overrun、DMA或HAL状态异常 |
| `00004` | ADC贴轨 | 原始码连续64帧低于4或高于4091，通常为前端饱和、量程或接线错误 |
| `00008` | HRTIM | 启动/Compare失败或Fault 3事件 |
| `00010` | 软件过流 | IPFC超过参数阈值 |
| `00020` | VBUS过压 | 母线超过软件阈值 |
| `00040` | VAC丢失 | 同步、频率或运行电压失效 |
| `00080` | 参数 | 参数数值或运行许可无效 |
| `00100` | 调制 | 调制数值或Compare更新失败 |
| `00200` | 状态机 | 非法状态 |
| `00800` | 母线目标超时 | 外环规定时间内未建立目标 |
| `01000` | IPFC AWD | ADC1 AWD1越窗 |
| `02000` | VBUS AWD | ADC1 AWD2越窗 |
| `04000` | VAC AWD | ADC2 AWD1越窗 |
| `08000` | 控制 | PI/PR未初始化或数值异常 |
| `10000` | 电流跟踪 | 电流环未通过RMS误差检查 |
| `20000` | 控制限幅 | 连续限幅约50 ms，或RUN中单周期限幅超过40%并连续5周期 |
| `40000` | 母线稳压失调 | 稳态PI已限幅且VBUS偏离目标超过容差约4 s |

故障发生后先关闭HRTIM输出，再复位控制器并进入`PFC_FAULT_LATCH`。软件不自动恢复，PD0不能清故障。IWDG随后可能复位系统，因此调试时同时记录复位来源和最后一帧VOFA数据。

## 12. Keil Watch建议

DMA和同步快照：

```text
PFC_Adc1Dma[0]
PFC_Adc1Dma[1]
PFC_Adc2Dma[0]
pfc_measurement.adc1_sequence
pfc_measurement.adc2_sequence
pfc_measurement.fast_heartbeat
pfc_measurement.raw_valid
pfc_measurement.offset_ready
pfc_measurement.valid
pfc_measurement.fault_bits
```

原始统计：

```text
pfc_calibration_stats.ipfc_mean
pfc_calibration_stats.ipfc_min
pfc_calibration_stats.ipfc_max
pfc_calibration_stats.vac_mean
pfc_calibration_stats.vac_min
pfc_calibration_stats.vac_max
pfc_calibration_stats.vbus_mean
pfc_calibration_stats.window_sequence
pfc_calibration_stats.valid
```

闭环遥测：

```text
app_state
control_telemetry.fast_heartbeat
control_telemetry.outer_heartbeat
control_telemetry.current_rms_command
control_telemetry.current_reference
control_telemetry.current_error_rms
control_telemetry.vbus_reference
control_telemetry.pr_output_v
control_telemetry.modulation
control_telemetry.pi_limited
control_telemetry.pr_limited
control_telemetry.modulation_limited
control_telemetry.period_saturation_samples
control_telemetry.period_saturation_ratio
control_telemetry.period_saturation_bad_cycles
control_telemetry.vbus_regulation_error_ms
control_telemetry.current_loop_qualified
control_telemetry.healthy
```

暂停CPU会停止回调和喂狗，可能导致IWDG复位；不能把调试暂停后的序列停滞当作固件DMA故障。

## 13. 保护边界

- 当前驱动板没有MCU Gate Enable；开放HRTIM四路输出就是软件功率许可边界。
- PB10虽配置低有效FLT3并有内部上拉，但外部未接OCP、DESAT或nFAULT，不能计入实际短路保护链。
- ADC AWD、软件过流和控制限幅都有采样及处理延迟，不能替代独立微秒级硬件保护。
- 当前测试输入必须来自36 V RMS隔离变压器，禁止直接连接市电或把系统板地与不确定的功率参考点短接。
- 普通示波器地夹不得接到浮动桥臂节点，高侧测量使用差分或隔离探头。
- 100 Ω在60 V时耗散约36 W；5 A额定电流满足0.60 A工作电流，但仍需确认电阻连续功率和温升。
- 每次只修改一组PI/PR或保护参数，并保存VAC、IPFC、Iref、VBUS和调制度波形。
- OLED、VOFA和`HAL_Delay()`禁止进入ADC DMA、AWD或HRTIM Fault中断。

本次重构没有修改`.ioc`、CubeMX生成的ADC/DMA/HRTIM/GPIO初始化或Simulink模型，也没有改变10 kHz采样、PWM死区、DMA Rank和Fault路由。

## 14. 板间连接、功率路径与当前实物边界

### 14.1 信号和能量路径

```text
隔离5 V RMS/50 Hz交流源
  -> PFC功率板交流输入
  -> 两只470 uH输入电感
  -> HRTIM Timer A/B组成的单相全桥
  -> 二极管/同步整流通路
  -> 4000 uF直流母线电容
  -> 30～100 ohm阻性负载

系统板 STM32G474
  PA8  = HRTIM1_CHA1，桥臂1高侧
  PA9  = HRTIM1_CHA2，桥臂1低侧
  PA10 = HRTIM1_CHB1，桥臂2高侧
  PA11 = HRTIM1_CHB2，桥臂2低侧
       -> 两块UCC21520驱动板的PWMA/PWMB
       -> PFC桥臂上下管

直流耦合采样板
  CURROUT -> ADC1 Rank 1 IPFC
  VOLTOUT/BIESVOLT -> 按当前接线确认ADC1 Rank 2 VBUS
  VAC采样输出 -> ADC2 Rank 1 VAC
       -> HRTIM Trigger 1同步采样
       -> DMA原始数组 -> 一致性测量快照 -> 控制/OLED/VOFA
```

上面的`VOLTOUT/BIESVOLT`是采样板候选网络名，最终映射必须以当前连接器、`03-1OpenLoop.ioc`生成的ADC通道和实测电压为准。不能只凭原理图网络名把某个输出直接当作VBUS或VAC。

### 14.2 功率板和驱动板必须知道的事实

| 项目 | 当前事实 | 对操作的影响 |
| --- | --- | --- |
| 输入电感 | 两只470 uH；若交流电流路径依次经过两只电感，差模总电感约940 uH | 不要把单只470 uH直接代入后续PR/PI重新整定公式 |
| 母线电容 | 实物约4000 uF | 上电被动充电能量和浪涌明显，必须使用限流电源或预充 |
| 负载 | 30 ohm定值，另有80 ohm和50 ohm可调/定值组合 | 第一次闭环先用80～160 ohm，再逐步降低 |
| 保险丝 | 输入侧F5A | 只能处理较慢的持续过流，不能保护微秒级MOSFET直通 |
| UCC21520 | 逻辑侧5 V、隔离侧12 V；`DISABLE`为硬件固定状态 | HRTIM输出一开放，驱动器可能立即响应 |
| MCU Gate Enable | 当前没有独立可控Gate Enable | PD0只是软件命令，不是硬件急停 |
| PB10/FLT3 | 软件保留，外部保护源未接 | 不能宣称已有DESAT/OCP硬件关断 |

驱动器的高侧输出必须以对应的Gate-Source为测量参考。普通示波器地夹只能测逻辑侧PWMA/PWMB；禁止把地夹接到浮动开关节点或高侧源极。

## 15. 上板前的逐级操作流程

### 15.1 P0：原始ADC和采样板

1. `PFC_USER_RUN_MODE`保持`PFC_USER_MODE_RAW_ADC`，`PFC_USER_5V_CALIBRATION_CONFIRMED`保持`0U`。
2. 断开功率板交流输入、驱动板PWM线和直流母线；只给系统板、采样板供电。
3. 测采样板5 V输入、板上3.3 V和`BIESVOLT/VREF`相关中点。偏置型双极性通道通常接近1.65 V，但VBUS单极性通道零输入可能接近0 V，不能一律按2048判断。
4. 在采样板输出端、系统板ADC连接器端分别测量同一信号，确认线缆没有错位、断线或短接。
5. Keil Watch确认`PFC_Adc1Dma[0]`为IPFC、`PFC_Adc1Dma[1]`为VBUS、`PFC_Adc2Dma[0]`为VAC。
6. 等待首个1 s统计窗口完成，记录`latest/mean/min/max`。此阶段PA8～PA11必须始终无PWM，PD0无效。

P0不通过时，禁止连接驱动板和功率母线。ADC输入不得悬空；没有信号源时，应接到采样板允许的确定安全电平，而不是直接用外部电源硬顶已经连接的运放输出。

### 15.2 P1：工程量和保护阈值复核

1. 填写零点、比例和极性后，把`PFC_USER_RUN_MODE`改为`PFC_USER_MODE_ENGINEERING_CHECK`并重新编译。
2. 不连接功率母线，给采样板施加可控的低压已知输入。
3. 用万用表同时记录真实VAC、IPFC和VBUS，与OLED/VOFA工程量比较。
4. 观察VAC正向过零、`vac_frequency_hz`和`vac_locked`；50 Hz输入应稳定在允许窗口内。
5. 核对运行期AWD窗口：正常波形的最大/最小原始码必须远离0和4095，并留出启动尖峰余量。
6. 在该模式下按PD0，PWM仍必须关闭。否则先停机，不进入带功率阶段。

### 15.3 P2：MCU和驱动波形，无母线

1. 切换到`PFC_USER_MODE_CLOSED_LOOP`，但继续断开功率板输入。
2. 等待状态进入`PFC_READY`，确认OLED状态为5、故障码为0。
3. 只有在受控采样信号满足READY条件时才短按PD0：按下50～1000 ms后释放，不能长按。无母线阶段只短时间观察`PFC_CURRENT_LOOP_RAMP`产生的PWM，并在电流跟踪超时前主动停机；由于没有真实功率响应，不要求也不允许用该阶段验证PI母线外环。
4. 在PA8、PA9、PA10、PA11对MCU地测量10 kHz互补PWM和约500 ns初始死区。
5. 接通驱动板5 V逻辑和12 V隔离电源，但不接母线；逐块测PWMA/PWMB和Gate-Source。
6. 确认上、下管没有重叠导通，实测死区包含HRTIM死区、驱动器DT网络和传播延时。
7. 再次短按PD0停机，确认四路输出关闭；停机后状态回到`PFC_STOP`，不会因按键持续按下自动重新启动。

### 15.4 P3：5 V限流、轻载带功率

1. 使用隔离、限流的5 V RMS/50 Hz电源，先将限流设置得较小。
2. 先把80 ohm与50 ohm串联得到约130 ohm高阻负载，不要一开始接30 ohm。
3. 先上系统板、采样板和驱动辅助电源，确认OLED工程量、故障码和ADC序列正常。
4. 最后接通交流功率输入，观察输入电流、母线电压、两个电感温升、MOSFET温升和负载功率。
5. 通过130 ohm、80 ohm、50 ohm、30 ohm逐级降低负载；每次换负载都必须先断开交流输入并确认母线已放电。
6. 任一桥臂异常发热、输入电流突升、母线快速越过保护阈值或波形出现重叠，立即断开交流源，不等待软件关断或F5A保险丝熔断。

## 16. 软启动、母线浪涌与停机

### 16.1 软件软启动能解决什么

当前电流指令和VBUS参考采用斜坡，能限制**PWM已经开放之后**的主动整流电流突变，降低PR/PI切入时的调制冲击。它不能限制交流源刚接通时由整流二极管、MOSFET体二极管、输入电感和4000 uF电容形成的被动充电浪涌。

因此以下做法不能替代硬件限流：增加`HAL_Delay()`、把PI增益调小、把VBUS参考斜坡变慢、把电流环探测值改成0。功率级上电瞬间仍可能先发生被动充电。

### 16.2 当前阶段的浪涌控制要求

- 5 V低压调试必须使用带电流限制的隔离交流源。
- 需要反复插拔或电源内阻很低时，应在功率路径中增加经验证的预充电阻/NTC和旁路开关；该硬件链路未接入前，不把软件状态机写成“已经完成预充”。
- 4000 uF母线储能即使在低压下也会保持一段时间。停机后必须测量母线，而不是凭OLED状态判断已经无电。
- 36 V或更高输入时浪涌能量和MOSFET应力会显著增加，不能直接沿用5 V阶段的限流值、保险丝和启动参数。

### 16.3 正常停机和紧急断电

正常停机：在`PFC_CURRENT_LOOP_RAMP`、`PFC_VBUS_LOOP_RAMP`或`PFC_VBUS_LOOP_RUN`状态下，按下PD0并保持50～1000 ms后释放。程序会关闭HRTIM输出、复位PI/PR/陷波器并进入`PFC_STOP`。长按超过1 s不产生事件；故障锁存状态下PD0不能清故障。

紧急情况：先关闭隔离交流源，再关闭驱动逻辑/隔离电源，等待母线放电，并用万用表确认母线电压处于安全值。PD0不是急停键，F5A也不是MOSFET短路保护。

## 17. OLED、VOFA和Keil Watch的正常反应

### 17.1 OLED

OLED只在主循环每100 ms刷新，不参与控制、故障判断和IWDG刷新。工程量页显示VAC原始统计、换算RMS和READY阻断位；闭环页显示RMS、母线参考、调制度、状态、PI限幅和周期削顶占比，不能用它判断50 Hz瞬时波形是否正弦。

| 场景 | OLED应看到的现象 |
| --- | --- |
| RAW_ADC | I/V原始统计，首个1 s窗口前可能为0，状态1，PWM关闭 |
| ENGINEERING_CHECK | VAC原始mean/min/max、换算RMS、VBUS、READY阻断位、参数错误和故障码 |
| PASSIVE_PRECHARGE | `S=04`且PWM关闭；查看第三行`B`阻断码定位VAC锁相、RMS、频率或VBUS条件 |
| READY | 母线和输入量稳定，`S=05`、`B=000`、`F=00000`，PWM关闭 |
| CURRENT_LOOP_RAMP | 状态6，IR逐步增加，M开始变化 |
| VBUS_LOOP_RAMP/RUN | 状态7/8，BR逐步接近当前档目标（5 V档9.00 V、36 V档60.00 V），VB跟随，P不应长期为1，PF趋势应为正 |
| STOP | 状态9，M回到接近0，PWM关闭 |
| FAULT_LATCH | 状态10，F锁存为非0；故障只能复位恢复 |

### 17.2 VOFA

VOFA每10 ms阻塞发送一帧，只能放在主循环。闭环CH0～CH2为瞬时VAC、IPFC和电流参考，CH3～CH6为VBUS、VBUS参考、RMS电流指令和调制度，CH7为状态与故障编码。

10 ms对50 Hz只有约两个点/周期，不能据此判断THD、相位或完整正弦。若I0～I5或某一瞬时量看起来不变，先确认显示采样频率和相位锁定，再用示波器或提高上位机采集率观察波形。

### 17.3 Keil Watch

建议同时观察：

```text
PFC_Adc1Dma[0] / [1]       原始IPFC/VBUS
PFC_Adc2Dma[0]              原始VAC
pfc_measurement.adc1_sequence
pfc_measurement.adc2_sequence
pfc_measurement.fast_heartbeat
pfc_measurement.offset_ready / valid / vac_locked
pfc_measurement.fault_bits
pfc_calibration_stats.ipfc_mean/min/max
pfc_calibration_stats.vac_mean/min/max
pfc_calibration_stats.vbus_mean/min/max
app_state
control_telemetry.fast_heartbeat / outer_heartbeat
control_telemetry.current_rms_command
control_telemetry.vbus_reference
control_telemetry.modulation
control_telemetry.pi_limited / pr_limited / modulation_limited
control_telemetry.period_saturation_ratio / period_saturation_bad_cycles
control_telemetry.vbus_regulation_error_ms
```

Watch在CPU运行时不一定实时刷新；按下暂停会停止ADC回调、控制和IWDG，暂停后的序列不再增加不能直接判定为DMA故障。连接功率母线时禁止断点、单步和长时间暂停。

## 18. 现象、原因和定位顺序

| 现象 | 优先检查 | 处理 |
| --- | --- | --- |
| 上电立即出现`00004` ADC贴轨 | 采样板3.3 V、SGND、输出线、ADC脚是否短接 | 断电后逐点测采样板输出和系统板输入；不要先放宽阈值 |
| 原始码固定在约2048但VAC/VBUS不变 | 采样输出是否真的连接到对应Rank，HRTIM Trigger是否运行 | 看DMA序列和原始数组；序列不变查DMA/HRTIM，序列变化查前端 |
| `adc1_sequence`或`adc2_sequence`停止 | DMA、ADC overrun、HRTIM触发或调试器暂停 | 先退出暂停，检查HAL错误回调和DMA使能，再重新上电 |
| VAC RMS为0或频率不锁定 | VAC零点/比例/极性、输入频率、采样前端饱和 | 工程量模式下用万用表核对，确认正向过零方向 |
| VBUS显示明显偏低/偏高 | VBUS零点模型、V/count、采样板电阻网络 | 用多个直流点重新拟合，不直接套用VAC比例 |
| READY无法进入 | VAC RMS是否在28.8～43.2 V、频率45～55 Hz、VBUS是否在43～64 V、Fault是否为0 | 查看READY阻断位和Watch，不反复按PD0强行启动 |
| 工程量页B包含`010` | VAC换算RMS不在4.5～5.5 V | 对照VM/VN/VX重新计算低压V/count，完成1/3/5 V多点线性检查 |
| 工程量页B包含`080` | VBUS已经达到9.8 V告警值 | 断开输入检查母线与采样比例，禁止在接近过压时重新投入PWM |
| 按PD0没有启动 | 是否已进入READY、按键是否先释放30 ms、按下时间是否50～1000 ms | 短按并释放；RAW/工程量模式下PD0本来就被忽略 |
| PWM一开放输入电流迅速增大 | IPFC/VAC/bridge极性、A/B桥臂中点接线、PR符号 | 立即断开输入；低压无功率先确认方向，不能靠加大限幅掩盖 |
| PR或调制度持续限幅 | 母线不足、负载过重、电感参数、采样比例、死区和驱动波形 | 退回高阻负载，记录VOFA和示波器波形，逐项只改一组参数 |
| 故障`40000` | PI打满4 s仍无法把VBUS维持在60 V目标容差 | 检查输入跌落、PI上限、VBUS比例和功率级损耗 |
| VBUS快速越过66 V | PI符号、VBUS比例、负载断开、过压阈值设置 | 立即断开交流源并检查最后一帧VOFA数据 |
| OLED看起来不变但VOFA正常 | OLED 100 ms刷新和50 Hz相位锁定 | OLED看RMS和状态，瞬时波形改用VOFA/示波器 |
| 看到短暂PWM后又复位 | 故障锁存后IWDG停止刷新，或调试器暂停导致喂狗停止 | 记录故障码和复位标志；修复根因后重新下载，不在故障后自动重试 |

## 19. 最终验收清单

### 19.1 无功率验收

- [ ] `PFC_USER_RUN_MODE`和标定确认值与当前阶段一致。
- [ ] ADC1/ADC2 Rank、DMA数组下标和采样板网络逐项核对。
- [ ] ADC序列约10 kHz，两个序列号同步推进，Half Transfer不进入业务控制。
- [ ] 一秒统计的mean/min/max稳定，原始码远离0和4095。
- [ ] 工程量模式下VAC RMS、VAC频率、VBUS和IPFC比例与仪表一致。
- [ ] AWD窗口覆盖正常波形，并保留足够尖峰余量。

### 19.2 MCU/驱动验收

- [ ] PA8～PA11为10 kHz中心对齐互补PWM。
- [ ] A/B桥臂死区在MCU引脚和Gate-Source两处分别实测。
- [ ] 短按PD0能启动，短按PD0能停机，长按不产生事件。
- [ ] 故障后HRTIM输出关闭、状态进入锁存，不会自动恢复。
- [ ] 暂停调试器时不连接功率母线，避免IWDG复位和功率失控。

### 19.3 36 V、100 Ω轻载闭环验收

- [ ] 输入为隔离36 V RMS/50 Hz，负载为100 Ω、额定电流5 A，并确认可连续耗散至少36 W。
- [ ] 上电后自然整流母线约48～51 V，OLED状态依次进入3、4、5且故障码为0。
- [ ] PD0短按后IPFC与VAC同相，PR/调制度不持续限幅，VBUS参考由当前值平滑升至60 V。
- [ ] 稳态VBUS约60 V，输入电流预计1.1～1.3 A RMS，软件PF为正且趋势接近1。
- [ ] 记录输入电流、母线电压、MOSFET、驱动板、电感和100 Ω负载温升。

## 20. 后续修改参数的同步检查

只要修改输入电压、母线目标、负载、频率、采样比例、极性、电感或电容，必须重新检查：

1. ADC零点和V/count/A/count；双极性通道的极性不能凭旧档复制。
2. VAC、IPFC、VBUS的AWD原始码窗口和软件过压/过流阈值。
3. `L`和`Cbus`对电流环、母线纹波、预充和浪涌的影响。
4. PR谐振频率、采样频率、PI输出上限和电流斜坡；仿真20 kHz系数不能直接用于10 kHz固件。
5. HRTIM死区、驱动器传播延时、MOSFET耐压/电流和独立OCP/DESAT能力。
6. 负载额定功率、输入源限流和保险丝的I²t；F5A不能替代半导体短路保护。
7. OLED/VOFA缩放、状态码、故障码和上板验收清单。

任何一项没有重新实测，都只能把该参数档标记为“待标定/禁止带功率”，不能仅因为程序能编译就进入闭环运行。

## 21. 本次控制优化记录

| 修改 | 原因 | 直接影响 |
| --- | --- | --- |
| 默认改为36 V轻载档+CLOSED_LOOP | 直接进行36 V、100 Ω闭环测试 | 实时条件满足后进入READY，PD0短按开放PWM |
| 电流峰值上限0.80 A改为1.00 A | 原PI上限不足以覆盖30 Ω及损耗 | PI最大指令变为0.707 A RMS，过流阈值仍为1.20 A peak |
| 电流跟踪门槛0.15 A改为0.10 A RMS | 避免跟踪很差时错误切入PI外环 | 初次调试更严格，未通过时先查极性、标定和PR参数 |
| READY增加VBUS 9.8 V上限和阻断位 | 避免接近过压时重新开放PWM | OLED/VOFA可区分模式、VAC、频率和母线阻断原因 |
| 增加工频周期限幅占比监督 | 连续计数无法识别只在正负波峰出现的周期性削顶 | 36 V轻载档单周期超过40%且连续5周期锁存`20000` |
| 增加稳态母线失调监督 | 原逻辑允许PI长期打满但VBUS无法达到目标 | 36 V轻载档PI限幅且偏差持续4 s锁存`40000` |
| 删除预充和快速OCP确认宏及字段 | 不再用不存在的硬件确认项阻断当前测试 | 功率许可只检查参数、标定确认和闭环模式 |
| 放宽36 V轻载诊断阈值 | 减少启动、频偏和采样峰值造成的误停 | VAC±20%、45～55 Hz、VBUS 43/40 V，ADC贴轨需连续64帧 |
| 放宽控制限幅判据 | 给PR接管和PI升压留出恢复时间 | 连续限幅50 ms、40%削顶连续5周期、稳压失调4 s才关断 |

2026-07-31当前交付目标为`36 V轻载+CLOSED_LOOP+标定确认1`。代码构建和受保护文件哈希结果见本次最终验证记录；Codex没有执行真实36 V功率母线或100 Ω负载上电，软件构建通过不等于实物闭环已经验收。

## 22. 36 V、100 Ω轻载直接闭环测试

### 22.1 100 Ω工作点

60 V母线接100 Ω时：

```text
输出电流 = 60 / 100 = 0.60 A
输出功率 = 60^2 / 100 = 36 W
预计输入电流约 = 36 / (36 * 效率 * PF) = 1.1~1.3 A RMS
```

这比赛题约111 W满载、约3.3 A RMS输入仍属于轻载验证。100 Ω电阻标称5 A足以覆盖0.60 A输出电流，但还要确认该额定值为连续值，并检查电阻可持续耗散约36 W；电流额定值不能代替功率和温升确认。

### 22.2 当前固定配置

```c
PFC_USER_ACTIVE_PROFILE            = PFC_USER_PROFILE_36V_LIGHT_100R
PFC_USER_RUN_MODE                  = PFC_USER_MODE_CLOSED_LOOP
PFC_USER_36V_CALIBRATION_CONFIRMED = 1U
```

本次不再要求重新执行ADC标定。当前换算值为IPFC/VAC零点2046、IPFC比例`0.003323 A/count`、VAC比例`0.02730 V/count`、VBUS比例`0.021062 V/count`，三个方向均为`+1`。

### 22.3 首次闭环操作顺序

1. 首次只选`PFC_USER_PROFILE_36V_LIGHT_100R`，不要选满功率赛题档。
2. 接通36 V RMS输入后先不要按PD0，确认自然整流母线约48～51 V、VAC约36 V RMS、频率约50 Hz、Fault为0。
3. OLED状态应从`PFC_ADC_CHECK(3)`进入`PFC_PASSIVE_PRECHARGE(4)`，约200 ms稳定后到`PFC_READY(5)`；此时PWM仍关闭。若保持`S=04`，读取第三行三位十六进制`B`并按READY阻断位表定位，只有`B=000`才会开始200 ms稳定计时。
4. PD0按下50～1000 ms后释放一次。PR电流指令从0以2 A/s升至0.20 A RMS，至少观察250 ms；误差RMS不大于0.18 A后切入PI。
5. PI接管后VBUS参考以2 V/s升至60 V，最长10 s；稳态PI限幅且母线失调持续4 s才关断。
6. 连续50 ms完全限幅在所有带功率阶段有效。40%/连续5周期的削顶保护只在`VBUS_LOOP_RUN`生效，启动阶段只统计。
7. Keil Watch观察`vac_bus_ratio`和`modulation_headroom`。后者接近0表示前馈已经接近0.90调制度边界，负值表示自然整流母线不足以无削顶复现当前VAC峰值。
8. 输入功率和PF软件估算见VOFA CH8/CH9；PF应为正且趋势接近1。CH1/IPFC应跟随CH0/VAC方向，CH3/VBUS逐渐接近CH4参考。
9. 正常停机仍是在状态6、7或8中再次短按并释放PD0；程序立即关闭四路HRTIM输出并复位PI/PR。

### 22.4 放宽后仍会取消PWM的条件

- PB10/FLT3出现低电平，HRTIM硬件直接关闭TA/TB输出。
- IPFC绝对峰值超过2.50 A，或VBUS超过66 V。
- ADC原始码连续64帧低于4或高于4091。
- ADC1/ADC2 DMA停止、严重失步或HAL报告ADC/DMA错误。
- VAC同步丢失、运行中VBUS低于40 V。
- PI/PR出现非有限数、Compare写入失败或控制路径未初始化。
- PR/调制度连续限幅50 ms，RUN中严重周期削顶连续5周期，或PI稳压失调持续4 s。

### 22.5 本次新增代码行为

| 修改 | 目的 | 结果 |
| --- | --- | --- |
| 新增36 V轻载档 | 防止反复覆盖满功率赛题参数 | 轻载和赛题档独立保存控制/保护参数，共用36 V ADC标定 |
| 删除预充与快速OCP确认链 | 按当前要求直接进行36 V轻载闭环 | 不再有相应宏、参数字段、编译错误或READY阻断位 |
| 周期限幅只在RUN关断 | 启动时VAC峰值接近自然整流母线，短时削顶可能是接管过程 | 启动仍受连续50 ms限幅、电流误差和超时保护 |
| 输入功率/PF统计 | 判断VAC/IPFC极性和有功趋势 | 每个50 Hz窗口更新，不参与保护和正式验收 |
| 前馈调制裕量 | 判断被动母线是否足以接管 | Keil Watch直接查看`vac_bus_ratio`和`modulation_headroom` |
| VOFA扩为10通道 | 保留原8路同时增加功率诊断 | 44字节/帧，460800 baud下发送时间约0.96 ms |
