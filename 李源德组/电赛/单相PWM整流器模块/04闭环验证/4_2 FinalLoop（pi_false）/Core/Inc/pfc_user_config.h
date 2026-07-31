#ifndef PFC_USER_CONFIG_H
#define PFC_USER_CONFIG_H

/*
 * 单相PWM整流器唯一人工配置入口。
 *
 * 当前默认用于“不启用ADC的5 V虚拟反馈波形试验”。配置风格参考inv_user_config.h：
 * 日常只修改本文件中的工作模式、外设开关和虚拟反馈参数，不需要进入各功能模块散改。
 * 本文件只保存编译期常量，不提供串口在线改参或Flash持久化。
 */

/* ======================== 1. 参数档与运行模式 ======================== */
/*
 * 参数档决定后续采用哪一整套电气、标定、保护和控制参数。参数档只在编译期选择，
 * 下载后不能通过按键或串口切换。切换参数档前必须先回到RAW_ADC模式，重新完成该档
 * 自己的ADC标定；5 V档的零点、比例和控制增益不能直接复制给36 V档。
 */
#define PFC_USER_PROFILE_5V                  0U /* 5 V RMS隔离低压调试档编号，不直接作为物理量使用。 */
#define PFC_USER_PROFILE_36V                 1U /* 36 V RMS赛题预留档编号，未完成实测前禁止闭环。 */

/*
 * RAW_ADC：只观察ADC原始码统计，工程量无效，PD0和PWM均禁止。
 * ENGINEERING_CHECK：启用标定换算和AWD，用万用表/示波器复核，但PD0和PWM仍禁止。
 * CLOSED_LOOP：允许状态机在全部条件满足后接受PD0短按并开放HRTIM输出。
 */
#define PFC_USER_MODE_RAW_ADC                0U /* 最安全的标定模式；首次上板、改采样板后都先选它。 */
#define PFC_USER_MODE_ENGINEERING_CHECK      1U /* 工程量复核模式；要求当前档CALIBRATION_CONFIRMED=1U。 */
#define PFC_USER_MODE_VIRTUAL_FEEDBACK       2U /* 虚拟母线反馈模式：PI不读取VBUS，PR暂不调用。 */
#define PFC_USER_MODE_CLOSED_LOOP            PFC_USER_MODE_VIRTUAL_FEEDBACK /* 兼容旧名称。 */

/*
 * 日常只需要先改下面两行。推荐顺序固定为5V+RAW_ADC -> 5V+ENGINEERING_CHECK
 * -> 5V+CLOSED_LOOP。切换到36V不会自动重算ADC比例、PR/PI或保护阈值。
 */
#define PFC_USER_ACTIVE_PROFILE              PFC_USER_PROFILE_5V  /* 当前采用的整套参数档；改后必须全量重新编译。 */
#define PFC_USER_RUN_MODE                    PFC_USER_MODE_VIRTUAL_FEEDBACK /* 当前无ADC虚拟反馈运行模式；修改后必须全量重新编译。 */

/*
 * 外设功能开关。当前两项组合表示：ADC1/2、DMA和AWD完全不初始化，但允许PD0短按后
 * 开放HRTIM A/B四路PWM。10 kHz控制节拍改由HRTIM Master Repetition中断提供。
 * 把ENABLE_ADC改回1U时，必须恢复实际采样板、标定值和保护阈值；只改开关不会自动标定。
 */
#define PFC_USER_ENABLE_ADC                  0U /* 0=完全跳过ADC/DMA/AWD；此时没有过流、过压、VAC丢失和母线监测保护。 */
#define PFC_USER_ENABLE_PWM_OUTPUT           1U /* 1=虚拟反馈模式下允许PD0启停PWM；首次验证应断开功率母线只看MCU波形。 */
#define PFC_USER_OLED_PERIOD_MS              500U /* ms，OLED数字刷新周期；增大可减少软件I2C占用，0U禁止使用。 */

