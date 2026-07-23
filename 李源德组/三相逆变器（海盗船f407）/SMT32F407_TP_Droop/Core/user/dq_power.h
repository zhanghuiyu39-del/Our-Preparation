#ifndef __DQ_POWER_H__
#define __DQ_POWER_H__

/* dq 坐标系三相功率: P = 1.5*(vd*id + vq*iq), Q = 1.5*(vq*id - vd*iq) */
#define DQ_PWR_LPF_NEW          0.01f
#define DQ_PWR_LPF_OLD          0.99f
#define DQ_PWR_THREE_PHASE      1.5f

extern float dq_P;
extern float dq_Q;

void DqPower_Init(void);
void DqPower_Reset(void);
void DqPower_Update(float vd, float vq, float id, float iq);

#endif /* __DQ_POWER_H__ */
