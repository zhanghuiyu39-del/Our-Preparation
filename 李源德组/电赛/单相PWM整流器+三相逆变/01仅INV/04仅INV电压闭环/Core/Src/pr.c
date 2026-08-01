/**
 ******************************************************************************
 * @file    pr.c
 * @brief   带限准比例谐振控制器。
 *
 * 本模块负责“固定周期误差 -> 准PR校正量”，不生成正弦参考、不读取ADC，
 * 也不操作CBSVPWM或HRTIM。全部运算使用单精度，供10 kHz控制ISR调用。
 ******************************************************************************
 */

#include "pr.h"

#include <stddef.h>
#include <string.h>

#define PR_PI_F (3.14159265358979323846f)

/** 判断单精度数是否为有限值，避免在快速路径引入双精度isfinite。 */
static uint8_t PR_IsFinite(float value)
{
    union
    {
        float value;
        uint32_t bits;
    } number;

    number.value = value;
    return ((number.bits & 0x7F800000UL) != 0x7F800000UL) ? 1U : 0U;
}

bool PR_Init(PR_t *pr,
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

    if (pr == NULL)
    {
        return false;
    }

    (void)memset(pr, 0, sizeof(*pr));
    if ((PR_IsFinite(kp) == 0U) || (PR_IsFinite(kr) == 0U) ||
        (PR_IsFinite(fundamental_hz) == 0U) ||
        (PR_IsFinite(bandwidth_rad_s) == 0U) ||
        (PR_IsFinite(sample_frequency_hz) == 0U) ||
        (PR_IsFinite(output_min) == 0U) || (PR_IsFinite(output_max) == 0U) ||
        (kp < 0.0f) || (kr < 0.0f) || (fundamental_hz <= 0.0f) ||
        (bandwidth_rad_s <= 0.0f) || (sample_frequency_hz <= 0.0f) ||
        (output_min >= output_max))
    {
        return false;
    }

    sample_time = 1.0f / sample_frequency_hz;
    omega_0 = 2.0f * PR_PI_F * fundamental_hz;
    t2_omega_0_2 = sample_time * sample_time * omega_0 * omega_0;
    denominator = 4.0f + 4.0f * sample_time * bandwidth_rad_s +
                  t2_omega_0_2;
    if ((PR_IsFinite(denominator) == 0U) || (denominator <= 0.0f))
    {
        return false;
    }

    pr->kp = kp;
    pr->kr = kr;
    pr->fundamental_hz = fundamental_hz;
    pr->bandwidth_rad_s = bandwidth_rad_s;
    pr->sample_frequency_hz = sample_frequency_hz;
    pr->output_min = output_min;
    pr->output_max = output_max;

    pr->b0 = (kp * denominator +
              4.0f * kr * bandwidth_rad_s * sample_time) / denominator;
    pr->b1 = kp * (2.0f * t2_omega_0_2 - 8.0f) / denominator;
    pr->b2 = (kp * (t2_omega_0_2 -
                    4.0f * sample_time * bandwidth_rad_s + 4.0f) -
              4.0f * kr * bandwidth_rad_s * sample_time) / denominator;
    pr->a1 = (2.0f * t2_omega_0_2 - 8.0f) / denominator;
    pr->a2 = (t2_omega_0_2 -
              4.0f * sample_time * bandwidth_rad_s + 4.0f) / denominator;

    if ((PR_IsFinite(pr->b0) == 0U) || (PR_IsFinite(pr->b1) == 0U) ||
        (PR_IsFinite(pr->b2) == 0U) || (PR_IsFinite(pr->a1) == 0U) ||
        (PR_IsFinite(pr->a2) == 0U))
    {
        (void)memset(pr, 0, sizeof(*pr));
        return false;
    }

    pr->initialized = 1U;
    PR_Reset(pr);
    return true;
}

void PR_Reset(PR_t *pr)
{
    if (pr == NULL)
    {
        return;
    }

    pr->w1 = 0.0f;
    pr->w2 = 0.0f;
    pr->error = 0.0f;
    pr->raw_output = 0.0f;
    pr->output = 0.0f;
    pr->valid = 0U;
    pr->limited = 0U;
}

bool PR_Calc(PR_t *pr, float reference, float feedback, float *output)
{
    float error;
    float w0;
    float raw_output;
    float limited_output;
    uint8_t freeze_state = 0U;

    if (output != NULL)
    {
        *output = 0.0f;
    }
    if ((pr == NULL) || (output == NULL) || (pr->initialized == 0U) ||
        (PR_IsFinite(reference) == 0U) || (PR_IsFinite(feedback) == 0U))
    {
        if (pr != NULL)
        {
            pr->valid = 0U;
        }
        return false;
    }

    error = reference - feedback;
    w0 = error - pr->a1 * pr->w1 - pr->a2 * pr->w2;
    raw_output = pr->b0 * w0 + pr->b1 * pr->w1 + pr->b2 * pr->w2;
    if ((PR_IsFinite(error) == 0U) || (PR_IsFinite(w0) == 0U) ||
        (PR_IsFinite(raw_output) == 0U))
    {
        pr->valid = 0U;
        return false;
    }

    limited_output = raw_output;
    pr->limited = 0U;
    if (raw_output > pr->output_max)
    {
        limited_output = pr->output_max;
        pr->limited = 1U;
        freeze_state = (error > 0.0f) ? 1U : 0U;
    }
    else if (raw_output < pr->output_min)
    {
        limited_output = pr->output_min;
        pr->limited = 1U;
        freeze_state = (error < 0.0f) ? 1U : 0U;
    }

    if (freeze_state == 0U)
    {
        pr->w2 = pr->w1;
        pr->w1 = w0;
    }
    pr->error = error;
    pr->raw_output = raw_output;
    pr->output = limited_output;
    pr->valid = 1U;
    *output = limited_output;
    return true;
}