/* ======================== 2. 5 V低压参数档 ======================== */
/*
 * 输入、母线和功率级参数。修改方法：先按万用表/元件实测填写，再检查保护阈值关系，
 * 最后在ENGINEERING_CHECK模式复核。VAC、VBUS和频率参数会参与状态机或控制计算；
 * R、L、C目前主要用于记录硬件和模型边界，不会自动替你整定PI/PR。
 */
#define PFC_USER_5V_VAC_NOMINAL_RMS          5.0f       /* V RMS，额定交流输入，也是Iref=Irms*VAC/本值的归一化分母；填大使同一Irms指令的瞬时Iref变小，填小则变大。 */
#define PFC_USER_5V_GRID_FREQUENCY_HZ        50.0f      /* Hz，决定PR中心频率、100 Hz陷波中心、RMS窗口和过零周期；改为其他频率后必须同时验证PR/陷波和同步锁定。 */
#define PFC_USER_5V_GRID_TOLERANCE_HZ        3.0f       /* Hz，VAC锁相允许范围为额定值±本值；增大可容忍频率漂移但更容易接受异常输入，减小会提高误报概率。 */
#define PFC_USER_5V_VBUS_TARGET_V            9.0f       /* V，PI外环最终母线参考；调高会增加升压比、器件应力和所需输入电流，且必须同步提高但重新校核过压阈值。 */
#define PFC_USER_5V_VBUS_START_MIN_V         5.0f       /* V，进入READY前被动母线最低门槛；过高会一直等不到READY，过低会在母线不足时允许启动并放大调制除法风险。 */
#define PFC_USER_5V_VBUS_RUN_MIN_V           4.0f       /* V，带功率期间母线跌落关断门槛；调高保护更保守但易欠压误停，调低会让控制在低母线下继续增大调制度。 */
#define PFC_USER_5V_LOAD_RESISTANCE_OHM      30.0f      /* ohm，目标直流负载记录值；当前控制器不直接使用它计算指令，改负载后仍需实测功率、PI上限和温升。 */
#define PFC_USER_5V_INPUT_INDUCTANCE_H       470.0e-6f  /* H，交流侧电感实测值；供开环模型/参数记录使用，改变电感会改变电流纹波和对象动态，必须重新整定PR。 */
#define PFC_USER_5V_BUS_CAPACITANCE_F        4700.0e-6f /* F，母线电容实测值；当前不会自动重算PI，电容减小会增大100 Hz纹波并加快动态，改动后需重整PI和过压裕量。 */
#define PFC_USER_5V_CONTROL_FREQUENCY_HZ     10000.0f   /* Hz，必须与HRTIM/ADC真实10 kHz节拍一致；它参与PR离散化、统计窗口和斜坡换算，禁止只改此宏而不改CubeMX时基。 */

/*
 * ADC换算公式：
 * IPFC = (raw - IPFC_ZERO_COUNT) * IPFC_POLARITY * IPFC_A_PER_COUNT
 * VAC  = (raw - VAC_ZERO_COUNT)  * VAC_POLARITY  * VAC_V_PER_COUNT
 * VBUS = raw * VBUS_V_PER_COUNT
 *
 * 零点应在RAW_ADC模式、对应物理输入为0时取1秒mean；双极性比例使用
 * “已知物理峰值 / ((max-min)/2)”计算，VBUS比例使用“万用表电压 / raw mean”。
 * 比例填大时同一ADC码会显示更大的工程量，并使AWD保护对应的原始码窗口变窄；
 * 比例填小时工程量偏小、保护窗口变宽，可能延迟过流/过压判断。
 */
