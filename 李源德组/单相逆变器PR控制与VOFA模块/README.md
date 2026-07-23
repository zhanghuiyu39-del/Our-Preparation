# 单相逆变器 PR 双环与 VOFA 模块

本目录面向 `STM32G474VCT6 + STM32CubeMX/HAL`，包含四个部分：

```text
reference_original/       原始 control.c/.h、vofa.c/.h 的只读备份
portable/Inc              与 MCU 无关的 PR 和双环控制头文件
portable/Src              与 MCU 无关的 PR 和双环控制实现
stm32g4_hal/Inc           STM32G4 HAL 的 TIM PWM、UART DMA 适配头文件
stm32g4_hal/Src           STM32G4 HAL 适配实现
```

## 与原代码相比的变化

- `pr_controller.c` 是独立的二阶离散 PR 控制器，保留原代码的 Tustin 离散公式，并把 `gain` 合并进归一化系数，降低再次移植时漏乘的风险；原文件中未赋值的 `CTRL_MODE` 不再使用。
- `single_phase_control.c` 实现开环、仅电压 PR、仅电流 PR、电压外环加电流内环四种模式。它不包含 `main.h`、`tim.h`、GPIO、ADC 或 UART，输入和输出都是结构体，适合单元测试和换芯片。
- `stm32g4_tim_unipolar_pwm.c` 从 CubeMX 生成的 `TIM_HandleTypeDef` 读取 ARR，不再写死 TIM8、通道或周期。它只负责设置 CCR，不负责配置时钟、GPIO、死区、刹车或功率级使能。
- `vofa_justfloat.c` 使用单生产者/单消费者环形队列。快速控制中断只做抽取和内存拷贝，主循环启动 UART DMA，发送完成回调推进队列，控制中断绝不会阻塞在 UART 上。
- `stm32g4_vofa_uart_dma.c` 只调用 CubeMX 已初始化的 UART DMA，不会重新初始化 UART、DMA 或 GPIO。

## 1. 加入 CubeMX/Keil 工程

在 CubeMX 生成 `STM32G474VCT6` 工程后，将以下文件加入 `Core/Src` 或 Keil 的对应 Group，将以下目录加入 Include Paths：

```text
portable/Src/pr_controller.c
portable/Src/single_phase_control.c
portable/Src/vofa_justfloat.c
stm32g4_hal/Src/stm32g4_tim_unipolar_pwm.c
stm32g4_hal/Src/stm32g4_vofa_uart_dma.c

portable/Inc
stm32g4_hal/Inc
```

保留 CubeMX 生成的 `main.c`、`tim.c`、`usart.c`、`dma.c`、`stm32g4xx_it.c`。本模块不应复制其它工程的 HAL 驱动、启动文件或 `main.h`。

## 2. CubeMX 配置要点

### MCU 和时钟

选择 `STM32G474VCT6`，工具链选择项目实际使用的 MDK-ARM/ARMCC 或 ARMCLANG。系统时钟、定时器时钟和 ADC 时钟以当前 `.ioc` 为准；控制模块的 `sample_frequency_hz` 必须等于快速控制实际调用频率，而不是凭 PWM 名义频率猜测。

### PWM

`stm32g4_tim_unipolar_pwm.c` 适用于普通 `TIM1/TIM8/TIM20` 等带 PWM 通道的定时器。配置两个 PWM 通道、预装载和合适的输出极性，然后在代码中传入 CubeMX 生成的句柄和通道：

```c
static stm32g4_tim_unipolar_pwm_t g_pwm;

/* 在 MX_TIMx_Init() 完成之后调用；这里不填死 TIMx 或 ARR */
stm32g4_tim_unipolar_pwm_init(&g_pwm,
                              &htim1,
                              TIM_CHANNEL_1,
                              TIM_CHANNEL_3,
                              0.90f);
stm32g4_tim_unipolar_pwm_set_neutral(&g_pwm);
```

中心对齐 PWM 的实际更新频率由 CubeMX 的定时器时钟、Prescaler、Counter Mode 和 ARR 共同决定。应使用示波器或计数器确认频率，并让控制 ISR 与 ADC 采样点同步。死区、Break 输入、互补输出和栅极驱动器使能必须在 CubeMX/硬件保护链路中完成；软件调制限幅不是短路保护。

