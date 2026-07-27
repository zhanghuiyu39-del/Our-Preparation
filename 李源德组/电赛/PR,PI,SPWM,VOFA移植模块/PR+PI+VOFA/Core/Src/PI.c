#include "PI.h"

#include <stddef.h>
#include <string.h>

/* ==================== PI 初始化 ====================
 * Ki 按连续域积分增益传入，这里乘以采样周期得到单次积分增量系数。 */
void PI_Init(PI_t *pi,
             float kp,
             float ki,
             float sample_frequency_hz,
             float output_min,
             float output_max)
{
    /* 空指针无法保存参数，直接返回。 */
    if (pi == NULL) {
        return;
    }

    /* 先清除旧参数和状态；参数非法时 initialized 保持为 0。 */
    (void)memset(pi, 0, sizeof(*pi));
    if ((kp < 0.0f) ||
        (ki < 0.0f) ||
        (sample_frequency_hz <= 0.0f) ||
        (output_min >= output_max)) {
        return;
    }

    /* 保存连续域参数，方便上板后检查实际配置。 */
    pi->Kp = kp;
    pi->Ki = ki;
    pi->sample_frequency_hz = sample_frequency_hz;
    pi->output_min = output_min;
    pi->output_max = output_max;

    /* 前向欧拉离散积分：I(k) = I(k-1) + Ki * Ts * e(k)。 */
    pi->integral_gain = ki / sample_frequency_hz;

    /* 参数全部有效后再置位，避免误用未完成初始化的控制器。 */
    pi->initialized = 1U;
    PI_Reset(pi);
}

/* ==================== PI 状态复位 ====================
 * 只清除运行状态，不修改已经设置好的增益、频率和输出限幅。 */
void PI_Reset(PI_t *pi)
{
    if (pi == NULL) {
        return;
    }

    /* 母线外环重新投入前从零积分开始，避免带着旧指令突然启动。 */
    pi->error = 0.0f;
    pi->proportional = 0.0f;
    pi->integral = 0.0f;
    pi->output = 0.0f;
}

/* ==================== PI 单步计算 ====================
 * 使用位置式 PI，并通过条件积分避免输出饱和后积分继续累积。 */
float PI_Calc(PI_t *pi, float reference, float feedback)
{
    float integral_candidate;
    float output_candidate;

    /* 未初始化或对象为空时输出零，避免使用无效控制参数。 */
    if ((pi == NULL) || (pi->initialized == 0U)) {
        return 0.0f;
    }

    /* 误差方向固定为“参考值减反馈值”，传感器极性应在测量层修正。 */
    pi->error = reference - feedback;
    pi->proportional = pi->Kp * pi->error;

    /* 先计算候选积分和候选输出，再根据饱和方向决定是否接受积分。 */
    integral_candidate = pi->integral + pi->integral_gain * pi->error;
    output_candidate = pi->proportional + integral_candidate;

    /* 上限饱和且正误差仍推动输出增大时冻结积分；负误差允许积分回退。 */
    if (output_candidate > pi->output_max) {
        if (pi->error < 0.0f) {
            pi->integral = integral_candidate;
        }
        pi->output = pi->output_max;
    }
    /* 下限饱和且负误差仍推动输出减小时冻结积分；正误差允许积分回升。 */
    else if (output_candidate < pi->output_min) {
        if (pi->error > 0.0f) {
            pi->integral = integral_candidate;
        }
        pi->output = pi->output_min;
    }
    /* 未饱和时接受本次积分，并直接输出位置式 PI 计算结果。 */
    else {
        pi->integral = integral_candidate;
        pi->output = output_candidate;
    }

    return pi->output;
}
