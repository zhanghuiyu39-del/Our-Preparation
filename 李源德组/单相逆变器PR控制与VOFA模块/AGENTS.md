# 单相 PWM 整流与三相逆变 PR、VOFA 模块工作规范

## 适用范围与目标

- 本文件适用于当前目录及全部子目录。
- 目标芯片为 `STM32G474VCT6`、Cortex-M4F、LQFP100，工程由 STM32CubeMX 生成并使用 HAL。
- 设备宏使用 `STM32G474xx`，链接空间按 VCT6 的 256 KB Flash 配置。
- 最终拓扑是单相 PWM 整流器加三相逆变器；不主动引入 SOGI-PLL、Clarke/Park 变换、RTOS 或复杂通信框架。
- 以容易复制、容易调参、容易在 Keil Watch 中观察为优先，不为了“通用”增加当前项目用不到的抽象。

## 用户协作与代码偏好

- 默认采用 CubeMX 生成框架加用户 `.c/.h` 模块，所有生成文件改动必须放在 `USER CODE BEGIN/END` 区域。
- 偏好裸机前后台结构：高频采样和控制放定时器、ADC 或 HRTIM 回调，VOFA/OLED 等低速调试放 `while(1)`。
- 控制器优先使用公开结构体保存参数和状态，并采用 `Init/Reset/Calc` 这种直接过程式接口。
- 允许在应用层使用直观的全局物理量和 `extern`，但必须注明单位、更新位置和读写上下文。
- 参数应集中、易改并靠近初始化代码；不要把同一参数散落在多个宏、局部常量和源文件中。
- 中文注释重点说明硬件用途、物理意义、调参方向、调用位置和安全限制，不写空泛说明。

## 目录与版本职责

```text
easy_port/            当前推荐版本，新项目只从这里移植
reference_original/   原始 control.c/.h、vofa.c/.h，只读基线
portable/             第一版通用算法和队列，高级参考
stm32g4_hal/          第一版 TIM/UART DMA 适配，高级参考
tests/                第一版高级模块测试
README.md             总入口与版本说明
```

- 默认只修改 `easy_port/`、根 `README.md` 和本文件。
- `reference_original/` 不得格式化、修复乱码、重命名或加入新工程编译；交付前核对 SHA-256。
- `portable/`、`stm32g4_hal/` 和 `tests/` 只在用户明确要求高级版本、DMA 或更强封装时修改。
- 不得把其它工程的 `main.h`、HAL 驱动、启动文件、链接脚本或 CubeMX 生成外设文件复制到本模块。

## 简化优先原则

- 新功能先判断能否在现有 `PR_t`、`PR_Init()`、`PR_Reset()`、`PR_Calc()`、`VOFA_Send()` 上完成。
- 没有明确需求时，不新增模式枚举、嵌套配置对象、函数指针、回调注册、环形队列、动态内存或多层适配器。
- 不把 ADC 换算、PWM/HRTIM 更新、软启动、按键、状态机和保护逻辑塞进 PR 模块。
- 不把串口初始化、DMA、调度器或控制器数据采集塞进 VOFA 模块。
- 示例代码应保持“初始化在哪里、ISR 调什么、主循环调什么”一眼可见，避免过长框架代码。

## PR 模块规则

- `easy_port/Core/Inc/pr.h` 公开 `PR_t`，方便观察 Kp、Kr、离散系数、历史状态、输出和限幅。
- `easy_port/Core/Src/pr.c` 只负责 PR 系数初始化、状态复位和单步计算，不依赖 HAL。
- PR 误差固定为 `reference - feedback`；传感器反相应在测量层修正，不在控制器内部增加隐藏符号。
- 保持参考代码的 Tustin 准 PR 公式。若改动系数，必须证明与原形式等价：

```text
output = gain * (B0*w0 + B1*w1 + B2*w2)
w0     = error - A1*w1 - A2*w2
```

- 新实现允许把 `gain` 预先合并进 B0/B1/B2，但默认 20 kHz、50 Hz 参数的新旧五个系数差值必须为 0 或处于单精度舍入范围内。
- `sample_frequency_hz` 必须等于 `PR_Calc()` 的实际调用频率。中心对齐 PWM 可能一周期产生两次更新，不能直接把 PWM 标称频率当控制频率。
- `PR_Calc()` 只使用 `float`，不得调用 HAL、串口、延时、动态分配或格式化函数。
- 停机、欠压、Fault 清除前后必须调用 `PR_Reset()`，避免带着旧状态重新闭环。
- 软件输出限幅不代替抗饱和设计或硬件过流保护；需要增加抗饱和时必须先说明算法影响。

## 最终拓扑的控制边界

- 单相 PWM 整流器默认使用一个交流电流 PR；母线电压 PI 外环属于最终应用，不封装进 `pr.c`。
- 不使用 SOGI-PLL。整流电流参考默认由去偏置、换算并限制到 `-1~1` 的 `VAC` 波形乘电流幅值指令得到。
- 三相逆变器默认沿用固定频率正弦表或相位索引，分别创建 `PR_INV_U`、`PR_INV_V`、`PR_INV_W`。
- 未经用户明确要求，不改为 alpha-beta 两 PR、dq PI 或空间矢量控制。
- 控制 ISR 的推荐顺序为：采样换算、参考生成、PR 计算、调制限幅、HRTIM 比较值更新。

