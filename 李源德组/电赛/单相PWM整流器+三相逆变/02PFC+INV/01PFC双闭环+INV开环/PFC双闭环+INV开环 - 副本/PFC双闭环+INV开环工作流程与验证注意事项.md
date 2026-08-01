# PFC双闭环 + INV开环工作流程与验证注意事项

## 1. 当前固件目标和边界

本说明对应同目录的`01Final.ioc`和联合工程源码。当前固件优先完成：

- 单相PWM整流器：10 kHz PR电流内环、1 kHz PI母线外环；
- 三相三线逆变器：10 kHz CBSVPWM开环，默认60 Hz，可改30 Hz；
- PD0短按后先启动PFC，PFC进入母线运行阶段后自动投入INV；
- 主要依靠OLED、VOFA和示波器调试，不依赖Keil Watch；
- 通过分阶段试验逐步接近三相Y接负载约1.8 A线电流。

当前不使用HRTIM F和第4桥臂，不接负载中性线。INV是固定母线参数的开环控制，不能保证题目要求的`32 V ±0.1 V`、`THD <= 2%`、`PF >= 0.98`或`效率 >= 95%`。

`01Final.ioc`保持不变，当前SHA-256为：

```text
F077E1504165F45B8FB1C58A5C09164F8FBCD392AE9C1BD9C6FB909ACD252754
```

## 2. 上电后的实际执行流程

```text
HAL_Init、170 MHz系统时钟
 -> GPIO、DMA、HRTIM、ADC1~5、USART2初始化
 -> 立即关闭HRTIM A~E全部输出
 -> OLED上电等待100 ms并初始化
 -> 初始化PFC参数、PI/PR、INV测量、DDS和联合状态机
 -> ADC5、4、3、2、1依次执行单端校准
 -> 根据标定确认状态配置ADC模拟看门狗
 -> ADC5、4、3、2、1依次启动循环DMA
 -> 关闭五路DMA Half Transfer中断
 -> 一次性启动HRTIM Master和A~E计数器
 -> 最多等待200 ms，确认五路采样序列均开始更新
 -> 最后启动IWDG
 -> 主循环执行1 ms状态机、10 ms VOFA、100 ms OLED和100 ms监督
```

HRTIM计数器启动不等于PWM引脚已经开放。RAW_ADC阶段中，Master和A~E计数器继续产生ADC触发，但TA1/2、TB1/2、TC1/2、TD1/2、TE1/2均保持关闭。

IWDG只在当前活动域的采样、控制心跳、状态和输出许可一致时刷新。致命故障后程序停止刷新IWDG，等待复位。

## 3. 唯一用户配置文件

日常只修改：

```text
Core/Inc/pfc_inv_user_config.h
```

`pfc_user_config.h`和`inv_user_config.h`只是旧模块兼容包装，不应重复填写参数。

### 3.1 默认安全配置

```c
#define PFC_INV_STAGE                  PFC_INV_STAGE_RAW_ADC
#define PFC_INV_ACTIVE_PROFILE         PFC_INV_PROFILE_36V
#define PFC_INV_PFC_36V_CALIBRATION_CONFIRMED 0U
#define PFC_INV_INV_CALIBRATION_CONFIRMED     0U
#define PFC_INV_PWM_ENABLE             0U
#define PFC_INV_PFC_RELAXED_PWM_TEST   1U
```

这个默认值只能采样和显示，PD0不能开放任何PWM。36 V PFC参数和INV六路参数未经实物复核前，确认位必须保持0。

### 3.2 测试阶段

| 阶段 | 功能 | 活动输出 |
| --- | --- | --- |
| `PFC_INV_STAGE_RAW_ADC` | 五路ADC/DMA、OLED、VOFA标定 | 全部关闭 |
| `PFC_INV_STAGE_PFC_ONLY` | 只运行PFC双闭环 | A/B |
| `PFC_INV_STAGE_INV_ONLY` | 只运行固定60 V参数的INV开环 | C/D/E |
| `PFC_INV_STAGE_JOINT_LOW_POWER` | PFC先运行，随后自动投入INV | A/B后C/D/E |
| `PFC_INV_STAGE_JOINT_CONTEST` | 36 V参数联合验证 | A~E，必须重新标定 |

功率阶段还必须同时满足`PFC_INV_PWM_ENABLE=1U`和对应ADC标定确认位。编译期检查会拒绝明显不完整的组合。

