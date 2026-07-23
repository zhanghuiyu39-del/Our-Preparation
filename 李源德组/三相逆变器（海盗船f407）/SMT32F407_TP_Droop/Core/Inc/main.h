#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

void Error_Handler(void);

/* 由 control.h 中 CTRL_MODE 决定; 此处仅声明按键 */
#define KEY_START_Pin GPIO_PIN_1
#define KEY_START_GPIO_Port GPIOE
#define KEY_FREQ_UP_Pin GPIO_PIN_3
#define KEY_FREQ_UP_GPIO_Port GPIOE
#define KEY_FREQ_DN_Pin GPIO_PIN_2
#define KEY_FREQ_DN_GPIO_Port GPIOE
#define KEY_CALIB_Pin GPIO_PIN_4
#define KEY_CALIB_GPIO_Port GPIOE

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
