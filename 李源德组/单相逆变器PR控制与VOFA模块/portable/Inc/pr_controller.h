#ifndef PR_CONTROLLER_H
#define PR_CONTROLLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef struct {
    float kp;
    float kr;
    float resonant_frequency_hz;
    float bandwidth_rad_s;
    float sample_time_s;
    float output_min;
    float output_max;
} pr_controller_config_t;

typedef struct {
    float b0;
    float b1;
    float b2;
    float a1;
    float a2;
} pr_controller_coefficients_t;

typedef struct {
    pr_controller_config_t config;
    pr_controller_coefficients_t coefficients;
    float w1;
    float w2;
    float output;
    bool initialized;
} pr_controller_t;

bool pr_controller_init(pr_controller_t *controller,
                        const pr_controller_config_t *config);
void pr_controller_reset(pr_controller_t *controller);
float pr_controller_step(pr_controller_t *controller, float error);

#ifdef __cplusplus
}
#endif

#endif
