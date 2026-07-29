#include "pfc_hrtim.h"
#include "hrtim.h"
#include "gpio.h"

/*
 * 本模块只封装单相全桥HRTIM A/B的启停和Compare写入，不计算调制量或处理状态机。
 * 数据流：SPWM调制量 -> Compare预装载 -> HRTIM周期边界生效 -> TA/TB互补输出。
 * Master负责ADC触发时基，Timer A/B负责桥臂PWM；计数器、输出和Gate三者独立管理。
 */
#define PFC_HRTIM_OUTPUTS  (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | \
                            HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)

#define PFC_HRTIM_TIMERS   (HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_A | \
                            HRTIM_TIMERID_TIMER_B)

/* 只启动计数器，为 ADC Trigger 1 提供 10 kHz 时基，不启动四路 PWM 引脚输出。 */
HAL_StatusTypeDef PFC_HRTIM_StartSampling(void)
{
    if (hhrtim1.Instance == HRTIM1)
    {
        return HAL_HRTIM_WaveformCounterStart(&hhrtim1, PFC_HRTIM_TIMERS);
    }

    return HAL_ERROR;
}

/* 比较值已经预装载且采样时基稳定后，才允许开放四路MCU PWM引脚。 */
HAL_StatusTypeDef PFC_HRTIM_StartOutputs(void)
{
    if (hhrtim1.Instance == HRTIM1)
    {
        return HAL_HRTIM_WaveformOutputStart(&hhrtim1, PFC_HRTIM_OUTPUTS);
    }

    return HAL_ERROR;
}

/* 正常停机只关闭Gate和PWM输出，保留HRTIM计数器继续触发ADC。 */
void PFC_HRTIM_StopPower(void)
{
    HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
    if (hhrtim1.Instance == HRTIM1)
    {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, PFC_HRTIM_OUTPUTS);
    }
}

/* 故障和异常统一从这里收尾：先关驱动，再关 PWM 输出和时基。 */
void PFC_HRTIM_StopAll(void)
{
    HAL_GPIO_WritePin(PFC_GATE_EN_GPIO_Port, PFC_GATE_EN_Pin, GPIO_PIN_RESET);
    if (hhrtim1.Instance == HRTIM1)
    {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, PFC_HRTIM_OUTPUTS);
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, PFC_HRTIM_TIMERS);
    }
}

/*
 * Cortex异常入口使用的最小关断路径。先直接拉低PE0，再写HRTIM输出禁止寄存器；
 * 不等待HAL状态机，也不执行可能依赖中断或Tick的阻塞操作。
 */
void PFC_HRTIM_EmergencyOff(void)
{
    GPIOE->BRR = PFC_GATE_EN_Pin;
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS |
                                HRTIM_ODISR_TB1ODIS | HRTIM_ODISR_TB2ODIS;
}

/* 后续 PR 电流环只更新预装载 CMP1；本采样验证阶段不调用此函数。 */
void PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b)
{
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                            HRTIM_COMPAREUNIT_1, cmp_a);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B,
                            HRTIM_COMPAREUNIT_1, cmp_b);
}