### 3.3 关键PFC参数

36 V档当前重要参数：

| 参数 | 当前值 | 修改影响 |
| --- | ---: | --- |
| 输入 | 36 V RMS / 50 Hz | 改变后要重算同步、VAC量程和保护阈值 |
| 母线目标 | 60 V | 同时影响PI目标和INV实际输出条件 |
| 最大电流指令 | 4.8 A peak | 正弦RMS约3.39 A；增大后器件、电感和采样量程都要复核 |
| 电流斜率 | 1 A/s | 越大启动越快，但母线和电流冲击更大 |
| 调制度限制 | 0.90 | 越接近1越容易进入非线性和死区失真 |
| PR | Kp=5、Kr=20 | 仅为现有10 kHz固件起点，必须根据实测波形整定 |
| PI | Kp=0.30、Ki=15 | 增大可加快母线响应，也会增加振荡风险 |

当前目标是先得到基本整流和足够功率，不代表这组PI/PR参数已经完成最终整定。

### 3.4 关键INV参数

```c
#define PFC_INV_INV_FIXED_DC_BUS_V     60.0f
#define PFC_INV_INV_LINE_RMS_V_36V     32.0f
#define PFC_INV_INV_OUTPUT_FREQUENCY   60U
#define PFC_INV_INV_SOFT_START_MS      1000U
#define PFC_INV_INV_MODULATION_LIMIT   0.90f
```

INV每个控制周期使用固定`60.0 V`计算CBSVPWM，不使用ADC1实测VBUS作为调制度分母。实际母线与60 V存在偏差时，实际线电压会近似按比例偏差。

32 V RMS线电压、60 V直流母线的理论调制度为：

```text
m = 2 * Vline_rms * sqrt(2/3) / Vdc
  = 2 * 32 * sqrt(2/3) / 60
  ≈ 0.871
```

该值已接近0.90上限。若线电压不足，应先测量实际母线、驱动压降和死区影响，不能直接无限提高调制度上限。

频率只允许30或60 Hz。改变频率不会改变线电压目标，但30 Hz时电感阻抗下降、磁性器件伏秒和负载电流可能变化，必须重新观察电流与温升。

## 4. ADC、DMA和控制数据流

### 4.1 Rank映射

| ADC | Rank 1 | Rank 2 | DMA |
| --- | --- | --- | --- |
| ADC1 | IPFC | VBUS | DMA1 Channel 1 |
| ADC2 | VAC | 无 | DMA1 Channel 2 |
| ADC3 | IU | VUV | DMA2 Channel 1 |
| ADC4 | IV | VVW | DMA2 Channel 2 |
| ADC5 | IW | VWU | DMA2 Channel 3 |

原始数组映射固定为：

```text
PFC_Adc1Dma[0]=IPFC  PFC_Adc1Dma[1]=VBUS
PFC_Adc2Dma[0]=VAC
INV_Adc3Dma[0]=IU    INV_Adc3Dma[1]=VUV
INV_Adc4Dma[0]=IV    INV_Adc4Dma[1]=VVW
INV_Adc5Dma[0]=IW    INV_Adc5Dma[1]=VWU
```

DMA数组由硬件持续改写。OLED、VOFA、状态机和控制器只读取模块发布的一致性快照，不直接读取DMA数组。

### 4.2 快速路径

```text
ADC2完整回调 -> 推进VAC序列
ADC1完整回调 -> 发布PFC同步快照 -> 10 kHz PR -> 每10帧执行1 kHz PI

ADC3/4/5各自完整回调
 -> 三路序列均前进后发布INV六通道快照
 -> DDS三相参考 + 1 s软启动
 -> CBSVPWM_Calc3Leg()
 -> HRTIM C/D/E Compare预装载
```

DMA Half Transfer不代表完整Rank帧，已在启动阶段关闭。ISR中禁止OLED、VOFA、阻塞USART和`HAL_Delay()`。

## 5. ADC标定流程

### 5.1 PFC已有参数状态

5 V档已迁移独立PFC工程的实测参数：

| 量 | 零点/比例 |
| --- | ---: |
| IPFC | 2046 count，0.003322 A/count |
| VAC | 2046 count，0.02730 V/count |
| VBUS | 0.02106 V/count |
| IPFC/VAC/桥臂极性 | +1/+1/+1 |

