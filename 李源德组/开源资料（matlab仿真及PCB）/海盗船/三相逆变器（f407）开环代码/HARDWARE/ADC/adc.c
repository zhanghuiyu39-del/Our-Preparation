#include "adc.h"
#include "delay.h"		 

volatile uint16_t ADC_ConvertedValue[10][7];		//ADC_ConvertedValue[0~9][0]储存了电压数据	

void Dma_Init(void)
{
    DMA_InitTypeDef DMA_InitStructure;
 
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE); 
	
	  DMA_InitStructure.DMA_PeripheralBaseAddr = ((u32)ADC1+0x4c);	
  
	  DMA_InitStructure.DMA_Memory0BaseAddr = (u32)ADC_ConvertedValue;  
	  DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;	
	  DMA_InitStructure.DMA_BufferSize = 70;	
	  DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	  DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; 
	  DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; 
	  DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;	
	  DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	  DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;  
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;  
    DMA_InitStructure.DMA_Channel = DMA_Channel_0; 
	  DMA_Init(DMA2_Stream0, &DMA_InitStructure);
    DMA_Cmd(DMA2_Stream0, ENABLE);
}

//void Dma_Config(void)
//{
//    DMA_InitTypeDef DMA_InitStructure;

//    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE); // 使能DMA2时钟

//    // ADC1配置
//    DMA_DeInit(DMA2_Stream0); // 将DMA流0寄存器设为默认值
//    DMA_InitStructure.DMA_Channel = DMA_Channel_0; // 选择DMA通道（这里使用流0）
//    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(ADC1->DR); // 设置外设基地址为ADC1数据寄存器
//    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Voltage[0][0]; // 设置存储ADC1转换结果的内存缓冲区地址
//    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory; // 配置DMA传输方向（从外设到内存）
//    DMA_InitStructure.DMA_BufferSize = SAMPLE_COUNT; // 设置要传输的数据项数量
//    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 禁用外设地址递增模式
//    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; // 启用内存地址递增模式
//    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 设置外设数据大小为16位（半字）
//    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; // 设置内存数据大小为16位（半字）
//    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular; // 设置DMA工作模式为循环模式
//    DMA_InitStructure.DMA_Priority = DMA_Priority_High; // 设置DMA优先级为高
//    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable; // 禁用FIFO模式
//    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull; // 设置FIFO阈值为半满
//    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single; // 设置内存突发为单次传输
//    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single; // 设置外设突发为单次传输
//    DMA_Init(DMA2_Stream0, &DMA_InitStructure); // 使用指定的配置初始化DMA流
//    DMA_Cmd(DMA2_Stream0, ENABLE); // 使能DMA流

//    // ADC2配置
//    DMA_DeInit(DMA2_Stream1); // 将DMA流1寄存器设为默认值
//    DMA_InitStructure.DMA_Channel = DMA_Channel_1; // 选择DMA通道（这里使用流1）
//    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(ADC2->DR); // 设置外设基地址为ADC2数据寄存器
//    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Voltage[0][1]; // 设置存储ADC2转换结果的内存缓冲区地址
//    // 其他配置参数同上（不再赘述）

//    DMA_Init(DMA2_Stream1, &DMA_InitStructure); // 使用指定的配置初始化DMA流
//    DMA_Cmd(DMA2_Stream1, ENABLE); // 使能DMA流

//    // ADC3配置
//    DMA_DeInit(DMA2_Stream2); // 将DMA流2寄存器设为默认值
//    DMA_InitStructure.DMA_Channel = DMA_Channel_2; // 选择DMA通道（这里使用流2）
//    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&(ADC3->DR); // 设置外设基地址为ADC3数据寄存器
//    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&Voltage[0][2]; // 设置存储ADC3转换结果的内存缓冲区地址
//    // 其他配置参数同上（不再赘述）

//    DMA_Init(DMA2_Stream2, &DMA_InitStructure); // 使用指定的配置初始化DMA流
//    DMA_Cmd(DMA2_Stream2, ENABLE); // 使能DMA流
//}

void ADC123_Init(void) 
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;
    ADC_CommonInitTypeDef ADC_CommonInitStruct;

    /* 使能GPIOA和GPIOC的时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);

    /* 使能ADC1、ADC2和ADC3的时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2 | RCC_APB2Periph_ADC3, ENABLE);

    /* 配置PA0和PA1为模拟输入模式 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3| GPIO_Pin_5;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置PC0和PC1为模拟输入模式 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_3;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ADC1和ADC2共同配置 */
    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div2; // 假设PCLK2经过4分频，设置ADC时钟频率为84MHz/4=21MHz
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; // 禁止DMA访问模式
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles; // 配置两个采样阶段之间的延迟为20个时钟周期
    ADC_CommonInit(&ADC_CommonInitStruct);

    /* ADC1和ADC2配置 */
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b; // 设置ADC分辨率为12位
    ADC_InitStruct.ADC_ScanConvMode = ENABLE; // 开启扫描模式
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE; // 开启连续转换模式
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 禁止外部触发转换
 //   ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_TRGO; // 使用定时器2触发转换
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right; // 数据右对齐
    ADC_InitStruct.ADC_NbrOfConversion = 1; // 每个ADC的转换个数为1
    ADC_Init(ADC1, &ADC_InitStruct); // 配置ADC1
    ADC_Init(ADC2, &ADC_InitStruct); // 配置ADC2
    ADC_Init(ADC3, &ADC_InitStruct); // 配置ADC3

    /* ADC1常规通道配置 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_3Cycles); // ADC1常规通道配置为PA0
    /* ADC2常规通道配置 */
    ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 1, ADC_SampleTime_3Cycles); // ADC2常规通道配置为PA1
    /* ADC3常规通道配置 */
    ADC_RegularChannelConfig(ADC3, ADC_Channel_13, 1, ADC_SampleTime_3Cycles); // ADC3常规通道配置为PA3
 // ADC_RegularChannelConfig(ADC1, ADC_Channel_4, 1, ADC_SampleTime_3Cycles); // ADC1常规通道配置为PA0
   
	 /* 使能ADC1、ADC2和ADC3 */
    ADC_Cmd(ADC1, ENABLE);
    ADC_Cmd(ADC2, ENABLE);
    ADC_Cmd(ADC3, ENABLE);

    /* 触发第一次转换 */
    ADC_SoftwareStartConv(ADC1);
    ADC_SoftwareStartConv(ADC2);
    ADC_SoftwareStartConv(ADC3);
}

