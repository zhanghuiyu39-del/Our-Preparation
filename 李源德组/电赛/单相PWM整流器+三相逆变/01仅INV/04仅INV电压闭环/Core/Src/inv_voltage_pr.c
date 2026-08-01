/**
 ******************************************************************************
 * @file    inv_voltage_pr.c
 * @brief   三相三线逆变器α/β双PR电压闭环。
 *
 * 数据流：VUV/VVW/VWU -> 虚拟相电压 -> Clarke -> α/β PR ->
 * 开环前馈叠加 -> 逆Clarke -> CBSVPWM三相命令。本模块不操作ADC DMA、
 * HRTIM、OLED或USART；反馈异常时只生成开环前馈，关断策略由应用层决定。
 ******************************************************************************
 */

#include "inv_voltage_pr.h"

#include "pr.h"
#include "stm32g4xx_hal.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define INV_ONE_THIRD_F       (0.3333333333333333f)
#define INV_ONE_OVER_SQRT3_F  (0.5773502691896258f)
#define INV_SQRT3_OVER_TWO_F  (0.8660254037844386f)

static INV_VoltagePRConfig voltage_pr_config;
static PR_t alpha_pr;
static PR_t beta_pr;
static volatile INV_VoltagePRTelemetry voltage_pr_telemetry;
static uint8_t voltage_pr_initialized;
static uint16_t valid_feedback_frames;
static uint32_t takeover_counter;
static uint32_t previous_phase;
static float vuv_square_sum;
static float vvw_square_sum;
static float vwu_square_sum;
static uint32_t rms_samples;

/** 判断单精度数是否为有限值。 */
static uint8_t INV_VoltagePR_IsFinite(float value)
{
    union
    {
        float value;
        uint32_t bits;
    } number;

    number.value = value;
    return ((number.bits & 0x7F800000UL) != 0x7F800000UL) ? 1U : 0U;
}

/** 检查三路线电压反馈是否可以进入PR。诊断位不等同于反馈数值无效。 */
static uint8_t INV_VoltagePR_FeedbackValid(const INV_Measurement *measurement)
{
    if ((measurement == NULL) || (measurement->offset_ready == 0U) ||
        (measurement->valid == 0U) ||
        (INV_VoltagePR_IsFinite(measurement->vuv) == 0U) ||
        (INV_VoltagePR_IsFinite(measurement->vvw) == 0U) ||
        (INV_VoltagePR_IsFinite(measurement->vwu) == 0U))
    {
        return 0U;
    }
    return 1U;
}

/** 在一个DDS周期回绕时发布三路线电压RMS。 */
static void INV_VoltagePR_UpdateRms(const INV_OpenLoopOutput *reference,
                                    const INV_Measurement *measurement)
{
    if (INV_VoltagePR_FeedbackValid(measurement) != 0U)
    {
        vuv_square_sum += measurement->vuv * measurement->vuv;
        vvw_square_sum += measurement->vvw * measurement->vvw;
        vwu_square_sum += measurement->vwu * measurement->vwu;
        rms_samples++;
    }

    if ((reference->phase_accumulator < previous_phase) && (rms_samples > 0U))
    {
        voltage_pr_telemetry.vuv_rms = sqrtf(vuv_square_sum / (float)rms_samples);
        voltage_pr_telemetry.vvw_rms = sqrtf(vvw_square_sum / (float)rms_samples);
        voltage_pr_telemetry.vwu_rms = sqrtf(vwu_square_sum / (float)rms_samples);
        vuv_square_sum = 0.0f;
        vvw_square_sum = 0.0f;
        vwu_square_sum = 0.0f;
        rms_samples = 0U;
    }
    previous_phase = reference->phase_accumulator;
}

/** 重新计算两路相同PR系数。 */
static bool INV_VoltagePR_Reconfigure(float frequency_hz)
{
    return PR_Init(&alpha_pr, voltage_pr_config.kp, voltage_pr_config.kr,
                   frequency_hz, voltage_pr_config.bandwidth_rad_s,
                   voltage_pr_config.sample_frequency_hz,
                   -voltage_pr_config.correction_limit_v,
                   voltage_pr_config.correction_limit_v) &&
           PR_Init(&beta_pr, voltage_pr_config.kp, voltage_pr_config.kr,
                   frequency_hz, voltage_pr_config.bandwidth_rad_s,
                   voltage_pr_config.sample_frequency_hz,
                   -voltage_pr_config.correction_limit_v,
                   voltage_pr_config.correction_limit_v);
}

