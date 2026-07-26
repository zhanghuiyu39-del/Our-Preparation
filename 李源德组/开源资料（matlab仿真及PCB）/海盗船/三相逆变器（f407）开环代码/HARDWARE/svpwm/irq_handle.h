#ifndef IRQ_HANDLE_H_
#define IRQ_HANDLE_H_

#include "sys.h"
#include <math.h>
#include "arm_math.h"
#include <pi_controller.h>
#include <svpwm.h>
#include "svpwm2.h"
#include <transformation.h>


extern float32_t _MAX_CURRENT;
extern float32_t Ia,Ib,Iq_real,Id_real,Iq_ref,Id_ref,Ud,Uq,Theta,sinevalue,cosinevalue;
extern uint16_t *switchtime[3];

void EXTI9_5_IRQHandler(void);
void TIM2_IRQHandler(void);

#endif /* INC_TRANSFORMATION_H_ */
