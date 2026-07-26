#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"
#include "pinmux.h"

extern u16 ARR;//定时器频率


void PWM_Pins_Init(void) {

    GPIO_InitTypeDef GPIO_InitStruct;
    
    /* Clock enable for GPIOE */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);
    /* Connect pins to Alternate function Tim1 */
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource12, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource13, GPIO_AF_TIM1);

    /* GPIOE Configuration: Channel 1, 2, 3 and 1N, 2N, 3N as alternate function push-pull */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9  | GPIO_Pin_10 |
    						  GPIO_Pin_11 | GPIO_Pin_12 | GPIO_Pin_13;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOE, &GPIO_InitStruct);


    /* For debugging, watching calculation time respectively */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOD, ENABLE);
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Irgendwas hab ich aber vergessen */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB, ENABLE);
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);


/*	// Output Internal Clock Frequency on MCO1 (Pin A8)

    //	Enable GPIOs clocks
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    GPIO_PinAFConfig(GPIOA, GPIO_PinSource8, GPIO_AF_MCO);

    //	Configure MCO (PA8)
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    RCC_MCO1Config(RCC_MCO1Source_PLLCLK, RCC_MCO1Div_4);
*/

}


/*********************************************************************
 *
 *					Timer1 for 16kHz PWM
 *
 ********************************************************************/

void TIMER_Init(void) {

    TIM_TimeBaseInitTypeDef TIM_BaseStruct;

    /* Enable clock for TIM1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);

    /* Time Base configuration for 168 MHz Clock */
    TIM_BaseStruct.TIM_Prescaler = 0;
    TIM_BaseStruct.TIM_CounterMode = TIM_CounterMode_CenterAligned1;
    TIM_BaseStruct.TIM_Period = ARR;		// Reset Value = Auto-Reload-Register
    TIM_BaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_BaseStruct.TIM_RepetitionCounter = 1; // Update event occurs after downcounting underflow

     /* Initialize TIM1 */
    TIM_TimeBaseInit(TIM1, &TIM_BaseStruct);

    /* Tim1 enable counter */
    TIM_Cmd(TIM1, ENABLE);
}


/*********************************************************************
 *
 *					PWM pins with Tim1 and Deadtime
 *
 ********************************************************************/

void SVPWM_Init(void) {

    TIM_OCInitTypeDef TIM_OCStruct;
    TIM_BDTRInitTypeDef TIM_BDTRStruct;

    /* PWM mode 2 = Set on compare match */
    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_OutputNState = TIM_OutputNState_Enable;
    TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCStruct.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCStruct.TIM_OCNIdleState = TIM_OCIdleState_Set;
    TIM_OCStruct.TIM_Pulse = 0;		// duty cycle, initialize all with 0

    /* Channel 1 alias E8 & E9 */
    TIM_OC1Init(TIM1, &TIM_OCStruct);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* Channel 2 alias E10 & E11 */
    TIM_OC2Init(TIM1, &TIM_OCStruct);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* Channel 3 alias E12 & E13 */
    TIM_OC3Init(TIM1, &TIM_OCStruct);
    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /* Automatic Output enable, Break, dead time and lock configuration
    *  For dead time calculation please see reference manual */
    TIM_BDTRStruct.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRStruct.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRStruct.TIM_LOCKLevel = TIM_LOCKLevel_1;
	TIM_BDTRStruct.TIM_DeadTime = 0;
    TIM_BDTRStruct.TIM_Break = TIM_Break_Enable;
    TIM_BDTRStruct.TIM_BreakPolarity = TIM_BreakPolarity_High;
    TIM_BDTRStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;

    TIM_BDTRConfig(TIM1, &TIM_BDTRStruct);

    /* Main Output enable */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}


/*********************************************************************
 *
 *					Timer2 as Interrupt
 *
 ********************************************************************/

void INT_TIM2_Config(void) {

	NVIC_InitTypeDef NVIC_InitStruct;
	TIM_TimeBaseInitTypeDef TIM_BaseStruct;

	/* Enable the TIM2 global Interrupt */
	NVIC_InitStruct.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStruct);

	/* TIM2 clock enable */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	/* Time base configuration */
	TIM_BaseStruct.TIM_Prescaler = 0;
	TIM_BaseStruct.TIM_Period = ARR;  // 84 MHz down to 16 KHz
	TIM_BaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_BaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM2, &TIM_BaseStruct);

	/* TIM IT enable */
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

	/* TIM2 enable counter */
	TIM_Cmd(TIM2, ENABLE);
}


/*********************************************************************
 *
 *					Resolver & Interrupt Pin C8 for Z
 *
 ********************************************************************/

