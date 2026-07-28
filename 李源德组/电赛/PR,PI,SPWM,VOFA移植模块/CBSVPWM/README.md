# STM32G474 三相 CBSVPWM 可移植模块使用说明

本目录提供从目标 Simulink 三相逆变调制部分移植得到的载波型空间矢量 PWM（Carrier-Based SVPWM，CBSVPWM）算法。

算法采用“最大值/最小值零序注入”实现，与扇区式 SVPWM 在线性调制区等效，但不需要扇区判断、矢量作用时间计算、Clarke/Park 变换或 dq 坐标系。模块只接收三相电压指令和直流母线电压，输出三桥臂或四桥臂的归一化占空比。

## 文件说明

| 文件 | 作用 |
| --- | --- |
| `CBSVPWM.c` | 三相归一化、零序注入、整体比例限幅和占空比计算 |
| `CBSVPWM.h` | 公开运行结构体、初始化、复位和三/四桥臂计算接口 |
| `README.md` | 算法公式、QPR连接、STM32G474 HRTIM适配和验收步骤 |

`CBSVPWM.c/h`是纯C99算法，不包含`hrtim.h`、HAL函数、定时器句柄、正弦表或动态内存。相同模块可以用于STM32 HRTIM、普通高级定时器或其他MCU，具体PWM寄存器映射由应用层完成。

## 控制链路

三相独立电压源逆变器推荐的数据流为：

```text
三相电压参考 Vu_ref / Vv_ref / Vw_ref
                    ↓
        U/V/W 三路有限带宽QPR
                    ↓
三相电压指令 Vu_cmd / Vv_cmd / Vw_cmd（V）
                    ↓
     CBSVPWM_Calc3Leg() 或 Calc4Leg()
                    ↓
      duty_u / duty_v / duty_w / duty_n
                    ↓
       HRTIM C / D / E / F Compare 1
                    ↓
       HRTIM死区单元生成上下管互补驱动
```

QPR负责改善50 Hz电压跟踪、负载扰动和稳态误差；CBSVPWM负责把三相电压指令变成占空比。QPR不会直接控制每只MOS管，互补输出和死区由HRTIM硬件产生。

开环试波不需要QPR。应用层生成三相相差120度的正弦电压指令后，同样送入CBSVPWM即可。

## 与仿真一致的调制公式

输入`Vu*、Vv*、Vw*`和`Vdc`单位均为V。首先按照仿真中的`ref/(0.5*Udc)`进行归一化：

```text
mu = 2 × Vu* / Vdc
mv = 2 × Vv* / Vdc
mw = 2 × Vw* / Vdc
```

仿真的`InvertQuTrans`子系统先提取三相平均值，并得到去平均值后的三相量：

```text
mavg = (mu + mv + mw) / 3

mu' = mu - mavg
mv' = mv - mavg
mw' = mw - mavg
```

`SVPWM发生器`子系统使用最大值和最小值计算载波型SVPWM零序量：

```text
mmax  = max(mu', mv', mw')
mmin  = min(mu', mv', mw')
mzero = -0.5 × (mmax + mmin)
```

三桥臂调制量为：

```text
Mu = mu' + mzero
Mv = mv' + mzero
Mw = mw' + mzero
```

启用第四中性桥臂时，仿真中的`Gn`对应：

```text
Mn = mzero - mavg
```

未发生限幅时满足：

```text
Mu - Mn = mu
Mv - Mn = mv
Mw - Mn = mw
```

因此四桥臂能够合成三相参考中的零序电压；三桥臂只能控制相间电压，三相共同的零序指令会被去除。

最终调制量通过下式换算为占空比：

```text
duty = 0.5 × (1 + M)
```

该换算等效于仿真中把`M+1`与`0～2`三角载波比较，不需要在MCU中逐点生成三角载波。

## 整体比例限幅

初始化参数`modulation_limit`限制所有已启用桥臂的最大绝对调制度。推荐低压调试从`0.90f`开始：

```text
M ∈ [-0.90, +0.90]
duty ∈ [0.05, 0.95]
```

若任一桥臂超过限制，模块对U/V/W以及可选N桥臂同时乘以同一个比例：

```text
scale = modulation_limit / max_abs_modulation
```

整体缩放能够保持相间电压矢量方向。禁止改成逐相独立削顶，否则过调制时三相比例和波形都会发生额外畸变。

结构体中的：