36 V档暂存相同传递系数，但确认位仍为0。VAC 36 V RMS的峰值约50.9 V，接近当前VAC理论量程上部，必须检查波峰削顶；不能因为5 V档已确认就直接继承36 V功率许可。

### 5.2 PFC复核步骤

1. 保持RAW_ADC、PWM=0，不接驱动功率。
2. IPFC和VAC零输入时观察VOFA统计页，mean应接近2046，min/max抖动应较小。
3. 给VAC通道施加已知隔离交流电压，计算`V/count = 已知峰值 / ((max-min)/2)`。
4. 给IPFC通道施加已知正向电流，计算`A/count = 已知电流 / abs(raw-zero)`。
5. 给VBUS通道施加多个已知直流电压点，计算`V/count = 已知电压 / mean(raw)`。
6. 对0%、25%、50%、75%和接近工作上限做多点线性复核。
7. 已知正方向换算成负数时，只修改对应`POLARITY`为-1，比例始终填正数。
8. 保持PWM=0切到工程阶段，用万用表和示波器复核工程量后，才把当前档确认位置1。

### 5.3 INV六路标定

INV默认的2048零点、0.001 A/count和0.010 V/count只是占位值。

1. RAW_ADC阶段使六路输入处于确定安全电平，禁止悬空。
2. 记录IU、IV、IW、VUV、VVW、VWU的零输入平均码，分别填写六个`OFFSET`。
3. 三个电流通道分别施加已知正向电流，独立计算A/count。
4. 三个线电压通道分别施加已知正向线电压，独立计算V/count。
5. 改变一路输入时，确认只有对应原始码明显变化，排除接错Rank。
6. 检查正方向；如符号相反，只改该通道`POLARITY`。
7. 保持PWM=0复核六个工程量，无贴轨和明显削顶后，才设置`PFC_INV_INV_CALIBRATION_CONFIRMED=1U`。

## 6. PD0和联合状态流程

PD0为内部上拉、低有效按键。程序每1 ms轮询，稳定30 ms完成消抖；50~1000 ms按下后释放产生一次短按。上电必须先稳定检测到释放，避免按住按键上电后松开而误启动。

联合阶段状态：

```text
SAFE -> ADC_CALIBRATION -> READY
短按PD0
 -> PFC_CURRENT_LOOP
 -> PFC_VBUS_RAMP
 -> PFC_VBUS_STABLE
 -> INV_SOFT_START
 -> JOINT_RUN
```

在`PFC_INV_PFC_RELAXED_PWM_TEST=1U`时，PFC进入母线运行阶段且PFC测量快照仍有效后，联合层累计约500 ms并投入INV，不再要求实测VBUS严格落在目标容差内。INV投入时先写三相50%占空比、开放C/D/E，再用约1 s软启动从零调制度上升。

运行中再次短按执行正常停机。任何锁存致命故障后，按键不能清故障，必须排除原因并复位MCU。

## 7. 放宽保护模式

`PFC_INV_PFC_RELAXED_PWM_TEST=1U`用于时间紧张时的隔离限流波形调试。它会降低软件诊断对PWM许可的耦合，但不会使功率级变安全。

### 7.1 只用于观察、不单独关波

| 项目 | 放宽模式行为 |
| --- | --- |
| VAC幅值、频率或过零不同步 | 不阻止READY，不因该项单独停机 |
| VBUS低于启动门槛或未进入目标容差 | PFC继续尝试，联合投入使用宽松时序 |
| 电流跟踪未通过、目标建立超时 | 不因该项单独停机 |
| ADC原始码贴轨 | 不锁存范围故障；通过OLED/VOFA检查 |
| ADC模拟看门狗越窗 | 回调不升级为联合停机 |
| 软件过流、软件过压 | 不因阈值越界单独停机 |
| PR或调制度持续限幅 | 计数饱和但不因限幅单独停机 |
| INV采样贴轨或AWD越窗 | 不因该项单独关闭C/D/E |

### 7.2 仍然必须立即关闭A~E

| 致命项目 | 原因 |
| --- | --- |
| ADC/DMA HAL错误、序列失步或停止 | 控制输入链路已失效 |
| 非有限浮点数 | Compare命令不可预测 |
| SPWM/CBSVPWM计算失败 | 调制命令无效 |
| HRTIM Compare写入或输出启动失败 | PWM硬件状态不可信 |
| PB10/HRTIM FLT3 | 硬件Fault路径 |
| CSS、NMI、HardFault或不可恢复HAL错误 | CPU/时钟执行不可信 |
| 活动控制心跳停止 | 10 kHz控制链路不再前进 |

