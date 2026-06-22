#include "hqpr.h"

// QPR 初始化
void QPR_Init(QPR_TypeDef *qpr, float Kp, float Kr, float wc, float w0)
{
    qpr->Kp = Kp;
    qpr->Kr = Kr;
    qpr->wc = wc;
    qpr->w0 = w0;

    qpr->err0 = 0;
    qpr->err1 = 0;
    qpr->err2 = 0;
    qpr->out0 = 0;
    qpr->out1 = 0;
    qpr->out2 = 0;
}

// QPR 迭代计算（20kHz 每周期调用1次）
// Ts = 0.00005f
float QPR_Calc(QPR_TypeDef *qpr, float ref, float fb, float Ts)
{
    // 历史值移位
    qpr->err2 = qpr->err1;
    qpr->err1 = qpr->err0;
    qpr->err0 = ref - fb;

    qpr->out2 = qpr->out1;
    qpr->out1 = qpr->out0;

    // 离散系数（后向差分，工程固化）
    float a0 = 1.0f;
    float a1 = -(2.0f + 2 * qpr->wc * Ts);
    float a2 = 1.0f + 2 * qpr->wc * Ts + qpr->w0 * qpr->w0 * Ts * Ts;

    float b0 = qpr->Kp + 2 * qpr->Kr * qpr->wc * Ts;
    float b1 = -qpr->Kp;
    float b2 = 0.0f;

    // 递推公式
    qpr->out0 = (b0 * qpr->err0 + b1 * qpr->err1 + b2 * qpr->err2
                 - a1 * qpr->out1 - a2 * qpr->out2) / a0;

    return qpr->out0;
}
