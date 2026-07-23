#include "control_dq.h"
#include "dq_power.h"
#include "tim.h"
#include <math.h>

extern TIM_HandleTypeDef htim8;

#define DQ_PWM_PERIOD           4199U
#define DQ_PWM_CMP_BLOCKED      0U
#define DQ_MODULATION_LIMIT     0.95f
#define DQ_VDC_NOMINAL          40.0f
#define DQ_VDC_MIN_PU           1.0f
#define DQ_FILTER_L_H           0.001f

float dq_vd = 0.0f;
float dq_vq = 0.0f;
float dq_id = 0.0f;
float dq_iq = 0.0f;
float dq_vd_ref = 0.0f;
float dq_droop_freq = DQ_FREQ_DEFAULT;  /* 下垂/控制输出频率 (Hz), VOFA CH0 */
float dq_output_freq = DQ_FREQ_DEFAULT;
float dq_theta = 0.0f;
float dq_svpwm_ta = 0.0f;
float dq_svpwm_tb = 0.0f;
float dq_svpwm_tc = 0.0f;
uint32_t dq_pwm_cmp_a = 0U;
uint32_t dq_pwm_cmp_b = 0U;
uint32_t dq_pwm_cmp_c = 0U;

static DqTransformState s_v;
static DqTransformState s_i;
static DqTransformState s_v_inv;
static DqPIControl s_vd_pi;
static DqPIControl s_vq_pi;
static DqPIControl s_id_pi;
static DqPIControl s_iq_pi;
static DqSVGen s_svpwm;
static DqSPLL_3P_DDSRF s_spll;

static float s_sinwt = 0.0f;
static float s_coswt = 0.0f;
static float s_Uo = 20.0f;
static float s_Uo_target = 20.0f;
static float s_Pref = DQ_PREF_DEFAULT;
static float s_Qref = DQ_QREF_DEFAULT;
static float s_m = DQ_DROOP_M_HZ_PER_W;
static float s_n = DQ_DROOP_N_V_PER_VAR;
static float s_freq = DQ_FREQ_DEFAULT;
static float s_d_open = 0.0f;
static float s_q_open = 0.0f;
static float s_open_ramp = 0.0f;
static uint16_t s_soft_cnt = 0;

static float clampf(float x, float min_v, float max_v)
{
    if (x > max_v) {
        return max_v;
    }
    if (x < min_v) {
        return min_v;
    }
    return x;
}

/* 闭环调制量标幺: m = u / Vdc, 使用 ADC 实测母线电压 */
static float inv_vdc_pu(float vdc)
{
    if (vdc < DQ_VDC_MIN_PU) {
        vdc = DQ_VDC_NOMINAL;
    }
    return 1.0f / vdc;
}

static float dq_2pi_f(void)
{
    return 2.0f * PI * dq_output_freq;
}

static uint16_t soft_period_samples(void)
{
    uint16_t n = (uint16_t)(PWM_FREQ_HZ / dq_output_freq);

    if (n < 1U) {
        n = 1U;
    }
    return n;
}

static void soft_start_uo(void)
{
    if (s_Uo >= s_Uo_target) {
        s_Uo = s_Uo_target;
        return;
    }

    s_soft_cnt++;
    if (s_soft_cnt >= soft_period_samples()) {
        s_soft_cnt = 0;
        s_Uo += DQ_UO_SOFT_STEP;
        if (s_Uo > s_Uo_target) {
            s_Uo = s_Uo_target;
        }
    }
}

static void update_vd_ref_soft(void)
{
    soft_start_uo();
    dq_vd_ref = s_Uo;
}

/* 离网/独立运行: 固定输出频率角度, 不启 PLL 与下垂 */
static void integrate_theta(void)
{
    dq_theta += dq_output_freq * 2.0f * PI * CTRL_TS;
    if (dq_theta >= PI) {
        dq_theta -= 2.0f * PI;
    }
    if (dq_theta <= -PI) {
        dq_theta += 2.0f * PI;
    }

    s_sinwt = sinf(dq_theta);
    s_coswt = cosf(dq_theta);
}

static void update_theta_fixed(void)
{
    integrate_theta();
    dq_droop_freq = dq_output_freq;
    update_vd_ref_soft();
}

