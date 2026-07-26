#include "pid.h"
#include "timer.h"
#include "led.h"
#include "oled.h"
#include "adc.h"
#include "sys.h"
#include "pid.h"
#include "Vofa.h"

// INIT PID coefficient
PI_CTRL_DATA_DEF vloop_pi = {
	0, // Out
	VLOOP_KP,
	VLOOP_KI,
	Vloop_UP,  // Umax
	Vloop_LOW, // Umin
	0,
	0,
	0,
	0,
	0};

// INIT PID coefficient
PI_CTRL_DATA_DEF iloop_pi = {
	0, // Out
	ILOOP_KP,
	ILOOP_KI,
	Iloop_UP,  // Umax
	Iloop_LOW, // Umin
	0,
	0,
	0,
	0,
	0};

AC_DC_CTRL_DEF g_acdc_ctrlpar = {
	0,
	48.0f,
	0,
	0,
	0,
	0,
};
DC_AC_CTRL_DEF g_dcac_ctrlpar= {
    0,
	 0,
	 0,
	 0,
};

/*******************************************************************************************
Function Name:   pi_func
Version:         V1.0
Input:           &pi->pid参数   give->设定参数  sen->返回采样参数
Output:          gpfc_linev_phasev转换后的结果保存
Description:     增量型pid控制函数
History:
*******************************************************************************************/
float32_t pi_func(PI_CTRL_DATA_DEF *pi, float32_t ref, float32_t fbk)
{

	/* proportional term */
	pi->up = ref - fbk;

	/* integral term */
	pi->ui = pi->up + pi->ui;

	pi->temp_p = pi->up * pi->Kp;

	pi->temp_imax = pi->Umax - pi->temp_p;
	pi->temp_imin = pi->Umin - pi->temp_p;

	// 积分抗饱和
	if (pi->ui < pi->temp_imin)
		pi->ui = pi->temp_imin;
	if (pi->ui > pi->temp_imax)
		pi->ui = pi->temp_imax;

	/* control output */
	pi->Out = pi->temp_p + pi->ui*pi->Ki;

	if (pi->Out < pi->Umin)
		pi->Out = pi->Umin;
	if (pi->Out > pi->Umax)
		pi->Out = pi->Umax;

	return (pi->Out);
}

float32_t acdc_bidirect_ctrl2(AC_DC_CTRL_DEF *ctrlpar, float32_t va)
{

	float32_t i_set = pi_func(&vloop_pi, ctrlpar->vbus_ref, ctrlpar->vbus);
	float32_t iacr = ctrlpar->sintheta * i_set; // 电流参考
	iacr = iacr / 5.0f + 1.5;
	DAC_SetChannel1Data(DAC_Align_12b_R, iacr / 3.3 * 4095);
	float32_t Iac_err = iacr - ctrlpar->iac; // 电流误差
	float32_t Voz;
	float32_t D2;

	Voz = ctrlpar->vbus;								   // 母线电压
	D2 = ctrlpar->vac / Voz - (Iac_err) / (Voz * Iloop_K); // 直接计算占空比
														   //	D2 = (ctrlpar->vac * Iloop_K - Iac_err) / (Voz * Iloop_K); // 直接计算占空比
	return D2;
}

// #include "usart.h"
unsigned short Voltage[10][2];
unsigned int VOLTAGE = 0;
float voltage_N = 0, voltage_L;
// 通用定时器3中断初始化
// 这里时钟选择为APB1的2倍，而APB1为36M
// arr：自动重装值。
// psc：时钟预分频数
// 这里使用的是定时器3!
void TIM3_Int_Init(uint16_t arr, uint16_t psc)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	// 使能定时器时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

	// 定时器基本配置
	TIM_TimeBaseStructure.TIM_Period = arr;						// 设置在下一个更新事件装入活动的自动重装载寄存器周期的值，计数到5000为500ms
	TIM_TimeBaseStructure.TIM_Prescaler = psc;					// 设置用来作为TIMx时钟频率除数的预分频值，10Khz的计数频率
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;				// 设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);				// 根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

	// 使能定时器更新中断
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

	// 配置定时器3的中断优先级
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;			  // TIM3中断
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 先占优先级1级
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;		  // 从优先级1级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  // IRQ通道被使能
	NVIC_Init(&NVIC_InitStructure);							  // 根据NVIC_InitStruct中指定的参数初始化外设NVIC寄存器

	// 使能定时器3
	TIM_Cmd(TIM3, ENABLE);
}