#define PFC_USER_5V_IPFC_ZERO_COUNT          2048U      /* ADC count，IPFC零电流时1秒mean；填高会把全部电流结果向负方向平移，填低则向正方向平移。 */
#define PFC_USER_5V_VAC_ZERO_COUNT           2048U      /* ADC count，VAC零输入时1秒mean；错误零点会造成正负半周不对称、RMS偏大和过零时间偏移。 */
#define PFC_USER_5V_IPFC_A_PER_COUNT         0.001f     /* A/count，低压占位值；按已知电流峰值标定，改后必须复核IPFC RMS、软件过流和ADC1 AWD1窗口。 */
#define PFC_USER_5V_VAC_V_PER_COUNT          0.010f     /* V/count，低压占位值；按已知VAC峰值标定，改后应看到5 V RMS、约7.07 V峰值并复核ADC2 AWD1。 */
#define PFC_USER_5V_VBUS_V_PER_COUNT         0.010f     /* V/count，单极性母线占位值且不减零点；改后用万用表复核，并重新计算9.8/10.5 V对应的AWD码。 */
#define PFC_USER_5V_IPFC_POLARITY            1          /* +1/-1，定义正向输入电流的符号；选错会使PR形成正反馈，必须先在无功率已知电流下确认。 */
#define PFC_USER_5V_VAC_POLARITY             1          /* +1/-1，定义VAC正半周；选错会让电流参考相位反转，并破坏过零同步与功率因数方向。 */
#define PFC_USER_5V_BRIDGE_POLARITY          1          /* +1/-1，定义正调制对应的桥臂电压方向；选错会导致电流越控越大，只能低压示波器验证后确定。 */
#define PFC_USER_5V_CALIBRATION_CONFIRMED    0U         /* ADC已关闭，保持0明确表示当前没有实测标定和ADC保护；恢复ADC后必须重新标定。 */

/*
 * 假反馈模式专用参数。virtual_vbus_feedback只送入PI，不代表真实母线电压。
 * 默认令反馈等于9 V参考，使PI误差为0；PI输出预置为0.10 A RMS，然后经斜坡送入
 * 开环电感前馈。若把固定反馈改得低于参考，PI会持续积分并最终顶到电流上限，
 * 这不会稳定真实VBUS，修改时必须同步观察实测母线、电流和调制度。
 */
#define PFC_USER_5V_VIRTUAL_VBUS_FEEDBACK_V       9.0f  /* V，固定PI反馈；零误差保持时应等于VBUS_TARGET。 */
#define PFC_USER_5V_VIRTUAL_CURRENT_PRESET_A_RMS  0.10f /* A RMS，首次带功率起点；提高会直接增加输入功率。 */
#define PFC_USER_5V_VIRTUAL_CURRENT_SLEW_A_S      0.10f /* A RMS/s，开环指令斜率；提高会增大启动冲击和过压风险。 */

/*
 * 软件保护与运行边界。保护阈值必须位于“正常波动最大值”和“硬件绝对安全值”之间。
 * 阈值调高会减少误报但扩大故障能量；调低会更保守但可能无法完成启动。当前PB10外部
 * 未接OCP/DESAT，以下软件/AWD阈值仍不能替代独立微秒级短路保护。
 */
#define PFC_USER_5V_CURRENT_TRIP_A_PEAK      1.20f      /* A peak，IPFC绝对值软件关断及ADC1 AWD1目标；增大前先确认传感器量程、MOSFET/电感和限流电源能力。 */
#define PFC_USER_5V_VAC_PEAK_TRIP_V          8.50f      /* V peak，VAC绝对值允许上限及ADC2 AWD1窗口；5 V RMS理论峰值7.07 V，太接近会在波动/噪声时误停。 */
#define PFC_USER_5V_VBUS_WARN_V              9.80f      /* V，接近过压时的遥测告警参考，不直接关PWM；应高于目标且低于TRIP。 */
#define PFC_USER_5V_VBUS_TRIP_V              10.50f     /* V，VBUS软件关断和ADC1 AWD2高阈值；调高会增加母线/器件应力，调低可能在100 Hz纹波峰值误触发。 */
#define PFC_USER_5V_VBUS_TOLERANCE_V         0.50f      /* V，判定母线目标建立的±容差；调大更容易进入RUN但稳压判据变松，调小会延长RAMP或触发超时。 */
#define PFC_USER_5V_MODULATION_LIMIT         0.90f      /* 归一化绝对值，限制桥臂电压指令；调高增加升压能力但减少占空比/死区裕量，禁止超过运行期校验上限0.95。 */
#define PFC_USER_5V_CURRENT_TARGET_A_PEAK    0.80f      /* A peak，PI最大RMS指令约为本值/sqrt(2)=0.566 A；调高可带更重负载但必须同步保留CURRENT_TRIP裕量。 */
#define PFC_USER_5V_CURRENT_RAMP_A_PER_S     0.50f      /* A/s，旧开环SPWM峰值指令斜率；当前PI+PR闭环启动主要使用PROBE_SLEW，本值不会改变闭环探测斜率。 */

