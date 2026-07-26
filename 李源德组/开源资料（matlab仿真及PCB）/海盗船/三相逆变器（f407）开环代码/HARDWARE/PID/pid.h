#ifndef __PID_H
#define __PID_H

#include "sys.h"
#include "arm_math.h"

#define VLOOP_KP            0.43f
#define VLOOP_KI            0.001f  //增加可以减小过冲0.5-1.0，减小可以减少低频纹波
#define Vloop_UP            10.0f
#define Vloop_LOW          -10.0f

#define ILOOP_KP            1.0f
#define ILOOP_KI            0.001f  //增加可以减小过冲0.5-1.0，减小可以减少低频纹波
#define Iloop_UP            500.0f
#define Iloop_LOW          -500.0f

#define Iloop_K 0.1f // L=1.6mH  Ts=1/20000  K= Ts/L

typedef struct _PID
{
	float32_t kp;								  // 输入比例
	float32_t ki;								  // 输入积分
	float32_t kd;								  // 输入微分
	float32_t initial_value;					  // 初始输出电压（AD采样值）//用于校准给定输出电压;默认160
	float32_t current_value;					  // 当前值
	float32_t different, L_different, LL_different; // 误差值,EK,EK-1,EK-2

} PID_TypeDef;

typedef struct PI_CTRL_DATA_TAG
{
	float32_t Out;	// Output: controller output
	float32_t Kp;	// Parameter: proportional loop gain
	float32_t Ki;	// Parameter: integral gain
	float32_t Umax; // Parameter: upper saturation limit
	float32_t Umin; // Parameter: lower saturation limit
	float32_t up;	// Data: proportional term
	float32_t ui;	// Data: integral term
	float32_t temp_p;
	float32_t temp_imax;
	float32_t temp_imin;
} PI_CTRL_DATA_DEF;

typedef struct AC_DC_CTRL_TAG
{
	float32_t vbus;		// 母线电压实时值
	float32_t vbus_ref; // 母线电压参考
	float32_t iac;		// 电网输入电流
	float32_t vac;		// 电网电压实时值
	float32_t vbus_f;
	float32_t sintheta; // 电网单位正弦电压实时值 （sin(wt)）
	float32_t ctrl_vol; //
} AC_DC_CTRL_DEF;

typedef struct DC_AC_CTRL_TAG {
	float32_t I1;
	float32_t U_o;
	float32_t I;
	float32_t ctrl_vol;
} DC_AC_CTRL_DEF;




float32_t pi_func(PI_CTRL_DATA_DEF *pi, float32_t ref, float32_t fbk);
float32_t acdc_bidirect_ctrl2(AC_DC_CTRL_DEF *ctrlpar, float32_t va);
void gei_initial_value(PID_TypeDef *p);
void PID_init(PID_TypeDef *p);
void VB_PID_init(PID_TypeDef *p);
void gei_current_value(PID_TypeDef *p, u16 z);
float32_t PID_count(PID_TypeDef *p, u16 z);
void PID_voltage_change(float kp, float ki, float kd, int init_vlaue);
float32_t PIDILOOP(DC_AC_CTRL_DEF* ctrlpar);

#endif
