# PI、PR 与 VOFA 简化移植说明

本目录是新项目推荐版本，面向 `STM32G474VCT6 + CubeMX + HAL`。
只包含通用 PI、PR 和阻塞式 VOFA，不使用 UART DMA、SOGI-PLL、坐标变换或复杂状态机。

## 1. 加入 CubeMX 工程

把下面六个文件复制到目标工程对应目录，并在 Keil 中加入三个 `.c` 文件：

```text
Core/Inc/PI.h
Core/Inc/pr.h
Core/Inc/vofa.h
Core/Src/PI.c
Core/Src/pr.c
Core/Src/vofa.c
```

`PI.c` 和 `pr.c` 不依赖 HAL；`vofa.c` 通过 CubeMX 生成的 `main.h` 使用 UART HAL 接口。

## 2. 推荐引脚分配

| 功能 | STM32G474VCT6 资源 |
| --- | --- |
| 单相整流桥臂 1 | HRTIM A：PA8/PA9 |
| 单相整流桥臂 2 | HRTIM B：PA10/PA11 |
| 三相逆变 U 相 | HRTIM C：PB12/PB13 |
| 三相逆变 V 相 | HRTIM D：PB14/PB15 |
| 三相逆变 W 相 | HRTIM E：PC8/PC9 |
| 整流输入电流 IPFC | PA1，ADC1_IN2 |
| 单相交流电压 VAC | PC0，ADC2_IN6 |
| 母线电压 VBUS | PC1，ADC1_IN7 |
| 逆变电流 IU/IV/IW | PB0、PD8、PD9，ADC3/4/5 |
| PFC/逆变驱动使能 | PE0、PE1，初始输出低 |
| 驱动总故障 | PB10，HRTIM1_FLT3，低有效 |
| VOFA 串口 | USART2：PD5/PD6 |

HRTIM 的死区、Fault 锁存和 ADC 触发由 CubeMX 配置。本模块不直接操作 HRTIM。

## 3. CubeMX 串口配置

USART2 选择 Asynchronous、8N1、无硬件流控，波特率使用 460800 或 921600。
本简化版本不启用 UART DMA，也不需要 UART 中断。VOFA+ 选择 `JustFloat`。

## 4. PI 与 PR 初始化

在 `main.c` 的用户变量区声明控制器：

```c
#include "PI.h"
#include "pr.h"
#include "vofa.h"

/* 单相整流器使用一个母线 PI 和一个电流 PR；三相逆变器每相使用一个 PR。 */
static PI_t PI_VBUS;
static PR_t PR_PFC_I;
static PR_t PR_INV_U;
static PR_t PR_INV_V;
static PR_t PR_INV_W;
```

在全部 `MX_xxx_Init()` 完成后初始化。下面数值只是参考起点，最终需要结合功率级调试：

```c
/* 母线 PI 每 1 ms 调用一次，输出限制为允许的交流电流幅值。 */
PI_Init(&PI_VBUS,
        PFC_VBUS_KP,
        PFC_VBUS_KI,
        1000.0f,
        0.0f,
        PFC_CURRENT_AMP_MAX_A);

/* 参数顺序：Kp、Kr、基波 Hz、带宽 rad/s、实际控制频率 Hz、最小/最大输出。 */
PR_Init(&PR_PFC_I, 0.25f, 5.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_U, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_V, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_W, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
```

`20000.0f` 必须等于 `PR_Calc()` 的真实调用频率。中心对齐 PWM 可能一周期触发两次更新，应以实际中断频率为准。

## 5. 母线电压 PI 外环

母线电压可以随快速 ADC 任务采样，但 `PI_Calc()` 固定每 1 ms 调用一次。参考值应先经过软启动斜坡，禁止直接给母线参考阶跃：

```c
/* 该代码放在 1 ms 控制任务中，PI 输出单位为交流电流幅值 A。 */
pfc_current_amp_cmd_a = PI_Calc(&PI_VBUS,
                                 vbus_ref_ramp_v,
                                 vbus_v);
```

`pfc_current_amp_cmd_a` 再与归一化 VAC 波形相乘，生成 PR 电流内环参考。若调制或硬件保护已经限制功率，应用状态机应停止母线闭环并复位 PI，不能只依赖软件输出限幅。

## 6. 控制中断调用顺序

### 单相 PWM 整流器

本方案不使用 SOGI-PLL。先去除 VAC 采样偏置并换算为物理量，再得到归一化电压波形：

```c
/* VAC_PEAK_EST 是当前电网电压峰值估计，vac_norm 最终限制到 -1~1。 */
vac_norm = vac / VAC_PEAK_EST;
if (vac_norm > 1.0f)  vac_norm = 1.0f;
if (vac_norm < -1.0f) vac_norm = -1.0f;

/* pfc_current_amp_cmd_a 由 1 kHz 母线电压 PI 外环给出。 */
ipfc_ref = pfc_current_amp_cmd_a * vac_norm;
pfc_modulation = PR_Calc(&PR_PFC_I, ipfc_ref, ipfc);
```

这种方式直接利用 VAC 波形获得电流相位，不需要 PLL。必须先确认 VAC 零点、极性和比例正确。

### 三相逆变器

继续使用现有正弦表或固定频率相位索引产生 `iu_ref/iv_ref/iw_ref`：

```c
/* 三相电流 PR 相互独立，结果再交给项目自己的 HRTIM 调制函数。 */
u_modulation = PR_Calc(&PR_INV_U, iu_ref, iu);
v_modulation = PR_Calc(&PR_INV_V, iv_ref, iv);
w_modulation = PR_Calc(&PR_INV_W, iw_ref, iw);
```

发生停机、欠压或硬件 Fault 后，先关闭 PE0/PE1，再调用 `PI_Reset()` 和 `PR_Reset()` 清除控制器状态。

## 7. 主循环发送 VOFA

控制中断只更新测量与控制全局量，串口发送放在 `while(1)`，每 10 ms 发送一次：

```c
static uint32_t vofa_tick = 0U;

if ((HAL_GetTick() - vofa_tick) >= 10U) {
    float vofa_data[6];

    vofa_tick = HAL_GetTick();

    /* 通道顺序必须与 VOFA+ 中显示顺序保持一致。 */
    vofa_data[0] = vac;
    vofa_data[1] = ipfc;
    vofa_data[2] = vbus;
    vofa_data[3] = iu;
    vofa_data[4] = iv;
    vofa_data[5] = iw;

    /* 阻塞发送只允许放在主循环，禁止在控制中断调用。 */
    (void)VOFA_Send(&huart2, vofa_data, 6U);
}
```

六通道一帧共 28 字节。100 Hz 刷新时，即使使用 460800 baud，也有足够余量。

## 8. 上板顺序

1. 关闭 PE0/PE1，只检查 ADC 换算和 VOFA 波形。
2. 低母线、低调制度验证 HRTIM 输出、死区和硬件 Fault。
3. 先闭合单相整流电流环，再接入母线电压 PI 外环。
4. 逐相确认逆变电流极性，再同时运行 U/V/W 三个 PR。
5. 软件限幅不能代替 DESAT/OCP、HRTIM Fault 和驱动使能硬件下拉。