/*
 * 10 kHz PR、1 kHz PI、软启动和超时参数。整定顺序：先保持PI不投入并调PR，确认
 * IPFC与Iref同相且不持续限幅；再从很小PI增益开始调外环。一次只改一个参数，保存
 * VAC/IPFC/Iref/VBUS/调制度波形。增益调大通常响应更快但噪声、超调和振荡风险增加。
 */
#define PFC_USER_5V_PR_KP                    0.50f      /* V/A，电流误差的即时比例校正；调大提高快速响应但放大采样噪声和开关纹波，过大会振荡。 */
#define PFC_USER_5V_PR_KR                    5.00f      /* V/A，50 Hz谐振校正强度；调大可减小工频稳态误差，但更易在频率偏差/延时下振荡或触及输出限幅。 */
#define PFC_USER_5V_PR_BANDWIDTH_RAD_S       5.00f      /* rad/s，准PR谐振带宽Wc；调大容忍电网频偏但作用频带更宽、噪声和相位影响增加，调小则频率偏差敏感。 */
#define PFC_USER_5V_PR_OUTPUT_LIMIT_V        3.00f      /* V，PR电感压降校正量绝对上限；过小会跟踪不足，过大会使桥侧电压/调制度突变并更快触发饱和。 */
#define PFC_USER_5V_PI_KP_A_PER_V            0.05f      /* A/V，VBUS误差立即转换成RMS电流指令；调大母线响应加快但100 Hz纹波耦合和超调增大。 */
#define PFC_USER_5V_PI_KI_A_PER_VS           1.00f      /* A/(V*s)，消除母线稳态误差的积分增益；调大消差更快但易积分累积、过冲和低频振荡。 */
#define PFC_USER_5V_VOLTAGE_LOOP_HZ          1000.0f    /* Hz，PI/陷波实际调用频率，必须等于10 kHz快速环的1/10；禁止单独改此值而不改代码分频。 */
#define PFC_USER_5V_NOTCH_DAMPING_RAD_S      50.0f      /* rad/s，100 Hz陷波器分母阻尼项；调大陷波影响带宽更宽，调小更窄，对噪声/参数偏差更敏感，改后复核相位与PI稳定性。 */
#define PFC_USER_5V_VBUS_SLEW_V_PER_S        1.0f       /* V/s，外环参考从当前VBUS爬到目标的速度；调大启动更快但电流冲击/超调增加，调小更安全但启动时间变长。 */
#define PFC_USER_5V_PROBE_CURRENT_A_RMS      0.20f      /* A RMS，正式投入PI前用于验证PR极性和跟踪的轻载电流；应明显高于噪声且远低于正常/过流上限。 */
#define PFC_USER_5V_PROBE_SLEW_A_PER_S       5.0f       /* A RMS/s，探测电流从0上升的速度；本值下到0.20 A约40 ms，调大冲击增大，调小可能接近超时。 */
#define PFC_USER_5V_CURRENT_ERROR_MAX_A_RMS  0.15f      /* A RMS，允许从PR探测切入PI的最大跟踪误差；调大可能放过错误极性/弱控制，调小容易因噪声无法通过。 */
#define PFC_USER_5V_PROBE_MIN_MS             60U        /* ms，PR投入后最短观察时间；至少覆盖一个50 Hz周期20 ms并留出斜坡时间，过短不能可靠判断。 */
#define PFC_USER_5V_CURRENT_TIMEOUT_MS       500U       /* ms，PR探测仍不合格的锁存故障时间；调大只延长异常带功率时间，不能解决极性或增益错误。 */
#define PFC_USER_5V_VBUS_TIMEOUT_MS          8000U      /* ms，PI投入后母线仍未达到目标容差的故障时间；应大于参考斜坡理论时间并保留负载动态裕量。 */
#define PFC_USER_5V_SATURATION_SAMPLES       200U       /* 个10 kHz样本，PR/调制度连续限幅关断门槛；200约20 ms，增大容忍更久但故障能量增加，减小易瞬态误停。 */

