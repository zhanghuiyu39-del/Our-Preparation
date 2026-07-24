#include "pr_controller.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define PR_TWO_PI (6.2831853071795864769f)

static float pr_clamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

bool pr_controller_init(pr_controller_t *controller,
                        const pr_controller_config_t *config)
{
    float omega_0;
    float t2_omega_0_2;
    float denominator;

    if ((controller == NULL) || (config == NULL) ||
        !isfinite(config->kp) || !isfinite(config->kr) ||
        !isfinite(config->resonant_frequency_hz) ||
        !isfinite(config->bandwidth_rad_s) ||
        !isfinite(config->sample_time_s) ||
        (config->resonant_frequency_hz <= 0.0f) ||
        (config->bandwidth_rad_s <= 0.0f) ||
        (config->sample_time_s <= 0.0f) ||
        !isfinite(config->output_min) || !isfinite(config->output_max) ||
        (config->output_min >= config->output_max)) {
        return false;
    }

    controller->initialized = false;
    controller->config = *config;
    omega_0 = PR_TWO_PI * config->resonant_frequency_hz;
    t2_omega_0_2 = config->sample_time_s * config->sample_time_s *
                   omega_0 * omega_0;
    denominator = 4.0f +
                  4.0f * config->sample_time_s * config->bandwidth_rad_s +
                  t2_omega_0_2;

    if (!isfinite(denominator) || (fabsf(denominator) <= FLT_MIN)) {
        return false;
    }

    controller->coefficients.b0 =
        (config->kp * denominator +
         4.0f * config->kr * config->bandwidth_rad_s * config->sample_time_s) /
        denominator;
    controller->coefficients.b1 =
        config->kp * (2.0f * t2_omega_0_2 - 8.0f) / denominator;
    controller->coefficients.b2 =
        (config->kp * (t2_omega_0_2 -
                       4.0f * config->sample_time_s * config->bandwidth_rad_s +
                       4.0f) -
         4.0f * config->kr * config->bandwidth_rad_s * config->sample_time_s) /
        denominator;
    controller->coefficients.a1 =
        (2.0f * t2_omega_0_2 - 8.0f) / denominator;
    controller->coefficients.a2 =
        (t2_omega_0_2 -
         4.0f * config->sample_time_s * config->bandwidth_rad_s + 4.0f) /
        denominator;

    controller->initialized = true;
    pr_controller_reset(controller);
    return true;
}

void pr_controller_reset(pr_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }

    controller->w1 = 0.0f;
    controller->w2 = 0.0f;
    controller->output = 0.0f;
}

float pr_controller_step(pr_controller_t *controller, float error)
{
    float w0;
    float raw_output;

    if ((controller == NULL) || !controller->initialized || !isfinite(error)) {
        return 0.0f;
    }

    w0 = error - controller->coefficients.a1 * controller->w1 -
         controller->coefficients.a2 * controller->w2;
    raw_output = controller->coefficients.b0 * w0 +
                 controller->coefficients.b1 * controller->w1 +
                 controller->coefficients.b2 * controller->w2;

    controller->w2 = controller->w1;
    controller->w1 = w0;
    controller->output = pr_clamp(raw_output,
                                  controller->config.output_min,
                                  controller->config.output_max);
    return controller->output;
}
