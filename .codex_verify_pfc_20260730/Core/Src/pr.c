#include "pr.h"

#include <stddef.h>
#include <string.h>
#include <float.h>

/* 单精度圆周率常量，避免依赖不同编译器对 M_PI 的支持。 */
#define PR_PI (3.14159265358979323846f)

/** @brief 判断单精度数值是否为有限数，防止异常值进入IIR历史。 */
static uint8_t PR_FloatValid(float value)
{
    return (uint8_t)((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX));
}

/* ==================== PR 初始化 ====================
 * 原连续控制器使用带宽有限的准 PR 形式，离散化方法与参考代码一致。
 * B0/B1/B2 在初始化时已经除以公共分母，运行时不再重复乘 gain。
 */
void PR_Init(PR_t *pr,
             float kp,
             float kr,
             float fundamental_hz,
             float bandwidth_rad_s,
             float sample_frequency_hz,
             float output_min,
             float output_max)
{
    float sample_time;
    float omega_0;
    float t2_omega_0_2;
    float denominator;

    /* 空指针无法保存参数，直接返回。 */
    if (pr == NULL) {
        return;
    }

    /* 先清零整个对象；参数非法时控制器保持未初始化，PR_Calc() 将输出 0。 */
    (void)memset(pr, 0, sizeof(*pr));
    if ((fundamental_hz <= 0.0f) ||
        (bandwidth_rad_s <= 0.0f) ||
        (sample_frequency_hz <= 0.0f) ||
        (output_min >= output_max)) {
        return;
    }

    /* 保存用户参数，方便在线调试时确认当前控制器配置。 */
    pr->Kp = kp;
    pr->Kr = kr;
    pr->fundamental_hz = fundamental_hz;
    pr->bandwidth_rad_s = bandwidth_rad_s;
    pr->sample_frequency_hz = sample_frequency_hz;
    pr->output_min = output_min;
    pr->output_max = output_max;

    /* 将频率和采样周期换算为离散系数计算所需的中间量。 */
    sample_time = 1.0f / sample_frequency_hz;
    omega_0 = 2.0f * PR_PI * fundamental_hz;
    t2_omega_0_2 = sample_time * sample_time * omega_0 * omega_0;
    denominator = 4.0f +
                  4.0f * sample_time * bandwidth_rad_s +
                  t2_omega_0_2;

    /* 归一化分子系数，对应原代码中的 gain*B0、gain*B1、gain*B2。 */
    pr->B0 = (kp * denominator +
              4.0f * kr * bandwidth_rad_s * sample_time) /
             denominator;
    pr->B1 = kp * (2.0f * t2_omega_0_2 - 8.0f) /
             denominator;
    pr->B2 = (kp * (t2_omega_0_2 -
                    4.0f * sample_time * bandwidth_rad_s + 4.0f) -
              4.0f * kr * bandwidth_rad_s * sample_time) /
             denominator;

    /* 归一化反馈系数，对应原代码中的 A1 和 A2。 */
    pr->A1 = (2.0f * t2_omega_0_2 - 8.0f) /
             denominator;
    pr->A2 = (t2_omega_0_2 -
              4.0f * sample_time * bandwidth_rad_s + 4.0f) /
             denominator;

    /* 系数有效后再置位，避免参数错误时误运行控制器。 */
    pr->initialized = 1U;
    PR_Reset(pr);
}

/* ==================== PR 状态复位 ====================
 * 仅清除运行状态，不改变已经计算好的参数和离散系数。
 */
void PR_Reset(PR_t *pr)
{
    if (pr == NULL) {
        return;
    }

    /* 停机后清零历史量，下一次闭环从无历史误差的状态开始。 */
    pr->error_1 = 0.0f;
    pr->error_2 = 0.0f;
    pr->output_1 = 0.0f;
    pr->output_2 = 0.0f;
    pr->raw_output = 0.0f;
    pr->output = 0.0f;
    pr->limited = 0U;
}

/* ==================== PR 单步计算 ====================
 * 使用显式误差/输出历史实现二阶差分方程，形式与Simulink MATLAB Function一致。
 */
float PR_Calc(PR_t *pr, float reference, float feedback)
{
    float error;
    float raw_output;

    /* 未初始化或对象为空时输出 0，避免使用无效系数。 */
    if ((pr == NULL) || (pr->initialized == 0U) ||
        (PR_FloatValid(reference) == 0U) ||
        (PR_FloatValid(feedback) == 0U)) {
        return 0.0f;
    }

    /*
     * 与Simulink MATLAB Function保持一致：直接保存e(k-1)、e(k-2)、
     * y(k-1)、y(k-2)。发生饱和时把限幅后的y写入历史，阻止输出历史失控。
     */
    error = reference - feedback;
    raw_output = pr->B0 * error +
                 pr->B1 * pr->error_1 +
                 pr->B2 * pr->error_2 -
                 pr->A1 * pr->output_1 -
                 pr->A2 * pr->output_2;
    pr->raw_output = raw_output;

    /* 对控制输出做软件限幅；硬件过流和 HRTIM Fault 仍需独立配置。 */
    pr->limited = 0U;
    if (PR_FloatValid(raw_output) == 0U) {
        pr->output = 0.0f;
        pr->limited = 1U;
    } else if (raw_output > pr->output_max) {
        pr->output = pr->output_max;
        pr->limited = 1U;
    } else if (raw_output < pr->output_min) {
        pr->output = pr->output_min;
        pr->limited = 1U;
    } else {
        pr->output = raw_output;
    }

    pr->error_2 = pr->error_1;
    pr->error_1 = error;
    pr->output_2 = pr->output_1;
    pr->output_1 = pr->output;

    return pr->output;
}
