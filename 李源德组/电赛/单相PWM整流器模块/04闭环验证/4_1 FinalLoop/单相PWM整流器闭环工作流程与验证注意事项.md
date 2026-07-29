# 单相PWM整流器闭环工作流程与验证注意事项

## 1. 当前闭环测试目标

当前`4_1 FinalLoop`工程用于验证单相PWM整流器PI+PR双闭环：

- 交流输入：5 V RMS、50 Hz，必须使用隔离且具有限流能力的电源。
- 直流负载：30 Ω，建议额定功率至少5 W。
- 直流母线目标：9.0 V，稳态判断范围为8.5～9.5 V。
- PWM、ADC同步采样和PR电流内环频率：10 kHz。
- 100 Hz母线陷波器和PI电压外环频率：1 kHz。
- HRTIM桥臂：Timer A和Timer B，输出PA8～PA11四路中心对齐互补PWM。
- ADC1 DMA：IPFC、VBUS；ADC2 DMA：VAC。
- PD0：低有效人工启动/停止按键。
- PB10：保留低有效HRTIM Fault 3功能，当前外部不接保护信号，由CubeMX内部上拉保持无故障电平。

当前默认`calibration_valid=0`。在IPFC/VAC零点、三个采样比例和三个极性完成实测前，程序只允许ADC、DMA、OLED和VOFA工作，任何PD0操作都不能开放PWM。

9 V母线接30 Ω负载时，负载功率约为：

```text
Pout = 9^2 / 30 = 2.7 W
```

忽略损耗时，5 V RMS输入至少需要约`2.7/5=0.54 A RMS`。当前PI电流指令上限约为`0.8/sqrt(2)=0.566 A RMS`，理论裕量只有约4.8%；加入MOSFET、驱动器、电感和整流损耗后，母线可能无法稳定达到9 V并触发限幅或目标超时。因此第一次闭环调试应先使用60～100 Ω轻载，确认极性和环路正常后再逐步降到30 Ω。

## 2. 上电初始化流程

```text
HAL和170 MHz系统时钟初始化
-> GPIO、DMA、HRTIM、ADC1/2、USART2初始化
-> 强制关闭HRTIM Timer A/B四路输出
-> OLED等待100 ms并初始化
-> 读取5 V参数档
-> 初始化测量模块、PI、PR、100 Hz陷波器和状态机
-> 校准ADC2，再校准ADC1
-> 根据有效标定参数配置ADC模拟看门狗
-> 先启动ADC2/VAC循环DMA
-> 再启动ADC1/IPFC+VBUS循环DMA
-> 关闭两路DMA Half Transfer中断
-> 启动HRTIM Master、Timer A和Timer B计数器
-> HRTIM只产生ADC触发，四路PWM输出继续关闭
-> 最后启动约500 ms IWDG
-> 主循环进入1 ms状态机、10 ms VOFA、100 ms OLED和安全监督
```

必须区分以下两种状态：

- HRTIM计数器已运行：Master CMP2持续产生ADC Trigger 1，ADC/DMA可正常采样。
- HRTIM输出已开放：TA1/TA2/TB1/TB2开始输出PWM，当前板卡没有独立MCU Gate Enable，此时可能直接驱动功率器件。

## 3. 应用状态机工作流程

Keil Watch中的`app_state`应按以下顺序变化：

