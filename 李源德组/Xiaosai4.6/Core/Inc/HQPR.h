#ifndef __HQPR_H__
#define __HQPR_H__


#include "stdint.h"
#include "math.h"


// QPR 控制器结构体（双环通用）
typedef struct
{
    float Kp;      // 比例系数
    float Kr;      // 谐振系数
    float wc;      // 谐振带宽
    float w0;      // 基波角频率 50Hz=314.16

    float err0;    // 当前误差
    float err1;    // 上一周期误差
    float err2;    // 上两周期误差

    float out0;    // 当前输出
    float out1;    // 上一周期输出
    float out2;    // 上两周期输出
} QPR_TypeDef;

// 函数声明
void QPR_Init(QPR_TypeDef *qpr, float Kp, float Kr, float wc, float w0);
float QPR_Calc(QPR_TypeDef *qpr, float ref, float fb, float Ts);

#endif




