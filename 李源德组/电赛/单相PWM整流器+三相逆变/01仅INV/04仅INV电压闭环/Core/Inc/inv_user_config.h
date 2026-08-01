#ifndef INV_USER_CONFIG_H
#define INV_USER_CONFIG_H

/*
 * 三相逆变唯一人工配置入口
 *
 * 日常切换ADC标定、MCU波形、开环和双PR电压闭环时只修改本文件。
 * HRTIM周期、ADC Rank、DMA通道与FLT3路由来自02OpenLoop.ioc，不应为调参修改。
 */

#include <stdint.h>

/* ======================== 工作模式常量（禁止修改数值） ======================== */

#define INV_MODE_ADC_CALIBRATION (0U) /**< ADC标定：PWM强制关闭，仅运行采样和ZERO/GAIN。 */
#define INV_MODE_PWM_WAVEFORM    (1U) /**< MCU波形：不使用反馈，只验证DDS/SVPWM/HRTIM。 */
#define INV_MODE_OPEN_LOOP       (2U) /**< 三相开环：使用设定母线和线电压产生SVPWM。 */
#define INV_MODE_VOLTAGE_PR      (3U) /**< 双PR闭环：三路线电压反馈经α/β PR校正。 */

#define INV_CAL_STEP_ZERO (0U) /**< 六路零输入平均，得到各通道offset。 */
#define INV_CAL_STEP_GAIN (1U) /**< 对单通道施加已知静态量，计算scale及极性。 */

#define INV_CAL_CHANNEL_IU  (0U) /**< ADC3 Rank1：U相电流。 */
#define INV_CAL_CHANNEL_IV  (1U) /**< ADC4 Rank1：V相电流。 */
#define INV_CAL_CHANNEL_IW  (2U) /**< ADC5 Rank1：W相电流。 */
#define INV_CAL_CHANNEL_VUV (3U) /**< ADC3 Rank2：U-V线电压。 */
#define INV_CAL_CHANNEL_VVW (4U) /**< ADC4 Rank2：V-W线电压。 */
#define INV_CAL_CHANNEL_VWU (5U) /**< ADC5 Rank2：W-U线电压。 */

/* ======================== 用户常用配置区 ======================== */

#define INV_USER_WORK_MODE INV_MODE_VOLTAGE_PR /**< 默认双PR闭环；修改后重新编译下载。 */
#define INV_USER_ENABLE_PWM_OUTPUT (1U) /**< 1允许开放C/D/E；闭环仍受标定确认和FLT3门槛约束。 */

/*
 * 标定确认是显式人工承诺：三路线电压offset、scale和极性均已经实测。
 * 当前保持0U，因此ADC/OLED/VOFA可以运行，但物理PWM不会开放。完成标定后才改1U。
 */
#define INV_USER_VOLTAGE_CALIBRATION_CONFIRMED (0U) /**< 仅填0U/1U；不能用占位比例直接闭环。 */

/* 赛题目标；LINE_RMS是线电压基波有效值，不是相电压或PWM占空比。 */
#define INV_USER_DC_BUS_V          (60.0f) /**< CBSVPWM归一化采用的直流母线，单位V；当前不是ADC实测值。 */
#define INV_USER_LINE_RMS_V        (32.0f) /**< 三相对称线电压目标，单位V RMS。 */
#define INV_USER_OUTPUT_FREQUENCY  (60U)   /**< 只允许30U或60U；同时决定DDS与PR中心频率。 */
#define INV_USER_SOFT_START_MS     (1000U) /**< 前馈幅值从0升至目标的时间，单位ms。 */
#define INV_USER_MODULATION_LIMIT  (0.90f) /**< 桥臂调制量上限；32V/60V前馈约0.871。 */

/*
 * 默认放宽档只让FLT3进入运行期锁存关断。ADC/AWD/贴轨/失步/限幅只记诊断，
 * 反馈不可用时双PR复位并退回开环前馈。正式带功率前必须改0U并恢复完整保护。
 */
#define INV_USER_RELAXED_PR_TEST (1U) /**< 1=调试放宽，0=诊断故障也关闭PWM。 */

/* α/β电压PR初值。模型中的电压PR后面还有电流内环，原增益不能直接照搬。 */
#define INV_USER_PR_KP                    (0.05f) /**< 比例增益V/V；增大可加快响应但放大噪声。 */
#define INV_USER_PR_KR                    (0.50f) /**< 谐振增益V/V；先小后大逐步整定。 */
#define INV_USER_PR_BANDWIDTH_RAD_S       (5.0f)  /**< 准PR带宽rad/s；增大后对频偏更宽容。 */
#define INV_USER_PR_CORRECTION_LIMIT_V    (0.75f) /**< α/β校正矢量峰值上限V；与0.90调制度共同校核。 */
#define INV_USER_PR_RECOVERY_VALID_FRAMES (100U)  /**< 回退后连续有效帧门槛；10kHz下100帧为10ms。 */
#define INV_USER_PR_TAKEOVER_MS           (100U)  /**< PR重新接管斜坡，单位ms，避免恢复瞬间跳变。 */