本仓库的 G474 引脚分配方案建议高功率桥臂使用 HRTIM。该模块没有假装把 TIM CCR API 套在 HRTIM 上：如果功率级使用 HRTIM，请保留 `single_phase_control_step()`，再在项目自己的 HRTIM 适配函数中把 `output.modulation` 转换为 HRTIM 比较值，并继续使用硬件 Fault/Break 关断。上面的普通 TIM 适配器适合低功率验证或确实使用 TIM PWM 的拓扑。

### ADC 和快速控制触发

推荐由 PWM/HRTIM 触发 ADC，在 ADC DMA 完成或与 PWM 同步的高优先级 ISR 中更新控制。传入控制器之前，将 ADC 原始码转换为物理量：

```text
inductor_current_a  -> 电感电流，单位 A
output_voltage_v    -> 输出电压，单位 V
dc_bus_voltage_v    -> 母线电压，单位 V
```

不要在控制器内部混入 ADC 分压比、运放偏置或 DMA 缓冲区下标，这些属于测量层。G474 是 Cortex-M4F，单精度 `sinf()` 可直接运行；如果测得 ISR 裕量不足，再将正弦发生器替换为查表，不需要改变控制器 API。

### VOFA 串口和 DMA

按照本项目引脚分配文档，优先使用 `USART2`，例如 `PD5 = TX、PD6 = RX`；若你的实际 PCB 采用 CubeMX 默认的 `PA2/PA3`，只需在 `.ioc` 中改引脚，C 代码仍然传入 `&huart2`。

CubeMX 中：

1. 使能 USART2 Asynchronous，8 data bits、1 stop bit、无校验、无硬件流控。
2. 增加 USART2 TX DMA，使用 Normal 模式，启用 DMA 和 USART 中断。
3. 建议从 `921600` baud 开始；若 USB-TTL 或上位机不稳定，使用 `460800` 并增大抽取倍数。
4. 不要在 `HAL_UART_TxCpltCallback()` 里再次启动 DMA；只转发给模块。

`JustFloat` 帧格式是每个通道一个 little-endian IEEE-754 float，帧尾固定为 `00 00 80 7F`。VOFA+ 中选择 **JustFloat**，通道数与代码一致，不要选择 FireWater/ASCII。

带宽计算（8N1 约 10 bit/byte）：

```text
每帧字节数 = 通道数 * 4 + 4
帧率       = 控制频率 / decimation
所需波特率 ≈ 每帧字节数 * 帧率 * 10
```

例如 20 kHz、3 通道、`decimation=10` 时约 2 kframe/s、32 kB/s，需要约 320 kbit/s；460800 有余量。原始代码的 `decimation=5` 会产生 4 kframe/s、64 kB/s，460800 不足，921600 才适合。

## 3. 推荐的用户代码接入

以下代码放在你自己的应用文件，或者放进 CubeMX 的 `USER CODE BEGIN 2/4` 区域。变量名按你的工程修改；不要直接把下面的 `htim1/huart2` 当成固定资源。