static void update_theta_droop(void)
{
    soft_start_uo();

    /* f = f0 + m*(Pref-P) */
    dq_droop_freq = (s_Pref - dq_P) * s_m + s_freq;
    dq_theta += dq_droop_freq * 2.0f * PI * CTRL_TS;
    if (dq_theta >= PI) {
        dq_theta -= 2.0f * PI;
    }
    if (dq_theta <= -PI) {
        dq_theta += 2.0f * PI;
    }

    s_sinwt = sinf(dq_theta);
    s_coswt = cosf(dq_theta);

    /* Vd = Uo + n*(Qref-Q), 送电压环/电流环作 d 轴电压期望 */
    dq_vd_ref = (s_Qref - dq_Q) * s_n + s_Uo;
    if (dq_vd_ref < 0.0f) {
        dq_vd_ref = 0.0f;
    }
}

#if 0 /* 并网锁相时可恢复: 电压/双环/单电流环现用 update_theta_fixed */
static void update_theta_pll(const DqAdcSample *sample)
{
    float va;
    float vb;
    float vc;

    s_v.AB = sample->u_ab;
    s_v.BC = sample->u_bc;
    s_v.CA = -s_v.AB - s_v.BC;
    DqTransform_LineToPhase(&s_v);

    va = s_v.A;
    vb = s_v.B;
    vc = s_v.C;

    DqSPLL_Run(&s_spll, va, vb, vc);

    dq_theta = s_spll.theta;
    s_sinwt = s_spll.sinwt;
    s_coswt = s_spll.coswt;
    s_w = s_spll.grid_freq;

    update_vd_ref_soft();
}
#endif

static void sample_to_dq(const DqAdcSample *sample)
{
    s_v.AB = sample->u_ab;
    s_v.BC = sample->u_bc;
    s_v.CA = -s_v.AB - s_v.BC;
    DqTransform_LineToPhase(&s_v);
    DqTransform_AbcToDq(&s_v, s_sinwt, s_coswt);

    s_i.A = sample->i_a;
    s_i.C = sample->i_c;
    s_i.B = -s_i.A - s_i.C;
    DqTransform_AbcToDq(&s_i, s_sinwt, s_coswt);

    dq_vd = s_v.d;
    dq_vq = s_v.q;
    dq_id = s_i.d;
    dq_iq = s_i.q;
}

static void run_voltage_pi(float vd_ref, float vq_ref)
{
    s_vd_pi.Ref = vd_ref;
    s_vq_pi.Ref = vq_ref;
    s_vd_pi.Fbk = s_v.d;
    s_vq_pi.Fbk = s_v.q;
    DqPI_Run(&s_vd_pi);
    DqPI_Run(&s_vq_pi);
}

static void run_current_pi(float id_ref, float iq_ref, float inv_vdc)
{
    float wL = dq_2pi_f() * DQ_FILTER_L_H;

    s_id_pi.Ref = id_ref;
    s_iq_pi.Ref = iq_ref;
    s_id_pi.Fbk = s_i.d;
    s_iq_pi.Fbk = s_i.q;
    DqPI_Run(&s_id_pi);
    DqPI_Run(&s_iq_pi);

    s_v_inv.d = (s_id_pi.Out + s_v.d + wL * s_i.q) * inv_vdc;
    s_v_inv.q = (s_iq_pi.Out + s_v.q - wL * s_i.d) * inv_vdc;
}

static void apply_modulation_limits(DqTransformState *inv)
{
    inv->d = clampf(inv->d, -DQ_MODULATION_LIMIT, DQ_MODULATION_LIMIT);
    inv->q = clampf(inv->q, -DQ_MODULATION_LIMIT, DQ_MODULATION_LIMIT);
}

static uint8_t s_pwm_outputs_on = 0U;

static void pwm_outputs_start(void)
{
    if (!s_pwm_outputs_on) {
        HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_2);
        HAL_TIMEx_PWMN_Start(&htim8, TIM_CHANNEL_3);
        s_pwm_outputs_on = 1U;
    }
    __HAL_TIM_MOE_ENABLE(&htim8);
}

static void pwm_outputs_stop(void)
{
    if (s_pwm_outputs_on) {
        HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_1);
        HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_2);
        HAL_TIMEx_PWMN_Stop(&htim8, TIM_CHANNEL_3);
        s_pwm_outputs_on = 0U;
    }
    __HAL_TIM_MOE_DISABLE(&htim8);
}

void ControlDq_Init(void)
{
    ControlDq_ResetRuntime();
}

