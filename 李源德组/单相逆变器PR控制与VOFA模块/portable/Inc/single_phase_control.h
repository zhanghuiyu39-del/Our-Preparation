#ifndef SINGLE_PHASE_CONTROL_H
#define SINGLE_PHASE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pr_controller.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SINGLE_PHASE_CONTROL_OPEN_LOOP = 0,
    SINGLE_PHASE_CONTROL_VOLTAGE_LOOP,
    SINGLE_PHASE_CONTROL_CURRENT_LOOP,
    SINGLE_PHASE_CONTROL_DUAL_LOOP
} single_phase_control_mode_t;

typedef struct {
    single_phase_control_mode_t mode;
    float sample_frequency_hz;
    float reference_frequency_hz;
    float reference_amplitude_target;
    float soft_start_step_per_cycle;
    float open_loop_modulation_target;
    float open_loop_step_per_cycle;
    float dc_bus_minimum_v;
    float modulation_limit;
    float voltage_error_sign;
    float current_reference_gain;
    pr_controller_config_t voltage_pr;
    pr_controller_config_t current_pr;
} single_phase_control_config_t;

typedef struct {
    float inductor_current_a;
    float output_voltage_v;
    float dc_bus_voltage_v;
} single_phase_control_input_t;

typedef struct {
    float sine;
    float voltage_reference_v;
    float current_reference_a;
    float voltage_loop_output;
    float modulation_raw;
    float modulation;
    bool dc_bus_valid;
} single_phase_control_output_t;

typedef struct {
    single_phase_control_config_t config;
    pr_controller_t voltage_pr;
    pr_controller_t current_pr;
    float phase_rad;
    float phase_step_rad;
    float reference_amplitude;
    float open_loop_amplitude;
    uint32_t soft_start_counter;
    uint32_t samples_per_reference_cycle;
    bool enabled;
    bool initialized;
} single_phase_control_t;

bool single_phase_control_init(single_phase_control_t *control,
                               const single_phase_control_config_t *config);
void single_phase_control_get_default_config(single_phase_control_config_t *config);
void single_phase_control_reset(single_phase_control_t *control);
void single_phase_control_enable(single_phase_control_t *control, bool enable);
bool single_phase_control_set_reference_amplitude(single_phase_control_t *control,
                                                  float amplitude);
single_phase_control_output_t single_phase_control_step(
    single_phase_control_t *control,
    const single_phase_control_input_t *input);

#ifdef __cplusplus
}
#endif

#endif
