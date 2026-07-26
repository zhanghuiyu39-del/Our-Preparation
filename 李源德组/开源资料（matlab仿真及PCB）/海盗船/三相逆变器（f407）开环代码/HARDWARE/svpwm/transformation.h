#ifndef INC_TRANSFORMATION_H_
#define INC_TRANSFORMATION_H_

#include "sys.h"
#include <arm_math.h>

void transform(float32_t current_a, float32_t current_b, float32_t theta,float32_t *current_Iq,float32_t *current_Id);


#endif /* INC_TRANSFORMATION_H_ */