| 数值 | 状态 | 正常行为 |
| ---: | --- | --- |
| 0 | `PFC_SAFE` | PWM关闭；普通复位等待100 ms，IWDG复位后等待1 s |
| 1 | `PFC_CALIBRATION` | 只观察ADC原始码；标定无效时一直停留在这里 |
| 2 | `PFC_ADC_CHECK` | 标定有效后检查同步快照，至少持续200 ms |
| 3 | `PFC_PASSIVE_PRECHARGE` | 等待VAC、频率和VBUS条件连续满足500 ms |
| 4 | `PFC_READY` | PWM关闭，等待PD0完成一次有效短按 |
| 5 | `PFC_CURRENT_LOOP_RAMP` | 开放PWM，PR电流环先进行0.20 A RMS探测 |
| 6 | `PFC_VBUS_LOOP_RAMP` | 无扰投入PI外环，VBUS参考以1 V/s爬升到9 V |
| 7 | `PFC_VBUS_LOOP_RUN` | 进入9 V目标附近的稳态双闭环 |
| 8 | `PFC_STOP` | 人工停机，PWM关闭，100 ms后重新判断READY条件 |
| 9 | `PFC_FAULT_LATCH` | 故障锁存，PWM关闭，不允许软件自动恢复 |

进入`PFC_READY`前必须同时满足：

- `calibration_valid=1`且所有参数通过范围检查。
- `raw_valid=1`、`valid=1`、`vac_locked=1`。
- VAC有效值在4.5～5.5 V范围内。
- VAC频率在47～53 Hz范围内。
- VBUS不低于5.0 V。
- 上述条件连续稳定500 ms。

## 4. PD0启动和停止操作

PD0使用内部上拉，按键按下时接地，属于低有效输入。程序采用1 ms轮询，不使用EXTI。

正常操作方法：

1. 上电时不要按住PD0。
2. 程序必须先检测到PD0连续释放30 ms，才会解锁按键。
3. 等待状态进入`PFC_READY`。
4. 按住PD0至少50 ms但不超过1000 ms，然后释放。
5. 释放后只产生一次启动事件，状态进入`PFC_CURRENT_LOOP_RAMP`。
6. 在状态5、6或7再次完成一次有效短按，可执行人工停机。

注意事项：

- 按住按键上电再释放不会产生启动事件。
- 小于50 ms的按压按抖动处理，不执行启停。
- 超过1 s的长按不执行动作。
- `PFC_FAULT_LATCH`状态不能通过PD0清故障，只能断电排查后复位MCU。
- PD0只是运行命令，不是急停或硬件短路保护。

## 5. ADC与DMA工作流程

### 5.1 采样数据流

```text
HRTIM Master CMP2
-> HRTIM ADC Trigger 1，10 kHz
-> ADC2采集VAC并由DMA写入PFC_Adc2Dma[0]
-> ADC2完整回调只递增adc2_sequence
-> ADC1依次采集IPFC、VBUS
-> DMA写入PFC_Adc1Dma[0]和[1]
-> ADC1完整回调核对ADC2序列
-> 发布PFC_Measurement一致性快照
-> 仅从该ADC1回调进入一次10 kHz控制
```

| ADC | DMA数组 | Rank | 信号 |
| --- | --- | ---: | --- |
| ADC1 | `PFC_Adc1Dma[0]` | 1 | IPFC输入电感电流 |
| ADC1 | `PFC_Adc1Dma[1]` | 2 | VBUS直流母线电压 |
| ADC2 | `PFC_Adc2Dma[0]` | 1 | VAC交流输入电压 |

DMA原始数组由硬件持续覆盖，适合在Keil Watch中检查Rank顺序；控制、OLED和VOFA使用`pfc_measurement`一致性快照，不能直接组合正在更新的DMA数组。

### 5.2 标定无效时的正常反应

默认`calibration_valid=0`时：

- 三个DMA原始码仍应以10 kHz持续更新。
- `pfc_measurement.adc1_sequence`、`adc2_sequence`和`fast_heartbeat`应持续增加。
- `raw_valid`应为1，表示两个ADC已经形成同步原始快照。
- `offset_ready=0`、`valid=0`，这是主动安全限制，不是ADC故障。
- 状态停留在`PFC_CALIBRATION`，PD0不能开放PWM。
- OLED右侧工程量保持0，VOFA只发送三个ADC原始码。

### 5.3 完成标定后的正常反应