- `zero_sequence`记录缩放前的SVPWM零序注入量；
- `applied_scale`记录本周期整体缩放系数；
- `limited=1`表示本周期发生了过调制限制；
- `valid=1`表示最近一次计算输入有效；
- `duty_u/v/w/n`是应用层应读取的最终占空比。

## 公共接口

### `CBSVPWM_Init()`

```c
uint8_t CBSVPWM_Init(CBSVPWM_t *svpwm,
                     float modulation_limit,
                     float minimum_dc_voltage);
```

- `modulation_limit`必须处于`(0, 1]`，首轮推荐`0.90f`。
- `minimum_dc_voltage`单位为V，应设置为允许逆变控制工作的最低母线电压。
- 参数有效返回1，否则返回0并保持未初始化状态。
- 初始化只修改算法对象，不启动HRTIM、不写比较寄存器、不开放Gate Enable。

### `CBSVPWM_Calc3Leg()`

```c
uint8_t CBSVPWM_Calc3Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage);
```

计算U/V/W三桥臂占空比。当前项目首版使用该接口，对应HRTIM C/D/E，HRTIM F保持关闭。三桥臂调用完成后`duty_n`固定为0.5，防止残留以前的四桥臂计算值。

### `CBSVPWM_Calc4Leg()`

```c
uint8_t CBSVPWM_Calc4Leg(CBSVPWM_t *svpwm,
                         float vu_command,
                         float vv_command,
                         float vw_command,
                         float dc_voltage);
```

在U/V/W之外计算与仿真`Gn`一致的N桥臂占空比。只有PCB已经具备中性桥臂、HRTIM F配置和对应硬件保护时才允许使用。

### `CBSVPWM_Reset()`

清除最近一次计算结果，将四路占空比恢复为0.5，但保留`modulation_limit`、`minimum_dc_voltage`和初始化状态。停机、Fault锁存、重新闭环及软启动前都应调用。

`Reset`不会写入HRTIM，也不会关闭功率输出。应用状态机必须先关闭Gate Enable并强制PWM无效。

## 异常输入行为

以下情况计算函数返回0：

- 对象为空或尚未成功初始化；
- 任一电压指令或母线电压为NaN或无穷大；
- `Vdc < minimum_dc_voltage`；
- 归一化或零序计算发生单精度溢出。

除空指针外，失败时对象恢复为：

```text
duty_u = duty_v = duty_w = duty_n = 0.5
valid = 0
limited = 0
```

50%中性占空比只是算法的确定性回退值，不是功率安全状态。运行时若计算返回0，必须进入统一故障关断：

```text
CBSVPWM计算失败
        ↓
拉低INV_GATE_EN / PE1
        ↓
强制HRTIM C/D/E/F输出Inactive
        ↓
锁存ADC或母线故障，禁止自动恢复
```

## 最小初始化示例

```c
#include "CBSVPWM.h"

/* 参数单位集中在应用层：0.90为调制度，INV_MIN_VDC_V单位为V。 */
#define INV_MODULATION_LIMIT (0.90f)
#define INV_MIN_VDC_V        (20.0f) /* 示例值，必须按实际母线和欠压策略修改。 */

static CBSVPWM_t inverter_svpwm;

void Inverter_ControlInit(void)
{
    /* 初始化阶段PE1和全部HRTIM逆变输出必须仍处于关闭状态。 */
    if (CBSVPWM_Init(&inverter_svpwm,
                     INV_MODULATION_LIMIT,
                     INV_MIN_VDC_V) == 0U) {
        PFC_Trip(PFC_FAULT_PARAMETER); /* 替换为最终工程的统一故障接口。 */
    }
}
```

`INV_MIN_VDC_V=20 V`只用于展示单位和调用方式，不是最终功率板阈值。最终值必须高于ADC噪声和分压换算可能产生的假母线电压，并与逆变器欠压状态机一致。

## STM32G474 HRTIM C/D/E/F适配

CBSVPWM模块不直接包含`hrtim.h`。以下代码应放在最终工程的逆变PWM适配层或`USER CODE`区域。

当前HRTIM配置假设：

```text
Timer C/D/E/F Prescaler = MUL4
Up-Down Mode             = Enable
Period                   = 34000
Compare 1                = 17000（初始）
Preload                  = Enable
Update                   = Valley边界同步生效
Output 1                 = Compare 1 Toggle
Output 2                 = Dead Time互补生成
```

在当前已经验证的Valley/Toggle极性下：

```text
CMP = Period × (1 - duty)
```

适配示例：

