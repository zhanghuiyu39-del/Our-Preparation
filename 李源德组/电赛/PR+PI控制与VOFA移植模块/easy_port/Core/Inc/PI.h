#ifndef PI_H
#define PI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==================== PI 控制器结构体 ====================
 * 参数和运行状态全部公开，便于在 Keil Watch 中直接观察。
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
    uint8_t initialized;
} PI_t;

/* 初始化 PI 参数并计算离散积分增益。
 * sample_frequency_hz 必须等于 PI_Calc() 的实际调用频率，单位为 Hz。
 * kp、ki 不允许为负数；参数无效时控制器保持未初始化状态。 */
void PI_Init(PI_t *pi,
             float kp,
             float ki,
             float sample_frequency_hz,
             float output_min,
             float output_max);

/* 清除 PI 运行状态但保留参数；停机、故障或退出母线闭环时调用。
 * 本函数不访问 HAL，可在控制中断或故障处理流程中调用。 */
void PI_Reset(PI_t *pi);

/* 执行一次位置式 PI 计算，误差方向固定为 reference - feedback。
 * 返回经过限幅的浮点输出；对象无效或未初始化时返回 0.0f。
 * 本函数不访问 HAL，可在固定周期控制任务中调用。 */
float PI_Calc(PI_t *pi, float reference, float feedback);

#ifdef __cplusplus
}
#endif

#endif /* PI_H */