- IPFC和VAC为双极性采样，零电流或零电压时应稳定在各自实测零点附近，不要求必须等于2048。
- VBUS为单极性采样，输入电压升高时原始码应单调升高。
- 5 V RMS正弦VAC换算值峰值应约为正负7.07 V，`vac_rms`应接近5 V。
- `vac_frequency_hz`应接近50 Hz，完成有效正向过零检测后`vac_locked=1`。
- `ipfc`与VAC同相跟踪时，VAC正半周的电流参考和IPFC应为相同符号。
- `valid=1`后才允许状态机使用工程量和进入带功率状态。

### 5.4 ADC异常反应

- IPFC或VAC原始码连续8帧小于16或大于4079，会锁存`PFC_FAULT_ADC_RANGE`。
- VBUS原始码连续8帧大于4079，也会锁存ADC贴轨故障。
- ADC2没有在ADC1周期前发布新VAC样本，连续3次后会锁存ADC同步故障。
- ADC2序列一次前进超过2，判定采样节拍失步并锁存故障。
- HAL ADC/DMA错误、Overrun或DMA停止都会关闭PWM并停止正常喂狗。
- ADC输入不得悬空；未接模拟前端时应连接到确定且安全的电平。
- AWD阈值只有在标定有效时才根据实测比例重写；`calibration_valid=0`时保留宽窗口，不能认为硬件过流/过压保护已经生效。

## 6. PI+PR双闭环控制流程

### 6.1 10 kHz PR电流内环

每个ADC1完整同步帧只执行一次：

```text
Iref = Irms_cmd * VAC / 5 V RMS
-> current_error = Iref - IPFC
-> PR_Calc()
-> v_bridge_ref = VAC - PR输出
-> modulation = bridge_polarity * v_bridge_ref / VBUS
-> SPWM_ApplyModulation()
-> 写入Timer A/B CMP1预装载值
-> HRTIM更新边界同步生效
```

当前PR初始参数为`Kp=0.5`、`Kr=5.0`、`Wc=5 rad/s`，输出限制为正负3 V。正常时：

- `current_reference`应与VAC同相，幅值由`current_rms_command`决定。
- `current_error`应在电流环建立后逐渐减小。
- `pr_limited`不应持续为1。
- `modulation`必须位于正负0.90以内。
- `fast_heartbeat`约以10 kHz持续增加。

### 6.2 电流环探测阶段

PD0启动后，先写入50%中性Compare并开放四路PWM，然后：

- RMS电流指令从0以5 A/s增加至0.20 A RMS，理论爬升时间约40 ms。
- 至少观察60 ms，并完成一个50 Hz误差RMS窗口。
- `current_error_rms`不大于0.15 A且未持续限幅时，`current_loop_qualified=1`。
- 500 ms内未通过检查，锁存`PFC_FAULT_CURRENT_TRACKING`。

如果IPFC与VAC明显反相，或电流越控越大，应立即停机检查`ipfc_polarity`、`vac_polarity`、`bridge_polarity`和全桥接线，不能通过增大PR增益掩盖极性错误。

### 6.3 1 kHz PI电压外环

电流环通过后：

1. 100 Hz陷波器历史预置为当前VBUS，减少切换瞬态。
2. VBUS参考从当前实测值开始。
3. PI积分项预置为当前RMS电流指令，实现无扰切换。
4. VBUS参考以1 V/s上升到9.0 V。
5. PI每10个ADC同步帧运行一次，即严格为1 kHz。
6. PI输出限制为0～0.566 A RMS。
7. VBUS参考到达9 V且实测进入8.5～9.5 V后，转入稳态双闭环。
8. 8 s内未建立目标，锁存`PFC_FAULT_TARGET_TIMEOUT`。

100 Hz陷波器只用于减小单相功率二倍频纹波对PI的影响，保护逻辑仍使用未陷波的瞬时VBUS，避免过压被滤波延迟掩盖。

## 7. VOFA通道与预期反应

USART2使用460800 baud、8N1和JustFloat，每10 ms发送一次8通道数据。

