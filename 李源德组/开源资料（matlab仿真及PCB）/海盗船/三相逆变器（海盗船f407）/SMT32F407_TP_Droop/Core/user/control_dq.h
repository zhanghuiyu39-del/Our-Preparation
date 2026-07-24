#ifndef __CONTROL_DQ_H__
#define __CONTROL_DQ_H__

#include "control.h"
#include "dq_transform.h"
#include "dq_pid.h"
#include "dq_svpwm.h"
#include "dq_pll.h"
#include "dq_power.h"

typedef struct {
    float u_ab;
    float u_bc;
    float i_a;
    float i_c;
} DqAdcSample;

extern float dq_vd;
extern float dq_vq;
extern float dq_id;
extern float dq_iq;
extern float dq_vd_ref;       /* 下垂/控制 d 轴电压期望 (V), VOFA CH1 */
extern float dq_droop_freq;   /* 下垂输出频率 (Hz), VOFA CH0 */
extern float dq_theta;
extern float dq_output_freq;
extern float dq_svpwm_ta;
extern float dq_svpwm_tb;
extern float dq_svpwm_tc;

extern uint32_t dq_pwm_cmp_a;
extern uint32_t dq_pwm_cmp_b;
extern uint32_t dq_pwm_cmp_c;



void ControlDq_Init(void);
void ControlDq_TimAdcTriggerStart(void);
void ControlDq_ResetRuntime(void);
void ControlDq_OnStart(void);
void ControlDq_OnFreqUp(void);
void ControlDq_OnFreqDown(void);
void ControlDq_SetOutputFreq(float hz);
float ControlDq_GetOutputFreq(void);
void ControlDq_SetUoTarget(float uo);
void ControlDq_Execute(const DqAdcSample *sample, float vdc);
void ControlDq_UpdateMeasurement(const DqAdcSample *sample);
void Svpwm_modulation(float ta, float tb, float tc);
void ControlDq_PwmStop(void);

#endif /* __CONTROL_DQ_H__ */
