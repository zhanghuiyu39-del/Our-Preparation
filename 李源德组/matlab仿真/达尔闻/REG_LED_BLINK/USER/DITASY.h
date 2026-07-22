/*
 * DITASY.h
 *
 *  Created on: 2026年3月27日
 *      Author: DITASY
 */

#ifndef DITASY_H_
#define DITASY_H_

#include "F28x_Project.h"

typedef struct
{
    uint16_t SCI_DATA[64];
    uint16_t ARR,FULL,OK;
} SCI_ARRAY;

extern SCI_ARRAY DI_SCIA_ARRAY;

typedef union {
    float f_data;       // 浮点数成员，占用4字节
    uint16_t c_data[2]; // 字符数组成员，占用4字节
} FTChar;

__interrupt void SCI_TXINT(void);
void DI_Send_Float(float Input1,float Input2);
void DI_Timer0_Init(float Freq,float Period);
void DI_EPWM1_Init();
void DI_EPWM2_SYNC_Init();
void DI_EPWM3_SYNC_Init();
void DI_EPWM4_SYNC1_Init();
void DI_EPWM5_SYNC_Init();
void DI_EPWM6_SYNC_Init();
void DI_EPWM7_SYNC1_Init();
void DI_EPWM8_SYNC_Init();
void DI_NEPWM9_SYNC_Init();
void DI_NEPWM10_SYNC1_Init();
void DI_NEPWM11_SYNC_Init();
void DI_NEPWM12_SYNC_Init();
void DI_SCIA_Init();
void SCIA_TRANSMIT_ARRAY();




#endif /* USER_DITASY_H_ */
