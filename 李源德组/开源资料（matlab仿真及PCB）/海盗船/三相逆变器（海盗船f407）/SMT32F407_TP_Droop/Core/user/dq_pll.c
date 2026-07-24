#include "dq_pll.h"
#include "dq_transform.h"
#include <math.h>

void DqSPLL_Init(DqSPLL_3P_DDSRF *pll, float freq, float ts, float k1, float k2, float kp, float ki)
{
    DqSPLL_Clear(pll);
    pll->grid_init_freq = freq;
    pll->Ts = ts;
    pll->LPF_k1 = k1;
    pll->LPF_k2 = k2;
    pll->LF_kp = kp;
    pll->LF_ki = ki;
}

void DqSPLL_Clear(DqSPLL_3P_DDSRF *pll)
{
    pll->sinwt = 0.0f;
    pll->coswt = 0.0f;
    pll->sin2wt = 0.0f;
    pll->cos2wt = 0.0f;
    pll->grid_init_freq = 0.0f;
    pll->grid_freq = 0.0f;
    pll->theta = 0.0f;
    pll->Ts = 0.0f;
    pll->LPF_k1 = 0.0f;
    pll->LPF_k2 = 0.0f;
    pll->LF_kp = 0.0f;
    pll->LF_ki = 0.0f;
    pll->LF_out = 0.0f;
    pll->LF_i = 0.0f;
    pll->ualpha = 0.0f;
    pll->ubeta = 0.0f;
    pll->d_p = 0.0f;
    pll->d_n = 0.0f;
    pll->q_p = 0.0f;
    pll->q_n = 0.0f;
    pll->d_p_decoupl = 0.0f;
    pll->d_n_decoupl = 0.0f;
    pll->q_p_decoupl = 0.0f;
    pll->q_n_decoupl = 0.0f;
    pll->d_p_decoupl_lpf = 0.0f;
    pll->d_n_decoupl_lpf = 0.0f;
    pll->q_p_decoupl_lpf = 0.0f;
    pll->q_n_decoupl_lpf = 0.0f;
}

void DqSPLL_Run(DqSPLL_3P_DDSRF *pll, float va, float vb, float vc)
{
    pll->sinwt = sinf(pll->theta);
    pll->coswt = cosf(pll->theta);
    pll->sin2wt = sinf(pll->theta * 2.0f);
    pll->cos2wt = cosf(pll->theta * 2.0f);

    pll->ualpha = DQ_CONST_2D3 * (va - DQ_CONST_1D2 * vb - DQ_CONST_1D2 * vc);
    pll->ubeta  = DQ_CONST_2D3 * (DQ_CONST_SQRT3D2 * vb - DQ_CONST_SQRT3D2 * vc);

    pll->d_p = pll->ualpha * pll->coswt + pll->ubeta * pll->sinwt;
    pll->d_n = pll->ualpha * pll->coswt - pll->ubeta * pll->sinwt;
    pll->q_p = -pll->ualpha * pll->sinwt + pll->ubeta * pll->coswt;
    pll->q_n = pll->ualpha * pll->sinwt + pll->ubeta * pll->coswt;

    pll->d_p_decoupl = pll->d_p - pll->d_n_decoupl_lpf * pll->cos2wt - pll->q_n_decoupl_lpf * pll->sin2wt;
    pll->d_n_decoupl = pll->d_n - pll->d_p_decoupl_lpf * pll->cos2wt + pll->q_p_decoupl_lpf * pll->sin2wt;
    pll->q_p_decoupl = pll->q_p + pll->d_n_decoupl_lpf * pll->sin2wt - pll->q_n_decoupl_lpf * pll->cos2wt;
    pll->q_n_decoupl = pll->q_n - pll->d_p_decoupl_lpf * pll->sin2wt - pll->q_p_decoupl_lpf * pll->cos2wt;

    pll->d_p_decoupl_lpf = pll->d_p_decoupl_lpf * pll->LPF_k1 + pll->d_p_decoupl * pll->LPF_k2;
    pll->d_n_decoupl_lpf = pll->d_n_decoupl_lpf * pll->LPF_k1 + pll->d_n_decoupl * pll->LPF_k2;
    pll->q_p_decoupl_lpf = pll->q_p_decoupl_lpf * pll->LPF_k1 + pll->q_p_decoupl * pll->LPF_k2;
    pll->q_n_decoupl_lpf = pll->q_n_decoupl_lpf * pll->LPF_k1 + pll->q_n_decoupl * pll->LPF_k2;

    pll->LF_i += -pll->q_p_decoupl * pll->LF_ki;
    pll->LF_out = -pll->q_p_decoupl * pll->LF_kp + pll->LF_i;

    pll->grid_freq = pll->grid_init_freq + pll->LF_out;

    pll->theta += 2.0f * PI * pll->Ts * pll->grid_freq;
    if (pll->theta >= PI) {
        pll->theta -= 2.0f * PI;
    }
    if (pll->theta <= -PI) {
        pll->theta += 2.0f * PI;
    }
}
