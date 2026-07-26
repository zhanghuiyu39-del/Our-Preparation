#ifndef __SPWM_H
#define __SPWM_H
#include "sys.h"

void TIM1_SPWM_Init(u32 arr, u32 psc);
float u_rms(float x);
#endif