// 定时器3中断服务程序
extern float PID, VB_PID;
extern PID_TypeDef pid_voltage_loop, pid_boost_loop; // 电压环PID参数
extern float32_t vrms;
extern Vofa_HandleTypedef vofa1;
float pid[4];

//void TIM3_IRQHandler(void)
//{
//	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) // 检查指定的TIM中断发生与否:TIM 中断源
//	{
//		
//		PID += PID_count(&pid_voltage_loop, 0);
//		if (PID > 42)
//			PID = 42;
//		if (PID < 25)
//			PID = 25;
//		
//		
//		//			pid[0]=PID;
//		//			pid[1]=pid_voltage_loop.initial_value;
//		//			pid[2]=VB_PID;
//		//			pid[3]=pid_boost_loop.initial_value;
//		//			Vofa_JustFloat(&vofa1,&pid[0],4);
//		
//		TIM_ClearITPendingBit(TIM3, TIM_IT_Update); // 清除TIMx的中断待处理位:TIM 中断源
//													// 中断处理代码
//	}
//}
// 函数:获取初始值
// 输入参数: 结构体指针
// 返回：无
void gei_initial_value(PID_TypeDef *p)
{
	unsigned short value_all = 0;
	int i;
	for (i = 0; i < 10; i++)
		value_all += Voltage[i][0];
	p->initial_value = value_all / 100;
}
// 函数：初始化PID参数
// 输入参数: 结构体指针
// 返回 ；无
void PID_init(PID_TypeDef *p)
{
	p->kp = 1;  // 6
	p->ki = 0.001; // 2.9
	p->kd = 0.000;
	p->initial_value = 24; // 206（1.68V）    404
	p->L_different = 0;
	p->LL_different = 0;
}
//
// 函数：初始化VB_PID参数
void VB_PID_init(PID_TypeDef *p)
{
	p->kp =0.61;  // 6
	p->ki = 0.021; // 2.9
	p->kd = 0.000;
	p->initial_value = 2; // 206（1.68V）    404
	p->L_different = 0;
	p->LL_different = 0;
}

// 函数:获取当前值
// 输入参数: 结构体指针
// 返回：无
void gei_current_value(PID_TypeDef *p, u16 z)
{
	unsigned short value_all = 0;
	int i;
	for (i = 0; i < 10; i++)
		value_all += Voltage[i][z];
	p->current_value = value_all / 100;
}
// 函数：计算PID值
// 输入参数 ：结构体指针
// 返回 ：PID值
float32_t PID_count(PID_TypeDef *p, u16 z)
{
	float32_t value;
	// gei_current_value(p, z);
	p->different = p->initial_value - p->current_value;

	value = (p->different - p->L_different) * p->kp + p->different * p->ki + p->kd * (p->different - 2 * p->L_different + p->LL_different);
	//	value = PID ;

	p->LL_different = p->L_different;
	
	p->L_different = p->different;

	return value;
	
}

float32_t PIDILOOP(DC_AC_CTRL_DEF* ctrlpar)
{
	
	float32_t i_set=pi_func(&vloop_pi, ctrlpar->I, ctrlpar->I1);
	
	const float32_t Uarr = ctrlpar->U_o;//电流参考
	
	float32_t D;

	D=Uarr+i_set;
	
	return D;
	
}


