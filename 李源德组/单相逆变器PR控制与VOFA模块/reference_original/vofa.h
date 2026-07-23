#ifndef __VOFA_H__
#define __VOFA_H__

#include "main.h"

void Vofa_Init(void);
void Vofa_PushISR(float ch0, float ch1, float ch2);
void Vofa_Send(void);

#endif /* __VOFA_H__ */
