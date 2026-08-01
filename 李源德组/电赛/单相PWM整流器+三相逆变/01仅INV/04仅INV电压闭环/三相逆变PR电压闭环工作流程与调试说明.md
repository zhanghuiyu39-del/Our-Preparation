# STM32G474三相逆变PR电压闭环工作流程与调试说明

## 1. 固件用途与边界

本工程在原三相开环程序上增加三相三线电压闭环，控制结构为：

```text
60 V直流母线、32 V RMS线电压参考
-> 30/60 Hz DDS三相前馈
-> VUV/VVW/VWU线电压反馈
-> 虚拟相电压重构和Clarke变换
-> alpha/beta两路准PR校正
-> 逆Clarke变换
-> CBSVPWM
-> HRTIM C/D/E六路互补PWM
```

当前只实现逆变器电压环，不启用PFC、电流内环、PI、HRTIM F、第四桥臂或三相四线控制。`02OpenLoop.ioc`未修改，HRTIM、ADC Rank、DMA和Fault路由仍以该文件为准。

默认配置有意设置为：

```c
#define INV_USER_WORK_MODE INV_MODE_VOLTAGE_PR
#define INV_USER_VOLTAGE_CALIBRATION_CONFIRMED (0U)
```

因此下载后ADC、DMA、HRTIM计数器、OLED、VOFA和IWDG会运行，但C/D/E物理PWM输出不会开放。OLED第三行显示`CALIBRATION OFF`，第四行状态`S=3`。这不是程序卡死，而是防止占位ADC比例直接进入闭环。

## 2. 修改文件

新增：

- `Core/Inc/pr.h`、`Core/Src/pr.c`：单精度带限准PR。
- `Core/Inc/inv_voltage_pr.h`、`Core/Src/inv_voltage_pr.c`：alpha/beta双PR电压环。
- 本说明文件。

更新：

- `Core/Src/main.c`
- `Core/Inc/inv_measure.h`
- `Core/Src/inv_measure.c`
- `Core/Inc/inv_user_config.h`
- `Core/Inc/CBSVPWM.h`
- `Core/Src/inv_open_loop.c`
- `MDK-ARM/02OpenLoop.uvprojx`

明确未修改：

- `02OpenLoop.ioc`
- CubeMX生成的ADC、DMA、HRTIM初始化参数
- Simulink模型、赛题PDF和公共移植模块目录中的原始`pr.c/.h`

## 3. 外设和采样映射

| ADC | DMA | Rank 1 | Rank 2 | 完整序列频率 |
| --- | --- | --- | --- | ---: |
| ADC3 | DMA2 Channel 1 | IU | VUV | 10 kHz |
| ADC4 | DMA2 Channel 2 | IV | VVW | 10 kHz |
| ADC5 | DMA2 Channel 3 | IW | VWU | 10 kHz |

三个ADC均由HRTIM Trigger 2触发。DMA数组只由DMA写入：

```text
INV_Adc3Dma[0]=IU    INV_Adc3Dma[1]=VUV
INV_Adc4Dma[0]=IV    INV_Adc4Dma[1]=VVW
INV_Adc5Dma[0]=IW    INV_Adc5Dma[1]=VWU
```

控制、OLED和VOFA不直接读取正在改写的DMA数组，而是读取`INV_Measurement`完整快照。Half Transfer中断在启动后关闭，只有两个Rank均完成后才参与拼帧。

## 4. 上电工作流程

```text
复位
-> 初始化GPIO/DMA/HRTIM/ADC3~5/USART/OLED
-> 关闭C/D/E六路物理输出
-> 初始化测量、DDS、CBSVPWM和两路PR
-> 校准ADC5、ADC4、ADC3
-> 启动三路循环DMA并关闭HT中断
-> 启动Master和Timer C/D/E计数器
-> 采集256个零输入同步帧求offset
-> 启动IWDG
-> 检查标定确认宏
```

标定确认宏为0时进入`INV_STATE_CALIBRATION_BLOCK`，只采样和显示。标定确认宏为1且PB10/FLT3正常时，先写50%占空比并开放六路PWM，随后执行1 s前馈软启动；反馈连续有效100帧后，PR校正再用100 ms从0平滑接管。

应用状态码：

| OLED状态S | 含义 |
| ---: | --- |
| 0 | SAFE，外设尚未进入采样流程 |
| 1 | ADC启动和校准 |
| 2 | ADC同步采样已经建立 |
| 3 | 电压标定未确认，物理PWM禁止 |
| 4 | 前馈软启动或PR接管斜坡 |
| 5 | 双PR电压闭环运行 |
| 6 | FLT3或完整保护档软件故障锁存 |

## 5. 电压闭环计算

三路线电压先重构和为0的虚拟相电压：

```text
VU = (VUV - VWU) / 3
VV = (VVW - VUV) / 3
VW = (VWU - VVW) / 3

Valpha = VU
Vbeta  = (VV - VW) / sqrt(3)
```

