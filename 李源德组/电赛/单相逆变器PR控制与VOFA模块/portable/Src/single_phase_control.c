#include "single_phase_control.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define SINGLE_PHASE_TWO_PI (6.2831853071795864769f)
#define SINGLE_PHASE_PI (3.1415926535897932385f)

static float control_clamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

static bool control_mode_valid(single_phase_control_mode_t mode)
{
    switch (mode) {
    case SINGLE_PHASE_CONTROL_OPEN_LOOP:
    case SINGLE_PHASE_CONTROL_VOLTAGE_LOOP:
    case SINGLE_PHASE_CONTROL_CURRENT_LOOP:
    case SINGLE_PHASE_CONTROL_DUAL_LOOP:
        return true;
    default:
        return false;
    }
}

static float control_ramp(float current, float target, float step)
{
    if (current < target) {
        current += step;
        return (current > target) ? target : current;
    }
    if (current > target) {
        current -= step;
        return (current < target) ? target : current;
    }
    return current;
}

bool single_phase_control_init(single_phase_control_t *control,
                               const single_phase_control_config_t *config)
{
    bool needs_voltage_pr;
    bool needs_current_pr;

    if ((control == NULL) || (config == NULL) ||
        !control_mode_valid(config->mode) ||
        !isfinite(config->sample_frequency_hz) ||
        !isfinite(config->reference_frequency_hz) ||
        !isfinite(config->reference_amplitude_target) ||
        !isfinite(config->soft_start_step_per_cycle) ||
        !isfinite(config->open_loop_modulation_target) ||
        !isfinite(config->open_loop_step_per_cycle) ||
        !isfinite(config->dc_bus_minimum_v) ||
        !isfinite(config->modulation_limit) ||
        !isfinite(config->voltage_error_sign) ||
        !isfinite(config->current_reference_gain) ||
        (config->sample_frequency_hz <= 0.0f) ||
        (config->reference_frequency_hz <= 0.0f) ||
        (config->sample_frequency_hz < config->reference_frequency_hz) ||
        (config->reference_amplitude_target < 0.0f) ||
        (config->soft_start_step_per_cycle <= 0.0f) ||
        (config->open_loop_modulation_target < 0.0f) ||
        (config->open_loop_step_per_cycle <= 0.0f) ||
        (config->dc_bus_minimum_v <= 0.0f) ||
        (config->modulation_limit <= 0.0f) ||
        (config->modulation_limit > 1.0f) ||
        ((config->voltage_error_sign != 1.0f) &&
         (config->voltage_error_sign != -1.0f)) ||
        (config->current_reference_gain <= 0.0f)) {
        return false;
    }

    (void)memset(control, 0, sizeof(*control));
    control->config = *config;
    control->config.voltage_pr.sample_time_s = 1.0f / config->sample_frequency_hz;
    control->config.current_pr.sample_time_s = 1.0f / config->sample_frequency_hz;
    control->phase_step_rad = SINGLE_PHASE_TWO_PI *
                              config->reference_frequency_hz /
                              config->sample_frequency_hz;
    control->samples_per_reference_cycle =
        (uint32_t)((config->sample_frequency_hz /
                    config->reference_frequency_hz) + 0.5f);
    if (control->samples_per_reference_cycle == 0U) {
        return false;
    }

    needs_voltage_pr = (config->mode == SINGLE_PHASE_CONTROL_VOLTAGE_LOOP) ||
                       (config->mode == SINGLE_PHASE_CONTROL_DUAL_LOOP);
    needs_current_pr = (config->mode == SINGLE_PHASE_CONTROL_CURRENT_LOOP) ||
                       (config->mode == SINGLE_PHASE_CONTROL_DUAL_LOOP);

    if (needs_voltage_pr &&
        !pr_controller_init(&control->voltage_pr,
                            &control->config.voltage_pr)) {
        return false;
    }
    if (needs_current_pr &&
        !pr_controller_init(&control->current_pr,
                            &control->config.current_pr)) {
        return false;
    }

    control->initialized = true;
    control->enabled = false;
    single_phase_control_reset(control);
    return true;
}

void single_phase_control_reset(single_phase_control_t *control)
{
    if (control == NULL) {
        return;
    }

    control->phase_rad = 0.0f;
    control->reference_amplitude = 0.0f;
    control->open_loop_amplitude = 0.0f;
    control->soft_start_counter = 0U;
    pr_controller_reset(&control->voltage_pr);
    pr_controller_reset(&control->current_pr);
}

void single_phase_control_enable(single_phase_control_t *control, bool enable)
{
    if ((control == NULL) || !control->initialized) {
        return;
    }

    if (enable != control->enabled) {
        single_phase_control_reset(control);
        control->enabled = enable;
    }
}