## VOFA 模块规则

- `easy_port` 使用 `HAL_UART_Transmit()` 阻塞发送 JustFloat，不使用 DMA、UART 中断、发送回调或队列。
- `VOFA_Send()` 接收 CubeMX 生成的 UART 句柄、float 数组和通道数，支持 1~8 通道。
- 帧格式固定为连续 little-endian IEEE-754 float 加帧尾 `00 00 80 7F`。
- 使用一个静态发送缓冲区，函数不可重入，只能在 `while(1)` 中单点调用。
- 禁止在控制 ISR、HRTIM Fault 回调或 ADC 快速回调中调用 `HAL_UART_Transmit()`。
- 阻塞超时默认 2 ms，不使用 `HAL_MAX_DELAY`。
- 默认每 10 ms 发送一次 `VAC、IPFC、VBUS、IU、IV、IW`，即六通道 28 字节、100 Hz。
- 改变通道数或刷新率时核算带宽：`required_baud = frame_bytes * frame_rate * 10`。

## CubeMX 与引脚约束

- `.ioc` 是 MCU、时钟、引脚、ADC 触发、HRTIM、Fault、UART 和工具链的唯一配置来源。
- 单相整流桥使用 HRTIM A/B：PA8/PA9、PA10/PA11。
- 三相逆变 U/V/W 使用 HRTIM C/D/E：PB12/PB13、PB14/PB15、PC8/PC9。
- 快速采样使用 PA1/IPFC、PC0/VAC、PB0/IU、PD8/IV、PD9/IW；母线采样使用 PC1/VBUS。
- PE0、PE1 分别为 PFC 和逆变 Gate Enable，复位和初始化阶段必须保持低电平并具有板级下拉。
- PB10 为低有效 HRTIM1_FLT3 总故障输入，硬件 Fault 必须能在 CPU 失效时独立关闭输出。
- VOFA 使用 USART2 PD5/PD6，460800 或 921600 baud，8N1，无 DMA；阻塞发送时不需要 UART NVIC。
- HRTIM 死区、同步更新、ADC Trigger 和 Fault 锁存由 CubeMX/最终工程负责，PR 模块不直接操作 HRTIM。

## 源码与中文注释要求

- 使用 C99、`float`、固定宽度整数、头文件保护和必要的 `extern "C"`。
- 文件名和标识符使用英文 ASCII；源码和 Markdown 注释使用中文 UTF-8。
- 任何新增或实质修改的代码都必须在相邻位置有中文注释，不能只在 README 或提交说明中解释。
- 结构体字段应按“用户参数、离散系数、历史状态、输出限制”分区注释。
- 公共函数声明旁必须注明参数单位、返回值、调用位置、副作用和是否允许在 ISR 调用。
- 控制公式旁必须注明来源、离散方式和关键假设；HAL 调用旁必须注明 CubeMX 依赖和阻塞限制。
- 简单连续赋值可由一条块注释统一说明，不要求逐行重复；禁止为了满足注释数量写无意义注释。
- 不保留大段注释掉的旧实现；需要保留旧方案时放到参考目录或版本历史中。

## 修改流程

1. 先读本文件、根 `README.md`、`easy_port/README.md` 和目标工程 `.ioc`。
2. 确认需求属于 PR、VOFA、测量、HRTIM 还是应用状态机，避免跨模块堆功能。
3. 优先修改最少文件并保持现有四个公共接口；接口变化时同步更新全部示例。
4. 修改 PR 后比较新旧系数，并检查未初始化、零误差、正负误差、上下限和 Reset。
5. 修改 VOFA 后检查空指针、1/6/8 通道、越界通道、帧长、帧尾、超时和调用位置。
6. 同步更新 `easy_port/README.md`，只保留用户实际需要的 CubeMX 和调用步骤。
7. 编译、静态检查、核对原始文件哈希，再检查工作区中是否残留 `.o` 或临时文件。

## 验证与交付标准

- 使用 ARMCC 5、C99、Cortex-M4F hard-float、`USE_HAL_DRIVER`、`STM32G474xx` 编译 `easy_port`。
- 编译必须达到 `0 Error(s), 0 Warning(s)`。
- 静态检索 `easy_port/Core`，默认不得出现 DMA、SOGI、PLL、队列或 UART 完成回调依赖。
- `HAL_UART_Transmit()` 只能出现在 `vofa.c`，示例只能从主循环调用。
- 六通道 JustFloat 帧必须为 28 字节，帧尾必须为 `00 00 80 7F`。
- `reference_original/` 四个文件的 SHA-256 必须与来源一致。
- 交付前逐块检查中文注释；缺少相邻注释即视为未完成。
- 软件测试不能替代上板验收。上板顺序固定为 ADC/VOFA、低压开环、整流电流环、母线外环、三相逐相闭环、最终联合运行。
- 软件限幅、PR Reset 和串口超时都不能代替 DESAT/OCP、HRTIM Fault、急停和 Gate Enable 硬件保护。
