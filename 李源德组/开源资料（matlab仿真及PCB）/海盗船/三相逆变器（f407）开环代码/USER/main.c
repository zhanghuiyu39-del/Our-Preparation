#include "sys.h"
#include "delay.h"
#include "usart.h"
#include "led.h"
#include "key.h"
#include "timer.h"
#include "math.h"
#include "arm_math.h"
#include "Vofa.h"
#include "oled.h"
#include "menu.h"
#include "adc.h"
#include "pfc.h"
#include "spwm.h"
#include "pid.h"
#include "dac.h"

PID_TypeDef pid_voltage_loop, pid_current_loop;//电压环，电流环初始化
SOGI_PLL_DATA_DEF spll_data;
Vofa_HandleTypedef vofa1;
float32_t x=1,deta_theta=0;
float32_t PID = 35, IPID = 35,I_set=2*1.414; // 调压系数
float32_t vdc = 40.0f;//母线电压
u16 K=1;
float ch[4];

int main(void)

{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    delay_init(168);    //
    uart_init(3000000); //

    LED_Init();
    KEY_Init();
    OLED_Init();
    MenuInit();

    Dac1_Init();

    ADC123_Init();
    sogi_pll_init(&spll_data, 50 * value_2pi, 0.00005f); // 锁相环初始化
    PID_init(&pid_voltage_loop); // 初始化结构体电压环
	VB_PID_init(&pid_current_loop); // 初始化结构体电压环
    TIM1_SPWM_Init(8399, 0); // 20k载波频率
    

    Vofa_Init(&vofa1, VOFA_MODE_SKIP); //
    LED0 = 0;
    LED1 = 1;

    TIM_CtrlPWMOutputs(TIM1, DISABLE);

    while (1)
    {
        //        ch[0] = spll_data.theta;
        //        ch[1] = temp_theta;
        //        ch[2] = g_acdc_ctrlpar.vac;
        //        Vofa_JustFloat(&vofa1, &ch[0],3);
        MenuControl();
        MenuDataDisplayRefresh();
        LED0 = !LED0;
        LED1 = !LED1;

	}
	
}




