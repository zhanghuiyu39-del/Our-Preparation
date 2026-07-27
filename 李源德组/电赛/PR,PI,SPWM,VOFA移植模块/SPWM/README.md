# STM32G474 HRTIM 单极性倍频 SPWM 模块

本目录提供适用于 STM32G474 HRTIM Timer A/B 的单相全桥单极性 SPWM 模块。

模块既可以生成固定 50 Hz 的开环正弦调制，也可以由后续 PR 电流环直接写入瞬时归一化调制量。两个桥臂各以 10 kHz 开关，差分线电压的主要开关纹波表现为 20 kHz。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `SPWM.c` | 正弦查表、调制度限幅及 HRTIM Timer A/B Compare 1 更新 |
| `SPWM.h` | 模块公共接口和调用约束 |

模块依赖 CubeMX 生成的 `hrtim.h` 和全局句柄 `hhrtim1`。

## 调制原理

瞬时调制量统一表示为：

```text
m ∈ [-0.90, +0.90]
```

HRTIM Timer A/B 周期均为 34000，中心比较值为 17000。模块使用以下映射：

```text
CMP_A = 17000 × (1 - m)
CMP_B = 17000 × (1 + m)
```

典型结果：

| 调制量 | CMP_A | CMP_B | 作用 |
| ---: | ---: | ---: | --- |
| `0.0` | 17000 | 17000 | 两个桥臂均为中性占空比 |
| `+0.5` | 8500 | 25500 | A、B 两桥臂占空比反向变化 |
| `-0.5` | 25500 | 8500 | 与 `+0.5` 时交换 |
| `+0.9` | 1700 | 32300 | 正向最大调制度 |
| `-0.9` | 32300 | 1700 | 负向最大调制度 |

NaN、正无穷或负无穷输入会回退到 `m=0`，防止非法浮点量被转换成 HRTIM 比较值。

## 开环频率关系

模块内置 400 点正弦表。`SPWM_OpenLoopStep()` 每次跨过 2 个表点，因此一个正弦周期需要 200 次调用：

```text
调用频率 = 10 kHz
正弦频率 = 10000 / 200 = 50 Hz
正弦周期 = 20 ms
```

`SPWM_OpenLoopStep()` 必须严格以 10 kHz 调用一次。重复调用或漏调用会改变输出正弦频率并引入相位误差。

## CubeMX/HRTIM 必要配置

以下参数与当前 `SPWM.c` 中的常量直接绑定。

### 时基

```text
HRTIM kernel clock       = 170 MHz
Master prescaler         = MUL2
Master period            = 34000
Master Compare 2         = 17000
Timer A/B prescaler      = MUL4
Timer A/B period         = 34000
Timer A/B mode           = Continuous, Up-Down
Timer A/B Compare 1      = 17000
Preload                  = Enabled
Repetition counter       = 0
Repetition update        = Enabled
```

Timer A/B 载波频率为：

```text
170 MHz × 4 / (2 × 34000) = 10 kHz
```

### 输出事件

当前已通过示波器验证的中心对齐 Toggle 配置为：

```text
TA1/TB1 Set Source 1     = Timer Compare 1
TA1/TB1 Reset Source 1   = Timer Compare 1
TA2/TB2 Set Source       = None
TA2/TB2 Reset Source     = None
```

不要把 TA1/TB1 的 `Reset Source 1` 改回 `None`。Set 和 Reset 同时选择 Compare 1 是 HAL 定义的 Toggle 模式，计数器上升和下降经过 CMP1 时输出翻转，从而形成连续的中心对齐 PWM。

TA2/TB2 不配置独立事件，它们由 HRTIM 死区单元根据 TA1/TB1 自动生成互补输出。

### 互补输出和死区

```text
PA8  = HRTIM1_CHA1 / TA1
PA9  = HRTIM1_CHA2 / TA2
PA10 = HRTIM1_CHB1 / TB1
PA11 = HRTIM1_CHB2 / TB2
Dead-time insertion      = Enabled
Rising dead time         = 85
Falling dead time        = 85
```

当前参数对应约 500 ns 死区。最终死区必须根据门极驱动器传播延迟、MOSFET 关断时间和实测桥臂波形确定。

### ADC 同步触发

推荐使用 HRTIM Master Compare 2 产生 10 kHz ADC Trigger 1：

```text
ADC Trigger 1 source     = Master Compare 2
ADC postscaler           = 0
ADC external trigger     = HRTIM Trigger 1 rising edge
DMA mode                 = Circular
```

只允许在一个 10 kHz 控制入口中调用 `SPWM_OpenLoopStep()`。当前工程选择 ADC1 完整转换回调作为唯一入口，ADC2回调不得再次推进 SPWM。

## 公共接口

### `SPWM_Init(float open_loop_amplitude)`

设置开环调制度，清零相位，并把 Timer A/B Compare 1 写回 17000。

