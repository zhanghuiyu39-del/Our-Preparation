#ifndef __CONTROL_H__
#define __CONTROL_H__

#include "main.h"
#include <math.h>

#ifndef PI
#define PI 3.141592653589793f
#endif

/* ── 控制模式 (必须用 #define 数值, 供 #if 预处理器使用, enum 无效) ── */
#define CTRL_MODE_OPEN_LOOP     0   /* 开环: ut = M*sin(θ) */
#define CTRL_MODE_VOLTAGE_TUNE  1   /* 仅电压PR: ut = PR_U(Uref-Uo)/Vdc */
#define CTRL_MODE_CURRENT_TUNE  2   /* 仅电流PR */
#define CTRL_MODE_DUAL_PR_LOOP  3   /* 电压PR + 电流PR 双环 */

#define CTRL_MODE               



/* 期望电压峰值 (V) */
#define UREF_AMP_DEFAULT    (24.2f * 1.414f)
#define UREF_AMP_MIN        2.0f
#define UREF_AMP_MAX        40.0f
#define UREF_AMP_KEY_STEP   1.0f        /* PE3升/PE2降, 每按一次 ±1V 峰值 */
#define UREF_FREQ_HZ        50.0f
#define PWM_FREQ_HZ         20000.0f
#define CTRL_TS             (1.0f / PWM_FREQ_HZ)

#define LOAD_R_OHM          20.0f

#define SOFT_PERIOD_SAMPLES ((uint16_t)(PWM_FREQ_HZ / UREF_FREQ_HZ))
#define SOFT_AMP_STEP       0.7f

/* 开环调制 (ut = open_mod_amp * sin(θ), 范围 ±OPEN_MOD_TARGET) */
#define OPEN_MOD_TARGET     0.50f       /* 目标调制比峰值, 可调 0~0.9 */
#define OPEN_MOD_STEP       0.02f       /* 软启每工频周期递增量 */

/* 电压PR (C=9.4uF, fs=20kHz) */
#define PR_U_KP   0.1f
#define PR_U_KR   100.0f

/* 电流PR (双环/电流单环用) */
#define PR_I_KP   0.25f
#define PR_I_KR   5.0f

#define PR_WC     (PI)
#define PR_WO     (2.0f * PI * UREF_FREQ_HZ)

/* PR 电压误差极性: +1 为 uref-uo (负反馈标准); 若 uo 与 uref 反相则改为 -1 */
#define PR_U_ERROR_SIGN     -1.0f

#define KEY_DEBOUNCE_MIN_SAMPLES   40u    /* 2ms @20kHz, 按下最短有效时间 */
#define KEY_DEBOUNCE_MAX_SAMPLES   40000u /* >2s 视为误触忽略 */

typedef struct {
    float gain;
    float B0;
    float B1;
    float B2;
    float A1;
    float A2;
} DIS_2ORDER_TF_COEF_DEF;

typedef struct {
    float output;
    float w0;
    float w1;
    float w2;
} DIS_2ORDER_TF_DATA_DEF;

typedef struct {
    float T;
    float Kp;
    float Kr;
    float Wo;
    float Wc;
    DIS_2ORDER_TF_COEF_DEF PR_coeff;
    DIS_2ORDER_TF_DATA_DEF PR_data;
} DIS_2ORDER_PR_DEF;

extern uint32_t pulse_a;
extern uint32_t pulse_b;

extern float pr_iref;
extern float pr_vout;
extern float uref_inst;
extern float uref_amp;
extern float uref_amp_target;   /* 期望电压峰值, PE2/PE3 可调 */
extern float open_mod_amp;    /* 开环当前调制比峰值 */
extern float ut_cmd;          /* 开环/闭环输出的 ut 指令(限幅前) */

void Control_Init(void);
void Control_UpdateKeys(void);
uint8_t Control_IsEnabled(void);
void Control_Execute(float il, float uo, float vdc, float *ut_out);

void PR_Init(DIS_2ORDER_PR_DEF *PR_obj, float Kp, float Kr, float Wc, float Wo, float T);
float discrete_2order_tf(float input, DIS_2ORDER_TF_COEF_DEF *coeff, DIS_2ORDER_TF_DATA_DEF *data);
void Unipolar_modulation(float ut);

#endif /* __CONTROL_H__ */