PB10外部目前未接真实OCP/DESAT，UCC21520板也没有返回给MCU的nFAULT。软件放宽、T5A保险丝和ADC采样都不能阻止MOSFET直通的微秒级破坏。带功率必须使用隔离限流电源并准备物理断电。

## 8. OLED和VOFA

### 8.1 OLED

OLED每100 ms在主循环刷新：

```text
P:VAC原始码   I:IPFC原始码
B:VBUS原始码  U:IU原始码
S:联合状态号  D:INV调制度*1000
F:合并故障码  H:INV心跳低5位
```

注意：前两行当前显示的是ADC原始码，不是伏特或安培。RAW_ADC时D接近0，H应随INV采样帧前进；联合运行时D从0逐步升到约871附近。故障码非0时先停电，再根据第10节排查。

### 8.2 VOFA

USART2使用460800 baud，主循环每10 ms发送一次8通道JustFloat数据。串口发送失败不参与PWM许可。

RAW_ADC阶段交替发送两页：

| CH | 第0页：PFC统计 | 第1页：INV原始码 |
| ---: | --- | --- |
| 0 | 0，页号 | 1，页号 |
| 1 | IPFC mean | IU |
| 2 | IPFC min | IV |
| 3 | IPFC max | IW |
| 4 | VAC mean | VUV |
| 5 | VAC min | VVW |
| 6 | VAC max | VWU |
| 7 | VBUS mean | INV故障位 |

非RAW阶段：

| CH | 数据 |
| ---: | --- |
| 0 | VAC，V |
| 1 | IPFC，A |
| 2 | VBUS，V |
| 3 | VUV，V |
| 4 | INV U相归一化正弦参考 |
| 5 | INV U相调制度 |
| 6 | 联合状态号 |
| 7 | PFC、INV和联合故障位组合 |

10 ms发送周期只有100 Hz显示带宽，不适合准确判断10 kHz开关波形，也可能让50/60 Hz曲线点数较少。PWM、死区、相位和THD必须以示波器或功率分析仪为准。

## 9. 分阶段上板流程

任何一级未通过，都不要进入下一级。

### P0：编译和无功率RAW_ADC

1. 保持默认RAW_ADC、36 V档、PWM=0。
2. 编译并下载，确认OLED和VOFA更新。
3. 确认五路ADC原始码均在0~4095且不长期贴轨。
4. 改变一路模拟输入，确认只有对应通道明显变化。
5. PD0无论如何操作，A~E都不应输出PWM。

### P1：MCU引脚PWM

1. 不连接驱动板和功率母线。
2. 选择PFC-only或INV-only，完成对应标定确认并临时设PWM=1。
3. 确认A/B或C/D/E为10 kHz中心对齐互补PWM。
4. 确认死区约500 ns，启动前占空比为50%。
5. 检查上下管映射，不能只按线色判断。

### P2：驱动板无母线

1. 只接驱动板逻辑和隔离侧电源，不接功率母线。
2. 先测PFC A/B，再测INV C/D/E。
3. Gate必须相对各自Source测量，高侧使用差分或隔离探头。
4. 确认无上下管重叠、幅值正常、关断可靠。

### P3：PFC低功率

1. 使用隔离、限流交流源，先用60~100 ohm直流负载。
2. 先PFC-only，限流从较小值开始。
3. 短按PD0，观察IPFC与VAC是否基本同相，VBUS是否可控上升。
4. 检查PR/SPWM是否长期饱和、母线是否快速过冲、器件是否异常发热。
5. 逐步降低负载，不要一步切到满功率。

### P4：INV独立低功率

1. PFC关闭，使用稳定、隔离、限流的直流母线。
2. 先从低于60 V和较大每相电阻开始，实际输出线电压会随母线同比降低。
3. 三相负载采用Y接且中性点悬浮，不接第4桥臂。
4. 验证60 Hz相序U->V->W、三相相差120度和软启动。
5. 再短时验证30 Hz，重新观察电流和温升。

### P5：联合低功率

1. PFC和INV标定分别确认，使用联合低功率阶段。
2. 短按PD0后，先看到A/B开放，C/D/E保持关闭。
3. PFC进入VBUS运行阶段并稳定计时约500 ms后，C/D/E从50%开始软启动。
4. OLED的状态号依次推进，INV调制度逐步增加，两套采样和控制心跳持续更新。
5. INV投入瞬间若VBUS明显塌陷，立即人工停机并减小INV负载。

