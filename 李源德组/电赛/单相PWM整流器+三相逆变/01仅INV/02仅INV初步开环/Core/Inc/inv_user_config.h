#ifndef INV_USER_CONFIG_H
#define INV_USER_CONFIG_H

/*
 * 三相逆变用户配置文件
 *
 * 日常开环调试和ADC标定只修改本文件“用户常用配置区”。其他模块中的
 * HRTIM周期、ADC Rank、DMA通道和Fault路由均来自CubeMX/硬件设计，不应
 * 为了调参而修改。
 *
 * 推荐使用顺序：
 * 1. INV_MODE_ADC_CALIBRATION + INV_CAL_STEP_ZERO：零输入求六路offset；
 * 2. 把Keil Watch中的inv_calibration_result.offset[]写回下方OFFSET；
 * 3. INV_CAL_STEP_GAIN：逐通道施加已知物理量，计算比例和极性；
 * 4. 把recommended_scale写回对应SCALE；
 * 5. INV_MODE_PWM_WAVEFORM：断开功率母线，仅验证MCU六路PWM；
 * 6. INV_MODE_OPEN_LOOP：接好采样板后进行5 V限流开环测试。
 */

#include <stdint.h>

/* ======================== 工作模式常量（不要修改） ======================== */

#define INV_MODE_ADC_CALIBRATION (0U) /**< ADC标定模式：PWM强制关闭，只运行三路ADC/DMA和标定流程；常量值禁止修改。 */
#define INV_MODE_PWM_WAVEFORM    (1U) /**< MCU波形模式：ADC3 DMA仅提供10 kHz节拍，不使用采样值做保护；常量值禁止修改。 */
#define INV_MODE_OPEN_LOOP       (2U) /**< 完整开环模式：使用ADC3/4/5同步测量与软件保护运行低压逆变；常量值禁止修改。 */

/* ======================== 标定步骤常量（不要修改） ======================== */

#define INV_CAL_STEP_ZERO (0U) /**< 零点步骤：六路保持真实零输入并同时平均，得到各通道offset；常量值禁止修改。 */
#define INV_CAL_STEP_GAIN (1U) /**< 增益步骤：对所选通道施加已知静态量，计算含极性scale；常量值禁止修改。 */

/* ======================== ADC通道常量（不要修改） ======================== */

#define INV_CAL_CHANNEL_IU  (0U) /**< U相电流IU：ADC3 Rank 1、INV_Adc3Dma[0]；通道编号禁止修改。 */
#define INV_CAL_CHANNEL_IV  (1U) /**< V相电流IV：ADC4 Rank 1、INV_Adc4Dma[0]；通道编号禁止修改。 */
#define INV_CAL_CHANNEL_IW  (2U) /**< W相电流IW：ADC5 Rank 1、INV_Adc5Dma[0]；通道编号禁止修改。 */
#define INV_CAL_CHANNEL_VUV (3U) /**< U-V线电压VUV：ADC3 Rank 2、INV_Adc3Dma[1]；通道编号禁止修改。 */
#define INV_CAL_CHANNEL_VVW (4U) /**< V-W线电压VVW：ADC4 Rank 2、INV_Adc4Dma[1]；通道编号禁止修改。 */
#define INV_CAL_CHANNEL_VWU (5U) /**< W-U线电压VWU：ADC5 Rank 2、INV_Adc5Dma[1]；通道编号禁止修改。 */

/* ======================== 用户常用配置区（通常只改这里） ======================== */

/** 当前工作模式：编译期只能选择上面三个INV_MODE_*之一；修改后必须重新编译并下载。 */
#define INV_USER_WORK_MODE INV_MODE_PWM_WAVEFORM /**< 默认完整低压开环；切换模式前先断开功率母线。 */

/** 1允许在波形/开环模式开放C/D/E六路PWM；0只运行算法和更新比较值。标定模式无条件禁止PWM。 */
#define INV_USER_ENABLE_PWM_OUTPUT (1U) /**< 仅填0U或1U；设为1U前必须完成分级波形、驱动和Fault检查。 */