/* ======================== 3. 36 V赛题参数档 ======================== */
/*
 * 该档目前只保留赛题迁移骨架，并非已经验证的36 V实物参数。修改顺序与5 V档相同，
 * 但必须在36 V档自己的RAW_ADC模式重新标定，确认分压器/传感器量程、隔离、器件耐压、
 * 限流和硬件OCP/DESAT后才能把CONFIRMED改为1。下面增益来自初始设计，只能作为整定起点。
 */
#define PFC_USER_36V_VAC_NOMINAL_RMS         36.0f      /* V RMS，赛题输入额定值和电流参考归一化分母；实测输入额定值改变时再修改。 */
#define PFC_USER_36V_GRID_FREQUENCY_HZ       50.0f      /* Hz，同时决定PR、二倍频陷波、RMS窗口和同步周期；不能只改显示值。 */
#define PFC_USER_36V_GRID_TOLERANCE_HZ       3.0f       /* Hz，允许同步锁定范围±3 Hz；扩大范围前确认异常频率不会被误接受。 */
#define PFC_USER_36V_VBUS_TARGET_V           60.0f      /* V，赛题档PI母线目标；调高前先核对母线电容、MOSFET、采样量程和过压阈值。 */
#define PFC_USER_36V_VBUS_START_MIN_V        45.0f      /* V，允许启动闭环前的被动母线门槛；必须低于目标且高于运行除法的危险区。 */
#define PFC_USER_36V_VBUS_RUN_MIN_V          40.0f      /* V，运行欠压关断值；太高易输入跌落误停，太低会在高调制度下继续运行。 */
#define PFC_USER_36V_LOAD_RESISTANCE_OHM     32.4f      /* ohm，赛题目标负载记录值；软件不据此自动算电流，负载改变后仍需重新验收功率与温升。 */
#define PFC_USER_36V_INPUT_INDUCTANCE_H      470.0e-6f  /* H，交流侧实测电感；电感改变会改变纹波/动态，需重新整定PR及检查饱和电流。 */
#define PFC_USER_36V_BUS_CAPACITANCE_F       4700.0e-6f /* F，母线实测电容；改变后需重新检查100 Hz纹波、PI动态、浪涌和放电时间。 */
#define PFC_USER_36V_CONTROL_FREQUENCY_HZ    10000.0f   /* Hz，必须与HRTIM/ADC实际节拍一致；单改此宏会让控制器离散系数和时间判据错误。 */

/* 36 V档ADC值全部是占位值，必须按该电压档和实际采样链单独测量。 */
#define PFC_USER_36V_IPFC_ZERO_COUNT         2048U      /* ADC count，36 V硬件零电流时1秒mean；不得直接使用5 V档测得的零点。 */
#define PFC_USER_36V_VAC_ZERO_COUNT          2048U      /* ADC count，36 V采样链零输入时1秒mean；错误会造成VAC偏置、RMS和过零误差。 */
#define PFC_USER_36V_IPFC_A_PER_COUNT        0.001f     /* A/count，占位；按已知电流标定，改后重算软件过流和ADC1 AWD1原始码窗口。 */
#define PFC_USER_36V_VAC_V_PER_COUNT         0.020f     /* V/count，占位；按已知36 V正弦峰值标定，确认量程不会在约50.9 V峰值附近贴轨。 */
#define PFC_USER_36V_VBUS_V_PER_COUNT        0.020f     /* V/count，占位；用万用表多点标定60 V母线通道，并保留66 V过压对应的ADC余量。 */
#define PFC_USER_36V_IPFC_POLARITY           1          /* +1/-1，正输入电流的软件方向；错误会使PR正反馈，只能无功率/限流验证后确定。 */
#define PFC_USER_36V_VAC_POLARITY            1          /* +1/-1，VAC正半周的软件方向；应与IPFC正功率方向一致。 */
#define PFC_USER_36V_BRIDGE_POLARITY         1          /* +1/-1，桥侧正电压方向；功率板接线变化后必须重新验证，不能凭5 V档推断。 */
#define PFC_USER_36V_CALIBRATION_CONFIRMED   0U         /* 必须保持0，直到36 V档零点/比例/极性/保护和硬件安全链全部实测完成。 */
#define PFC_USER_36V_VIRTUAL_VBUS_FEEDBACK_V      60.0f /* V，仅保留迁移接口；36 V档未验证前禁止带功率。 */
#define PFC_USER_36V_VIRTUAL_CURRENT_PRESET_A_RMS 0.10f /* A RMS，赛题档占位起点，不是最终2 A输出工况参数。 */
#define PFC_USER_36V_VIRTUAL_CURRENT_SLEW_A_S     0.10f /* A RMS/s，赛题档占位斜率，实测前不得提高。 */