### 7.1 标定模式

| 通道 | 数据 | 正常反应 |
| ---: | --- | --- |
| CH0 | IPFC原始码 | 零电流时稳定在IPFC实测零点附近 |
| CH1 | VAC原始码 | 5 V RMS输入时围绕VAC零点正弦变化 |
| CH2 | VBUS原始码 | 随母线电压单调变化 |
| CH3～CH6 | 0 | 标定无效时固定为0 |
| CH7 | 状态和故障编码 | 正常CAL状态约为1000000 |

### 7.2 闭环运行模式

| 通道 | 数据 | 正常反应 |
| ---: | --- | --- |
| CH0 | VAC，V | 约正负7.07 V峰值、50 Hz |
| CH1 | IPFC，A | 与CH0同相，幅值随外环指令变化 |
| CH2 | 瞬时电流参考，A | 与CH0同相，CH1应逐渐跟随它 |
| CH3 | VBUS，V | 由被动值逐步上升并稳定在9 V附近 |
| CH4 | VBUS参考，V | 切入外环后以约1 V/s平滑上升到9 V |
| CH5 | RMS电流指令，A | 先到0.20 A，切外环后由PI在0～0.566 A调节 |
| CH6 | 调制度 | 正负变化且绝对值不超过0.90，不应持续贴边 |
| CH7 | 状态和完整故障位 | 无故障时依次约5000000、6000000、7000000 |

CH7编码方法为：

```text
CH7 = PFC_State * 1000000 + fault_bits
状态 = floor(CH7 / 1000000)
故障位 = CH7 - 状态 * 1000000
```

### 7.3 VOFA观察限制

VOFA当前每10 ms只发送一个快照，相当于100 Hz采样50 Hz波形，每个工频周期只有两个显示点。因此：

- VAC、IPFC和电流参考可能表现为正负交替、固定在少数相位点，甚至看起来幅值不完整。
- 当前VOFA适合观察VBUS爬升、RMS电流指令、调制度限幅、状态和故障趋势。
- 不能仅凭当前VOFA曲线判断IPFC正弦度、THD或精确相位差。
- 判断50 Hz电流波形应优先使用示波器、电流探头，或后续增加10 kHz缓冲记录后在主循环分批发送。
- 不得为了增加显示点而把阻塞式`VOFA_Send()`移入ADC DMA中断。

如果VOFA没有数据，应依次检查：

1. VOFA+协议是否选择JustFloat。
2. 串口是否为460800、8N1。
3. USB转串口与控制板是否共地。
4. `vofa_tx_ok_count`是否约以100次/s增加。
5. `vofa_tx_error_count`是否保持不变。
6. ADC序列和`fast_heartbeat`是否仍在增加。

## 8. OLED和Keil Watch观察

OLED每100 ms刷新，布局含义为：

```text
I: IPFC原始码 / 电流mA
V: VAC原始码  / VAC的0.01 V
D: VBUS原始码 / VBUS的0.01 V
S: 快速心跳后5位 / 状态 / 5位十六进制故障码
```

标定无效时右侧工程量固定显示0，这是正常安全行为。`S`持续变化说明ADC1/2同步快照仍在发布；它不代表闭环已经运行。

Keil Watch建议观察：

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
pfc_measurement.vac_locked
pfc_measurement.vac_rms
pfc_measurement.vac_frequency_hz
pfc_measurement.fault_bits

