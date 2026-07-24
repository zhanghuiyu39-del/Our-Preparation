# STM32 工程工作规范

## 范围与安全

- 只能在用户指定的组别工程目录内操作，不得修改其他组的工程或无关的工作区改动。
- 以每个工程的 `.ioc` 文件为 MCU、引脚、时钟、工具链和固件包的唯一配置来源。
- 应用代码必须写在 CubeMX 生成文件的 `USER CODE BEGIN/END` 区域内，以保证重新生成后仍能保留。
- 不得将 STM32F1 的启动文件、头文件或 HAL 驱动复制到 STM32G4 工程中。

## STM32G474VET6 基线

- 使用 LQFP100 封装的 `STM32G474VETx`，使用本地已安装的 `STM32Cube FW_G4 V1.6.2`。设置 `ProjectManager.LastFirmware=false`，不下载或引入更新的固件包。
- 除非任务明确要求，否则保持 HSI 16 MHz 系统时钟。
- 生成含 HAL 驱动的 `MDK-ARM` 工程，设备定义必须为 `STM32G474xx`。
- 参考按键点灯工程中，PC13 配置为 `LED_Pin`：推挽输出、无上下拉、高速、初始低电平。PA7 配置为 `LED_Input`：输入、内部上拉。
- 该接法为低电平有效：PA7 为低表示按键按下，PC13 为低点亮 LED。

## CubeMX 生成

- CubeMX 安装路径为 `D:\\STM32CubeMX\\STM32CubeMX.exe`。无人值守生成时，使用 `D:\\STM32CubeMX\\jre\\bin\\java.exe -jar D:\\STM32CubeMX\\STM32CubeMX.exe -q <脚本路径>` 执行脚本模式。
- 生成脚本应依次加载 `.ioc`、设置工程名、工程路径和 `MDK-ARM` 工具链，再执行 `project generate` 和 `exit`。
- CubeMX 脚本模式对中文路径可能编码异常。命令行生成时，可以创建临时 ASCII 目录联接，例如 `C:\\codex_workspace`，令其指向对应组别目录，并在脚本中使用该路径。只能删除经过目标验证的临时联接，不使用 `SUBST`。
- 生成成功后删除临时脚本和临时联接，不在工程元数据中留下临时 ASCII 路径。

## HAL 应用模式

- 必须使用 CubeMX 生成的引脚宏，例如 `LED_Pin_GPIO_Port`、`LED_Pin_Pin`、`LED_Input_GPIO_Port` 和 `LED_Input_Pin`；不引入从其他工程复制而来的过期名称。
- 轮询按键时，首次检测到 PA7 低电平后再延时 20 ms，第二次仍为低才判定为有效按下。
- 主循环中，消抖后的按键状态为低时将 PC13 写为低点亮 LED，否则写为高熄灭 LED。

## 编译与验收

- Keil uVision 安装路径为 `D:\\Keil5\\UV4\\UV4.exe`。每次修改代码或 CubeMX 配置后，都要编译生成的 `.uvprojx` 工程。
- 成功编译必须报告 `0 Error(s), 0 Warning(s)`，并生成 `MDK-ARM/<工程名>/<工程名>.hex`。
- 交付前检查 `.ioc` 中的 MCU、G4 固件包、GPIO 模式/上下拉/初始电平，检查 Keil 设备定义和启动文件，检查按键消抖逻辑、编译结果和 HEX 文件。
- 烧录后的硬件验证标准：PA7 空闲时为高、PC13 为高且 LED 熄灭；PA7 接地时 LED 点亮；释放 PA7 后 LED 熄灭。
