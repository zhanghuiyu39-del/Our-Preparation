#ifndef PR_H
#define PR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 带限准比例谐振控制器对象。
 * @note 参数和离散系数在初始化后只读；运行状态只允许PR_Calc()和PR_Reset()修改。
 */
typedef struct
{
    float kp;                  /**< 比例增益，输出单位/输入单位。 */
    float kr;                  /**< 谐振增益，输出单位/输入单位。 */
    float fundamental_hz;      /**< 谐振中心频率，单位Hz。 */
    float bandwidth_rad_s;     /**< 准PR带宽，单位rad/s。 */
    float sample_frequency_hz; /**< PR_Calc()实际调用频率，单位Hz。 */

    float b0;                  /**< 已归一化离散分子系数。 */
    float b1;                  /**< 已归一化离散分子系数。 */
    float b2;                  /**< 已归一化离散分子系数。 */
    float a1;                  /**< 已归一化离散反馈系数。 */
    float a2;                  /**< 已归一化离散反馈系数。 */

    float w1;                  /**< 直接二型上一拍状态，只由PR_Calc()更新。 */
    float w2;                  /**< 直接二型上两拍状态，只由PR_Calc()更新。 */
    float error;               /**< 最近一次reference-feedback。 */
    float raw_output;          /**< 最近一次限幅前输出。 */
    float output;              /**< 最近一次限幅后输出。 */
    float output_min;          /**< 输出下限。 */
    float output_max;          /**< 输出上限。 */
    uint8_t initialized;       /**< 参数和系数有效时为1。 */
    uint8_t valid;             /**< 最近一次计算输入、状态和结果均有效时为1。 */
    uint8_t limited;           /**< 最近一次输出发生限幅时为1。 */
} PR_t;

/**
 * @brief 初始化准PR参数并按Tustin方法计算10 kHz离散系数。
 * @param pr 控制器对象地址，不能为NULL。
 * @param kp 比例增益。
 * @param kr 谐振增益。
 * @param fundamental_hz 谐振中心频率，单位Hz。
 * @param bandwidth_rad_s 谐振带宽，单位rad/s。
 * @param sample_frequency_hz 实际调用频率，单位Hz。
 * @param output_min 输出下限，必须小于output_max。
 * @param output_max 输出上限。
 * @retval true 参数、系数均有效；false 对象保持未初始化状态。
 * @note 只允许在PWM关闭、频率切换或控制器复位阶段调用，不访问HAL外设。
 */
bool PR_Init(PR_t *pr,
             float kp,
             float kr,
             float fundamental_hz,
             float bandwidth_rad_s,
             float sample_frequency_hz,
             float output_min,
             float output_max);

/**
 * @brief 清除PR历史状态和最近一次遥测，保留参数及离散系数。
 * @param pr 已初始化或待清零的控制器对象。
 * @note 可在10 kHz ISR中调用，无阻塞且不访问HAL。
 */
void PR_Reset(PR_t *pr);

/**
 * @brief 执行一次reference-feedback准PR计算。
 * @param pr 已初始化的控制器对象。
 * @param reference 本周期参考输入。
 * @param feedback 本周期反馈输入。
 * @param output 有效输出地址；失败时写入0。
 * @retval true 本次输出有效；false 输入、状态或计算结果非有限。
 * @note 饱和且误差继续推动同方向饱和时冻结谐振状态，避免状态持续累积。
 */
bool PR_Calc(PR_t *pr, float reference, float feedback, float *output);

#ifdef __cplusplus
}
#endif

#endif /* PR_H */
