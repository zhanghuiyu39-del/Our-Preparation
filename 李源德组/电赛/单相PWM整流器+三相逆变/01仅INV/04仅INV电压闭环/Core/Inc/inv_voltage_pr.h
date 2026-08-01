#ifndef INV_VOLTAGE_PR_H
#define INV_VOLTAGE_PR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "inv_measure.h"
#include "inv_open_loop.h"

/** α/β双PR电压闭环参数，初始化后由快速控制路径只读。 */
typedef struct
{
    float kp;                     /**< PR比例增益，单位V/V。 */
    float kr;                     /**< PR谐振增益，单位V/V。 */
    float fundamental_hz;         /**< PR中心频率，只允许30或60 Hz。 */
    float bandwidth_rad_s;        /**< 准PR带宽，单位rad/s。 */
    float sample_frequency_hz;    /**< 实际控制频率，单位Hz。 */
    float correction_limit_v;     /**< α/β校正矢量峰值上限，单位V。 */
    uint16_t recovery_valid_frames; /**< 回退后重新接管前要求的连续有效帧数。 */
    uint32_t takeover_samples;    /**< PR校正从0恢复到100%的控制周期数。 */
} INV_VoltagePRConfig;

/** 10 kHz ISR发布、主循环通过快照读取的电压闭环遥测。 */
typedef struct
{
    float alpha_reference; /**< α轴虚拟相电压参考，单位V。 */
    float beta_reference;  /**< β轴虚拟相电压参考，单位V。 */
    float alpha_feedback;  /**< 由三路线电压重构的α轴反馈，单位V。 */
    float beta_feedback;   /**< 由三路线电压重构的β轴反馈，单位V。 */
    float alpha_correction;/**< α轴最终PR校正，单位V。 */
    float beta_correction; /**< β轴最终PR校正，单位V。 */
    float vu_command;      /**< 送入CBSVPWM的U相虚拟相电压，单位V。 */
    float vv_command;      /**< 送入CBSVPWM的V相虚拟相电压，单位V。 */
    float vw_command;      /**< 送入CBSVPWM的W相虚拟相电压，单位V。 */
    float vuv_rms;         /**< 最近完整DDS周期的U-V线电压有效值，单位V RMS。 */
    float vvw_rms;         /**< 最近完整DDS周期的V-W线电压有效值，单位V RMS。 */
    float vwu_rms;         /**< 最近完整DDS周期的W-U线电压有效值，单位V RMS。 */
    float takeover;        /**< PR接管比例，范围0～1。 */
    uint8_t feedback_valid;/**< 本周期反馈可用于PR时为1。 */
    uint8_t fallback_active;/**< 当前退回开环前馈时为1。 */
    uint8_t limited;       /**< PR轴限幅或校正矢量限幅时为1。 */
    uint8_t valid;         /**< 本周期三相命令均为有限值时为1。 */
} INV_VoltagePRTelemetry;

/**
 * @brief 初始化α/β两路PR、恢复计数和RMS统计。
 * @param config 配置地址，所有字段在运行期间必须保持不变。
 * @retval true 参数有效；false 模块保持未初始化。
 * @note 在ADC/HRTIM启动前调用，不访问任何外设。
 */
bool INV_VoltagePR_Init(const INV_VoltagePRConfig *config);

/**
 * @brief 同步更改DDS频率与PR中心频率，并清除PR历史。
 * @param frequency 只允许30 Hz或60 Hz。
 * @retval true 系数重算成功；false 参数非法。
 * @note DDS只改变phase_step并保留当前相位；PR随后按takeover_samples平滑接管。
 */
bool INV_VoltagePR_SetFrequency(INV_OutputFrequency frequency);

/**
 * @brief 根据三路线电压反馈执行一次α/β双PR电压校正。
 * @param reference 本周期DDS开环参考。
 * @param measurement 本周期六通道一致性测量快照，可为NULL触发开环回退。
 * @param output 返回本周期遥测及三相最终电压命令。
 * @retval true 已生成可交给CBSVPWM的有限命令；false 模块或参考无效。
 * @note 在10 kHz ADC DMA完整帧路径调用，禁止阻塞、串口、OLED和动态内存。
 */
bool INV_VoltagePR_Step(const INV_OpenLoopOutput *reference,
                        const INV_Measurement *measurement,
                        INV_VoltagePRTelemetry *output);

/** @brief 清除PR、反馈恢复和RMS状态，保留初始化参数。 */
void INV_VoltagePR_Reset(void);

/** @brief 复制最近一次完整电压闭环遥测，供OLED和VOFA在主循环读取。 */
void INV_VoltagePR_GetSnapshot(INV_VoltagePRTelemetry *output);

#ifdef __cplusplus
}
#endif

#endif /* INV_VOLTAGE_PR_H */
