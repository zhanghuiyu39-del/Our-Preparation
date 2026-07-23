#include "pr_controller.h"
#include "single_phase_control.h"
#include "vofa_justfloat.h"

#include <stdbool.h>
#include <stdint.h>

static bool smoke_start_tx(const uint8_t *data,
                           uint16_t length,
                           void *user_context)
{
    (void)data;
    (void)length;
    (void)user_context;
    return true;
}

int module_smoke_test(void)
{
    single_phase_control_config_t config;
    single_phase_control_t control;
    single_phase_control_input_t input = {0.0f, 0.0f, 36.0f};
    single_phase_control_output_t output;
    vofa_justfloat_t vofa;
    float channels[3] = {0.0f, 1.0f, -1.0f};

    single_phase_control_get_default_config(&config);
    if (!single_phase_control_init(&control, &config)) {
        return 1;
    }
    single_phase_control_enable(&control, true);
    output = single_phase_control_step(&control, &input);
    if (!output.dc_bus_valid) {
        return 2;
    }

    if (!vofa_justfloat_init(&vofa, 3U, 1U, smoke_start_tx, 0)) {
        return 3;
    }
    if (!vofa_justfloat_push_isr(&vofa, channels, 3U)) {
        return 4;
    }
    if (!vofa_justfloat_process(&vofa)) {
        return 5;
    }
    vofa_justfloat_on_tx_complete_isr(&vofa);
    return (vofa_justfloat_queued(&vofa) == 0U) ? 0 : 6;
}
