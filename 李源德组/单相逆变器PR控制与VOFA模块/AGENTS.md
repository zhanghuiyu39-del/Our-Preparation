# 单相逆变器 PR 控制与 VOFA 模块工作规范

## 适用范围

- 本文件适用于当前目录及全部子目录。
- 本模块目标平台是 `STM32G474VCT6`、Cortex-M4F、LQFP100，使用 STM32CubeMX 生成的 HAL 工程。
- 本目录的目标器件约束优先于上级目录中面向 `STM32G474VET6` 的默认假设。设备宏仍为 `STM32G474xx`，但链接空间必须按 VCT6 的 256 KB Flash 配置。
- 项目的 `.ioc` 文件是 MCU、引脚、时钟、DMA、NVIC、工具链和固件包版本的唯一配置来源。不得根据旧工程源码反向猜测这些配置。

## 目录职责

```text
reference_original/  原始 control.c/.h、vofa.c/.h，只作行为和参数参考
portable/Inc         不依赖 STM32/HAL 的公共接口
portable/Src         PR、单相控制器和 JustFloat 队列的纯 C 实现
stm32g4_hal/Inc      STM32G4 HAL 外设适配接口
stm32g4_hal/Src      TIM PWM 与 UART DMA 适配实现
tests/               编译检查和不依赖硬件的测试入口
README.md            CubeMX 配置、接入方法和上板步骤
```

- `reference_original/` 中的四个文件视为只读基线。不得格式化、修复乱码、重命名或直接加入新工程编译。
- 修改基线只能由用户明确要求；修改前后必须保留原副本并记录 SHA-256。
- 通用算法放在 `portable/`，STM32 句柄、HAL 宏和外设调用只能放在 `stm32g4_hal/` 或最终应用工程。
- 不得把 CubeMX 生成的 `main.h`、`tim.h`、`usart.h`、启动文件或整套 HAL 驱动复制进本模块。

## 架构边界

### 纯算法层

- `pr_controller` 只实现二阶离散 PR，不读取 ADC、不写 PWM、不访问全局 HAL 句柄。
- `single_phase_control` 只接收已经换算为物理单位的电感电流、输出电压和母线电压，并输出调制量及观测数据。
- `vofa_justfloat` 只负责抽取、组帧和单生产者/单消费者队列，通过回调请求发送，不直接包含 STM32 HAL。
- API 必须通过对象和配置结构体保存状态。禁止重新引入固定的 `htim8`、`huart1`、GPIO 宏、ARR 常数或可变的模块级控制全局量。
- 不使用动态内存。快速路径必须有确定执行时间，不得递归，不得等待锁或外设。

### STM32G4 HAL 适配层

- 适配层接收 CubeMX 生成并已初始化的 `TIM_HandleTypeDef`、`UART_HandleTypeDef` 等句柄，不负责重新配置 RCC、GPIO、DMA 或 NVIC。
- `stm32g4_tim_unipolar_pwm` 仅适用于普通 TIM PWM，运行时读取 ARR 并更新 CCR。不得将普通 TIM API 冒充 HRTIM API。
- 高功率桥臂使用 HRTIM 时，应新增独立 HRTIM 适配器，把 `single_phase_control_step()` 的调制输出换算为 HRTIM 比较值，并保留硬件 Fault、死区和同步更新。
- CubeMX 生成文件中的用户代码只能写在 `USER CODE BEGIN/END` 区域，避免重新生成后丢失。

## PR 与控制算法规则

- 保持当前 Tustin 离散 PR 结构和符号约定。修改系数计算时，必须证明其与原始 `gain * (B0*w0 + B1*w1 + B2*w2)` 形式等价。
- `sample_frequency_hz` 必须等于 `single_phase_control_step()` 的实际调用频率。不得简单复制 PWM 标称频率；中心对齐 PWM 可能每周期产生两次更新事件。
- 使用 `float` 和单精度数学函数，以匹配 STM32G474 的单精度 FPU。只有测量证明 ISR 超时后，才允许用查表或 CMSIS-DSP 替换 `sinf()`。
- 默认模式、PR 参数、软启动速度、调制限幅、欠压门槛或反馈极性发生变化时，必须同步更新 `README.md` 和测试。
- 当前 `voltage_error_sign = -1.0f` 仅用于保持原工程行为，不代表所有硬件的正确负反馈方向。上板时必须通过低压开环和传感器极性验证。
- 母线电压无效、控制器禁用或输入不是有限数时，控制输出必须进入安全路径并复位动态状态，避免除零、NaN 和带状态重启。
- 软件限幅不等于抗积分饱和，也不等于硬件保护。新增限幅或抗饱和策略时必须说明对 PR 状态和环路相位的影响。

## 实时性与中断规则

- 快速控制 ISR 中允许：读取已完成的采样、物理量换算、调用控制步进、更新 PWM 比较值、向 VOFA 队列压入抽取后的帧。
- 快速控制 ISR 中禁止：`HAL_UART_Transmit()`、轮询等待、文件/Flash 写入、格式化字符串、动态分配、长临界区和调试延时。
- VOFA 使用单生产者/单消费者模型：控制 ISR 推进 `head`，UART 完成/错误回调推进 `tail`，主循环调用 `process()` 启动 DMA。
- 未增加临界区或原子协议前，不得从两个 ISR 同时调用 `vofa_justfloat_push_isr()`，也不得让两个上下文同时调用 `vofa_justfloat_process()`。
- 队列满时应丢弃新帧并增加统计计数，不能阻塞控制环等待串口。
- 控制 ISR 优先级应高于 UART/DMA 调试链路。修改 NVIC 优先级时检查是否出现嵌套中断竞争或控制抖动。
- STM32G474 是 Cortex-M4F、无数据缓存，不套用 M7 的 D-Cache 维护流程；移植到带 D-Cache 的 M7/H7 时必须重新处理 DMA 缓冲区对齐和缓存一致性。

