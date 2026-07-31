# 单相PWM整流器无ADC虚拟反馈工作流程与验证注意事项

## 1. 当前版本的真实功能

当前工程是5 V RMS、50 Hz、30 ohm负载的低压虚拟反馈试验版本。ADC1、ADC2、DMA和模拟看门狗均未初始化；PI使用固定VBUS反馈，PR不运行。它不是电压闭环，也没有基于采样的过流、过压、输入丢失或母线欠压保护。

~~~text
HRTIM Master Repetition 10 kHz中断
-> 软件50 Hz相位
-> 理想VAC与电流参考
-> 1 kHz固定反馈PI + 电流指令斜坡
-> 电感压降前馈
-> SPWM Compare预装载
-> HRTIM A/B四路互补PWM
~~~

当前唯一仍可起作用的停机路径是：PD0人工停止、控制计算错误、持续调制限幅、Cortex异常、CSS/NMI、IWDG复位，以及HRTIM Fault 3。PB10外部未接故障源，所以FLT3当前不能算实际硬件过流保护。

> 无ADC模式只能用于断开功率母线的MCU波形验证，或5 V隔离限流条件下的短时试验。禁止直接迁移到36 V赛题功率运行。

## 2. 唯一用户配置入口

日常只修改：

~~~text
Core/Inc/pfc_user_config.h
~~~

当前默认开关为：

~~~c
#define PFC_USER_ACTIVE_PROFILE      PFC_USER_PROFILE_5V
#define PFC_USER_RUN_MODE            PFC_USER_MODE_VIRTUAL_FEEDBACK
#define PFC_USER_ENABLE_ADC          0U
#define PFC_USER_ENABLE_PWM_OUTPUT   1U
#define PFC_USER_OLED_PERIOD_MS      500U
~~~

| 参数 | 当前含义 | 修改后果 |
| --- | --- | --- |
| `PFC_USER_ENABLE_ADC` | `0U`完全跳过ADC/DMA/AWD | 改回`1U`前必须接回采样板、完成标定并复核保护阈值。 |
| `PFC_USER_ENABLE_PWM_OUTPUT` | `1U`允许PD0启动PWM | 改为`0U`后HRTIM计数器仍运行，但四路PWM始终关闭。 |
| `PFC_USER_OLED_PERIOD_MS` | OLED数字刷新周期，当前500 ms | 增大可减少软件I2C占用；过小可能出现肉眼闪动，不允许填0。 |
| `PFC_USER_5V_VIRTUAL_VBUS_FEEDBACK_V` | 固定送入PI的反馈，默认9 V | 低于9 V会使PI继续增加电流命令；它不是实测母线，不能自动稳压。 |
| `PFC_USER_5V_VIRTUAL_CURRENT_PRESET_A_RMS` | PI预置电流，默认0.10 A RMS | 增大会直接增加输入功率和母线过压风险，每次只允许小步增加。 |
| `PFC_USER_5V_VIRTUAL_CURRENT_SLEW_A_S` | 电流命令斜率，默认0.10 A/s | 增大会缩短启动时间，同时增加浪涌和调制突变。 |

0.10 A RMS输入约对应0.5 W理想输入功率。9 V、30 ohm负载需要约2.7 W，理想输入电流约0.54 A RMS；当前没有真实反馈和过压保护，不能直接把预置值一次改到0.54 A。

## 3. 上电与启停流程

~~~text
HAL/时钟/GPIO初始化
-> 跳过DMA、ADC1和ADC2初始化
-> 初始化HRTIM、USART、OLED、参数和应用模块
-> HRTIM输出保持关闭
-> 启动Master/A/B计数器
-> 打开HRTIM Master Repetition 10 kHz中断
-> 启动IWDG
-> SAFE等待100 ms
-> READY等待PD0有效短按
-> VIRTUAL_RAMP逐步增加电流命令
-> VIRTUAL_RUN维持虚拟反馈运行
~~~

PD0内部上拉、低有效。所谓“稳定释放30 ms”是指按键没有按下、PD0连续保持高电平30 ms。上电时若一直按住按键，系统不会解锁；松开并稳定30 ms后，还必须重新按下50～1000 ms再释放，才会产生一次启动事件。运行中再次短按会关闭四路PWM。

HRTIM计数器运行与PWM输出开放是两个状态：上电后Master/A/B始终计数，但PA8～PA11要到READY短按成功后才开放。当前驱动板没有MCU Gate Enable，四路PWM一旦开放就可能驱动MOSFET。

## 4. 10 kHz控制和调制关系

ADC关闭后，控制不再从`HAL_ADC_ConvCpltCallback()`进入，而从：

