#include "dq_power.h"

float dq_P = 0.0f;
float dq_Q = 0.0f;

void DqPower_Init(void)
{
    DqPower_Reset();
}

void DqPower_Reset(void)
{
    dq_P = 0.0f;
    dq_Q = 0.0f;
}

void DqPower_Update(float vd, float vq, float id, float iq)
{
    float p_inst = DQ_PWR_THREE_PHASE * (vd * id + vq * iq);
    float q_inst = DQ_PWR_THREE_PHASE * (vq * id - vd * iq);

    dq_P = dq_P * DQ_PWR_LPF_OLD + DQ_PWR_LPF_NEW * p_inst;
    dq_Q = dq_Q * DQ_PWR_LPF_OLD + DQ_PWR_LPF_NEW * q_inst;
}
