#include "spwm.h"
#include "pfc.h"
#include "pid.h"
#include "arm_math.h"
#include "math.h"
#include "Vofa.h"


u16 maxVac = 0;
u16 minVac = 4095; // 初始值设置为最大
u16 maxIac = 0;
u16 minIac = 4095; // 初始值设置为最大
extern float D1;
extern float D2;
extern PI_CTRL_DATA_DEF vloop_pi;
extern PI_CTRL_DATA_DEF iloop_pi;
extern AC_DC_CTRL_DEF g_acdc_ctrlpar;
extern SOGI_PLL_DATA_DEF spll_data;
extern Vofa_HandleTypedef vofa1;
extern PID_TypeDef pid_voltage_loop, pid_current_loop; // 电压环PID参数
extern float32_t PID,IPID;                                // 调压系数
int i=0;
const u16 Sin_wave_Table[200] =
    {
        0, 2, 3, 5, 6, 8, 9, 11, 13, 14, 16, 17, 19, 20, 22, 23, 25, 26, 28, 29,
        31, 32, 34, 35, 37, 38, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 54, 55, 56, 58,
        59, 60, 61, 63, 64, 65, 66, 67, 68, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80,
        81, 82, 83, 84, 84, 85, 86, 87, 88, 88, 89, 90, 90, 91, 92, 92, 93, 94, 94, 95,
        95, 96, 96, 96, 97, 97, 98, 98, 98, 99, 99, 99, 99, 99, 100, 100, 100, 100, 100, 100,
        100, 100, 100, 100, 100, 100, 100, 99, 99, 99, 99, 99, 98, 98, 98, 97, 97, 96, 96, 96,
        95, 95, 94, 94, 93, 92, 92, 91, 90, 90, 89, 88, 88, 87, 86, 85, 84, 84, 83, 82,
        81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 68, 67, 66, 65, 64, 63, 61, 60,
        59, 58, 56, 55, 54, 52, 51, 50, 48, 47, 45, 44, 43, 41, 40, 38, 37, 35, 34, 32,
        31, 29, 28, 26, 25, 23, 22, 20, 19, 17, 16, 14, 13, 11, 9, 8, 6, 5, 3, 2};



void TIM1_SPWM_Init(u32 arr, u32 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_BDTRInitTypeDef TIM_BDTRStruct;

    // 使能定时器1和GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOE, ENABLE);

    // 将GPIO引脚复用为定时器功能
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource8, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource9, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource10, GPIO_AF_TIM1);
    GPIO_PinAFConfig(GPIOE, GPIO_PinSource11, GPIO_AF_TIM1);

    // 配置GPIO引脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11; // PE9, PE11, PE13
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // 定时器基本配置
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0; // 重复计数次数，更新事件发生在计数下溢后
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    // PWM通道配置

    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCStruct.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStruct.TIM_OutputNState = TIM_OutputNState_Enable;
    TIM_OCStruct.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCStruct.TIM_OCNPolarity = TIM_OCPolarity_High;
    TIM_OCStruct.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCStruct.TIM_OCNIdleState = TIM_OCIdleState_Reset;
    TIM_OCStruct.TIM_Pulse = 0;
   
	
    TIM_OC1Init(TIM1, &TIM_OCStruct);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable); // 预装载

    TIM_OCStruct.TIM_OCMode = TIM_OCMode_PWM2;
    TIM_OC2Init(TIM1, &TIM_OCStruct);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);

    // 使能PWM输出
    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    /*自动输出使能，断开，死区时间和锁定配置
     *死区时间计算请参见参考手册*/
    TIM_BDTRStruct.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRStruct.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRStruct.TIM_LOCKLevel = TIM_LOCKLevel_1;
    TIM_BDTRStruct.TIM_DeadTime = 0x50;
    TIM_BDTRStruct.TIM_Break = TIM_Break_Disable;
    TIM_BDTRStruct.TIM_BreakPolarity = TIM_BreakPolarity_High;
    TIM_BDTRStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;
    TIM_BDTRConfig(TIM1, &TIM_BDTRStruct);

    // 配置定时器1中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM1_UP_TIM10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    // 使能定时器1更新中断
    TIM_ITConfig(TIM1, TIM_IT_Update, ENABLE);

    // 使能定时器1
    TIM_Cmd(TIM1, ENABLE);
}

float32_t cac_rms(float32_t x)
{
    static float32_t fs = 4000.0f;
    static float32_t times = 0;
    static float32_t sum = 0.0f;
    static float32_t rms = 0.0f;
    float temp;

    temp = x * x;
    // sum = sum*(0.9998f) + temp*0.0002f;
    sum += temp;

    times += 1.0f;
    if (times >= fs)
    {
        temp = sum / times;
        rms = temp;
        times = fs - 1.0f;
        sum = rms * times;

        //  sum = 0;
        //   times = 0;
    }
    return sqrtf(rms); // sqrtf(sum);
}