/* 36 V保护阈值调高会扩大故障能量，任何修改都必须先核对器件绝对额定值和硬件保护。 */
#define PFC_USER_36V_CURRENT_TRIP_A_PEAK     5.20f      /* A peak，软件/AWD过流目标；必须高于正常峰值且低于采样、电感、MOSFET和保险链安全边界。 */
#define PFC_USER_36V_VAC_PEAK_TRIP_V         55.0f      /* V peak，VAC允许绝对峰值；36 V RMS理论峰值约50.9 V，阈值需覆盖正常波动但不能超过前端量程。 */
#define PFC_USER_36V_VBUS_WARN_V             64.0f      /* V，母线告警参考，不直接关PWM；应在60 V目标与66 V关断值之间。 */
#define PFC_USER_36V_VBUS_TRIP_V             66.0f      /* V，软件和ADC1 AWD2过压关断；调高前必须有母线器件和采样余量证据。 */
#define PFC_USER_36V_VBUS_TOLERANCE_V        1.0f       /* V，60 V目标建立的±容差；调小提高判据但可能因100 Hz纹波无法进入RUN。 */
#define PFC_USER_36V_MODULATION_LIMIT        0.90f      /* 归一化绝对值，保留占空比/死区裕量；提高会增大可用桥压同时增加失真和应力风险。 */
#define PFC_USER_36V_CURRENT_TARGET_A_PEAK   4.80f      /* A peak，PI最大RMS指令约3.39 A；提高前要同步留出CURRENT_TRIP保护裕量并核对输入功率。 */
#define PFC_USER_36V_CURRENT_RAMP_A_PER_S    1.00f      /* A/s，旧开环峰值指令斜率；当前闭环探测速度由PROBE_SLEW决定。 */