/* ADC标定步骤，仅在INV_MODE_ADC_CALIBRATION中使用。 */
#define INV_USER_CALIBRATION_STEP      INV_CAL_STEP_ZERO
#define INV_USER_CALIBRATION_CHANNEL   INV_CAL_CHANNEL_VUV
#define INV_USER_CALIBRATION_REFERENCE (1.0f) /**< 电流通道填A，电压通道填V；只用稳定直流/准静态量。 */

/*
 * 六路offset为零物理量时ADC码。当前2048U只是1V65REF占位值；每路必须分别
 * 执行ZERO并填写，不能因三相对称而复制同一个结果。
 */
#define INV_USER_IU_OFFSET  (2048U)
#define INV_USER_IV_OFFSET  (2048U)
#define INV_USER_IW_OFFSET  (2048U)
#define INV_USER_VUV_OFFSET (2048U)
#define INV_USER_VVW_OFFSET (2048U)
#define INV_USER_VWU_OFFSET (2048U)

/* physical=(raw-offset)*scale；负scale表示采样链路反相，是合法标定结果。 */
#define INV_USER_IU_SCALE_A_PER_COUNT  (0.001f) /**< 占位A/count，闭环当前不使用电流反馈。 */
#define INV_USER_IV_SCALE_A_PER_COUNT  (0.001f) /**< 占位A/count，后续按本通道实测替换。 */
#define INV_USER_IW_SCALE_A_PER_COUNT  (0.001f) /**< 占位A/count，后续按本通道实测替换。 */
#define INV_USER_VUV_SCALE_V_PER_COUNT (0.010f) /**< 占位V/count，必须用VUV实测比例和极性替换。 */
#define INV_USER_VVW_SCALE_V_PER_COUNT (0.010f) /**< 占位V/count，必须用VVW实测比例和极性替换。 */
#define INV_USER_VWU_SCALE_V_PER_COUNT (0.010f) /**< 占位V/count，必须用VWU实测比例和极性替换。 */

/* ======================== 高级配置区（通常保持默认） ======================== */

#define INV_USER_CONTROL_FREQUENCY_HZ   (10000U) /**< 必须等于HRTIM Trigger2和DMA完整帧实际频率。 */
#define INV_USER_MINIMUM_DC_V           (1.0f)   /**< 参数有效性下限，不是实测母线欠压保护。 */
#define INV_USER_OFFSET_SAMPLES         (256U)   /**< ZERO平均帧数，10kHz下约25.6ms。 */
#define INV_USER_GAIN_SAMPLES           (256U)   /**< GAIN平均帧数，采集期间输入必须稳定。 */
#define INV_USER_CAL_MIN_DELTA_COUNTS    (32U)   /**< GAIN计算允许的最小原始码差。 */
#define INV_USER_ADC_MAX_SKEW_FRAMES     (2U)    /**< 超过后置同步诊断位；放宽档不关PWM。 */
#define INV_USER_ADC_RAIL_LOW_COUNT      (16U)   /**< 小于等于该值记低端贴轨候选。 */
#define INV_USER_ADC_RAIL_HIGH_COUNT     (4079U) /**< 大于等于该值记高端贴轨候选。 */
#define INV_USER_ADC_RAIL_CONFIRM_FRAMES (8U)    /**< 连续贴轨确认帧数，10kHz下约0.8ms。 */
#define INV_USER_STARTUP_TIMEOUT_MS      (500U)  /**< ADC零点/标定等待上限，单位ms。 */
#define INV_USER_SUPERVISOR_PERIOD_MS    (100U)  /**< 低速诊断和IWDG刷新周期，单位ms。 */
#define INV_USER_VOFA_PERIOD_MS          (10U)   /**< VOFA阻塞发送周期，单位ms。 */
#define INV_USER_OLED_PERIOD_MS          (100U)  /**< 软件I2C OLED刷新周期，单位ms。 */

/* ======================== 编译期选择检查 ======================== */

#if ((INV_USER_WORK_MODE != INV_MODE_ADC_CALIBRATION) && \
     (INV_USER_WORK_MODE != INV_MODE_PWM_WAVEFORM) && \
     (INV_USER_WORK_MODE != INV_MODE_OPEN_LOOP) && \
     (INV_USER_WORK_MODE != INV_MODE_VOLTAGE_PR))
#error "INV_USER_WORK_MODE is invalid"
#endif

#if ((INV_USER_OUTPUT_FREQUENCY != 30U) && (INV_USER_OUTPUT_FREQUENCY != 60U))
#error "INV_USER_OUTPUT_FREQUENCY must be 30U or 60U"
#endif

#if ((INV_USER_ENABLE_PWM_OUTPUT != 0U) && (INV_USER_ENABLE_PWM_OUTPUT != 1U)) || \
    ((INV_USER_VOLTAGE_CALIBRATION_CONFIRMED != 0U) && \
     (INV_USER_VOLTAGE_CALIBRATION_CONFIRMED != 1U)) || \
    ((INV_USER_RELAXED_PR_TEST != 0U) && (INV_USER_RELAXED_PR_TEST != 1U))
#error "INV boolean configuration must be 0U or 1U"
#endif

#endif /* INV_USER_CONFIG_H */
