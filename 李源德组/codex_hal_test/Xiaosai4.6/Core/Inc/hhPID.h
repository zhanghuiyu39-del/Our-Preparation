#include <stdint.h>

#ifndef __PID_H
#define __PID_H

//已经略去微分项
typedef struct {
    // 设定值与反馈值
    float target;   // 目标值（如目标电流）
    float actual;   // 实际反馈值

    // PI 参数（注意：Ki 应已包含采样周期 T，即 Ki = Ki_cont * T）
    float Kp;
    float Ki;

    // 误差历史
    float error;      // e(k)
    float error_1;    // e(k-1)

    // 输出历史（用于累加增量）
    float output;     // u(k) —— 当前总输出（浮点，便于限幅）
    float output_1;   // u(k-1)

    // 输出限幅（根据 PWM 范围设定，如 0～100 或 0～1000）
    float out_min;    // 最小输出值（建议用 float，避免整数截断问题）
    float out_max;    // 最大输出值

    // 最终输出（可选：转换为整型 PWM 占空比）
    uint16_t pwm_output;
}PID_t;

void PID_Init(PID_t *pid, float kp, float ki, float out_min, float out_max);
void PID_Reset(PID_t *pid);
void PID_Calculate(PID_t *pid);


#endif

