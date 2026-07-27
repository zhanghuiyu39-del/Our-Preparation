#include "control.h"
#include "tim.h"

#define period 4199
#define PHASE_DELTA (2.0f * PI * UREF_FREQ_HZ / PWM_FREQ_HZ)

static float phase = 0.0f;

static DIS_2ORDER_PR_DEF PR_U;
static DIS_2ORDER_PR_DEF PR_I;

float pr_iref = 0.0f;
float pr_vout = 0.0f;
float uref_inst = 0.0f;
float uref_amp = 0.0f;
float uref_amp_target = UREF_AMP_DEFAULT;
float open_mod_amp = 0.0f;
float ut_cmd = 0.0f;

static uint16_t soft_cnt = 0;
static uint8_t control_enabled = 0;

static uint16_t key_start_cnt = 0;
static uint8_t key_start_was_pressed = 0;

static uint16_t key_up_cnt = 0;
static uint8_t key_up_was_pressed = 0;

static uint16_t key_dn_cnt = 0;
static uint8_t key_dn_was_pressed = 0;

void PR_Init(DIS_2ORDER_PR_DEF *PR_obj, float Kp, float Kr, float Wc, float Wo, float T)
{
    PR_obj->T  = T;
    PR_obj->Kp = Kp;
    PR_obj->Kr = Kr;
    PR_obj->Wo = Wo;
    PR_obj->Wc = Wc;

    PR_obj->PR_coeff.B0 = Kp * (4.0f + 4.0f * T * Wc + T * T * Wo * Wo) + 4.0f * Kr * Wc * T;
    PR_obj->PR_coeff.B1 = Kp * (2.0f * T * T * Wo * Wo - 8.0f);
    PR_obj->PR_coeff.B2 = Kp * (T * T * Wo * Wo - 4.0f * T * Wc + 4.0f) - 4.0f * Kr * Wc * T;

    PR_obj->PR_coeff.gain = 1.0f / (4.0f + 4.0f * T * Wc + T * T * Wo * Wo);
    PR_obj->PR_coeff.A1 = (2.0f * T * T * Wo * Wo - 8.0f) * PR_obj->PR_coeff.gain;
    PR_obj->PR_coeff.A2 = (T * T * Wo * Wo - 4.0f * T * Wc + 4.0f) * PR_obj->PR_coeff.gain;

    PR_obj->PR_data.output = 0.0f;
    PR_obj->PR_data.w0 = 0.0f;
    PR_obj->PR_data.w1 = 0.0f;
    PR_obj->PR_data.w2 = 0.0f;
}

float discrete_2order_tf(float input, DIS_2ORDER_TF_COEF_DEF *coeff, DIS_2ORDER_TF_DATA_DEF *data)
{
    data->w0 = input - coeff->A1 * data->w1 - coeff->A2 * data->w2;
    data->output = coeff->gain * (coeff->B0 * data->w0 + coeff->B1 * data->w1 + coeff->B2 * data->w2);
    data->w2 = data->w1;
    data->w1 = data->w0;
    return data->output;
}

static void control_reset_runtime(void)
{
    phase = 0.0f;
    uref_amp = 0.0f;
    soft_cnt = 0;
    pr_iref = 0.0f;
    pr_vout = 0.0f;
    uref_inst = 0.0f;
    open_mod_amp = 0.0f;
    ut_cmd = 0.0f;

#if (CTRL_MODE == CTRL_MODE_VOLTAGE_TUNE) || (CTRL_MODE == CTRL_MODE_DUAL_PR_LOOP)
    PR_Init(&PR_U, PR_U_KP, PR_U_KR, PR_WC, PR_WO, CTRL_TS);
#endif
#if (CTRL_MODE == CTRL_MODE_CURRENT_TUNE) || (CTRL_MODE == CTRL_MODE_DUAL_PR_LOOP)
    PR_Init(&PR_I, PR_I_KP, PR_I_KR, PR_WC, PR_WO, CTRL_TS);
#endif
}

