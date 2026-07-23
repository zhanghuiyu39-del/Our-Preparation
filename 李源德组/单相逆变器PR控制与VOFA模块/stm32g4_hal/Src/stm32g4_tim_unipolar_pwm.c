#include "stm32g4_tim_unipolar_pwm.h"

#include <math.h>
#include <stddef.h>

static float pwm_clamp(float value, float minimum, float maximum)
{
    if (value > maximum) {
        return maximum;
    }
    if (value < minimum) {
        return minimum;
    }
    return value;
}

bool stm32g4_tim_unipolar_pwm_init(stm32g4_tim_unipolar_pwm_t *pwm,
                                   TIM_HandleTypeDef *timer,
                                   uint32_t channel_a,
                                   uint32_t channel_b,
                                   float modulation_limit)
{
    if ((pwm == NULL) || (timer == NULL) ||
        !isfinite(modulation_limit) || (modulation_limit <= 0.0f) ||
        (modulation_limit > 1.0f)) {
        return false;
    }

    pwm->timer = timer;
    pwm->channel_a = channel_a;
    pwm->channel_b = channel_b;
    pwm->modulation_limit = modulation_limit;
    return true;
}

void stm32g4_tim_unipolar_pwm_apply(stm32g4_tim_unipolar_pwm_t *pwm,
                                    float modulation)
{
    uint32_t arr;
    uint32_t compare_a;
    uint32_t compare_b;
    float duty_a;
    float duty_b;

    if ((pwm == NULL) || (pwm->timer == NULL) || !isfinite(modulation)) {
        return;
    }

    modulation = pwm_clamp(modulation,
                           -pwm->modulation_limit,
                           pwm->modulation_limit);
    arr = __HAL_TIM_GET_AUTORELOAD(pwm->timer);
    duty_a = 0.5f * (1.0f + modulation);
    duty_b = 0.5f * (1.0f - modulation);
    compare_a = (uint32_t)(duty_a * (float)arr + 0.5f);
    compare_b = (uint32_t)(duty_b * (float)arr + 0.5f);
    if (compare_a > arr) {
        compare_a = arr;
    }
    if (compare_b > arr) {
        compare_b = arr;
    }

    __HAL_TIM_SET_COMPARE(pwm->timer, pwm->channel_a, compare_a);
    __HAL_TIM_SET_COMPARE(pwm->timer, pwm->channel_b, compare_b);
}

void stm32g4_tim_unipolar_pwm_set_neutral(stm32g4_tim_unipolar_pwm_t *pwm)
{
    if ((pwm == NULL) || (pwm->timer == NULL)) {
        return;
    }

    __HAL_TIM_SET_COMPARE(pwm->timer,
                          pwm->channel_a,
                          __HAL_TIM_GET_AUTORELOAD(pwm->timer) / 2U);
    __HAL_TIM_SET_COMPARE(pwm->timer,
                          pwm->channel_b,
                          __HAL_TIM_GET_AUTORELOAD(pwm->timer) / 2U);
}

HAL_StatusTypeDef stm32g4_tim_unipolar_pwm_start(
    stm32g4_tim_unipolar_pwm_t *pwm)
{
    HAL_StatusTypeDef status;

    if ((pwm == NULL) || (pwm->timer == NULL)) {
        return HAL_ERROR;
    }
    status = HAL_TIM_PWM_Start(pwm->timer, pwm->channel_a);
    if (status != HAL_OK) {
        return status;
    }
    status = HAL_TIM_PWM_Start(pwm->timer, pwm->channel_b);
    if (status != HAL_OK) {
        (void)HAL_TIM_PWM_Stop(pwm->timer, pwm->channel_a);
    }
    return status;
}

HAL_StatusTypeDef stm32g4_tim_unipolar_pwm_stop(
    stm32g4_tim_unipolar_pwm_t *pwm)
{
    HAL_StatusTypeDef status_a;
    HAL_StatusTypeDef status_b;

    if ((pwm == NULL) || (pwm->timer == NULL)) {
        return HAL_ERROR;
    }
    status_a = HAL_TIM_PWM_Stop(pwm->timer, pwm->channel_a);
    status_b = HAL_TIM_PWM_Stop(pwm->timer, pwm->channel_b);
    return (status_a != HAL_OK) ? status_a : status_b;
}
