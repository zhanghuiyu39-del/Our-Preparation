#ifndef __VOFA_H__
#define __VOFA_H__

#include "main.h"

#ifndef VOFA_CH_COUNT
#define VOFA_CH_COUNT  4
#endif

void Vofa_Init(void);
void Vofa_PushISR(const float *channels);
void Vofa_Send(void);

#endif /* __VOFA_H__ */