```c
#include "CBSVPWM.h"
#include "hrtim.h"

#define INV_HRTIM_PERIOD_COUNTS (34000U)

/* 把0～1占空比映射为当前Valley/Toggle配置使用的Compare 1计数值。 */
static uint32_t Inverter_DutyToCompare(float duty)
{
    float compare = (float)INV_HRTIM_PERIOD_COUNTS * (1.0f - duty);
    return (uint32_t)(compare + 0.5f);
}

/* 本函数只更新预装载比较值，不负责启动输出、Gate Enable或Fault清除。 */
static void Inverter_ApplyThreeLegPwm(const CBSVPWM_t *svpwm)
{
    __HAL_HRTIM_SETCOMPARE(&hhrtim1,
                           HRTIM_TIMERINDEX_TIMER_C,
                           HRTIM_COMPAREUNIT_1,
                           Inverter_DutyToCompare(svpwm->duty_u));
    __HAL_HRTIM_SETCOMPARE(&hhrtim1,
                           HRTIM_TIMERINDEX_TIMER_D,
                           HRTIM_COMPAREUNIT_1,
                           Inverter_DutyToCompare(svpwm->duty_v));
    __HAL_HRTIM_SETCOMPARE(&hhrtim1,
                           HRTIM_TIMERINDEX_TIMER_E,
                           HRTIM_COMPAREUNIT_1,
                           Inverter_DutyToCompare(svpwm->duty_w));
}

/* 只有四桥臂硬件和HRTIM F均通过验收后，才在三相更新后追加N桥臂。 */
static void Inverter_ApplyNeutralLegPwm(const CBSVPWM_t *svpwm)
{
    __HAL_HRTIM_SETCOMPARE(&hhrtim1,
                           HRTIM_TIMERINDEX_TIMER_F,
                           HRTIM_COMPAREUNIT_1,
                           Inverter_DutyToCompare(svpwm->duty_n));
}
```

如果Timer C/D/E/F的Set/Reset极性与当前A/B配置不同，`CMP=Period×(1-duty)`可能反向。上板必须先在`PE1=0`时分别验证25%、50%和75%占空比，再允许连接功率驱动。

## 10 kHz开环调用示例

CBSVPWM内部不再保存400点正弦表或相位索引。开环参考由应用层DDS、正弦表或调试用`sinf()`产生：

```text
Vu* = Vpk × sin(theta)
Vv* = Vpk × sin(theta - 2π/3)
Vw* = Vpk × sin(theta + 2π/3)

theta(k+1) = theta(k) + 2π × 50 Hz / 10000 Hz
```

固定10 kHz控制入口的核心调用为：

```c
if (CBSVPWM_Calc3Leg(&inverter_svpwm,
                     vu_reference,
                     vv_reference,
                     vw_reference,
                     measured_vdc) != 0U) {
    Inverter_ApplyThreeLegPwm(&inverter_svpwm);
} else {
    PFC_Trip(PFC_FAULT_CBSVPWM); /* 计算失败时禁止继续带功率运行。 */
}
```

首轮开环测试使用受限直流母线、较小`Vpk`和阻性负载。相位累加、正弦查表和软启动属于应用控制层，不重新塞入CBSVPWM模块。

## 三相QPR闭环接入

现有`PR_t`已经实现有限带宽准PR，可分别建立U/V/W三个实例。`PR_Calc()`输出在本应用中定义为电压指令，单位V：

```c
static PR_t qpr_u;
static PR_t qpr_v;
static PR_t qpr_w;

void Inverter_QprInit(void)
{
    /* sample_frequency_hz必须等于PR_Calc()实际调用频率，这里每100 us调用一次。 */
    PR_Init(&qpr_u, INV_KP, INV_KR, 50.0f, INV_WC_RAD_S, 10000.0f,
            -INV_COMMAND_LIMIT_V, INV_COMMAND_LIMIT_V);
    PR_Init(&qpr_v, INV_KP, INV_KR, 50.0f, INV_WC_RAD_S, 10000.0f,
            -INV_COMMAND_LIMIT_V, INV_COMMAND_LIMIT_V);
    PR_Init(&qpr_w, INV_KP, INV_KR, 50.0f, INV_WC_RAD_S, 10000.0f,
            -INV_COMMAND_LIMIT_V, INV_COMMAND_LIMIT_V);
}
```

控制周期的数据流为：