app_state
control_telemetry.mode
control_telemetry.fast_heartbeat
control_telemetry.outer_heartbeat
control_telemetry.current_rms_command
control_telemetry.current_reference
control_telemetry.current_error_rms
control_telemetry.vbus_reference
control_telemetry.vbus_filtered
control_telemetry.pr_output_v
control_telemetry.modulation
control_telemetry.pi_limited
control_telemetry.pr_limited
control_telemetry.modulation_limited
control_telemetry.saturation_count
control_telemetry.current_loop_qualified
control_telemetry.vbus_reference_reached
control_telemetry.healthy
```

正常带功率时，`control_telemetry.fast_heartbeat`约以10 kHz增加，`outer_heartbeat`约以1 kHz增加，两者长期增量比例应约为10:1。

## 9. 建议的分阶段上板流程

### 阶段A：只验证ADC、DMA、OLED和VOFA

1. 保持`calibration_valid=0`。
2. 不连接功率驱动或5 V交流功率输入。
3. 给各采样通道输入确定的安全测试电平，禁止悬空。
4. 核对三个DMA Rank与实际信号一一对应。
5. 确认ADC序列和测量心跳以约10 kHz增加。
6. 确认无论如何操作PD0，PA8～PA11都不开放PWM。
7. 记录IPFC、VAC零点和三个通道的实测比例。

### 阶段B：验证标定和极性，不开放实际功率

1. 在`pfc_params.c`填写实测零点、比例和采样极性。
2. 先保持功率驱动断开，将`calibration_valid`改为1并重新编译。
3. 用已知正负输入验证IPFC和VAC换算方向。
4. 用万用表参考值验证VAC RMS和VBUS比例。
5. 确认状态能通过ADC_CHECK，但不要通过伪造ADC值绕过实际保护条件带功率。

### 阶段C：只验证HRTIM逻辑波形

1. 驱动器供电前先确认PA8～PA11复位和初始化期间无异常脉冲。
2. 使用受控采样信号满足READY条件，短按PD0。
3. 示波器确认Timer A/B为10 kHz中心对齐互补PWM。
4. 确认死区约500 ns，两个桥臂Compare同步更新。
5. 确认人工停机和软件故障会关闭四路输出。

### 阶段D：5 V隔离输入轻载闭环

1. 使用隔离、限流5 V RMS/50 Hz电源，先把电流限值设置得较低。
2. 先使用60～100 Ω、额定功率足够的负载。
3. 确认被动母线达到5 V以上并进入READY。
4. 短按PD0，先观察0.20 A RMS电流环探测阶段。
5. 确认IPFC与VAC同相、PR不持续限幅后，再允许自动进入外环。
6. 观察CH4参考以1 V/s上升，CH3 VBUS平稳跟随。
7. 母线稳定后检查器件温升、输入电流和调制度裕量。

### 阶段E：逐步接入30 Ω负载

1. 断电并确认母线放电后再更换负载。
2. 按100 Ω、60 Ω、47 Ω、30 Ω逐步降低阻值，不要一步直接满载。
3. 每一步检查PI、PR和调制度限幅状态。
4. 若30 Ω时PI长期达到0.566 A RMS上限，说明当前电流裕量不足，不能直接提高限幅继续运行。
5. 应先核对效率、输入电压跌落、电感、电流采样量程和独立过流保护，再决定是否调整电流上限。

## 10. 故障码与系统反应

| 十六进制 | 故障 | 典型原因 |
| ---: | --- | --- |
| `00001` | ADC同步故障 | 任一路DMA停止或ADC1/2序列失步 |
| `00002` | ADC/DMA HAL错误 | Overrun、DMA错误或HAL状态异常 |
| `00004` | ADC原始码贴轨 | 输入悬空、前端饱和、接线或量程错误 |
| `00008` | HRTIM故障 | 启动/Compare写入失败或Fault 3事件 |
| `00010` | 软件过流 | IPFC超过1.2 A peak |
| `00020` | 母线软件过压 | VBUS超过10.5 V |
| `00040` | VAC丢失 | VAC同步、频率或运行电压条件失效 |
| `00080` | 参数故障 | 参数范围或标定状态不合法 |
| `00100` | 调制故障 | SPWM输入、调制或Compare更新失败 |
| `00200` | 状态机故障 | 进入未定义状态 |
| `00400` | IWDG复位诊断预留 | 当前不自动锁存，仅保留位定义 |
| `00800` | 母线建立超时 | 外环8 s内未建立9 V目标 |
| `01000` | IPFC AWD越窗 | ADC1 AWD1检测过流窗口越界 |
| `02000` | VBUS AWD越窗 | ADC1 AWD2检测母线越窗 |
| `04000` | VAC AWD越窗 | ADC2 AWD1检测输入电压越窗 |
| `08000` | 控制故障 | PI/PR未初始化、NaN或控制路径失败 |
| `10000` | 电流跟踪失败 | 500 ms内电流误差RMS未通过检查 |
| `20000` | 持续控制限幅 | PR或调制度连续限幅达到20 ms |

多个故障可同时出现，最终故障码为按位或结果。故障发生后：

```text
HRTIM输出立即关闭
-> PI、PR、陷波器和软启动状态复位
-> 状态进入PFC_FAULT_LATCH
-> 软件不自动重新开放PWM
-> 停止满足IWDG刷新条件
-> 约500 ms后可能由IWDG复位
```

IWDG复位后故障位会随RAM重新初始化，OLED上的故障码可能只短暂显示。调试时应同时观察`pfc_iwdg_reset_seen`、VOFA最后数据和Keil断点，不能只依赖复位后的OLED判断原因。

## 11. 保护边界与操作安全

- 当前板没有MCU可控Gate Enable，开放HRTIM四路输出就是软件功率许可边界。
- PB10虽然保留FLT3配置并启用了内部上拉，但外部没有连接OCP、DESAT或nFAULT，因此不能自动发现功率管短路。
- PB10外部不接时应保持焊盘和测试线清洁、短小；上板前确认PB10稳定为高电平，低电平会使HRTIM硬件禁止输出。
- ADC软件过流、AWD和持续限幅检测均经过采样、转换和软件路径，不能替代驱动器独立短路保护。
- 第一次带功率必须使用隔离、限流的5 V交流源，禁止直接接入非隔离市电或自耦调压器输出。
- 示波器地夹不得直接连接浮动桥臂节点，优先使用差分探头或隔离测量。
- 更换负载、调整接线或连接探头前必须断开输入，并确认4700 uF母线电容已经放电。
- 30 Ω负载在9 V时耗散约2.7 W，不能使用普通小功率电阻长时间运行。
- `vbus_overvoltage_warn=9.8 V`只用于告警，`10.5 V`才是软件关断阈值；调试时看到接近9.8 V就应检查外环，而不是等待保护动作。
- PR、PI初始增益只是低压保守起点。每次只调整一组参数，并保存VAC、IPFC、Iref、VBUS、参考和调制度波形。
- 不得在ADC DMA、AWD或HRTIM Fault中断中调用OLED、VOFA、`HAL_Delay()`或其他阻塞函数。

## 12. 标定和后续参数修改位置

5 V参数集中在`Core/Src/pfc_params.c`的`pfc_5v_test_params`：

```c
0.001f, 0.010f, 0.010f, /* IPFC A/count、VAC V/count、VBUS V/count */
2048U, 2048U,           /* IPFC零点、VAC零点，ADC count */
1, 1, 1,                /* IPFC、VAC、全桥调制极性 */
0U                       /* calibration_valid */
```

正确修改顺序：

1. 测量并填写IPFC零电流码和VAC零电压码。
2. 用已知输入计算IPFC的`A/count`、VAC和VBUS的`V/count`。
3. 确认IPFC正方向、VAC正半周和全桥调制正方向，填写三个`+1/-1`极性。
4. 根据实测比例复核软件过流、母线过压和AWD原始码窗口。
5. 仍保持`calibration_valid=0`完成一次代码复核。
6. 最后才改为`1U`，重新编译并从轻载、限流条件开始验证。

不得只把最后的`0U`改为`1U`而继续使用占位比例和零点。这样会让状态机把错误的工程量当作真实值，同时生成错误的模拟看门狗阈值和控制指令。