void ControlDq_TimAdcTriggerStart(void)
{
    dq_pwm_cmp_a = DQ_PWM_CMP_BLOCKED;
    dq_pwm_cmp_b = DQ_PWM_CMP_BLOCKED;
    dq_pwm_cmp_c = DQ_PWM_CMP_BLOCKED;

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, dq_pwm_cmp_a);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, dq_pwm_cmp_b);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, dq_pwm_cmp_c);
    __HAL_TIM_MOE_DISABLE(&htim8);

    /* 仅启动主通道 PWM 计数, 产生 TIM8 TRGO 触发 ADC; 不启动 PWMN, MOE=0 封波 */
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
    s_pwm_outputs_on = 0U;
}

void ControlDq_ResetRuntime(void)
{
    pwm_outputs_stop();

    DqTransform_Clear(&s_v);
    DqTransform_Clear(&s_i);
    DqTransform_Clear(&s_v_inv);
    DqSVGen_Clear(&s_svpwm);
    DqSPLL_Clear(&s_spll);

    DqSPLL_Init(&s_spll, UREF_FREQ_HZ, CTRL_TS, 0.99f, 0.01f, 5.0f, 0.5f);
    DqPower_Reset();
    DqPI_Init(&s_vd_pi, DQ_VD_KP, DQ_VD_KI, 20.0f, -20.0f);
    DqPI_Init(&s_vq_pi, DQ_VQ_KP, DQ_VQ_KI, 20.0f, -20.0f);
    DqPI_Init(&s_id_pi, DQ_ID_KP, DQ_ID_KI, 400.0f, -400.0f);
    DqPI_Init(&s_iq_pi, DQ_IQ_KP, DQ_IQ_KI, 400.0f, -400.0f);

    s_sinwt = 0.0f;
    s_coswt = 1.0f;
    s_Uo = 0.0f;
    s_Uo_target = DQ_UO_DEFAULT;
    s_Pref = DQ_PREF_DEFAULT;
    s_Qref = DQ_QREF_DEFAULT;
    s_m = DQ_DROOP_M_HZ_PER_W;
    s_n = DQ_DROOP_N_V_PER_VAR;
    dq_output_freq = DQ_FREQ_DEFAULT;
    s_freq = DQ_FREQ_DEFAULT;
    dq_droop_freq = DQ_FREQ_DEFAULT;
    dq_theta = 0.0f;
    s_d_open = 0.0f;
    s_q_open = 0.0f;
    s_open_ramp = 0.0f;
    s_soft_cnt = 0;
    dq_vd_ref = 0.0f;
}

void ControlDq_SetUoTarget(float uo)
{
    s_Uo_target = clampf(uo, DQ_UO_MIN, DQ_UO_MAX);
}

void ControlDq_SetOutputFreq(float hz)
{
    dq_output_freq = clampf(hz, DQ_FREQ_MIN, DQ_FREQ_MAX);
    s_freq = dq_output_freq;
}

float ControlDq_GetOutputFreq(void)
{
    return dq_output_freq;
}

void ControlDq_OnStart(void)
{
    float target = s_Uo_target;
    float freq = dq_output_freq;

    ControlDq_ResetRuntime();
    s_Uo_target = target;
    ControlDq_SetOutputFreq(freq);
    s_Uo = 0.0f;
    s_soft_cnt = 0;
    s_open_ramp = 0.0f;
}

void ControlDq_OnFreqUp(void)
{
    ControlDq_SetOutputFreq(dq_output_freq + DQ_FREQ_KEY_STEP);
}

void ControlDq_OnFreqDown(void)
{
    ControlDq_SetOutputFreq(dq_output_freq - DQ_FREQ_KEY_STEP);
}