/*
 * 开环命令用于三相三线逆变，LINE_RMS表示线电压基波有效值。程序按下式检查：
 * modulation = 2 * LINE_RMS * sqrt(2/3) / DC_BUS <= MODULATION_LIMIT。
 * DC_BUS当前是软件命令而不是ADC实测值，母线偏差、器件压降和死区会直接造成
 * 实际输出偏差；升高母线或输出命令前必须重新核对器件、驱动、采样和保护范围。
 */
#define INV_USER_DC_BUS_V          (5.0f)  /**< 开环计算使用的直流母线电压，单位V；应填实际稳定母线值且必须大于0。 */
#define INV_USER_LINE_RMS_V        (2.5f)  /**< 目标三相线电压基波有效值，单位V RMS；不是相电压，也不是闭环保证值。 */
#define INV_USER_OUTPUT_FREQUENCY  (60U)   /**< 输出基波频率，单位Hz；当前实现只接受30U或60U，其他值会编译报错。 */
#define INV_USER_SOFT_START_MS     (1000U) /**< 调制度从0爬升到目标值的时间，单位ms；必须大于0，减小会增大启动冲击。 */
#define INV_USER_MODULATION_LIMIT  (0.90f) /**< 允许的最大调制度，范围(0,1]；提高前必须确认死区、最小脉宽和线性区。 */

/* ADC标定步骤：仅在INV_MODE_ADC_CALIBRATION中使用。 */
#define INV_USER_CALIBRATION_STEP      INV_CAL_STEP_ZERO  /**< 选择ZERO或GAIN；修改后重新编译，标定时功率母线必须断开。 */
#define INV_USER_CALIBRATION_CHANNEL   INV_CAL_CHANNEL_IU /**< GAIN步骤的目标通道；只能选择一个INV_CAL_CHANNEL_*常量。 */
#define INV_USER_CALIBRATION_REFERENCE (1.0f)             /**< 已知非零物理输入：电流通道单位A，线电压通道单位V，可带正负号。 */
/*
 * GAIN步骤的reference单位由通道决定：IU/IV/IW填A，VUV/VVW/VWU填V。
 * 可填负值。程序根据(reference)/(raw_average-offset)自动判断比例极性。
 * 该算法使用平均码，只适用于稳定直流或准静态已知输入，不能直接填正弦RMS。
 * 施加已知输入后再复位启动；PWM在整个标定模式中始终关闭。
 */

/*
 * 六路固定零点码：先执行ZERO步骤，再把offset[0]～offset[5]逐项写回。
 * 当前2048U是1.65 V中点偏置的低压调试占位值，合法ADC码范围为0～4095；
 * 每路运放、偏置和ADC误差不同，禁止仅标定一路后把同一结果复制到其余通道。
 */
#define INV_USER_IU_OFFSET  (2048U) /**< IU真实零电流时的ADC码，对应offset[0]；用本板本通道ZERO结果替换。 */
#define INV_USER_IV_OFFSET  (2048U) /**< IV真实零电流时的ADC码，对应offset[1]；用本板本通道ZERO结果替换。 */
#define INV_USER_IW_OFFSET  (2048U) /**< IW真实零电流时的ADC码，对应offset[2]；用本板本通道ZERO结果替换。 */
#define INV_USER_VUV_OFFSET (2048U) /**< VUV真实零线电压时的ADC码，对应offset[3]；用本板本通道ZERO结果替换。 */
#define INV_USER_VVW_OFFSET (2048U) /**< VVW真实零线电压时的ADC码，对应offset[4]；用本板本通道ZERO结果替换。 */
#define INV_USER_VWU_OFFSET (2048U) /**< VWU真实零线电压时的ADC码，对应offset[5]；用本板本通道ZERO结果替换。 */

/*
 * 六路比例满足physical=(raw-offset)*scale；绝对值表示每ADC码对应的物理量，
 * 正负号表示传感链路极性。以下数值均为低压观察占位值，必须逐通道用GAIN结果
 * 替换；负scale是有效标定结果，不得为了数值为正而在控制算法中再次隐藏反号。
 */
