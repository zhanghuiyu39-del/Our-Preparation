/*
 * pi_control.h
 *
 *  Created on: 19.05.2016
 *      Author: Michael Meiler
 */

#ifndef SRC_PI_CONTROL_H_
#define SRC_PI_CONTROL_H_
#include "arm_math.h"
#include <stdbool.h>

/*********************************************************************
*
*                   Defines
*
*********************************************************************/
#define _OUTMIN -2//value       // limits output alias command signal
#define _OUTMAX 2//value       // limits output alias command signal
#define _ERRORMIN -2//value     // limits control error
#define _ERRORMAX 2//value     // limits control error
#define _K_p 4
#define _K_i 1


/*********************************************************************
*
*                   Variables
*
*********************************************************************/



/*********************************************************************
*
*                   Functions
*
*********************************************************************/
typedef struct {
    float k_p;
    float k_i;
    float k_d;

    float u_i;
    float u;

    bool saturation_flag;
    float last_error;
    float u_max;
} PID;

void pid_init(PID *pid, float k_p, float k_i, float k_d, const float u_max);
float PI_control(float32_t ref, float32_t real);
float pid_calc(PID *pid, float32_t ref, float32_t real);


#endif /* SRC_PI_CONTROL_H_ */
