#include "dq_pid.h"

void DqPI_Init(DqPIControl *pi, float kp, float ki, float umax, float umin)
{
    pi->Kp = kp;
    pi->Ki = ki;
    pi->Umax = umax;
    pi->Umin = umin;
    pi->Ref = 0.0f;
    pi->Fbk = 0.0f;
    pi->Out = 0.0f;
    pi->up = 0.0f;
    pi->ui = 0.0f;
    pi->v1 = 0.0f;
    pi->i1 = 0.0f;
}

void DqPI_Clear(DqPIControl *pi)
{
    DqPI_Init(pi, pi->Kp, pi->Ki, pi->Umax, pi->Umin);
}

void DqPI_ClearIntegral(DqPIControl *pi)
{
    pi->i1 = 0.0f;
    pi->ui = 0.0f;
}

void DqPI_Run(DqPIControl *pi)
{
    pi->up = pi->Kp * (pi->Ref - pi->Fbk);
    pi->ui = (pi->Out == pi->v1) ? (pi->Ki * pi->up + pi->i1) : pi->i1;
    pi->i1 = pi->ui;
    pi->v1 = pi->up + pi->ui;
    pi->Out = pi->v1;

    if (pi->Out > pi->Umax) {
        pi->Out = pi->Umax;
    }
    if (pi->Out < pi->Umin) {
        pi->Out = pi->Umin;
    }
}
