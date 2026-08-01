#ifndef PR_H
#define PR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ==================== PR 控制器结构体 ====================
 * 参数、离散系数和历史状态保存在结构体中，便于统一初始化、复位和遥测。
 * 所有频率单位为 Hz，带宽单位为 rad/s，采样周期单位为 s。
 */
typedef struct {
    /* 用户设置的控制参数。 */
    float Kp;
    float Kr;
    float fundamental_hz;
    float bandwidth_rad_s;
    float sample_frequency_hz;

    /* Tustin 离散后并已归一化的二阶差分系数。 */
    float B0;
    float B1;
    float B2;
    float A1;
    float A2;

    /* 与Simulink差分式对应的误差和饱和输出历史。 */
    float error_1;
    float error_2;
    float output_1;
    float output_2;
    float raw_output;
    float output;

    /* 输出限幅以及初始化完成标志。 */
    float output_min;
    float output_max;
    uint8_t limited;
    uint8_t initialized;
} PR_t;

/**
 * @brief 初始化PR参数并按实际采样频率计算离散系数。
 * fundamental_hz：PR 谐振频率，工频控制通常填 50.0f。
 * sample_frequency_hz：PR_Calc() 的实际调用频率，不一定等于 PWM 标称频率。
 */
void PR_Init(PR_t *pr,
             float kp,
             float kr,
             float fundamental_hz,
             float bandwidth_rad_s,
             float sample_frequency_hz,
             float output_min,
             float output_max);

/** @brief 清除PR误差和饱和输出历史，停机、故障或重新闭环前调用。 */
void PR_Reset(PR_t *pr);

/**
 * @brief 执行一次PR计算，内部误差固定为reference-feedback。
 * 本函数可在控制中断中调用，不访问 HAL，也不会阻塞。
 */
float PR_Calc(PR_t *pr, float reference, float feedback);

#ifdef __cplusplus
}
#endif

#endif /* PR_H */
