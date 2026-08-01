#ifndef PI_H
#define PI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==================== PI 控制器结构体 ====================
 * 参数和运行状态保存在结构体中，便于控制模块统一初始化、复位和遥测。
 * 本模块只处理浮点控制量，不包含 ADC 换算或 PWM/HRTIM 输出。 */
typedef struct {
    /* 用户设置的连续域参数；Ki 的单位包含“每秒”。 */
    float Kp;
    float Ki;
    float sample_frequency_hz;

    /* 初始化时计算 Ki * Ts，中断运行期间不再进行除法。 */
    float integral_gain;

    /* 当前运行状态；integral 是需要重点观察的积分累加量。 */
    float error;
    float proportional;
    float integral;
    float output;

    /* 输出上下限，以及参数初始化完成标志。 */
    float output_min;
    float output_max;
    uint8_t limited;
    uint8_t initialized;
} PI_t;

/**
 * @brief 初始化PI参数并计算离散积分增益。
 * sample_frequency_hz 必须等于 PI_Calc() 的实际调用频率，单位为 Hz。
 * kp、ki 不允许为负数；参数无效时控制器保持未初始化状态。 */
void PI_Init(PI_t *pi,
             float kp,
             float ki,
             float sample_frequency_hz,
             float output_min,
             float output_max);

/**
 * @brief 清除PI运行状态但保留参数；停机、故障或退出母线闭环时调用。
 * 本函数不访问 HAL，可在控制中断或故障处理流程中调用。 */
void PI_Reset(PI_t *pi);

/**
 * @brief  按期望输出预置PI状态，实现控制模式切换时的无扰衔接。
 * @param  pi 已初始化的PI对象。
 * @param  reference 当前参考值。
 * @param  feedback 当前反馈值。
 * @param  desired_output 希望切换瞬间保持的PI输出。
 * @retval 1表示预置成功，0表示对象、参数或数值无效。
 * @note   本函数只修改PI内部状态，不访问HAL，可在关中断临界区调用。
 */
uint8_t PI_PresetOutput(PI_t *pi,
                        float reference,
                        float feedback,
                        float desired_output);

/**
 * @brief 执行一次位置式PI计算，误差方向固定为reference-feedback。
 * 返回经过限幅的浮点输出；对象无效或未初始化时返回 0.0f。
 * 本函数不访问 HAL，可在固定周期控制任务中调用。 */
float PI_Calc(PI_t *pi, float reference, float feedback);

#ifdef __cplusplus
}
#endif

#endif /* PI_H */