/* 36 V闭环增益只能在低压、限流、轻载下逐项重新整定，不能直接视为赛题最终值。 */
#define PFC_USER_36V_PR_KP                   5.0f       /* V/A，电流环比例增益；增大响应更快但噪声、延时导致的振荡风险更高。 */
#define PFC_USER_36V_PR_KR                   20.0f      /* V/A，50 Hz谐振增益；增大可减小稳态误差，但更易持续限幅或对频偏敏感。 */
#define PFC_USER_36V_PR_BANDWIDTH_RAD_S      5.0f       /* rad/s，准PR带宽；调宽可容忍频偏但扩大谐振影响范围，需复核稳定裕量。 */
#define PFC_USER_36V_PR_OUTPUT_LIMIT_V       80.0f      /* V，PR电压校正限幅；当前值必须结合60 V母线和调制度复核，过大可能立即把调制推到上限。 */
#define PFC_USER_36V_PI_KP_A_PER_V           0.30f      /* A/V，母线比例增益；增大加快响应但会把100 Hz纹波和噪声转换为更大电流指令。 */
#define PFC_USER_36V_PI_KI_A_PER_VS          15.0f      /* A/(V*s)，母线积分增益；过大易超调/低频振荡，必须从轻载小值逐步增加。 */
#define PFC_USER_36V_VOLTAGE_LOOP_HZ         1000.0f    /* Hz，必须保持为10 kHz快速环的1/10，和代码固定分频一致。 */
#define PFC_USER_36V_NOTCH_DAMPING_RAD_S     50.0f      /* rad/s，100 Hz陷波宽度相关参数；修改后需复核滤波相位和PI稳定性。 */
#define PFC_USER_36V_VBUS_SLEW_V_PER_S       5.0f       /* V/s，母线参考爬升速度；加快会提高启动电流和过冲，减慢则需同步放宽VBUS超时。 */
#define PFC_USER_36V_PROBE_CURRENT_A_RMS     0.50f      /* A RMS，PI投入前的PR探测电流；应足以压过噪声，但远低于额定/过流值。 */
#define PFC_USER_36V_PROBE_SLEW_A_PER_S      10.0f      /* A RMS/s，探测电流斜率；到0.5 A约50 ms，调快会增加冲击，调慢需考虑探测超时。 */
#define PFC_USER_36V_CURRENT_ERROR_MAX_A_RMS 0.50f      /* A RMS，允许切入PI的电流跟踪误差；过宽可能放过错误控制，过窄易受噪声/标定误差影响。 */
#define PFC_USER_36V_PROBE_MIN_MS            60U        /* ms，PR最短观察时间；至少覆盖完整工频周期和电流斜坡。 */
#define PFC_USER_36V_CURRENT_TIMEOUT_MS      500U       /* ms，电流环不能通过检查的故障时间；不能用延长超时掩盖极性或增益问题。 */
#define PFC_USER_36V_VBUS_TIMEOUT_MS         8000U      /* ms，母线目标建立超时；改变目标/爬升速度后按理论斜坡时间重新计算。 */
#define PFC_USER_36V_SATURATION_SAMPLES      200U       /* 个10 kHz样本，连续限幅约20 ms关断；增大会允许更长过应力，减小易瞬态误停。 */

/* ======================== 4. 每次改参后的固定检查 ======================== */
/*
 * 1. 改ADC零点/比例/极性：先退回RAW_ADC且CONFIRMED=0，重新记录1秒统计；填写后
 *    进入ENGINEERING_CHECK，用万用表/示波器复核工程量、RMS、频率和正负方向，
 *    同时检查AWD阈值没有落在正常波形内，也没有超出0~4095。
 * 2. 改VAC额定值或频率：重新检查Iref归一化、PR中心频率、二倍频陷波、RMS窗口、
 *    VAC峰值保护和同步容差。实际PWM/ADC频率不变时不要修改CONTROL_FREQUENCY_HZ。
 * 3. 改VBUS目标：同步检查START_MIN、RUN_MIN、WARN、TRIP、调制度裕量、母线器件耐压、
 *    参考爬升所需时间和VBUS_TIMEOUT，不能只改TARGET一个数。
 * 4. 改电流目标或负载：按P=Vbus^2/R估算输出功率和输入RMS电流，检查PI输出上限、
 *    CURRENT_TRIP、采样量程、电感饱和、MOSFET温升和电源限流；先轻载再逐步加负载。
 * 5. 改PR/PI：先只调PR并确认IPFC跟随Iref，再投入PI；一次只改一个增益，观察限幅、
 *    超调、振荡和ISR执行时间。控制不稳定时先恢复上一个已验证值，不靠放宽保护继续试。
 * 6. 准备带功率前：确认ACTIVE_PROFILE正确、RUN_MODE=CLOSED_LOOP、当前档CONFIRMED=1，
 *    且PB10外部未接的事实已知。软件/AWD保护不能替代驱动器独立OCP/DESAT。
 */

/* ======================== 5. 编译期安全检查 ======================== */
#if ((PFC_USER_ACTIVE_PROFILE != PFC_USER_PROFILE_5V) && \
     (PFC_USER_ACTIVE_PROFILE != PFC_USER_PROFILE_36V))
#error "PFC_USER_ACTIVE_PROFILE is invalid"
#endif

