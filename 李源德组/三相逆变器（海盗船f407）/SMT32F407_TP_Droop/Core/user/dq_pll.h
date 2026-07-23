#ifndef __DQ_PLL_H__
#define __DQ_PLL_H__

#include "control.h"

typedef struct {
    float sinwt;
    float coswt;
    float sin2wt;
    float cos2wt;
    float grid_init_freq;
    float grid_freq;
    float theta;
    float Ts;
    float LPF_k1;
    float LPF_k2;
    float LF_kp;
    float LF_ki;
    float LF_out;
    float LF_i;
    float ualpha;
    float ubeta;
    float d_p;
    float d_n;
    float q_p;
    float q_n;
    float d_p_decoupl;
    float d_n_decoupl;
    float q_p_decoupl;
    float q_n_decoupl;
    float d_p_decoupl_lpf;
    float d_n_decoupl_lpf;
    float q_p_decoupl_lpf;
    float q_n_decoupl_lpf;
} DqSPLL_3P_DDSRF;

void DqSPLL_Init(DqSPLL_3P_DDSRF *pll, float freq, float ts, float k1, float k2, float kp, float ki);
void DqSPLL_Clear(DqSPLL_3P_DDSRF *pll);
void DqSPLL_Run(DqSPLL_3P_DDSRF *pll, float va, float vb, float vc);

#endif /* __DQ_PLL_H__ */