bool INV_VoltagePR_Init(const INV_VoltagePRConfig *config)
{
    if ((config == NULL) || (INV_VoltagePR_IsFinite(config->kp) == 0U) ||
        (INV_VoltagePR_IsFinite(config->kr) == 0U) ||
        (INV_VoltagePR_IsFinite(config->fundamental_hz) == 0U) ||
        (INV_VoltagePR_IsFinite(config->bandwidth_rad_s) == 0U) ||
        (INV_VoltagePR_IsFinite(config->sample_frequency_hz) == 0U) ||
        (INV_VoltagePR_IsFinite(config->correction_limit_v) == 0U) ||
        (config->kp < 0.0f) || (config->kr < 0.0f) ||
        ((config->fundamental_hz != 30.0f) &&
         (config->fundamental_hz != 60.0f)) ||
        (config->bandwidth_rad_s <= 0.0f) ||
        (config->sample_frequency_hz <= 0.0f) ||
        (config->correction_limit_v <= 0.0f) ||
        (config->recovery_valid_frames == 0U) ||
        (config->takeover_samples == 0U))
    {
        voltage_pr_initialized = 0U;
        return false;
    }

    voltage_pr_config = *config;
    voltage_pr_initialized = 0U;
    if (!INV_VoltagePR_Reconfigure(config->fundamental_hz))
    {
        return false;
    }
    voltage_pr_initialized = 1U;
    INV_VoltagePR_Reset();
    return true;
}

bool INV_VoltagePR_SetFrequency(INV_OutputFrequency frequency)
{
    if ((voltage_pr_initialized == 0U) ||
        ((frequency != INV_FREQ_30HZ) && (frequency != INV_FREQ_60HZ)))
    {
        return false;
    }

    /* DDS接口只更新phase_step，因此当前电角度连续；随后重算对应PR系数。 */
    if (!INV_OpenLoop_SetFrequency(frequency))
    {
        return false;
    }
    voltage_pr_config.fundamental_hz = (float)frequency;
    if (!INV_VoltagePR_Reconfigure((float)frequency))
    {
        voltage_pr_initialized = 0U;
        return false;
    }
    INV_VoltagePR_Reset();
    return true;
}