#define INV_USER_IU_SCALE_A_PER_COUNT  (0.001f) /**< IU电流比例，单位A/count；用IU的recommended_scale替换。 */
#define INV_USER_IV_SCALE_A_PER_COUNT  (0.001f) /**< IV电流比例，单位A/count；用IV的recommended_scale替换。 */
#define INV_USER_IW_SCALE_A_PER_COUNT  (0.001f) /**< IW电流比例，单位A/count；用IW的recommended_scale替换。 */
#define INV_USER_VUV_SCALE_V_PER_COUNT (0.010f) /**< VUV线电压比例，单位V/count；用VUV的recommended_scale替换。 */
#define INV_USER_VVW_SCALE_V_PER_COUNT (0.010f) /**< VVW线电压比例，单位V/count；用VVW的recommended_scale替换。 */
#define INV_USER_VWU_SCALE_V_PER_COUNT (0.010f) /**< VWU线电压比例，单位V/count；用VWU的recommended_scale替换。 */

/* ======================== 高级配置区（通常保持默认） ======================== */

/*
 * 下列参数与HRTIM、ADC/DMA节拍、IWDG和主循环负载相互关联，通常保持默认。
 * 若确需修改，应同时核对CubeMX配置、启动时序、故障检测时间和Keil实测执行时间。
 */
#define INV_USER_CONTROL_FREQUENCY_HZ   (10000U) /**< 控制/DDS计算频率，单位Hz；必须与HRTIM触发ADC的实际10kHz一致，不能单独改宏。 */
#define INV_USER_MINIMUM_DC_V           (1.0f)   /**< 软件接受的最小母线命令，单位V；仅做参数校验，不是实测欠压保护。 */
#define INV_USER_OFFSET_SAMPLES         (256U)   /**< ZERO平均帧数；10kHz下约25.6ms，增大可降噪但必须同步增加启动超时裕量。 */
#define INV_USER_GAIN_SAMPLES           (256U)   /**< GAIN平均帧数；10kHz下约25.6ms，期间已知输入必须保持稳定且非零。 */
#define INV_USER_CAL_MIN_DELTA_COUNTS    (32U)   /**< GAIN有效最小码差，单位count；过小易受噪声影响，过大可能拒绝安全的小信号标定。 */
#define INV_USER_ADC_MAX_SKEW_FRAMES     (2U)    /**< ADC3/4/5允许的最大DMA序列滞后帧数；必须大于0，放宽会增加跨周期混样风险。 */
#define INV_USER_ADC_RAIL_LOW_COUNT      (16U)   /**< ADC低端贴轨边界，原始码小于等于此值视为异常候选；范围须低于HIGH。 */
#define INV_USER_ADC_RAIL_HIGH_COUNT     (4079U) /**< ADC高端贴轨边界，原始码大于等于此值视为异常候选；不得超过12位满量程4095。 */
#define INV_USER_ADC_RAIL_CONFIRM_FRAMES (8U)    /**< 同一通道连续贴轨确认帧数；10kHz下8帧约0.8ms，减小会提高噪声误报概率。 */
#define INV_USER_STARTUP_TIMEOUT_MS      (500U)  /**< 等待ADC零点/标定完成的最长时间，单位ms；必须大于采样时间并保留启动裕量。 */
#define INV_USER_SUPERVISOR_PERIOD_MS    (100U)  /**< 主循环安全监督周期，单位ms；影响DMA/心跳故障发现时间，不能超过IWDG安全预算。 */
#define INV_USER_VOFA_PERIOD_MS          (10U)   /**< VOFA阻塞发送周期，单位ms；减小会增加USART和主循环占用，禁止设为0。 */
#define INV_USER_OLED_PERIOD_MS          (100U)  /**< 软件I2C OLED刷新周期，单位ms；减小会增加阻塞时间并可能干扰后台监督。 */
#define INV_USER_LIMITED_MAX_FRAMES      (100U)  /**< SVPWM连续限幅故障确认帧数；10kHz下100帧约10ms，瞬时限幅不会立即停机。 */

#endif /* INV_USER_CONFIG_H */
