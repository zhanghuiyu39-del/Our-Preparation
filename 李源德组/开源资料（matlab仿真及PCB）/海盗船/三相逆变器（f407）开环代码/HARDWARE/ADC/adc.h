#ifndef __ADC_H
#define __ADC_H	
#include "sys.h" 

#define SAMPLE_COUNT 10 // 定义采样数量 					



void Dma_Config(void);
void Dma_Init(void);
void ADC123_Init(void);                // ADC通道初始化
void  Adc2_Init(void);
float adc_dma(unsigned int j,unsigned int count);
u16  Get_Adc(u8 ch); 				//获得某个通道值 
u16 Get_Adc_Average(u8 ch,u8 times);//得到某个通道给定次数采样的平均值  
void ADC1_DMA_Configuration(void);

#endif 