该函数不启动 HRTIM、不开放输出、不控制门极使能，也不处理 Fault。

### `SPWM_SetOpenLoopAmplitude(float amplitude)`

运行期间修改开环调制度，不改变当前正弦相位。输入范围为 `0.0f` 到 `0.90f`；负值或非法值回退到 0。

### `SPWM_OpenLoopStep(void)`

推进一次开环正弦并更新 Timer A/B Compare 1。必须由固定 10 kHz 时基调用。

### `SPWM_ApplyModulation(float modulation)`

直接应用瞬时归一化调制量，并返回限幅后实际使用的值。后续 PR 闭环应调用本接口，不再调用 `SPWM_OpenLoopStep()`。

### `SPWM_Reset(void)`

清零开环相位并写入中性比较值。故障、停机或重新开始软启动前应调用本函数。

## 最小集成示例

初始化阶段应先保持门极驱动关闭，再启动采样、HRTIM计数器和MCU输出：

```c
/* 门极驱动默认关闭。 */
HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);

/* 带功率测试应从零调制度开始。 */
SPWM_Init(0.0f);

/* Master、Timer A和Timer B同步启动。 */
HAL_HRTIM_WaveformCounterStart(&hhrtim1,
    HRTIM_TIMERID_MASTER |
    HRTIM_TIMERID_TIMER_A |
    HRTIM_TIMERID_TIMER_B);

/* 开放四路MCU HRTIM输出；此时门极驱动仍保持关闭。 */
HAL_HRTIM_WaveformOutputStart(&hhrtim1,
    HRTIM_OUTPUT_TA1 |
    HRTIM_OUTPUT_TA2 |
    HRTIM_OUTPUT_TB1 |
    HRTIM_OUTPUT_TB2);
```

开环更新示例：

```c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc != NULL) && (hadc->Instance == ADC1))
    {
        SPWM_OpenLoopStep(); /* 每 100 us 只调用一次。 */
    }
}
```

低压带功率测试建议在主循环或启动状态机中将调制度从 0 缓慢提高到目标值，不要在中断中使用 `HAL_Delay()`。

## Fault 与门极使能边界

SPWM 模块只负责比较值映射，不负责以下功能：

- 启动或停止 HRTIM；
- 控制 `PFC_GATE_EN/PE0`；
- 清除或锁存 HRTIM Fault；
- 电流、电压保护和母线预充；
- 软启动状态机；
- PLL、交流过零同步或闭环控制。

实际功率项目应使用如下安全链路：

```text
驱动器 nFAULT / DESAT / OCP
            ↓
PB10 / HRTIM1_FLT3（低有效）
            ↓
HRTIM A/B 硬件立即进入 Inactive
            ↓
Fault ISR 拉低 PE0 并锁存软件故障
```

Fault释放后不得自动重新使能功率级。应由主状态机确认故障来源已经消失，再显式清故障、重新初始化调制度并执行软启动。

## 示波器验收

保持 `PE0` 为低且不连接功率级时，依次检查：

1. PA8单通道为 0 到 3.3 V、10 kHz、100 us 周期的 PWM。
2. 固定 `m=0、+0.5、-0.5` 时，PA8和PA10占空比按预期反向变化。
3. PA8/PA9以及PA10/PA11分别互补，切换边沿存在约500 ns死区。
4. 恢复 `SPWM_OpenLoopStep()` 后，载波仍为10 kHz，占空比按20 ms周期变化。
5. 示波器数学通道 `PA8-PA10` 呈现 `+3.3 V、0 V、-3.3 V` 三电平差分波形。
6. 拉低PB10时，PA8至PA11立即进入Inactive状态，Fault回调被记录。

PA8本身始终是数字PWM，不会直接显示为平滑正弦波。长时基下可能出现抽样混叠，检查载波时建议使用 `20 us/div` 和约 `1.5 V` 上升沿触发。

## 使用限制

- 本模块的开环正弦相位由MCU启动时刻决定，没有与交流输入同步。
- 不带PLL或过零同步时，禁止直接连接市电进行单相PWM整流测试。
- 开环模块不能实现母线稳压、输入电流跟踪或功率因数校正。
- 第一次带功率测试必须使用隔离、限流的低压电源和阻性假负载。
- 高侧 `VGS` 和桥臂中点必须使用差分探头或隔离测量设备。
- 若修改 HRTIM Period、调用频率或正弦表长度，必须同步修改 `SPWM.c` 中的常量和本文频率计算。

## 后续闭环接入

后续PR电流环计算出瞬时调制命令后，直接调用：

```c
float modulation_used;

modulation_used = SPWM_ApplyModulation(modulation_command);
```

闭环运行时不要同时调用 `SPWM_OpenLoopStep()`，否则开环和闭环会竞争写入同一组 HRTIM Compare寄存器。
