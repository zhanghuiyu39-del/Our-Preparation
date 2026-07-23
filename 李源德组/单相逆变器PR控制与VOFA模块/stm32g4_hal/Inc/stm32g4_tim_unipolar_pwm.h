#ifndef STM32G4_TIM_UNIPOLAR_PWM_H
#define STM32G4_TIM_UNIPOLAR_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32g4xx_hal.h"

#include <stdbool.h>

typedef struct {
    TIM_HandleTypeDef *timer;
    uint32_t channel_a;
    uint32_t channel_b;
    float modulation_limit;
} stm32g4_tim_unipolar_pwm_t;

bool stm32g4_tim_unipolar_pwm_init(stm32g4_tim_unipolar_pwm_t *pwm,
                                   TIM_HandleTypeDef *timer,
                                   uint32_t channel_a,
                                   uint32_t channel_b,
                                   float modulation_limit);
void stm32g4_tim_unipolar_pwm_apply(stm32g4_tim_unipolar_pwm_t *pwm,
                                    float modulation);
void stm32g4_tim_unipolar_pwm_set_neutral(stm32g4_tim_unipolar_pwm_t *pwm);
HAL_StatusTypeDef stm32g4_tim_unipolar_pwm_start(
    stm32g4_tim_unipolar_pwm_t *pwm);
HAL_StatusTypeDef stm32g4_tim_unipolar_pwm_stop(
    stm32g4_tim_unipolar_pwm_t *pwm);

#ifdef __cplusplus
}
#endif

#endif