bool single_phase_control_set_reference_amplitude(single_phase_control_t *control,
                                                  float amplitude)
{
    if ((control == NULL) || !control->initialized ||
        !isfinite(amplitude) || (amplitude < 0.0f)) {
        return false;
    }

    control->config.reference_amplitude_target = amplitude;
    if (control->reference_amplitude > amplitude) {
        control->reference_amplitude = amplitude;
    }
    return true;
}

single_phase_control_output_t single_phase_control_step(
    single_phase_control_t *control,
    const single_phase_control_input_t *input)
{
    single_phase_control_output_t output = {0};
    float current_loop_output;

    if ((control == NULL) || (input == NULL) || !control->initialized ||
        !control->enabled || !isfinite(input->inductor_current_a) ||
        !isfinite(input->output_voltage_v) ||
        !isfinite(input->dc_bus_voltage_v)) {
        return output;
    }

    output.dc_bus_valid = input->dc_bus_voltage_v >=
                          control->config.dc_bus_minimum_v;
    if (!output.dc_bus_valid) {
        single_phase_control_reset(control);
        return output;
    }

    output.sine = sinf(control->phase_rad);
    control->phase_rad += control->phase_step_rad;
    if (control->phase_rad >= SINGLE_PHASE_TWO_PI) {
        control->phase_rad -= SINGLE_PHASE_TWO_PI;
    }

    control->soft_start_counter++;
    if (control->soft_start_counter >= control->samples_per_reference_cycle) {
        control->soft_start_counter = 0U;
        control->reference_amplitude = control_ramp(
            control->reference_amplitude,
            control->config.reference_amplitude_target,
            control->config.soft_start_step_per_cycle);
        control->open_loop_amplitude = control_ramp(
            control->open_loop_amplitude,
            control->config.open_loop_modulation_target,
            control->config.open_loop_step_per_cycle);
    }

    if (control->config.mode == SINGLE_PHASE_CONTROL_OPEN_LOOP) {
        output.voltage_reference_v = control->open_loop_amplitude * output.sine;
        output.modulation_raw = output.voltage_reference_v;
    } else {
        output.voltage_reference_v = control->reference_amplitude * output.sine;

        if ((control->config.mode == SINGLE_PHASE_CONTROL_VOLTAGE_LOOP) ||
            (control->config.mode == SINGLE_PHASE_CONTROL_DUAL_LOOP)) {
            output.voltage_loop_output = pr_controller_step(
                &control->voltage_pr,
                control->config.voltage_error_sign *
                    (output.voltage_reference_v - input->output_voltage_v));
        }

        if (control->config.mode == SINGLE_PHASE_CONTROL_VOLTAGE_LOOP) {
            output.modulation_raw = output.voltage_loop_output /
                                    input->dc_bus_voltage_v;
        } else {
            output.current_reference_a =
                (control->config.mode == SINGLE_PHASE_CONTROL_CURRENT_LOOP)
                    ? output.voltage_reference_v *
                          control->config.current_reference_gain
                    : output.voltage_loop_output;
            current_loop_output = pr_controller_step(
                &control->current_pr,
                output.current_reference_a - input->inductor_current_a);
            output.modulation_raw = current_loop_output /
                                    input->dc_bus_voltage_v;
        }
    }

    output.modulation = control_clamp(output.modulation_raw,
                                      -control->config.modulation_limit,
                                      control->config.modulation_limit);
    return output;
}

void single_phase_control_get_default_config(single_phase_control_config_t *config)
{
    if (config == NULL) {
        return;
    }

    (void)memset(config, 0, sizeof(*config));
    config->mode = SINGLE_PHASE_CONTROL_DUAL_LOOP;
    config->sample_frequency_hz = 20000.0f;
    config->reference_frequency_hz = 50.0f;
    config->reference_amplitude_target = 24.2f * 1.41421356237f;
    config->soft_start_step_per_cycle = 0.7f;
    config->open_loop_modulation_target = 0.50f;
    config->open_loop_step_per_cycle = 0.02f;
    config->dc_bus_minimum_v = 5.0f;
    config->modulation_limit = 0.90f;
    config->voltage_error_sign = -1.0f;
    config->current_reference_gain = 1.0f / 20.0f;

    config->voltage_pr.kp = 0.1f;
    config->voltage_pr.kr = 100.0f;
    config->voltage_pr.resonant_frequency_hz = 50.0f;
    config->voltage_pr.bandwidth_rad_s = SINGLE_PHASE_PI;
    config->voltage_pr.output_min = -1000.0f;
    config->voltage_pr.output_max = 1000.0f;

    config->current_pr.kp = 0.25f;
    config->current_pr.kr = 5.0f;
    config->current_pr.resonant_frequency_hz = 50.0f;
    config->current_pr.bandwidth_rad_s = SINGLE_PHASE_PI;
    config->current_pr.output_min = -1000.0f;
    config->current_pr.output_max = 1000.0f;
}
