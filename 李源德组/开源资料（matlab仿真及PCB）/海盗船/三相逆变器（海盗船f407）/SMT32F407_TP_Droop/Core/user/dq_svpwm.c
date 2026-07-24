#include "dq_svpwm.h"
#include "dq_transform.h"

void DqSVGen_Clear(DqSVGen *sv)
{
    sv->Ualpha = 0.0f;
    sv->Ubeta = 0.0f;
    sv->Ta = 0.0f;
    sv->Tb = 0.0f;
    sv->Tc = 0.0f;
    sv->tmp1 = 0.0f;
    sv->tmp2 = 0.0f;
    sv->tmp3 = 0.0f;
    sv->VecSector = 0;
}

void DqSVGen_Run(DqSVGen *sv)
{
    sv->tmp1 = sv->Ubeta;
    sv->tmp2 = sv->Ubeta * DQ_CONST_1D2 + (DQ_CONST_SQRT3D2 * sv->Ualpha);
    sv->tmp3 = sv->tmp2 - sv->tmp1;

    sv->VecSector = 3;
    sv->VecSector = (sv->tmp2 > 0.0f) ? (sv->VecSector - 1U) : sv->VecSector;
    sv->VecSector = (sv->tmp3 > 0.0f) ? (sv->VecSector - 1U) : sv->VecSector;
    sv->VecSector = (sv->tmp1 < 0.0f) ? (7U - sv->VecSector) : sv->VecSector;

    if (sv->VecSector == 1U || sv->VecSector == 4U) {
        sv->Ta = sv->tmp2;
        sv->Tb = sv->tmp1 - sv->tmp3;
        sv->Tc = -sv->tmp2;
    } else if (sv->VecSector == 2U || sv->VecSector == 5U) {
        sv->Ta = sv->tmp3 + sv->tmp2;
        sv->Tb = sv->tmp1;
        sv->Tc = -sv->tmp1;
    } else {
        sv->Ta = sv->tmp3;
        sv->Tb = -sv->tmp3;
        sv->Tc = -(sv->tmp1 + sv->tmp2);
    }
}
