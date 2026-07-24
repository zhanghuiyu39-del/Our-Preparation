#ifndef __DQ_PID_H__
#define __DQ_PID_H__

typedef struct {
    float Ref;
    float Fbk;
    float Out;
    float Kp;
    float Ki;
    float Umax;
    float Umin;
    float up;
    float ui;
    float v1;
    float i1;
} DqPIControl;

void DqPI_Init(DqPIControl *pi, float kp, float ki, float umax, float umin);
void DqPI_Clear(DqPIControl *pi);
void DqPI_ClearIntegral(DqPIControl *pi);
void DqPI_Run(DqPIControl *pi);

#endif /* __DQ_PID_H__ */
