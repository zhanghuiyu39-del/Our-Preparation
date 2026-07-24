#ifndef __CONTROL_H__
#define __CONTROL_H__

#include "main.h"
#include <math.h>

#ifndef PI
#define PI 3.141592653589793f
#endif

/* 控制模式 (供 #if 预处理器使用) *输入电压都是60v，先跑开环/

#define CTRL_MODE_DQ_OPEN_LOOP      0   /* 三相dq开环 SVPWM + 输出线电压 Uab=20v */
#define CTRL_MODE_DQ_VOLTAGE_PI     1   /* 三相dq电压环PI + SVPWM+ 输出线电压 Uab=14.58v*/
#define CTRL_MODE_DQ_VOLT_CUR_PI    2   /* 三相dq电压+电流双环PI + 输出线电压 Uac=32v 推荐使用 */
#define CTRL_MODE_DQ_DROOP          3   /* 三相dq下垂 (P-f/Q-V) + 双环PI+ 输出线电压 Uab=32v */
#define CTRL_MODE_DQ_CUR_PI         4   /* 三相dq单电流环 + 输出电压前馈+ 输出线电压 Uab=38v，单电流环波形不太好看，可以自己细调参数，离网模式不建议使用 */

#define CTRL_MODE                   CTRL_MODE_DQ_OPEN_LOOP

#define UREF_FREQ_HZ        50.0f
#define PWM_FREQ_HZ         20000.0f
#define CTRL_TS             (1.0f / PWM_FREQ_HZ)

/* 输出频率 (按键调节) */
#define DQ_FREQ_DEFAULT     49.5f //目标值50，补偿误差0.5
#define DQ_FREQ_MIN         20.0f
#define DQ_FREQ_MAX         100.0f
#define DQ_FREQ_KEY_STEP    1.0f

/* dq 电压/电流环参数 */
#define DQ_UO_DEFAULT       26.0f   /* 空载 d 轴电压基准 Uo (V) */
#define DQ_UO_MIN           2.0f
#define DQ_UO_MAX           80.0f
#define DQ_UO_SOFT_STEP     1.0f
#define DQ_D_OPEN_TARGET    0.50f
#define DQ_D_OPEN_STEP      0.0002f

/*
 * 下垂控制 (P-f / Q-V), 额定: 输入 180W (60V×3A)
 *
 *   f_droop = f0 + m * (Pref - P)     [Hz],  m 单位 Hz/W
 *   Vd_ref  = Uo + n * (Qref - Q)     [V],   n 单位 V/var
 *
 * 推荐按下垂百分比计算:
 *   m = kf * f0 / P_rated
 *   n = kv * Uo  / Q_rated
 *
 * kf: 满载(P=P_rated)相对 f0 的频率下垂比例, 典型 1%~5%
 * kv: 满载(Q=Q_rated)相对 Uo 的电压下垂比例, 典型 2%~5%
 */
#define DQ_PRATED_W             180.0f
#define DQ_QRATED_VAR           180.0f
#define DQ_DROOP_KF             0.0f   /* 3% P-f 下垂 @ P_rated */
#define DQ_DROOP_KV             0.0f   /* 3% Q-V 下垂 @ Q_rated */
#define DQ_PREF_DEFAULT         90.0f   /* 有功参考 (W), P=Pref 时 f≈f0 */
#define DQ_QREF_DEFAULT         0.0f    /* 无功参考 (var) */
#define DQ_DROOP_M_HZ_PER_W     (DQ_DROOP_KF * DQ_FREQ_DEFAULT / DQ_PRATED_W)
#define DQ_DROOP_N_V_PER_VAR    (DQ_DROOP_KV * DQ_UO_DEFAULT / DQ_QRATED_VAR)

#define DQ_VD_KP            0.075f
#define DQ_VD_KI            0.0015f
#define DQ_VQ_KP            0.075f
#define DQ_VQ_KI            0.0015f
#define DQ_ID_KP            10.5f
#define DQ_ID_KI            0.0008f
#define DQ_IQ_KP            10.5f
#define DQ_IQ_KI            0.0008f

#define KEY_DEBOUNCE_MIN_SAMPLES   40u
#define KEY_DEBOUNCE_MAX_SAMPLES   40000u


void Control_Init(void);
void Control_UpdateKeys(void);
uint8_t Control_IsEnabled(void);
void Control_PwmStop(void);

#endif /* __CONTROL_H__ */
