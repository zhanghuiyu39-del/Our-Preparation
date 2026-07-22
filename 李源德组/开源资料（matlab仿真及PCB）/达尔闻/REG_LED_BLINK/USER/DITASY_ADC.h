/*
 * DITASY_ADC.h
 *
 *  Created on: 2026年3月31日
 *      Author: DITASY
 */

#ifndef DITASY_ADC_H_
#define DITASY_ADC_H_

#include "F28x_Project.h"

__interrupt void ADCA_INIT_HANDLE(void);//ADCA中断服务函数
void DI_ADCA_Init();
void DI_ADCB_Init();
void DI_ADCD_Init();
void DI_CMPSS1_Init();
void DI_CMPSS_CLR();

#endif /* USER_DITASY_ADC_H_ */
