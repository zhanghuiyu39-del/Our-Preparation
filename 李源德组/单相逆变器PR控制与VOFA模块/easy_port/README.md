# PR 与 VOFA 简化移植说明

本目录是新项目推荐版本，面向 `STM32G474VCT6 + CubeMX + HAL`。
只包含通用 PR 和阻塞式 VOFA，不使用 DMA、SOGI-PLL、坐标变换或复杂状态机。

## 1. 加入 CubeMX 工程

把下面四个文件复制到目标工程对应目录，并在 Keil 中加入两个 `.c` 文件：

```text
Core/Inc/pr.h
Core/Inc/vofa.h
Core/Src/pr.c
Core/Src/vofa.c
```

`pr.c` 不依赖 HAL；`vofa.c` 通过 CubeMX 生成的 `main.h` 使用 UART HAL 接口。

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

## 4. PR 初始化

在 `main.c` 的用户变量区声明控制器：

```c
#include "pr.h"
#include "vofa.h"

/* 单相整流器使用一个电流 PR；三相逆变器每相使用一个 PR。 */
static PR_t PR_PFC_I;
static PR_t PR_INV_U;
static PR_t PR_INV_V;
static PR_t PR_INV_W;
```

在全部 `MX_xxx_Init()` 完成后初始化。下面数值只是参考起点，最终需要结合功率级调试：

```c
/* 参数顺序：Kp、Kr、基波 Hz、带宽 rad/s、实际控制频率 Hz、最小/最大输出。 */
PR_Init(&PR_PFC_I, 0.25f, 5.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_U, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_V, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
PR_Init(&PR_INV_W, 0.10f, 100.0f, 50.0f, 3.14159f, 20000.0f, -0.90f, 0.90f);
```

`20000.0f` 必须等于 `PR_Calc()` 的真实调用频率。中心对齐 PWM 可能一周期触发两次更新，应以实际中断频率为准。

## 5. 控制中断调用顺序

### 单相 PWM 整流器

本方案不使用 SOGI-PLL。先去除 VAC 采样偏置并换算为物理量，再得到归一化电压波形：

```c
/* VAC_PEAK_EST 是当前电网电压峰值估计，vac_norm 最终限制到 -1~1。 */
vac_norm = vac / VAC_PEAK_EST;
if (vac_norm > 1.0f)  vac_norm = 1.0f;
if (vac_norm < -1.0f) vac_norm = -1.0f;

/* pfc_current_amp_cmd 由现有母线电压 PI 外环给出。 */
ipfc_ref = pfc_current_amp_cmd * vac_norm;
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

发生停机、欠压或硬件 Fault 后，先关闭 PE0/PE1，再调用 `PR_Reset()` 清除四个控制器状态。

## 6. 主循环发送 VOFA

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

## 7. 上板顺序

1. 关闭 PE0/PE1，只检查 ADC 换算和 VOFA 波形。
2. 低母线、低调制度验证 HRTIM 输出、死区和硬件 Fault。
3. 先闭合单相整流电流环，再接入母线电压 PI 外环。
4. 逐相确认逆变电流极性，再同时运行 U/V/W 三个 PR。
5. 软件限幅不能代替 DESAT/OCP、HRTIM Fault 和驱动使能硬件下拉。
