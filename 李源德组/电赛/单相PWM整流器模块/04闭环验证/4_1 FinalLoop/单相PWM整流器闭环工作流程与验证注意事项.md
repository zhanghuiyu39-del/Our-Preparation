# 单相PWM整流器闭环工作流程与验证注意事项

## 1. 当前工程与安全默认值

当前`4_1 FinalLoop`工程用于分阶段完成单相PWM整流器ADC标定、工程量复核和PI+PR双闭环调试。用户参数只允许在以下文件修改：

```text
Core/Inc/pfc_user_config.h
```

不要再到`main.c`、`pfc_params.c`、`pfc_measure.c`或控制模块中查找和分散修改参数。默认交付设置为：

```c
#define PFC_USER_ACTIVE_PROFILE  PFC_USER_PROFILE_5V
#define PFC_USER_RUN_MODE        PFC_USER_MODE_RAW_ADC
#define PFC_USER_5V_CALIBRATION_CONFIRMED  0U
```

该默认值会运行ADC、DMA、HRTIM采样时基、OLED、VOFA和IWDG，但强制禁止PA8～PA11四路PWM输出。无论如何操作PD0都不会启动功率级。

5 V低压档的目标参数为：

- 隔离交流输入：5 V RMS、50 Hz。
- 直流母线目标：9.0 V。
- 负载：最终30 Ω，第一次闭环必须先使用60～100 Ω。
- PWM、ADC同步采样和PR电流内环：10 kHz。
- 100 Hz母线陷波器和PI电压外环：1 kHz。
- HRTIM Timer A/B：PA8～PA11中心对齐互补PWM。
- ADC1 DMA：Rank 1 IPFC、Rank 2 VBUS；ADC2 DMA：Rank 1 VAC。

9 V接30 Ω时负载功率约为2.7 W，忽略损耗所需输入电流已约0.54 A RMS，而当前PI上限约0.566 A RMS。实际损耗会进一步缩小裕量，所以30 Ω不能作为第一次闭环负载。

## 2. 只改一个文件的使用流程

### 2.1 第一步：原始ADC标定

保持默认三行配置不变，编译并上板。此阶段：

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

只修改5 V档的以下宏：

```c
PFC_USER_5V_IPFC_ZERO_COUNT
PFC_USER_5V_VAC_ZERO_COUNT
PFC_USER_5V_IPFC_A_PER_COUNT
PFC_USER_5V_VAC_V_PER_COUNT
PFC_USER_5V_VBUS_V_PER_COUNT
PFC_USER_5V_IPFC_POLARITY
PFC_USER_5V_VAC_POLARITY
PFC_USER_5V_BRIDGE_POLARITY
```

完成万用表、示波器和接线方向复核后，将：

```c
#define PFC_USER_5V_CALIBRATION_CONFIRMED  1U
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

### 2.4 第四步：闭环调试

所有工程量、极性和保护阈值复核通过后才改为：

```c
#define PFC_USER_RUN_MODE  PFC_USER_MODE_CLOSED_LOOP
```

先接60～100 Ω负载验证PR电流环，再观察PI外环，最后按100 Ω、60 Ω、47 Ω、30 Ω逐步降低负载。每次更换负载前必须断开输入并确认母线已经放电。

### 2.5 36 V赛题档

36 V档的完整参数骨架也集中在`pfc_user_config.h`，但默认确认值为0。未完成36 V采样比例、保护阈值、器件耐压、限流和独立短路保护实测前，不得把活动档切到36 V闭环模式。5 V档标定结果不能直接复制。

## 3. 三种运行模式

| 模式 | 工程量 | AWD运行阈值 | PD0 | PWM输出 |
| --- | --- | --- | --- | --- |
| `PFC_USER_MODE_RAW_ADC` | 无，只发布原始统计 | 保留CubeMX宽窗口 | 忽略 | 强制关闭 |
| `PFC_USER_MODE_ENGINEERING_CHECK` | IPFC/VAC/VBUS、RMS、频率 | 按实测参数配置 | 忽略 | 强制关闭 |
| `PFC_USER_MODE_CLOSED_LOOP` | 全部有效工程量与控制遥测 | 按实测参数配置 | 短按启停 | 条件满足后允许开放 |

参数接口的含义已经分开：

- `PFC_Params_Validate()`：参数数值是否自洽。
- `PFC_Params_CalibrationReady()`：ADC工程量是否可用。
- `PFC_Params_PowerAllowed()`：是否为已确认标定的闭环模式。
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
| 3 | `PFC_ADC_CHECK` | 闭环模式检查同步快照至少200 ms |
| 4 | `PFC_PASSIVE_PRECHARGE` | 等待VAC、频率和VBUS条件连续500 ms |
| 5 | `PFC_READY` | PWM关闭，等待一次有效PD0短按 |
| 6 | `PFC_CURRENT_LOOP_RAMP` | PR内环执行0.20 A RMS探测斜坡 |
| 7 | `PFC_VBUS_LOOP_RAMP` | PI无扰投入，VBUS参考按1 V/s上升 |
| 8 | `PFC_VBUS_LOOP_RUN` | 9 V目标附近稳态双闭环 |
| 9 | `PFC_STOP` | 人工停机，PWM关闭后重新判断条件 |
| 10 | `PFC_FAULT_LATCH` | 故障锁存，禁止自动恢复 |

闭环模式进入READY前要求：工程量有效、VAC RMS为额定值正负10%、频率47～53 Hz、VAC同步锁定、VBUS不低于5 V且无故障。

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
V:VAC瞬时(0.01V) R:VAC RMS(0.01V)
I:IPFC瞬时(mA)  R:IPFC RMS(mA)
D:VBUS(0.01V)   H:频率(0.01Hz)
E:参数错误       F:故障码
```