void configureEncoder(void) {

    GPIO_InitTypeDef GPIO_InitStruct;
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStruct;
    TIM_ICInitTypeDef TIM_ICInitStruct;
    TIM_TimeBaseInitTypeDef  TIM_TimeBaseStruct;

    /*******************************************
     *	Resolver
     *******************************************/

    /* Clock enable for GPIOC */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);

    /* GPIOC Encoder Configuration */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_6| GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStruct);
    /* Alternate Function */
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource6, GPIO_AF_TIM8);
    GPIO_PinAFConfig(GPIOC, GPIO_PinSource7, GPIO_AF_TIM8);

    /* Configure Timer */
    TIM_TimeBaseStruct.TIM_Prescaler = 0;
    TIM_TimeBaseStruct.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStruct.TIM_Period = 4095;
    TIM_TimeBaseStruct.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStruct);
    TIM_ARRPreloadConfig(TIM8, ENABLE);


    /* Debounce filter */
    TIM_ICInitStruct.TIM_Channel=TIM_Channel_1;
    TIM_ICInitStruct.TIM_ICFilter=3;
    TIM_ICInit(TIM8, &TIM_ICInitStruct);
    TIM_ICInitStruct.TIM_Channel=TIM_Channel_2;
    TIM_ICInitStruct.TIM_ICFilter=3;
    TIM_ICInit(TIM8, &TIM_ICInitStruct);

    /* Setup encoder */
    TIM_EncoderInterfaceConfig(TIM8, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
    /* Tim8 counter enable */
    TIM_Cmd(TIM8, ENABLE);

    /********************************************
     *	Reset Pin Z
     ********************************************/

    /* Enable clock for SYSCFG */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

    /* GPIOC Input Interrupt Configuration */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Tell system that you will use PC8 for EXTI_Line8 */
    SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource8);


    /* PC8 connected to EXTI_Line8 */
    EXTI_InitStruct.EXTI_Line = EXTI_Line8;
    /* Enable interrupt */
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    /* Interrupt mode */
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    /* Triggers on rising edge */
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
    /* Add to EXTI */
    EXTI_Init(&EXTI_InitStruct);


    /* Add IRQ vector to NVIC */
    /* PC8 is connected to EXTI_Line8, which has EXTI9_5_IRQn vector */
    NVIC_InitStruct.NVIC_IRQChannel = EXTI9_5_IRQn;
    NVIC_InitStruct.NVIC_IRQChannelPreemptionPriority = 0x02;
    NVIC_InitStruct.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStruct.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStruct);

}




/*********************************************************************
 *
 *					ADC123
 *
 ********************************************************************/


void ADC123_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;
    ADC_CommonInitTypeDef ADC_CommonInitStruct;

    /* 使能GPIOA和GPIOC的时钟 */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOC, ENABLE);

    /* 使能ADC1、ADC2和ADC3的时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_ADC2 | RCC_APB2Periph_ADC3, ENABLE);

    /* 配置PA0和PA1为模拟输入模式 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* 配置PC0和PC1为模拟输入模式 */
    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AN;
    GPIO_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ADC1和ADC2共同配置 */
    ADC_CommonInitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_CommonInitStruct.ADC_Prescaler = ADC_Prescaler_Div4; // 假设PCLK2经过4分频，设置ADC时钟频率为84MHz/4=21MHz
    ADC_CommonInitStruct.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled; // 禁止DMA访问模式
    ADC_CommonInitStruct.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles; // 配置两个采样阶段之间的延迟为20个时钟周期
    ADC_CommonInit(&ADC_CommonInitStruct);

    /* ADC1和ADC2配置 */
    ADC_InitStruct.ADC_Resolution = ADC_Resolution_12b; // 设置ADC分辨率为12位
    ADC_InitStruct.ADC_ScanConvMode = ENABLE; // 开启扫描模式
    ADC_InitStruct.ADC_ContinuousConvMode = ENABLE; // 开启连续转换模式
    ADC_InitStruct.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None; // 禁止外部触发转换
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_TRGO; // 使用定时器2触发转换
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right; // 数据右对齐
    ADC_InitStruct.ADC_NbrOfConversion = 1; // 每个ADC的转换个数为1
    ADC_Init(ADC1, &ADC_InitStruct); // 配置ADC1
    ADC_Init(ADC2, &ADC_InitStruct); // 配置ADC2

    /* ADC3配置 */
    ADC_InitStruct.ADC_NbrOfConversion = 2; // ADC3的转换个数为2
    ADC_Init(ADC3, &ADC_InitStruct); // 配置ADC3

    /* ADC1常规通道配置 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_3Cycles); // ADC1常规通道配置为PA0
    /* ADC2常规通道配置 */
    ADC_RegularChannelConfig(ADC2, ADC_Channel_1, 1, ADC_SampleTime_3Cycles); // ADC2常规通道配置为PA1
    /* ADC3常规通道配置 */
    ADC_RegularChannelConfig(ADC3, ADC_Channel_10, 1, ADC_SampleTime_3Cycles); // ADC3常规通道配置为PC0
    ADC_RegularChannelConfig(ADC3, ADC_Channel_11, 2, ADC_SampleTime_3Cycles); // ADC3常规通道配置为PC1

    /* 使能ADC1、ADC2和ADC3 */
    ADC_Cmd(ADC1, ENABLE);
    ADC_Cmd(ADC2, ENABLE);
    ADC_Cmd(ADC3, ENABLE);

    /* 触发第一次转换 */
    ADC_SoftwareStartConv(ADC1);
    ADC_SoftwareStartConv(ADC2);
    ADC_SoftwareStartConv(ADC3);
}