参考值由同一DDS三相前馈做相同变换。两路PR分别处理`Valpha_ref-Valpha_fb`和`Vbeta_ref-Vbeta_fb`。PR输出是电压校正量，不是电流参考；它与Simulink中后接电流内环的电压PR用途不同，不能直接照搬模型增益。

当前初值：

| 参数 | 默认值 | 修改影响 |
| --- | ---: | --- |
| Kp | 0.05 | 增大可加快即时响应，但会放大采样噪声 |
| Kr | 0.50 | 增大可减小基波稳态误差，但更容易振荡和限幅 |
| Wc | 5 rad/s | 增大可容忍频偏，但扩大谐振作用范围 |
| 校正矢量上限 | 0.75 V peak | 受60 V母线与0.90调制度余量限制 |
| 恢复确认 | 100帧 | 10 kHz下为10 ms |
| 接管斜坡 | 100 ms | 防止反馈恢复时校正突跳 |

调参顺序固定为：先确认三路反馈极性，再保持`Kr=0`验证比例方向，然后逐步增加`Kp`，最后从小到大增加`Kr`。一次只修改一个参数，并同时观察VOFA参考、反馈、校正和示波器输出。

## 6. 调制度、占空比与Compare

三相对称输出的相电压峰值为：

```text
Vphase_peak = Vline_rms * sqrt(2/3)
            = 32 * sqrt(2/3)
            = 26.13 V
```

60 V母线下前馈调制度约为：

```text
m = 2 * Vphase_peak / Vdc
  = 2 * 26.13 / 60
  = 0.871
```

0.75 V最大PR校正叠加后理论峰值调制度约0.896，仍低于0.90软件限制。CBSVPWM加入零序后得到三桥臂调制量，再换算：

```text
duty = 0.5 + 0.5 * modulation_leg
Compare = Period * (1 - duty)
```

`inv_hrtim.c`采用反向占空比到CMP1映射，Period为34000。调制度不是某一个固定占空比；U/V/W占空比随30/60 Hz基波连续变化，三相相差120度。实际线电压还受死区、驱动压降、MOSFET压降、输出电感和母线波动影响。

## 7. ADC标定步骤

### 7.1 ZERO

1. 断开功率母线，确保IU/IV/IW为0 A，VUV/VVW/VWU为0 V。
2. 设置`INV_USER_WORK_MODE=INV_MODE_ADC_CALIBRATION`。
3. 设置`INV_USER_CALIBRATION_STEP=INV_CAL_STEP_ZERO`。
4. 编译下载，通过OLED原始码和VOFA前六通道确认六路稳定。
5. ZERO完成后VOFA CH0～CH5会从瞬时原始码切换为六路平均offset，分别写回六个`INV_USER_*_OFFSET`。

零输入理论上接近1V65REF和ADC码2048，但六路运放、参考与ADC误差不同，禁止复制同一个offset。

### 7.2 GAIN和极性

1. 选择一个`INV_CAL_CHANNEL_*`。
2. 对该通道施加稳定、已知、方向明确的直流或准静态量。
3. 电流通道的`REFERENCE`单位为A，电压通道单位为V。
4. 设置`INV_CAL_STEP_GAIN`，下载并读取VOFA CH6的`recommended_scale`。
5. 将结果写入对应`SCALE`，六个通道逐一完成。

公式为：

```text
physical = (raw - offset) * scale
```

负scale表示模拟链路反相，是合法结果。不能使用正弦RMS直接执行GAIN，也不能在PR内部再隐藏一次反号。

### 7.3 开放闭环

三路线电压全部标定并核对`VUV+VVW+VWU`接近0后，才设置：

```c
#define INV_USER_VOLTAGE_CALIBRATION_CONFIRMED (1U)
```

仅修改确认宏不会自动验证硬件，填写错误的比例或极性可能立即形成正反馈。

## 8. OLED和VOFA

闭环OLED布局：

```text
UV:xxxx VW:xxxx
WU:xxxx RF:xxxx
CA:+xxx CB:+xxx
S:x D:xxx F:xx
```

- `UV/VW/WU`：最近一个DDS周期的三路线电压RMS，单位0.01 V。
- `RF`：目标线电压，单位0.01 V，当前显示3200。
- `CA/CB`：alpha/beta PR校正，单位0.01 V。
- `S`：应用状态。
- `D`：诊断位，三位十六进制。
- `F`：真正锁存故障，正常为00，FLT3为01。

VOFA闭环通道：

| 通道 | 数据 | 单位 |
| --- | --- | --- |
| CH0 | Valpha参考 | V |
| CH1 | Valpha反馈 | V |
| CH2 | Vbeta参考 | V |
| CH3 | Vbeta反馈 | V |
| CH4 | alpha PR校正 | V |
| CH5 | beta PR校正 | V |
| CH6 | VUV RMS | V RMS |
| CH7 | `diagnostic_bits + (fault_bits << 16)` | 编码整数 |

正常闭环时CH0/CH1同相且幅值接近，CH2/CH3同相且幅值接近。反馈相位相反或误差增加时，立即断开功率并检查VUV/VVW/VWU接线极性和scale符号，不应靠继续增大PR解决。