~~~text
HRTIM1_Master_IRQHandler()
-> HAL_HRTIM_IRQHandler(...MASTER)
-> HAL_HRTIM_RepetitionEventCallback()
-> PFC_AppFastStep()
-> PFC_Control_Step10k()
~~~

软件以参数档的50 Hz生成相位，不再跟随真实交流输入过零。输入源必须稳定为50 Hz并在接线时人工确认相位；否则桥侧指令和真实输入可能失配。

当前调制关系为：

~~~text
m = bridge_polarity * (synthetic_vac - L * d(i_ref)/dt) / vbus_target
duty_A = 0.5 * (1 - m)
duty_B = 0.5 * (1 + m)
CMP_A = 17000 * (1 - m)
CMP_B = 17000 * (1 + m)
~~~

`m=0`时两桥臂均为50%；`m=+0.5`时A约25%、B约75%。调制度被限制在正负0.90，Compare通过预装载在HRTIM更新边界生效。首次验证只接示波器观察PA8～PA11的10 kHz中心对齐互补波形和约500 ns死区。

## 5. OLED、VOFA与Keil Watch

OLED每500 ms只更新数字区域，不周期调用`OLED_Clear()`。页面为：

~~~text
R:xxxxx X:xxxxx   R=虚拟VBUS参考*100，X=固定反馈*100
A:xxxxx M:xxxxx   A=电流命令mA RMS，M=调制度*10000
H:xxxxx O:x       H=控制心跳后5位，O=PWM输出状态
F:xxxxx S:xx      F=锁存故障十六进制，S=应用状态
~~~

若整屏仍约每500 ms重新亮起，优先在Keil Watch检查`pfc_iwdg_reset_seen`。它为1表示仍发生IWDG复位；正常情况下`H`在PWM开启后持续增加，`O`在READY为0、RAMP/RUN为1。

VOFA每10 ms发送：

| 通道 | 无ADC模式数据 |
| ---: | --- |
| CH0 | 软件生成的VAC瞬时参考，V |
| CH1 | 软件生成的瞬时电流参考，A |
| CH2 | 虚拟VBUS参考，V |
| CH3 | 固定虚拟VBUS反馈，V |
| CH4 | PI输出电流，A RMS |
| CH5 | 斜坡后的电流命令，A RMS |
| CH6 | 调制度 |
| CH7 | 状态乘1000000加故障位 |

上述量全部是软件命令或状态，不是ADC实测值。VOFA曲线正常不能证明真实电压、电流安全。

## 6. IWDG与保护边界

主循环每100 ms只在以下条件全部成立时刷新IWDG：

- HRTIM 10 kHz时基心跳前进；
- 状态机未进入`FAULT_LATCH`；
- HRTIM计数器保持运行；
- 状态机要求的PWM状态与输出软件镜像一致；
- RAMP/RUN期间控制心跳前进且控制器健康；
- 没有已锁存的HRTIM、控制、调制或系统故障。

ADC序列、DMA心跳、样本有效、AWD、IPFC过流、VBUS过压和VAC丢失均不再参与监督。停止ADC不会触发故障，因为ADC本来就没有启动。

## 7. 分阶段验证

1. 不接驱动板和功率母线，上电确认OLED不再周期重启，`S`进入READY，`O=0`。
2. 短按PD0，确认`O=1`、`H`持续增加，PA8～PA11为10 kHz互补PWM。
3. 示波器检查两桥臂调制包络为50 Hz、Compare方向正确、死区无重叠。
4. 再接驱动板但不接5 V功率母线，使用差分探头确认门极波形和死区。
5. 只有前四步通过后，才使用隔离、限流5 V交流源和轻载短时试验；先保持0.10 A RMS命令。
6. 每次只增加一个小档电流，使用独立万用表/示波器测量真实VBUS和输入电流。任何异常立即断电，不能依赖当前软件保护。

## 8. 恢复ADC的方法

恢复采样时先断开功率母线，然后在`pfc_user_config.h`中将`PFC_USER_ENABLE_ADC`改为`1U`，将PWM开关改为`0U`，重新完成RAW ADC统计、零点/比例/极性标定和AWD阈值复核。确认后再允许PWM。

不要同时保留HRTIM Master和ADC1 DMA两个快速控制入口。代码已通过编译期开关保证：ADC关闭时只使用HRTIM Master ISR；ADC启用时只使用ADC1完整DMA回调。

## 9. 当前构建记录

2026-07-30使用Keil ARMCC 5.06 update 5构建：所有C/汇编文件编译完成且为0 warning；链接阶段因工程配置引用的`stm32g474xx_flash.sct`文件不存在而停止，报告`L6031U`。补回正确scatter文件后仍需重新完整链接并确认HEX生成。

本次未修改`04-2FinalLoop.ioc`、CubeMX生成的ADC/HRTIM配置文件或硬件接线。