#if ((PFC_USER_RUN_MODE != PFC_USER_MODE_RAW_ADC) && \
     (PFC_USER_RUN_MODE != PFC_USER_MODE_ENGINEERING_CHECK) && \
     (PFC_USER_RUN_MODE != PFC_USER_MODE_VIRTUAL_FEEDBACK))
#error "PFC_USER_RUN_MODE is invalid"
#endif

#if ((PFC_USER_ENABLE_ADC != 0U) && (PFC_USER_ENABLE_ADC != 1U)) || \
    ((PFC_USER_ENABLE_PWM_OUTPUT != 0U) && (PFC_USER_ENABLE_PWM_OUTPUT != 1U)) || \
    (PFC_USER_OLED_PERIOD_MS == 0U)
#error "Peripheral switches must be 0U/1U and OLED period must be non-zero"
#endif

#if (PFC_USER_ENABLE_PWM_OUTPUT == 1U) && \
    (PFC_USER_RUN_MODE != PFC_USER_MODE_VIRTUAL_FEEDBACK)
#error "PWM output is only allowed in virtual-feedback mode"
#endif

#if ((PFC_USER_5V_CALIBRATION_CONFIRMED != 0U) && \
     (PFC_USER_5V_CALIBRATION_CONFIRMED != 1U)) || \
    ((PFC_USER_36V_CALIBRATION_CONFIRMED != 0U) && \
     (PFC_USER_36V_CALIBRATION_CONFIRMED != 1U))
#error "Calibration confirmation must be 0U or 1U"
#endif

#if ((PFC_USER_5V_IPFC_ZERO_COUNT > 4095U) || (PFC_USER_5V_VAC_ZERO_COUNT > 4095U) || \
     (PFC_USER_36V_IPFC_ZERO_COUNT > 4095U) || (PFC_USER_36V_VAC_ZERO_COUNT > 4095U))
#error "ADC zero count must be in the 12-bit range"
#endif

#if (((PFC_USER_5V_IPFC_POLARITY != 1) && (PFC_USER_5V_IPFC_POLARITY != -1)) || \
     ((PFC_USER_5V_VAC_POLARITY != 1) && (PFC_USER_5V_VAC_POLARITY != -1)) || \
     ((PFC_USER_5V_BRIDGE_POLARITY != 1) && (PFC_USER_5V_BRIDGE_POLARITY != -1)) || \
     ((PFC_USER_36V_IPFC_POLARITY != 1) && (PFC_USER_36V_IPFC_POLARITY != -1)) || \
     ((PFC_USER_36V_VAC_POLARITY != 1) && (PFC_USER_36V_VAC_POLARITY != -1)) || \
     ((PFC_USER_36V_BRIDGE_POLARITY != 1) && (PFC_USER_36V_BRIDGE_POLARITY != -1)))
#error "PFC polarity must be +1 or -1"
#endif

#if PFC_USER_ACTIVE_PROFILE == PFC_USER_PROFILE_5V
#define PFC_USER_SELECTED_CALIBRATION_CONFIRMED PFC_USER_5V_CALIBRATION_CONFIRMED
#else
#define PFC_USER_SELECTED_CALIBRATION_CONFIRMED PFC_USER_36V_CALIBRATION_CONFIRMED
#endif

#if ((PFC_USER_RUN_MODE == PFC_USER_MODE_ENGINEERING_CHECK) || \
     (PFC_USER_RUN_MODE == PFC_USER_MODE_CLOSED_LOOP)) && \
    (PFC_USER_ENABLE_ADC == 1U) && \
    (PFC_USER_SELECTED_CALIBRATION_CONFIRMED != 1U)
#error "Engineering or closed-loop mode requires confirmed ADC calibration"
#endif

#if (PFC_USER_RUN_MODE == PFC_USER_MODE_CLOSED_LOOP) && \
    (PFC_USER_ENABLE_ADC == 1U) && \
    (PFC_USER_ACTIVE_PROFILE == PFC_USER_PROFILE_36V) && \
    (PFC_USER_36V_CALIBRATION_CONFIRMED != 1U)
#error "Unconfirmed 36 V profile cannot enter closed-loop mode"
#endif

#endif /* PFC_USER_CONFIG_H */