### P6：逐步接近1.8 A

三相Y接、32 V RMS线电压时：

```text
Vphase_rms = 32 / sqrt(3) ≈ 18.48 V
Rphase     = 18.48 / 1.8 ≈ 10.27 ohm
Pout       = 3 * 18.48 * 1.8 ≈ 99.8 W
```

因此最终约1.8 A目标对应每相约10.3 ohm的对称阻性负载，每相功率约33.3 W，电阻额定功率必须留足余量。考虑损耗后，36 V输入侧电流约需3 A RMS；PFC当前4.8 A peak指令上限对应约3.39 A RMS，只留有限余量。

实际步骤：

1. 从每相更大阻值开始，记录Vline、Iline、VBUS、VAC和IPFC。
2. 逐级降低三相对称负载，每一级只短时运行并检查温升。
3. 确认PFC输入电流没有持续削顶，母线没有随INV投入大幅塌陷。
4. 接近10.3 ohm时使用功率分析仪测三相电流，不用软件换算代替仪器。
5. 先完成60 Hz，再单独测试30 Hz；两种频率分别记录温升。

## 10. 故障和现象排查

联合故障位：

| 位 | 含义 |
| --- | --- |
| `PFC_INV_FAULT_PFC` | PFC活动域出现致命故障 |
| `PFC_INV_FAULT_INV` | INV活动域出现致命故障 |
| `PFC_INV_FAULT_SEQUENCE` | 启动时五路序列未建立 |
| `PFC_INV_FAULT_HRTIM` | HRTIM启动、Fault或Compare失败 |
| `PFC_INV_FAULT_SYSTEM` | 系统、时钟或不可恢复HAL错误 |

| 现象 | 优先检查 |
| --- | --- |
| OLED不亮 | PA15/PB7软件I2C、OLED供电、100 ms上电等待 |
| OLED更新但PD0无效 | 当前是否RAW_ADC、PWM是否为0、按键是否先释放解锁 |
| VOFA无数据 | USART2 460800、JustFloat、串口地和通道数 |
| RAW_ADC某路为0或4095 | 采样板供电、信号悬空、接地、连接器和ADC Rank |
| PFC有PWM但母线不上升 | 桥臂极性、VAC/IPFC极性、输入限流、负载过重、PR饱和 |
| PFC母线快速过冲 | PI方向/参数、VBUS比例、桥臂极性，立即断电 |
| INV有PWM但线电压不足 | 实际VBUS不是60 V、死区/驱动压降、调制度限幅 |
| INV三相电流不平衡 | 三相负载、电感、桥臂映射、采样极性和功率管状态 |
| 联合时INV不投入 | PFC是否进入VBUS_LOOP_RUN、INV确认位、PWM总许可、PFC快照valid |
| 反复IWDG复位 | ADC/DMA序列或活动控制心跳停止，不能通过延长IWDG掩盖 |
| 拉低PB10后全关 | 这是预期的FLT3关断；排除原因后复位，禁止自动重启 |

## 11. 升功率前的禁止条件

出现任一项都禁止继续升功率：

- ADC通道未完成多点标定或高量程削顶；
- MCU互补PWM、死区和上下管映射未用示波器确认；
- 驱动板无母线测试存在重叠、振铃或关断异常；
- 没有隔离限流源、保险丝、放电电阻或物理断电手段；
- 示波器接地方式不明确；
- PFC母线过冲、输入电流严重削顶或PR长期饱和；
- INV三相明显不平衡、母线投入时塌陷或器件快速发热；
- PB10未接真实保护，却误认为软件放宽可以替代OCP/DESAT。

## 12. 当前验证记录

- Keil工程已恢复`MDK-ARM/stm32g474xx_flash.sct`，布局为Flash `0x08000000/0x40000`、RAM `0x20000000/0x20000`。
- 当前构建结果：`0 Error(s), 0 Warning(s)`，已生成HEX。
- 镜像大小：`Code=38976`、`RO-data=2916`、`RW-data=348`、`ZI-data=3868`。
- 默认交付仍为`RAW_ADC + 36 V档 + PWM=0`，不会上电自动发波。
- 本次只完成静态检查和Keil构建；ADC标定、PWM波形、带功率整流、三相逆变和1.8 A目标仍必须按第9节逐级上板验证。
