#include <stdint.h>
#include <math.h>
#include <arm_math.h>
#include <pi_controller.h>
#include <stdbool.h>

float32_t error_sum = 0;


/*********************************************************************
*
*                   Functions
*
*********************************************************************/
float PI_control(float32_t ref, float32_t real)  {

	float32_t out, pi_error;

    pi_error = ref - real;
    error_sum += pi_error;

    /* Limiting Error */
    if (error_sum < _ERRORMIN)  {
      error_sum = _ERRORMIN;
    }
    else if (error_sum > _ERRORMAX)  {
      error_sum = _ERRORMAX;
    }

    out = _K_p*pi_error + _K_i*error_sum;

    /* Limiting output */
    if (out < _OUTMIN)  {
      out = _OUTMIN;
    }
    else if (out > _OUTMAX) {
      out = _OUTMAX;
    }

    return (out);

}



void pid_init(PID *pid, float k_p, float k_i, float k_d, const float u_max) {
    pid->k_p = k_p;
    pid->k_i = k_i;
    pid->k_d = k_d;
    pid->u_max = u_max;
}

void pid_reset(PID *pid) {
    pid->u_i = 0;
    pid->saturation_flag = false;
    pid->last_error = 0;
}

float pid_calc(PID *pid, float32_t ref, float32_t real) 
	{
		float32_t error = ref - real;
    if (pid->saturation_flag == false) {
        pid->u_i += pid->k_i * error;
    }
    pid->u = pid->k_p * error + pid->u_i + pid->k_d * (error - pid->last_error);
    pid->last_error = error;

    if (pid->u > pid->u_max) {
        pid->u = pid->u_max;
        pid->saturation_flag = true;
    } else if (pid->u < -pid->u_max) {
        pid->u = -pid->u_max;
        pid->saturation_flag = true;
    } else {
        pid->saturation_flag = false;
    }
    return pid->u;
}