## CubeMX 外设约束

### PWM 与 ADC

- PWM、ADC 触发和控制 ISR 必须共享清晰的时序基准。优先由 PWM/HRTIM 在开关噪声较低的位置触发 ADC。
- ADC 原始码、分压比、运放偏置和传感器增益属于测量层，不得写入通用控制器。
- 上板前用示波器确认实际 PWM 频率、两桥臂占空关系、死区和 ADC 采样点。
- Gate Enable、预充、放电、DESAT/OCP、Break/Fault 和急停由硬件及应用状态机管理。不得只靠调制量为零或 50% 中性占空比关断功率级。

### VOFA UART DMA

- 优先采用 CubeMX 配置的 `USART2 + TX DMA Normal`；引脚由 `.ioc` 和实际 PCB 决定，不在模块里固定 PA2/PA3 或 PD5/PD6。
- HAL 完成回调和错误回调只负责转发到 `stm32g4_vofa_uart_dma`，不得在回调中循环发送下一帧。
- VOFA+ 选择 `JustFloat`，通道数必须与初始化参数一致。帧格式为 little-endian IEEE-754 float 数组加 `00 00 80 7F` 帧尾。
- 修改通道数、控制频率、抽取倍数或波特率前必须核算带宽：

```text
frame_bytes   = channels * 4 + 4
frame_rate    = control_frequency / decimation
required_baud = frame_bytes * frame_rate * 10
```

- 为中断、DMA 间隙和 USB-TTL 抖动保留余量。20 kHz、3 通道、460800 baud 时推荐 `decimation >= 10`；原始 `decimation = 5` 不适用于 460800 baud。

## C 代码约定

- 使用 C99、固定宽度整数、`bool`、头文件保护和必要的 `extern "C"` 包装。
- 类型使用 `_t` 后缀，公共函数以模块名为前缀，内部函数声明为 `static`。
- 初始化函数检查空指针、范围、有限数和不变量，并明确返回成功或失败。
- 配置默认值集中在 `single_phase_control_get_default_config()`；调用者先获取默认配置，再覆盖项目参数。
- **任何新增或实质修改的代码都必须在相邻位置带有注释。** 注释应紧靠对应的函数、结构体字段、宏、条件分支或逻辑块，说明修改目的及其使用约束，不能只在提交说明或远处的文档中解释。
- 新增或修改公共 API 时，必须在头文件声明旁注释输入/输出、物理单位、调用上下文、返回值和副作用。
- 新增或修改控制公式、系数、限幅、软启动、采样周期和反馈符号时，必须在代码旁注释公式来源、单位及关键假设。
- 新增或修改 ISR、DMA、环形队列或共享状态时，必须在代码旁注释生产者/消费者、读写所有权、中断上下文以及是否允许阻塞。
- 新增或修改 HAL 外设调用时，必须在代码旁注释依赖的 CubeMX 配置、句柄来源和硬件安全边界。
- 简单连续赋值可以由其上方一条逻辑块注释统一说明，无需机械地逐行重复；但不得出现没有任何相邻注释的新逻辑块。
- 注释解释用途、单位、并发所有权、硬件限制和非显然公式，不得只复述代码字面行为。
- 保持源代码和标识符为 ASCII；面向使用者的 Markdown 可以使用中文 UTF-8。

## 修改流程

1. 阅读本文件、`README.md`、相关公共头文件和原始基线，确认需求属于算法层、HAL 适配层还是最终应用层。
2. 从目标工程 `.ioc` 核对 MCU 型号、实际控制频率、TIM/HRTIM、ADC 触发、UART、DMA、NVIC 和工具链。
3. 只修改职责对应的最小文件集合，保持公共 API 向后兼容；需要破坏性 API 变化时同步修改所有调用示例，并为每个新增或修改的代码块补充相邻注释。
4. 控制公式变化后，对默认参数逐项比较新旧系数，并检查零输入、欠压、禁用、软启动和正负限幅。
5. VOFA 变化后检查帧尾、通道顺序、队列满、DMA Busy、发送完成和错误回调路径。
6. 更新 `README.md`，说明 CubeMX 配置、回调接法、带宽预算和新增安全边界。
7. 编译全部模块并检查工作区，只交付源码、测试和文档，不保留 `.o`、临时工程或自动生成日志。

## 验证与交付标准

- 使用目标工程相同的编译器和宏进行编译。当前已验证基线是 ARMCC 5、C99、Cortex-M4F hard-float、`USE_HAL_DRIVER`、`STM32G474xx`。
- 至少编译以下文件：三个 `portable/Src`、两个 `stm32g4_hal/Src` 和 `tests/module_smoke.c`。
- 编译目标为 `0 Error(s), 0 Warning(s)`。最终集成工程还必须生成适用于 VCT6 Flash 空间的 HEX/ELF。
- 交付前逐个检查新增和修改的代码块是否带有相邻注释；缺少注释视为未完成，即使编译已经通过也不得交付。
- 确认 `reference_original/` 的四个 SHA-256 与来源一致，确认没有意外修改 CubeMX 生成文件或其它组目录。
- 纯算法测试不能替代硬件验收。上板顺序固定为：ADC/VOFA 测量验证、低压低调制开环、电流环、电压外环，最后才提高母线和负载。
- 每次上板都要验证硬件 Fault/Break 能在 CPU 失效时独立关断，并确认欠压、传感器越界和通信丢帧不会阻塞快速控制环。