```c
vu_command = PR_Calc(&qpr_u, vu_reference, vu_feedback);
vv_command = PR_Calc(&qpr_v, vv_reference, vv_feedback);
vw_command = PR_Calc(&qpr_w, vw_reference, vw_feedback);

if (CBSVPWM_Calc3Leg(&inverter_svpwm,
                     vu_command,
                     vv_command,
                     vw_command,
                     measured_vdc) != 0U) {
    Inverter_ApplyThreeLegPwm(&inverter_svpwm);
} else {
    PFC_Trip(PFC_FAULT_CBSVPWM);
}
```

仿真中的三角载波周期为`1/20000 s`，即20 kHz；当前STM32G474 HRTIM方案为10 kHz、每周期调用一次控制算法，即`Ts=100 us`。不能复制按`Ts=50 us`得到的QPR差分系数，必须通过`PR_Init(..., 10000.0f, ...)`重新计算。

停机、欠压、HRTIM Fault和重新软启动前，应同时执行：

```text
拉低PE1
→ 关闭或强制HRTIM C/D/E/F为Inactive
→ PR_Reset(U/V/W)
→ CBSVPWM_Reset()
```

## 可复算数值样例

以下样例使用：

```text
Vdc = 100 V
modulation_limit = 0.90
```

### 零指令

```text
Vu*=Vv*=Vw*=0
duty_u=duty_v=duty_w=duty_n=0.5
valid=1, limited=0
```

### 平衡三相瞬时值

```text
Vu*=20 V, Vv*=-10 V, Vw*=-10 V
mu=0.4, mv=-0.2, mw=-0.2
mavg=0, mzero=-0.1
Mu=0.3, Mv=-0.3, Mw=-0.3
```

三桥臂输出：

```text
duty_u=0.65, duty_v=0.35, duty_w=0.35, duty_n=0.50
```

四桥臂还会得到：

```text
Mn=-0.1, duty_n=0.45
```

### 纯零序指令

```text
Vu*=Vv*=Vw*=10 V
```

- 三桥臂无法合成共同零序，U/V/W占空比均为0.5。
- 四桥臂得到U/V/W占空比0.5、N桥臂占空比0.4，桥臂相对中性点电压仍对应10 V指令。

### 整体比例限幅

```text
Vu*=100 V, Vv*=-50 V, Vw*=-50 V
限幅前Mu=1.5, Mv=-1.5, Mw=-1.5
scale=0.90/1.50=0.60
限幅后Mu=0.9, Mv=-0.9, Mw=-0.9
duty_u=0.95, duty_v=0.05, duty_w=0.05
limited=1
```

## 软件验证清单

1. `CBSVPWM_Init(NULL, ...)`及非法参数返回0。
2. 未初始化对象调用Calc返回0。
3. 零指令得到四路0.5占空比。
4. 上述平衡三相样例与手算结果一致。
5. 三相相同指令在三桥臂和四桥臂下分别得到预期结果。
6. 过调制时所有启用桥臂使用相同`applied_scale`。
7. NaN、正负无穷和母线欠压使`valid=0`并恢复中性占空比。
8. `CBSVPWM_Reset()`保留两个配置参数和`initialized`。
9. 源文件中不存在HAL、HRTIM、DMA、延时、串口、动态内存和固定正弦频率依赖。
10. 最终Keil工程使用ARMCC 5、C99和Cortex-M4F hard-float构建达到`0 Error(s), 0 Warning(s)`。

## 上板验收与安全边界

保持`INV_GATE_EN/PE1=0`，按以下顺序验证：

1. 固定写入25%、50%、75%占空比，确认C/D/E Compare映射方向。
2. 检查PB12/PB13、PB14/PB15、PC8/PC9三对输出均为10 kHz中心对齐互补PWM。
3. 检查每对上下管约500 ns死区，并根据驱动器和功率器件实测调整。
4. 输入低幅三相正弦参考，确认占空比包络相差120度。
5. 使用受限低压母线和阻性负载逐步开放PE1。
6. 拉低PB10，确认无需CPU即可让全部已启用HRTIM输出进入Inactive。
7. 停止ADC/DMA或使Vdc低于阈值，确认软件锁存故障且不会自动恢复。
8. 四桥臂需在三桥臂全部验收完成后，单独验证HRTIM F、N桥臂死区和中性线电流保护。

CBSVPWM软件限幅和异常回退不能替代驱动器DESAT/OCP、PB10/HRTIM1_FLT3、急停、Gate Enable板级下拉及母线预充/放电安全链路。没有独立短路保护时禁止高压带功率运行。
