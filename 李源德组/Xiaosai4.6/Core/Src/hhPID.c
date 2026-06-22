
#include "hhPID.h"

// 初始化 PI 控制器
void PID_Init(PID_t *pid, float kp, float ki, float out_min, float out_max)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->out_min = out_min;
    pid->out_max = out_max;
    PID_Reset(pid);  // 清零状态
}

// 重置控制器状态（清零历史值）
void PID_Reset(PID_t *pid)
{
    pid->target = 0.0f;
    pid->actual = 0.0f;
    pid->error = 0.0f;
    pid->error_1 = 0.0f;
    pid->output = 0.0f;
    pid->output_1 = 0.0f;
		// PWM输出初始值
    pid->pwm_output = (uint16_t)((pid->out_min + pid->out_max) / 2.0f); 
}

// 增量式 PI 计算
void PID_Calculate(PID_t *pid)
{
    // 1. 计算当前误差
    pid->error = pid->target - pid->actual;
		
    // 2. 增量式 PI 公式：
    // Δu(k) = Kp * [e(k) - e(k-1)] + Ki * e(k)
    float delta_u = pid->Kp * (pid->error - pid->error_1) + pid->Ki * pid->error;

    // 3. 累加得到当前总输出
    pid->output = pid->output_1 + delta_u;

    // 4. 输出限幅（关键！防止饱和）
    if (pid->output > pid->out_max) 
		{
        pid->output = pid->out_max;
    } 
		else if (pid->output < pid->out_min) 
		{
        pid->output = pid->out_min;
    }

    // 5. 更新历史值
    pid->error_1 = pid->error;
    pid->output_1 = pid->output;

    // 6. 转换为整型 PWM（四舍五入）
    pid->pwm_output = (uint16_t)(pid->output + 0.5f);
}


