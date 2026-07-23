#ifndef __DQ_SVPWM_H__
#define __DQ_SVPWM_H__

typedef struct {
    float Ualpha;
    float Ubeta;
    float Ta;
    float Tb;
    float Tc;
    float tmp1;
    float tmp2;
    float tmp3;
    unsigned int VecSector;
} DqSVGen;


void DqSVGen_Clear(DqSVGen *sv);
void DqSVGen_Run(DqSVGen *sv);

#endif /* __DQ_SVPWM_H__ */
