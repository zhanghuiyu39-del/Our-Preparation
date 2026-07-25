#ifndef __PFC_HRTIM_H
#define __PFC_HRTIM_H

#include <stdint.h>

/* 仅管理单相全桥的 HRTIM A/B；本阶段默认不启动 PWM 输出。 */
void PFC_HRTIM_StartSampling(void);
void PFC_HRTIM_StopAll(void);
void PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b);

#endif