## 9. 诊断与保护

默认`INV_USER_RELAXED_PR_TEST=1U`，以下诊断只置`D`，不会关闭PWM：

| D位 | 含义 | 默认处理 |
| ---: | --- | --- |
| 0x001 | ADC序列偏差或DMA停滞 | 记录；无新完整帧时Compare保持上次值 |
| 0x002 | ADC/DMA HAL错误 | 记录并等待后续帧 |
| 0x004 | ADC持续贴轨 | 记录，反馈无效时PR回退前馈 |
| 0x008 | ADC AWD越窗 | 记录；当前宽窗口本身不是有效保护 |
| 0x010 | PR输入或结果无效 | PR复位并回退开环前馈 |
| 0x020 | CBSVPWM计算异常 | 再尝试纯前馈命令 |
| 0x040 | HRTIM Compare写入失败 | 本周期不更新非法命令 |
| 0x080 | PR或CBSVPWM限幅 | 记录，继续运行 |
| 0x100 | 控制入口100 ms未推进 | 记录，继续喂IWDG |
| 0x200 | 参数未确认或非法 | 标定阻断时属于预期显示 |

运行期真正主动关断只保留PB10/HRTIM Fault3。FLT3硬件先把C/D/E输出置为Inactive，ISR随后记录`F=01`；故障后继续刷新IWDG并停在锁存态，必须人工复位，不能自动恢复。

`INV_USER_RELAXED_PR_TEST=0U`时，上述诊断会重新升级为软件关断。HardFault、CSS/NMI和不可恢复初始化错误无论何种模式都会关闭输出，因为CPU已无法保证继续产生合法PWM。

注意：当前板卡若没有把真实OCP/DESAT接入PB10，则FLT3只是一条预留路由或人工拉低测试入口，不能检测MOSFET短路。

## 10. 分阶段操作

### 阶段A：无功率ADC标定

- 断开直流母线和驱动功率电源。
- 完成六路ZERO以及三路线电压GAIN/极性。
- VOFA确认三路线电压和约为0，改变一路输入时映射正确。

### 阶段B：MCU PWM引脚

- 选择`INV_MODE_PWM_WAVEFORM`。
- 用示波器检查PB12/PB13、PB14/PB15、PC8/PC9。
- 应为10 kHz中心对齐互补PWM，死区约500 ns，三相包络为60 Hz或30 Hz。
- 拉低PB10，确认六路无需CPU参与即变为Inactive。

### 阶段C：驱动无母线

- 驱动器供电但直流母线断开。
- 分别测量每个MOSFET Gate-Source，确认互补关系、死区和幅值。
- 高侧必须使用差分或隔离探头，禁止普通示波器地夹连接浮动开关节点。

### 阶段D：低压缩比闭环

- 推荐先把母线和目标改为5 V、2.5 V RMS，并重新核对调制度。
- 使用限流电源、对称星形阻性负载和独立硬件过流措施。
- 先令`Kr=0`验证反馈方向，再逐步增加Kp和Kr。

### 阶段E：60 V、32 V RMS赛题目标

只有低压闭环稳定、三路采样标定、FLT3实测和外部独立OCP/DESAT均通过后才进入。逐级提高母线和目标，不允许直接从5 V跳到60 V。每一级检查线电压RMS、THD、负载电流、MOSFET/电感温升、限幅占比和母线裕量。

## 11. 后续修改规则

- 改输出频率：只填30或60，同时重算PR中心；运行期切换会保持DDS相位并重新平滑接管PR。
- 改母线或线电压：重新计算`2 * Vline_rms * sqrt(2/3) / Vdc`，并为PR校正、死区和母线跌落保留裕量。
- 改控制频率：必须同时修改CubeMX的PWM/ADC触发，不能只改宏；PR系数、软启动和恢复帧数都会受影响。
- 改采样板或量程：重新执行六路ZERO和逐通道GAIN，不得复制旧板参数。
- 追求赛题0.3%稳压和THD指标：需要实测母线、最终PR整定、滤波器和负载建模，并恢复完整保护；当前放宽档只用于先验证闭环方向和PWM可用性。

## 12. 当前验证记录

- `02OpenLoop.ioc`修改前SHA-256：`803A58B449CE6331518D9F34D7970FAFB23FD552E76016DEAA6602BA381ACB8A`。
- CubeMX外设参数未由本次代码修改。
- Keil ARMCC 5.06默认配置（双PR、60 Hz、标定确认0）完整重构建：`0 Error(s), 0 Warning(s)`；`Code=30600, RO-data=2556, RW-data=152, ZI-data=3712`。
- 临时切换到30 Hz并将标定确认置1的编译分支：`0 Error(s), 0 Warning(s)`；该结果只验证条件编译和链接，不代表已完成上板闭环验收。
- 临时切换到ADC标定模式的完整重构建：`0 Error(s), 0 Warning(s)`；最终交付宏已恢复为双PR、60 Hz、标定确认0。
- 上板波形、ADC标定、FLT3和60 V带载结果仍需按上述阶段实测，静态代码检查不能替代功率验收。
