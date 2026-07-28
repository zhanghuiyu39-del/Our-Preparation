# STM32G474 三相逆变 HRTIM 与 ADC-DMA 模块

本目录提供三相逆变器专用的HRTIM功率接口和六通道ADC-DMA测量接口。配置基线是目标工程的`01.ioc`，算法调制使用同级目录下的`CBSVPWM`模块。

本模块只管理逆变器资源，不访问整流器的ADC1/2、HRTIM A/B和`PFC_GATE_EN/PE0`。HRTIM F和四桥臂调制暂不支持。

## 文件职责

| 文件 | 作用 |
| --- | --- |
| `inv_hrtim.c/.h` | 启动逆变采样时基、更新U/V/W占空比、开放功率输出和安全停机 |
| `inv_measure.c/.h` | 管理ADC3/4/5循环DMA、六通道同步、零点标定、物理量换算和故障锁存 |
| `README.md` | CubeMX依赖、初始化顺序、HAL回调、CBSVPWM连接和上板验收 |

`CBSVPWM.c/.h`保持纯C99算法，不包含HAL、HRTIM、ADC或GPIO访问。

## 01.ioc资源对应

### 三相PWM

| 相别 | HRTIM | 高侧/主输出 | 低侧/互补输出 |
| --- | --- | --- | --- |
| U | Timer C | PB12/HRTIM1_CHC1 | PB13/HRTIM1_CHC2 |
| V | Timer D | PB14/HRTIM1_CHD1 | PB15/HRTIM1_CHD2 |
| W | Timer E | PC8/HRTIM1_CHE1 | PC9/HRTIM1_CHE2 |

共同配置：

- HRTIM内核时钟170 MHz。
- Timer C/D/E为`MUL4`、Up-Down、Period 34000，对应10 kHz中心对齐PWM。
- CMP1启用预装载，在Valley/Repetition边界更新。
- 死区上升和下降计数均为85，初始约500 ns，最终值需按驱动器和功率器件实测调整。
- PB10为低有效`HRTIM1_FLT3`，Fault 3直接使Timer A～E输出进入Inactive。
- PE1为`INV_GATE_EN`，复位和故障时必须为低并具有板级下拉。
- PE6为低有效`INV_NFAULT_DIAG`下降沿EXTI，只负责软件诊断；快速硬件关断由PB10完成。

### 三相采样

| ADC与DMA | Rank 1 | Rank 2 | DMA缓冲区 |
| --- | --- | --- | --- |
| ADC3 / DMA2_Channel1 | PB0/ADC3_IN12/IU | PD10/ADC3_IN7/VU | `INV_Adc3Dma[0..1]` |
| ADC4 / DMA2_Channel2 | PD8/ADC4_IN12/IV | PE14/ADC4_IN1/VV | `INV_Adc4Dma[0..1]` |
| ADC5 / DMA2_Channel3 | PD9/ADC5_IN13/IW | PD14/ADC5_IN11/VW | `INV_Adc5Dma[0..1]` |

三个ADC均为12位、异步时钟除4、24.5 cycles、规则组两次转换、循环DMA和Half Word数据宽度。外部触发均为`ADC_EXTERNALTRIG_HRTIM_TRG2`，Trigger 2由Master CMP2每100 us产生一次。

## 公共接口与数据流

CBSVPWM输出的占空比直接交给：

```c
HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w);
```

占空比必须位于`0.05～0.95`。当前已经确认的Valley/Set-Reset极性使用反向映射：

```text
CMP1 = round(34000 × (1 - duty))

duty=0.05 -> CMP1=32300
duty=0.50 -> CMP1=17000
duty=0.95 -> CMP1=1700
```

测量数据流为：

```text
Master CMP2
 -> HRTIM ADC Trigger 2
 -> ADC3/4/5规则组并行转换
 -> DMA2 Channel1/2/3循环缓冲
 -> 三个ADC完成回调
 -> 三路序列全部更新
 -> 发布一次INV_Measurement完整快照
```

回调先后顺序不作为同步依据。`fast_heartbeat`只在六个Rank全部形成一帧后增加一次，正常增速约为10000/s。

启动后的前256个完整帧用于计算IU、IV、IW、VU、VV、VW零点，约需25.6 ms。标定期间必须保持无输出电压、无负载电流、PE1为低且六路PWM输出关闭。

当前代码默认六路模拟前端都以ADC中点表示0。如果VU/VV/VW是0～3.3 V单极性采样，应修改`inv_measure.c`，使三路电压直接按原始值乘比例，不能继续扣除启动零点。