void ControlDq_Execute(const DqAdcSample *sample, float vdc)
{
#if (CTRL_MODE == CTRL_MODE_DQ_DROOP)
    s_sinwt = sinf(dq_theta);
    s_coswt = cosf(dq_theta);
    sample_to_dq(sample);
    DqPower_Update(dq_vd, dq_vq, dq_id, dq_iq);
    update_theta_droop();
#elif (CTRL_MODE == CTRL_MODE_DQ_VOLTAGE_PI) || (CTRL_MODE == CTRL_MODE_DQ_VOLT_CUR_PI) || (CTRL_MODE == CTRL_MODE_DQ_CUR_PI)
    update_theta_fixed();
    sample_to_dq(sample);
#else
    integrate_theta();
    sample_to_dq(sample);
#endif

#if (CTRL_MODE != CTRL_MODE_DQ_DROOP)
    DqPower_Update(dq_vd, dq_vq, dq_id, dq_iq);
#endif

#if (CTRL_MODE == CTRL_MODE_DQ_OPEN_LOOP)
    if (s_open_ramp < DQ_D_OPEN_TARGET) {
        s_open_ramp += DQ_D_OPEN_STEP;
        if (s_open_ramp > DQ_D_OPEN_TARGET) {
            s_open_ramp = DQ_D_OPEN_TARGET;
        }
    }
    s_v_inv.d = s_open_ramp;
    s_v_inv.q = s_q_open;
#else
    float inv_vdc = inv_vdc_pu(vdc);

#if (CTRL_MODE == CTRL_MODE_DQ_VOLTAGE_PI)
    run_voltage_pi(dq_vd_ref, 0.0f);
    s_v_inv.d = s_vd_pi.Out * inv_vdc;
    s_v_inv.q = s_vq_pi.Out * inv_vdc;
#elif (CTRL_MODE == CTRL_MODE_DQ_VOLT_CUR_PI) || (CTRL_MODE == CTRL_MODE_DQ_DROOP)
    run_voltage_pi(dq_vd_ref, 0.0f);
    run_current_pi(s_vd_pi.Out, s_vq_pi.Out, inv_vdc);
#elif (CTRL_MODE == CTRL_MODE_DQ_CUR_PI)
    /* Id参考=Uo(按键调节), Iq=0; 电流PI + 输出电压前馈vd/vq + 解耦 */
    run_current_pi(dq_vd_ref, 0.0f, inv_vdc);
#else
    s_v_inv.d = 0.0f;
    s_v_inv.q = 0.0f;
#endif
#endif

    apply_modulation_limits(&s_v_inv);
    DqTransform_InverseDqToAlphaBeta(&s_v_inv, s_sinwt, s_coswt);

    s_svpwm.Ualpha = s_v_inv.alpha;
    s_svpwm.Ubeta = s_v_inv.beta;
    DqSVGen_Run(&s_svpwm);

    dq_svpwm_ta = s_svpwm.Ta;
    dq_svpwm_tb = s_svpwm.Tb;
    dq_svpwm_tc = s_svpwm.Tc;

    Svpwm_modulation(dq_svpwm_ta, dq_svpwm_tb, dq_svpwm_tc);
}

void ControlDq_UpdateMeasurement(const DqAdcSample *sample)
{
    integrate_theta();
    dq_droop_freq = dq_output_freq;
    sample_to_dq(sample);
    DqPower_Update(dq_vd, dq_vq, dq_id, dq_iq);
}

void Svpwm_modulation(float ta, float tb, float tc)
{
    ta = clampf(ta, -DQ_MODULATION_LIMIT, DQ_MODULATION_LIMIT);
    tb = clampf(tb, -DQ_MODULATION_LIMIT, DQ_MODULATION_LIMIT);
    tc = clampf(tc, -DQ_MODULATION_LIMIT, DQ_MODULATION_LIMIT);

    dq_pwm_cmp_a = (uint32_t)(0.5f * (1.0f + ta) * (float)DQ_PWM_PERIOD);
    dq_pwm_cmp_b = (uint32_t)(0.5f * (1.0f + tb) * (float)DQ_PWM_PERIOD);
    dq_pwm_cmp_c = (uint32_t)(0.5f * (1.0f + tc) * (float)DQ_PWM_PERIOD);

    /* 先写 CCR, 再开输出, 避免 CCR=0 时互补管(PA7/PB0/PB1)被拉 high */
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, dq_pwm_cmp_a);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, dq_pwm_cmp_b);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, dq_pwm_cmp_c);
    pwm_outputs_start();
}

void ControlDq_PwmStop(void)
{
    dq_pwm_cmp_a = DQ_PWM_CMP_BLOCKED;
    dq_pwm_cmp_b = DQ_PWM_CMP_BLOCKED;
    dq_pwm_cmp_c = DQ_PWM_CMP_BLOCKED;

    pwm_outputs_stop();

    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, dq_pwm_cmp_a);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_2, dq_pwm_cmp_b);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, dq_pwm_cmp_c);
}