闭环模式：

```text
I:IPFC(mA)      V:VAC(0.01V)
D:VBUS(0.01V)   R:VBUS参考(0.01V)
M:调制度*10000  S:状态
F:故障码         H:控制心跳后5位
```

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

工程量复核模式：

| 通道 | 数据 |
| ---: | --- |
| CH0 | VAC，V |
| CH1 | VAC RMS，V |
| CH2 | IPFC，A |
| CH3 | IPFC RMS，A |
| CH4 | VBUS，V |
| CH5 | VAC频率，Hz |
| CH6 | 参数校验错误位 |
| CH7 | 状态和故障编码 |

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

CH7计算为：

```text
CH7 = PFC_State * 1000000 + fault_bits
```

VOFA的10 ms周期等于100 Hz，对50 Hz波形每周期只有约两个显示点，不能用于判断完整正弦度、THD或精确相位。波形质量必须使用示波器/电流探头，原始峰值标定使用一秒统计。不得把阻塞式`VOFA_Send()`移入ADC ISR。

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

电流环至少观察60 ms并完成一个50 Hz误差窗口。误差RMS不超过0.15 A才进入PI外环，500 ms内未通过则锁存故障。

PI外环每10个ADC帧运行一次：

```text
VBUS -> 100 Hz陷波 -> 1 kHz PI -> RMS电流指令
```

切入时预置PI输出以减小跳变，VBUS参考从当前值以1 V/s升至9 V。8 s内未进入8.5～9.5 V则锁存目标超时。保护仍使用未陷波VBUS，避免陷波延迟掩盖过压。

正常运行时，控制快速心跳约10 kHz，外环心跳约1 kHz；PR、PI和调制度不应持续限幅。IPFC与VAC反相或电流越控越大时，立即停机检查三个极性与全桥接线，不能靠增大控制器增益掩盖错误。

## 10. 分阶段上板准入

### A. 原始ADC，无功率

1. 使用默认RAW模式和未确认标定。
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
2. 先接60～100 Ω足额功率负载。
3. 观察PR探测阶段IPFC与VAC同相且不持续限幅。
4. 观察VBUS参考和实测值平滑上升。
5. 检查MOSFET、驱动器、电感、负载温升和输入电流。

### E. 逐步降到30 Ω

按100 Ω、60 Ω、47 Ω、30 Ω逐步验证。若PI长期达到0.566 A RMS或调制度持续限幅，不得直接提高阈值，应先检查效率、输入跌落、采样量程和独立过流保护。

## 11. 故障码

| 十六进制 | 故障 | 常见原因 |
| ---: | --- | --- |
| `00001` | ADC同步 | DMA停止或ADC1/2失步 |
| `00002` | ADC/DMA HAL错误 | Overrun、DMA或HAL状态异常 |
| `00004` | ADC贴轨 | 悬空、前端饱和、量程或接线错误 |
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
| `20000` | 持续限幅 | PR或调制度连续限幅约20 ms |

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
control_telemetry.current_loop_qualified
control_telemetry.healthy
```

暂停CPU会停止回调和喂狗，可能导致IWDG复位；不能把调试暂停后的序列停滞当作固件DMA故障。

## 13. 保护边界

- 当前驱动板没有MCU Gate Enable；开放HRTIM四路输出就是软件功率许可边界。
- PB10虽配置低有效FLT3并有内部上拉，但外部未接OCP、DESAT或nFAULT，不能计入实际短路保护链。
- ADC AWD、软件过流和控制限幅都有采样及处理延迟，不能替代独立微秒级硬件保护。
- 首次带功率必须使用隔离、限流的5 V交流源，禁止直接连接市电或非隔离调压器。
- 普通示波器地夹不得接到浮动桥臂节点，高侧测量使用差分或隔离探头。
- 30 Ω在9 V时耗散约2.7 W，负载额定功率至少5 W并关注温升。
- 每次只修改一组PI/PR或保护参数，并保存VAC、IPFC、Iref、VBUS和调制度波形。
- OLED、VOFA和`HAL_Delay()`禁止进入ADC DMA、AWD或HRTIM Fault中断。

本次重构没有修改`.ioc`、CubeMX生成的ADC/DMA/HRTIM/GPIO初始化或Simulink模型，也没有改变10 kHz采样、PWM死区、DMA Rank和Fault路由。