## 初始化顺序

以下顺序应放在CubeMX外设初始化完成后、功率状态机开放输出前：

```c
#include "adc.h"
#include "inv_hrtim.h"
#include "inv_measure.h"
#include "CBSVPWM.h"

static CBSVPWM_t inv_svpwm;

static HAL_StatusTypeDef INV_AppStartSampling(void)
{
    /* 第一步始终建立硬件安全态。 */
    INV_HRTIM_StopAll();
    INV_Measure_Init();

    if (HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_ADCEx_Calibration_Start(&hadc5, ADC_SINGLE_ENDED) != HAL_OK) {
        return HAL_ERROR;
    }

    /* 先让三个ADC等待外部触发，再启动产生Trigger 2的HRTIM时基。 */
    if (HAL_ADC_Start_DMA(&hadc3, (uint32_t *)INV_Adc3Dma, 2U) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_ADC_Start_DMA(&hadc4, (uint32_t *)INV_Adc4Dma, 2U) != HAL_OK) {
        return HAL_ERROR;
    }
    if (HAL_ADC_Start_DMA(&hadc5, (uint32_t *)INV_Adc5Dma, 2U) != HAL_OK) {
        return HAL_ERROR;
    }

    /* 长度为2时Half Transfer只代表一个Rank完成，不能用于六通道控制。 */
    __HAL_DMA_DISABLE_IT(hadc3.DMA_Handle, DMA_IT_HT);
    __HAL_DMA_DISABLE_IT(hadc4.DMA_Handle, DMA_IT_HT);
    __HAL_DMA_DISABLE_IT(hadc5.DMA_Handle, DMA_IT_HT);

    if (INV_HRTIM_StartSampling() != HAL_OK) {
        INV_Measure_Trip(INV_FAULT_HRTIM);
        return HAL_ERROR;
    }

    /* 初始化只计算参数，不启动PWM；母线最低允许值按最终硬件填写。 */
    if (CBSVPWM_Init(&inv_svpwm, 0.90f, 20.0f) == 0U) {
        INV_Measure_Trip(INV_FAULT_CBSVPWM);
        return HAL_ERROR;
    }

    return HAL_OK;
}
```

状态机应等待`offset_ready=1`、`valid=1`、三个DMA心跳持续更新、母线与驱动器正常后，才调用`INV_HRTIM_EnablePowerStage()`。该函数先写入三相50%中性Duty、启动六路输出、复查FLT3，最后才拉高PE1。

## HAL回调转发

### ADC与DMA

```c
static uint32_t last_control_heartbeat;

/* 此函数由用户在应用层实现，输入是一帧已经完成标定的六通道测量。 */
static void INV_AppFastControl(const INV_Measurement *measurement);

/* 三个ADC分支都会调用；只有刚发布完整帧的分支会进入一次快速控制。 */
static void INV_AppTryFastControl(void)
{
    INV_Measurement measurement;

    INV_Measure_GetSnapshot(&measurement);
    if ((measurement.valid != 0U) &&
        (measurement.fast_heartbeat != last_control_heartbeat)) {
        last_control_heartbeat = measurement.fast_heartbeat;
        INV_AppFastControl(&measurement); /* 用户实现的QPR与CBSVPWM控制步骤。 */
    }
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3) {
        INV_Measure_OnAdc3Complete();
    } else if (hadc->Instance == ADC4) {
        INV_Measure_OnAdc4Complete();
    } else if (hadc->Instance == ADC5) {
        INV_Measure_OnAdc5Complete();
    }

    if ((hadc->Instance == ADC3) ||
        (hadc->Instance == ADC4) ||
        (hadc->Instance == ADC5)) {
        INV_AppTryFastControl();
    }
}

void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *hadc)
{
    if ((hadc->Instance == ADC3) ||
        (hadc->Instance == ADC4) ||
        (hadc->Instance == ADC5)) {
        INV_Measure_AdcError();
    }
}
```

如果项目另外实现DMA错误回调，也应将DMA2_Channel1/2/3错误转发到`INV_Measure_AdcError()`。不要在DMA Half Complete回调中运行控制算法。

### 驱动器诊断与公共FLT3