static uint8_t key_is_pressed(GPIO_TypeDef *port, uint16_t pin)
{
    return (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1u : 0u;
}

/* 松开触发: 短按即可, 避免需按住 50ms 才生效 */
static void key_on_release(uint8_t pressed, uint16_t *press_cnt, uint8_t *was_pressed,
                           void (*on_trigger)(void))
{
    if (pressed) {
        if (*press_cnt < 0xFFFFu) {
            (*press_cnt)++;
        }
    } else if (*was_pressed) {
        if (*press_cnt >= KEY_DEBOUNCE_MIN_SAMPLES &&
            *press_cnt <= KEY_DEBOUNCE_MAX_SAMPLES) {
            on_trigger();
        }
        *press_cnt = 0;
    } else {
        *press_cnt = 0;
    }
    *was_pressed = pressed;
}

static void on_start_toggle(void)
{
    control_enabled = !control_enabled;
    if (control_enabled) {
        control_reset_runtime();
    }
}

static void on_volt_up(void)
{
    uref_amp_target += UREF_AMP_KEY_STEP;
    if (uref_amp_target > UREF_AMP_MAX) {
        uref_amp_target = UREF_AMP_MAX;
    }
    uref_amp += UREF_AMP_KEY_STEP;
    if (uref_amp > uref_amp_target) {
        uref_amp = uref_amp_target;
    }
    soft_cnt = 0;
}

static void on_volt_down(void)
{
    uref_amp_target -= UREF_AMP_KEY_STEP;
    if (uref_amp_target < UREF_AMP_MIN) {
        uref_amp_target = UREF_AMP_MIN;
    }
    if (uref_amp > uref_amp_target) {
        uref_amp = uref_amp_target;
    }
}

void Control_Init(void)
{
    control_enabled = 0;
    uref_amp_target = UREF_AMP_DEFAULT;

    key_start_cnt = 0;
    key_start_was_pressed = 0;
    key_up_cnt = 0;
    key_up_was_pressed = 0;
    key_dn_cnt = 0;
    key_dn_was_pressed = 0;

    control_reset_runtime();
}

void Control_UpdateKeys(void)
{
    key_on_release(key_is_pressed(KEY_START_GPIO_Port, KEY_START_Pin),
                   &key_start_cnt, &key_start_was_pressed, on_start_toggle);

    key_on_release(key_is_pressed(KEY_VOLT_UP_GPIO_Port, KEY_VOLT_UP_Pin),
                   &key_up_cnt, &key_up_was_pressed, on_volt_up);

    key_on_release(key_is_pressed(KEY_VOLT_DN_GPIO_Port, KEY_VOLT_DN_Pin),
                   &key_dn_cnt, &key_dn_was_pressed, on_volt_down);
}

uint8_t Control_IsEnabled(void)
{
    return control_enabled;
}

static void soft_start_update(float *amp, float target, float step)
{
    if (*amp >= target) {
        *amp = target;
        return;
    }

    soft_cnt++;
    if (soft_cnt >= SOFT_PERIOD_SAMPLES) {
        soft_cnt = 0;
        *amp += step;
        if (*amp > target) {
            *amp = target;
        }
    }
}

static float reference_step_sin(void)
{
    float sin_val = sinf(phase);
    phase += PHASE_DELTA;
    if (phase >= 2.0f * PI) {
        phase -= 2.0f * PI;
    }
    return sin_val;
}

static float voltage_pr_control(float uref, float uo)
{
    return discrete_2order_tf(PR_U_ERROR_SIGN * (uref - uo), &PR_U.PR_coeff, &PR_U.PR_data);
}

static float current_pr_control(float iref, float il, float vdc)
{
    float iloop_out = discrete_2order_tf(iref - il, &PR_I.PR_coeff, &PR_I.PR_data);
    return iloop_out / vdc;
}

void Control_Execute(float il, float uo, float vdc, float *ut_out)
{
    float sin_val = reference_step_sin();

#if (CTRL_MODE == CTRL_MODE_OPEN_LOOP)
    soft_start_update(&open_mod_amp, OPEN_MOD_TARGET, OPEN_MOD_STEP);

    ut_cmd = open_mod_amp * sin_val;
    uref_inst = ut_cmd;
    pr_vout = 0.0f;
    pr_iref = 0.0f;
    *ut_out = ut_cmd;

#elif (CTRL_MODE == CTRL_MODE_VOLTAGE_TUNE)
    soft_start_update(&uref_amp, uref_amp_target, SOFT_AMP_STEP);
    uref_inst = uref_amp * sin_val;

    pr_vout = voltage_pr_control(uref_inst, uo);
    pr_iref = 0.0f;
    ut_cmd = pr_vout / vdc;
    *ut_out = ut_cmd;

#elif (CTRL_MODE == CTRL_MODE_CURRENT_TUNE)
    soft_start_update(&uref_amp, uref_amp_target, SOFT_AMP_STEP);
    uref_inst = uref_amp * sin_val;

    pr_iref = uref_inst / LOAD_R_OHM;
    pr_vout = 0.0f;
    ut_cmd = current_pr_control(pr_iref, il, vdc);
    *ut_out = ut_cmd;

#else /* CTRL_MODE_DUAL_PR_LOOP */
    soft_start_update(&uref_amp, uref_amp_target, SOFT_AMP_STEP);
    uref_inst = uref_amp * sin_val;

    pr_vout = voltage_pr_control(uref_inst, uo);
    pr_iref = pr_vout;
    ut_cmd = current_pr_control(pr_iref, il, vdc);
    *ut_out = ut_cmd;
#endif
}

uint32_t pulse_a;
uint32_t pulse_b;

void Unipolar_modulation(float ut)
{
    extern TIM_HandleTypeDef htim8;

    const float modulation_limit = 0.90f;
    if (ut >  modulation_limit) ut =  modulation_limit;
    if (ut < -modulation_limit) ut = -modulation_limit;

    float duty_a = 0.5f * (1.0f + ut);
    float duty_b = 0.5f * (1.0f - ut);

    pulse_a = (uint32_t)(duty_a * (float)period);
    pulse_b = (uint32_t)(duty_b * (float)period);

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, pulse_a);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, pulse_b);
}
