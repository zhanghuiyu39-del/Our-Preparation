#include "dq_transform.h"

void DqTransform_Clear(DqTransformState *st)
{
    st->AB = 0.0f;
    st->BC = 0.0f;
    st->CA = 0.0f;
    st->A = 0.0f;
    st->B = 0.0f;
    st->C = 0.0f;
    st->alpha = 0.0f;
    st->beta = 0.0f;
    st->d = 0.0f;
    st->q = 0.0f;
}

void DqTransform_LineToPhase(DqTransformState *st)
{
    st->A = st->AB * DQ_CONST_2D3 + st->BC * DQ_CONST_1D3;
    st->B = (st->BC - st->AB) * DQ_CONST_1D3;
    st->C = -st->A - st->B;
}

void DqTransform_AbcToAlphaBeta(DqTransformState *st)
{
    st->alpha = DQ_CONST_2D3 * (st->A - DQ_CONST_1D2 * st->B - DQ_CONST_1D2 * st->C);
    st->beta  = DQ_CONST_2D3 * (DQ_CONST_SQRT3D2 * st->B - DQ_CONST_SQRT3D2 * st->C);
}

void DqTransform_AlphaBetaToDq(DqTransformState *st, float sinwt, float coswt)
{
    st->d = st->alpha * coswt + st->beta * sinwt;
    st->q = -st->alpha * sinwt + st->beta * coswt;
}

void DqTransform_AbcToDq(DqTransformState *st, float sinwt, float coswt)
{
    DqTransform_AbcToAlphaBeta(st);
    DqTransform_AlphaBetaToDq(st, sinwt, coswt);
}

void DqTransform_InverseDqToAlphaBeta(DqTransformState *st, float sinwt, float coswt)
{
    st->alpha = coswt * st->d - sinwt * st->q;
    st->beta  = sinwt * st->d + coswt * st->q;
}

void DqTransform_InverseAlphaBetaToAbc(DqTransformState *st)
{
    st->A = st->alpha;
    st->C = -DQ_CONST_1D2 * st->alpha + DQ_CONST_SQRT3D2 * st->beta;
    st->B = -DQ_CONST_1D2 * st->alpha - DQ_CONST_SQRT3D2 * st->beta;
}