```c
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == INV_NFAULT_DIAG_Pin) {
        INV_Measure_Trip(INV_FAULT_DRIVER);
    }
}

void HAL_HRTIM_Fault3Callback(HRTIM_HandleTypeDef *hhrtim)
{
    if (hhrtim->Instance == HRTIM1) {
        /* PB10是PFC与逆变器共用总Fault，两个功率级都必须收到通知。 */
        PFC_Measure_Trip(PFC_FAULT_DRIVER); /* PFC模块的实际统一Trip接口。 */
        INV_Measure_Trip(INV_FAULT_DRIVER);
    }
}
```

上例中的PFC函数名按最终整流器模块调整，但不能删除这一分支。硬件已在CPU进入回调前关闭相关PWM；软件回调负责拉低两个Gate Enable并锁存故障。

## CBSVPWM连接

在`INV_AppFastControl()`对应的10 kHz完整测量帧中，先由开环参考或三相QPR得到单位为V的`vu/vv/vw_command`，再执行：

```c
if (CBSVPWM_Calc3Leg(&inv_svpwm,
                     vu_command,
                     vv_command,
                     vw_command,
                     vbus) != 0U) {
    if (INV_HRTIM_SetDuty(inv_svpwm.duty_u,
                          inv_svpwm.duty_v,
                          inv_svpwm.duty_w) != HAL_OK) {
        INV_Measure_Trip(INV_FAULT_PWM_COMMAND);
    }
} else {
    INV_Measure_Trip(INV_FAULT_CBSVPWM);
}
```

- `vu/vv/vw_command`和`vbus`单位均为V。
- `modulation_limit=0.90`使正常Duty落在`0.05～0.95`，与HRTIM接口范围一致。
- `limited=1`只表示本周期发生整体比例限幅，不自动等同于Fault；持续限幅由应用状态机判断。
- CBSVPWM失败时会把Duty恢复为0.5，但50%不是安全关断状态，必须调用`INV_Measure_Trip()`。
- 本模块只接收`CBSVPWM_Calc3Leg()`的U/V/W结果，不接入`duty_n`或`CBSVPWM_Calc4Leg()`。

## 快速回调限制

ADC/DMA快速回调中禁止调用：

- `OLED_*()`；
- `VOFA_Send()`或`HAL_UART_Transmit()`；
- `HAL_Delay()`；
- 动态内存、格式化字符串或其他阻塞HAL函数。

OLED和VOFA应在`while(1)`中读取`INV_Measure_GetSnapshot()`返回的副本，并按10～100 ms周期刷新。

## 故障与恢复

`INV_Measure_Trip()`会按位锁存故障、令测量失效并调用`INV_HRTIM_StopAll()`。停机顺序固定为：

```text
PE1拉低
 -> C/D/E六路输出关闭
 -> Timer C/D/E计数器停止
 -> Master继续运行，避免影响PFC共享时基
```

Master继续运行时ADC DMA可能仍然更新原始值和序列号，但只要`fault_bits`非0，测量层就不会把`valid`重新置1，也不会允许状态机据此自动恢复。

Fault输入恢复后不得自动重新开放PE1。人工确认故障原因、驱动器状态和母线安全后，应重新执行采样启动、256帧零点标定、CBSVPWM/QPR Reset及功率级显式使能流程。

## 无功率验证

1. 断开栅极驱动或保持PE1低，启动ADC3/4/5 DMA和HRTIM采样时基。
2. 在Keil Watch中确认三个序列和`fast_heartbeat`约以10 kHz增加。
3. 核对六个原始值的Rank顺序，并分别施加受限直流测试电压验证方向和比例。
4. 确认约25.6 ms后`offset_ready=1`，随后`valid=1`。
5. 分别调用`INV_HRTIM_SetDuty(0.05f,...)`、`0.50f`、`0.95f`，确认CMP1为32300、17000、1700。
6. 保持PE1低，用示波器检查C/D/E为10 kHz中心对齐互补PWM、相位更新同步且死区约500 ns。
7. 拉低PB10，确认不等待CPU即可使Timer A～E输出进入Inactive。
8. 停止任一路DMA或注入ADC错误，确认PE1保持低并锁存`INV_FAULT_ADC_SYNC/ADC_ERROR`。
9. 输入NaN、越界Duty或制造CBSVPWM欠压失败，确认不写入非法CMP并关闭功率级。
10. 使用ARMCC5、C99、`STM32G474xx`和hard-float构建达到`0 Error(s), 0 Warning(s)`。

软件限幅、零点标定和故障回调不能替代驱动器DESAT/OCP、PB10硬件Fault、急停、PE1板级下拉以及母线预充/放电保护。没有独立短路保护时禁止高压带功率运行。
