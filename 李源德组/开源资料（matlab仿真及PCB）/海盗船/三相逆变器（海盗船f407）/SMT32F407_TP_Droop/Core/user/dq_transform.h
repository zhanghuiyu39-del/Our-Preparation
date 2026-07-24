#ifndef __DQ_TRANSFORM_H__
#define __DQ_TRANSFORM_H__

#include "main.h"

#define DQ_CONST_1D3      0.33333333f
#define DQ_CONST_2D3      0.66666667f
#define DQ_CONST_1D2      0.5f
#define DQ_CONST_SQRT3D2  0.86602540f

typedef struct {
    float AB;
    float BC;
    float CA;
    float A;
    float B;
    float C;
    float alpha;
    float beta;
    float d;
    float q;
} DqTransformState;

void DqTransform_Clear(DqTransformState *st);
void DqTransform_LineToPhase(DqTransformState *st);
void DqTransform_AbcToAlphaBeta(DqTransformState *st);
void DqTransform_AlphaBetaToDq(DqTransformState *st, float sinwt, float coswt);
void DqTransform_AbcToDq(DqTransformState *st, float sinwt, float coswt);
void DqTransform_InverseDqToAlphaBeta(DqTransformState *st, float sinwt, float coswt);
void DqTransform_InverseAlphaBetaToAbc(DqTransformState *st);

#endif /* __DQ_TRANSFORM_H__ */