```c
#include "single_phase_control.h"
#include "stm32g4_tim_unipolar_pwm.h"
#include "stm32g4_vofa_uart_dma.h"

extern TIM_HandleTypeDef htim1;   /* CubeMX 生成 */
extern UART_HandleTypeDef huart2; /* CubeMX 生成 */

static single_phase_control_t g_control;
static stm32g4_tim_unipolar_pwm_t g_pwm;
static stm32g4_vofa_uart_dma_t g_vofa;

static void App_Control_Init(void)
{
    single_phase_control_config_t config;

    single_phase_control_get_default_config(&config);
    config.mode = SINGLE_PHASE_CONTROL_DUAL_LOOP;
    config.sample_frequency_hz = 20000.0f; /* 必须匹配实际 ISR 频率 */
    config.reference_amplitude_target = 24.2f * 1.41421356f;
    config.voltage_error_sign = -1.0f;     /* 原工程保持此极性 */

    if (!single_phase_control_init(&g_control, &config)) {
        Error_Handler();
    }
    if (!stm32g4_tim_unipolar_pwm_init(&g_pwm,
                                       &htim1,
                                       TIM_CHANNEL_1,
                                       TIM_CHANNEL_3,
                                       config.modulation_limit)) {
        Error_Handler();
    }
    if (!stm32g4_vofa_uart_dma_init(&g_vofa, &huart2, 3U, 10U)) {
        Error_Handler();
    }

    stm32g4_tim_unipolar_pwm_set_neutral(&g_pwm);
    /* 保护、预充和 Gate Enable 确认后再启动 PWM，并单独使能控制器 */
    single_phase_control_enable(&g_control, true);
}

/* 在 ADC/PWM 同步的快速 ISR 中调用。这里的三个量已经是物理单位。 */
static void App_Control_FastStep(float il_a, float uo_v, float vdc_v)
{
    single_phase_control_input_t input = {il_a, uo_v, vdc_v};
    single_phase_control_output_t output =
        single_phase_control_step(&g_control, &input);

    if (output.dc_bus_valid) {
        stm32g4_tim_unipolar_pwm_apply(&g_pwm, output.modulation);
    } else {
        /* 欠压时关闭控制器和功率级；不要仅依赖 50% 中性占空比 */
        single_phase_control_enable(&g_control, false);
        (void)stm32g4_tim_unipolar_pwm_stop(&g_pwm);
    }

    (void)stm32g4_vofa_uart_dma_push3_isr(&g_vofa,
                                          output.voltage_reference_v,
                                          uo_v,
                                          output.modulation);
}

/* 主循环中反复调用，DMA 忙时立即返回，不会阻塞。 */
static void App_Background(void)
{
    (void)stm32g4_vofa_uart_dma_process(&g_vofa);
}

/* 放入 stm32g4xx_it.c 或用户应用文件的 HAL 回调转发区。 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    stm32g4_vofa_uart_dma_on_tx_complete_isr(&g_vofa, huart);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    stm32g4_vofa_uart_dma_on_error_isr(&g_vofa, huart);
}
```

`App_Control_FastStep()` 应由你实际的 ADC DMA/HRTIM/TIM 触发回调调用。例如使用 TIM 更新中断时，在 `HAL_TIM_PeriodElapsedCallback()` 中先确认 `htim->Instance`，再读取已经完成的 ADC 数据。中心对齐计数器可能每周期产生两次更新事件，必须通过 CubeMX 的 repetition/update 选择和实测频率保证它只以期望频率执行。

## 4. 控制器参数和调试顺序

`single_phase_control_get_default_config()` 复制了原工程的主要数值：20 kHz 采样、50 Hz 基波、24.2 Vrms 目标、`Kp/Kr = 0.1/100` 的电压 PR、`0.25/5` 的电流 PR、`Wc = pi`、`Wo = 2*pi*50`，默认双环。原代码的 `PR_U_ERROR_SIGN=-1` 也被保留，但最终应根据你的电压传感器极性和功率级方向，用低压开环确认反馈极性；正反馈迹象出现时立即停机并改为 `+1` 或调整测量符号。

建议顺序：

1. 只启动 ADC 和 VOFA，确认三路物理量比例、零点和符号。
2. 使用 `SINGLE_PHASE_CONTROL_OPEN_LOOP`，低母线电压、低 `open_loop_modulation_target`，确认两路 PWM 占空互补且 Gate/硬件 Fault 正常。
3. 单独调电流环，再接入电压外环；逐步增加 `reference_amplitude_target`。
4. 观察 `dc_bus_valid`、`output.modulation` 和 VOFA 队列丢帧计数。任何 Fault、欠压、传感器越界都应由应用状态机关闭 Gate Enable，而不是只把软件调制量写成零。

## 5. 重要边界

- 控制器只做浮点算法和软件限幅，不替代 HRTIM Fault、驱动器 DESAT/OCP、硬件急停、预充和放电状态机。
- `single_phase_control_enable(false)` 会清除 PR 状态和软启动状态；重新使能会从零开始软启动。
- 欠压时 `single_phase_control_step()` 返回 `dc_bus_valid=false` 并清除 PR 状态，应用层仍必须关闭 PWM/Gate。
- VOFA 队列满时丢弃新帧并递增 `dropped_frames`，不会阻塞控制 ISR。请根据带宽降低通道数或增大抽取倍数。
- 普通 TIM 适配器只写 CCR，不处理 HRTIM 的重复更新、死区和 Fault；使用 G474 高功率 HRTIM 时必须编写对应的 HRTIM 比较值适配。