float32_t u_rms(float32_t x)
{
  static float32_t fs=4000.0f;
  static float32_t times=0;
  static float32_t sum=0.0f;
  static float32_t rms=0.0f;
  float32_t temp;

   temp=x*x;
  // sum = sum*(0.9998f) + temp*0.0002f;
   sum +=temp;

   times+=1.0f;
   if(times>=fs)
   {
      temp = sum/times;
      rms = temp;
      times=fs-1.0f;
      sum = rms*times;

    //  sum = 0;
   //   times = 0;
   }
   return sqrtf(rms);//sqrtf(sum);
}


extern DC_AC_CTRL_DEF g_dcac_ctrlpar;
extern u16 K;
extern float32_t I_set;
extern float32_t vdc;
extern u16 flagloop;
float32_t temp_theta = 0.0f;
float32_t Mid_Value = 4200;
float32_t Out_Point_Count = 0; // 输出点数，总共400个点
float32_t ta[4];
float32_t flag = 0;
float32_t vacs = 0, iacs = 0, vrms = 0,vacgs=0,skiacs=0,skvrms=0;
float32_t Cos_wave = 0;
float32_t uref = 3.3f / 4095.0f; //
float32_t D=0;
void TIM1_UP_TIM10_IRQHandler() // TIM1中断
{
    if (TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
    {

        float32_t vrm = ADC_GetConversionValue(ADC3)* uref; //PC3
        float32_t iac = ADC_GetConversionValue(ADC1); // 获取交流电流PA0
        float32_t vacg = ADC_GetConversionValue(ADC2); // 获取交流电压有效值PA1
		
		
//        vrms = vrm * uref*10.53;
		   // vrms= vrm * uref*33.125-53.66;
        iacs = iac * uref*4.96-7.956;
		    vacgs= vacg * uref*33.125-53.66;
//        vrms = vrm * uref * 10.53;

		if(flagloop==0)
		{
			skiacs=cac_rms(iacs);
			skvrms=u_rms(vacgs);
			pid_voltage_loop.current_value = skvrms-0.25; // 电压环
			
			PID += PID_count(&pid_voltage_loop, 0);
			if (PID > 42)
				PID = 42;
			if (PID < 0)
				PID = 0;
			if(skiacs<0.2&&skiacs>-0.2)
				PID=28.35;

			spll_sogi_func(&spll_data, vacgs);
			Cos_wave = arm_cos_f32(spll_data.theta);
			if(i<200)
			{
				TIM1->CCR2=(30*Sin_wave_Table[i]+Mid_Value);
				TIM1->CCR1=(30*Sin_wave_Table[i]+Mid_Value);
			
			}
	else
			{
				TIM1->CCR2=(Mid_Value-30*Sin_wave_Table[i-200]);
		    TIM1->CCR1=(Mid_Value-30*Sin_wave_Table[i-200]);
	    }
			i++;
	   
			if(i>=399)
			
			i=0;
		}
		
		
		else
		{
			
			
			skiacs=cac_rms(iacs);
			pid_current_loop.current_value =skiacs; // 电liu环
			IPID += PID_count(&pid_current_loop, 0);
			if (IPID > 42)
				IPID = 42;
			if (IPID < 6)
				IPID = 6;
	
			
			/////////这是第二台的代码从机//////////			
//			spll_sogi_func(&spll_data, vacgs);
//			Cos_wave = arm_cos_f32(spll_data.theta);
//			TIM1->CCR1 = Mid_Value + Cos_wave * 100 * IPID ;
//			TIM1->CCR2 = Mid_Value + Cos_wave * 100 * IPID ;
//			
			
			/////////这是第一台的代码主机//////////
						if(i<200)
			{
				TIM1->CCR2=(30*Sin_wave_Table[i]+Mid_Value);
				TIM1->CCR1=(30*Sin_wave_Table[i]+Mid_Value);
			
			}
	else
			{
				TIM1->CCR2=(Mid_Value-30*Sin_wave_Table[i-200]);
		    TIM1->CCR1=(Mid_Value-30*Sin_wave_Table[i-200]);
	    }
			i++;
			if(i>=399)
			i=0;


		}
				
			
	
//        ta[0] = iac * uref;
//        ta[0] =  IPID;
 //       ta[0] =spll_data.theta;
//////      ta[1] = PID;
//       Vofa_JustFloat(&vofa1, &ta[0], 1);
   
    }
		
	TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
}