//获得ADC值
//ch: @ref ADC_channels 
//通道值 0~16取值范围为：ADC_Channel_0~ADC_Channel_16
//返回值:转换结果
u16 Get_Adc(u8 ch)   
{
	  	//设置指定ADC的规则组通道，一个序列，采样时间
	ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_480Cycles );	//ADC1,ADC通道,480个周期,提高采样时间可以提高精确度			    
  
	ADC_SoftwareStartConv(ADC1);		//使能指定的ADC1的软件转换启动功能	
	 
	while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC ));//等待转换结束

	return ADC_GetConversionValue(ADC1);	//返回最近一次ADC1规则组的转换结果
}
//获取通道ch的转换值，取times次,然后平均 
//ch:通道编号
//times:获取次数
//返回值:通道ch的times次转换结果平均值
u16 Get_Adc_Average(u8 ch,u8 times)
{
	u32 temp_val=0;
	u8 t;
	for(t=0;t<times;t++)
	{
		temp_val+=Get_Adc(ch);
		delay_ms(5);
	}
	return temp_val/times;
} 
void  Adc2_Init(void)
{    
   GPIO_InitTypeDef  GPIO_InitStructure;
	  ADC_CommonInitTypeDef ADC_CommonInitStructure;
	  ADC_InitTypeDef       ADC_InitStructure;
	
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);
	  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); 
 
    GPIO_InitStructure.GPIO_Pin =GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
	
//	GPIO_InitStructure.GPIO_Pin =GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AN;
//    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
//    GPIO_Init(GPIOC, &GPIO_InitStructure);
 
 
	  RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,ENABLE);	  
	  RCC_APB2PeriphResetCmd(RCC_APB2Periph_ADC1,DISABLE);	 
 
    ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_5Cycles;
    ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; 
    ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;
    ADC_CommonInit(&ADC_CommonInitStructure);
	
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 7;
    ADC_Init(ADC1, &ADC_InitStructure);
	  ADC_Cmd(ADC1, ENABLE);
 
    //PA0->PA5
        ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_15Cycles );
		ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_15Cycles );
		ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 3, ADC_SampleTime_15Cycles );
		ADC_RegularChannelConfig(ADC1, ADC_Channel_3, 4, ADC_SampleTime_15Cycles );

		ADC_DMARequestAfterLastTransferCmd(ADC1, ENABLE);
		ADC_DMACmd(ADC1, ENABLE);
		ADC_SoftwareStartConv(ADC1);	 	
}
float adc_dma(unsigned int j,unsigned int count)
{
	u16 s=0;
    u16 i=0;
  for(i=0;i<count;i++)
	{
	  s=s+ADC_ConvertedValue[i][j];
	}
	return s*0.1f;
}

uint16_t adc1_buffer1[1]; // 缓冲区1，用于存储ADC1通道0的转换结果
uint16_t adc1_buffer2[1]; // 缓冲区2，用于存储ADC1通道2的转换结果

void ADC1_DMA_Configuration(void) {
    // 使能ADC1和DMA2的时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2, ENABLE);

    // 初始化DMA控制结构体
    DMA_InitTypeDef DMA_InitStructure;
    DMA_InitStructure.DMA_Channel = DMA_Channel_0;
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&ADC1->DR);
    DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)&adc1_buffer1[0];
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;
    DMA_InitStructure.DMA_BufferSize = 1;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
    DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;
    DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
    DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
    DMA_Init(DMA2_Stream0, &DMA_InitStructure);

    // 配置ADC1
    ADC_InitTypeDef ADC_InitStructure;
    ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE; // 扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE; // 连续转换模式
    ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 无外部触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfConversion = 2; // 转换两个通道
    ADC_Init(ADC1, &ADC_InitStructure);

    // 配置ADC1的通道0和通道2
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_3Cycles);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_3Cycles);

    // 使能ADC1的DMA功能
    ADC_DMACmd(ADC1, ENABLE);

    // 使能DMA流
    DMA_Cmd(DMA2_Stream0, ENABLE);

    // 使能ADC1
    ADC_Cmd(ADC1, ENABLE);

    // 启动ADC1软件转换
    ADC_SoftwareStartConv(ADC1);
}



	 