bool INV_VoltagePR_Step(const INV_OpenLoopOutput *reference,
                        const INV_Measurement *measurement,
                        INV_VoltagePRTelemetry *output)
{
    INV_VoltagePRTelemetry next;
    float vu_feedback;
    float vv_feedback;
    float vw_feedback;
    float alpha_raw;
    float beta_raw;
    float correction_magnitude;
    float correction_scale = 1.0f;

    if ((output == NULL) || (reference == NULL) ||
        (voltage_pr_initialized == 0U) || (reference->valid == 0U))
    {
        return false;
    }

    next = voltage_pr_telemetry;
    next.alpha_reference = reference->vu_command;
    next.beta_reference =
        (reference->vv_command - reference->vw_command) * INV_ONE_OVER_SQRT3_F;
    next.alpha_correction = 0.0f;
    next.beta_correction = 0.0f;
    next.limited = 0U;
    next.valid = 0U;
    INV_VoltagePR_UpdateRms(reference, measurement);
    next.vuv_rms = voltage_pr_telemetry.vuv_rms;
    next.vvw_rms = voltage_pr_telemetry.vvw_rms;
    next.vwu_rms = voltage_pr_telemetry.vwu_rms;

    if (INV_VoltagePR_FeedbackValid(measurement) == 0U)
    {
        valid_feedback_frames = 0U;
        takeover_counter = 0U;
        PR_Reset(&alpha_pr);
        PR_Reset(&beta_pr);
        next.alpha_feedback = 0.0f;
        next.beta_feedback = 0.0f;
        next.feedback_valid = 0U;
        next.fallback_active = 1U;
        next.takeover = 0.0f;
    }
    else
    {
        vu_feedback = (measurement->vuv - measurement->vwu) * INV_ONE_THIRD_F;
        vv_feedback = (measurement->vvw - measurement->vuv) * INV_ONE_THIRD_F;
        vw_feedback = (measurement->vwu - measurement->vvw) * INV_ONE_THIRD_F;
        next.alpha_feedback = vu_feedback;
        next.beta_feedback = (vv_feedback - vw_feedback) * INV_ONE_OVER_SQRT3_F;
        next.feedback_valid = 1U;

        if (valid_feedback_frames < voltage_pr_config.recovery_valid_frames)
        {
            valid_feedback_frames++;
            takeover_counter = 0U;
            PR_Reset(&alpha_pr);
            PR_Reset(&beta_pr);
            next.fallback_active = 1U;
            next.takeover = 0.0f;
        }
        else if (!PR_Calc(&alpha_pr, next.alpha_reference,
                          next.alpha_feedback, &alpha_raw) ||
                 !PR_Calc(&beta_pr, next.beta_reference,
                          next.beta_feedback, &beta_raw))
        {
            valid_feedback_frames = 0U;
            takeover_counter = 0U;
            PR_Reset(&alpha_pr);
            PR_Reset(&beta_pr);
            next.fallback_active = 1U;
            next.takeover = 0.0f;
        }
        else
        {
            if (takeover_counter < voltage_pr_config.takeover_samples)
            {
                takeover_counter++;
            }
            next.takeover = (float)takeover_counter /
                            (float)voltage_pr_config.takeover_samples;
            if (next.takeover > 1.0f)
            {
                next.takeover = 1.0f;
            }
            alpha_raw *= next.takeover;
            beta_raw *= next.takeover;
            correction_magnitude = sqrtf(alpha_raw * alpha_raw +
                                         beta_raw * beta_raw);
            if (correction_magnitude > voltage_pr_config.correction_limit_v)
            {
                correction_scale = voltage_pr_config.correction_limit_v /
                                   correction_magnitude;
                next.limited = 1U;
            }
            next.alpha_correction = alpha_raw * correction_scale;
            next.beta_correction = beta_raw * correction_scale;
            next.limited |= (uint8_t)(alpha_pr.limited | beta_pr.limited);
            next.fallback_active = 0U;
        }
    }

    next.vu_command = next.alpha_reference + next.alpha_correction;
    next.vv_command = -0.5f * next.alpha_reference +
                      INV_SQRT3_OVER_TWO_F * next.beta_reference -
                      0.5f * next.alpha_correction +
                      INV_SQRT3_OVER_TWO_F * next.beta_correction;
    next.vw_command = -0.5f * next.alpha_reference -
                      INV_SQRT3_OVER_TWO_F * next.beta_reference -
                      0.5f * next.alpha_correction -
                      INV_SQRT3_OVER_TWO_F * next.beta_correction;

    if ((INV_VoltagePR_IsFinite(next.vu_command) == 0U) ||
        (INV_VoltagePR_IsFinite(next.vv_command) == 0U) ||
        (INV_VoltagePR_IsFinite(next.vw_command) == 0U))
    {
        return false;
    }

    next.valid = 1U;
    voltage_pr_telemetry = next;
    *output = next;
    return true;
}

void INV_VoltagePR_Reset(void)
{
    float vuv_rms = voltage_pr_telemetry.vuv_rms;
    float vvw_rms = voltage_pr_telemetry.vvw_rms;
    float vwu_rms = voltage_pr_telemetry.vwu_rms;

    PR_Reset(&alpha_pr);
    PR_Reset(&beta_pr);
    (void)memset((void *)&voltage_pr_telemetry, 0,
                 sizeof(voltage_pr_telemetry));
    voltage_pr_telemetry.vuv_rms = vuv_rms;
    voltage_pr_telemetry.vvw_rms = vvw_rms;
    voltage_pr_telemetry.vwu_rms = vwu_rms;
    voltage_pr_telemetry.fallback_active = 1U;
    valid_feedback_frames = 0U;
    takeover_counter = 0U;
    previous_phase = 0U;
    vuv_square_sum = 0.0f;
    vvw_square_sum = 0.0f;
    vwu_square_sum = 0.0f;
    rms_samples = 0U;
}

void INV_VoltagePR_GetSnapshot(INV_VoltagePRTelemetry *output)
{
    uint32_t primask;

    if (output == NULL)
    {
        return;
    }
    primask = __get_PRIMASK();
    __disable_irq();
    *output = voltage_pr_telemetry;
    __set_PRIMASK(primask);
}
