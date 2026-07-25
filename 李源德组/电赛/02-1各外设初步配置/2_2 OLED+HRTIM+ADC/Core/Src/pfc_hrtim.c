#include "pfc_hrtim.h"
#include "hrtim.h"
#include "gpio.h"

#define PFC_HRTIM_OUTPUTS  (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | \
                            HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

#define PFC_HRTIM_TIMERS   (HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | \
                            HRTIM_TIMERID_TIMER_B)

/* 只启动计数器，为 ADC Trigger 1 提供 10 kHz 时基，不启动四路 PWM 引脚输出。 */
void PFC_HRTIM_StartSampling(void)
{
    (void)HAL_HRTIM_WaveformCounterStart(&hhrtim1, PFC_HRTIM_TIMERS);
}

/* 故障和异常统一从这里收尾：先关驱动，再关 PWM 输出和时基。 */
void PFC_HRTIM_StopAll(void)
{
    HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
    (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, PFC_HRTIM_OUTPUTS);
    (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, PFC_HRTIM_TIMERS);
}

/* 后续 PR 电流环只更新预装载 CMP1；本采样验证阶段不调用此函数。 */
void PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b)
{
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                            HRTIM_COMPAREUNIT_1, cmp_a);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B,
                            HRTIM_COMPAREUNIT_1, cmp_b);
}
