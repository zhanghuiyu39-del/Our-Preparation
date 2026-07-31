#include "pfc_hrtim.h"
#include "hrtim.h"
#include "pfc_user_config.h"

/*
 * 本模块只封装HRTIM A/B的计数器、输出和Compare，不计算调制或处理状态机。
 * 数据流：SPWM调制 -> Compare预装载 -> HRTIM更新边界 -> TA/TB互补输出。
 *
 * Master/A/B计数器运行只代表10 kHz时基存在；WaveformOutputStart才会把PWM
 * 送到PA8~PA11。驱动板的UCC21520 DISABLE由硬件固定，因此没有第三路软件Gate。
 */
/* 单相全桥的四路MCU输出集合；不包含未使用的Timer C~F。 */
#define PFC_HRTIM_OUTPUTS  (HRTIM_OUTPUT_TA1 | HRTIM_OUTPUT_TA2 | \
                            HRTIM_OUTPUT_TB1 | HRTIM_OUTPUT_TB2)
/* 快速时基所需计数器集合：Master提供节拍，Timer A/B产生两桥臂PWM。 */
#define PFC_HRTIM_TIMERS_AB (HRTIM_TIMERID_TIMER_A | HRTIM_TIMERID_TIMER_B)
#define PFC_HRTIM_TIMERS_ALL (HRTIM_TIMERID_MASTER | PFC_HRTIM_TIMERS_AB)

/*
 * 下列变量是本模块发出的启动/停止命令的软件镜像，不是HRTIM寄存器或引脚电平回读。
 * 本模块及紧急关断路径写入，状态机和IWDG监督通过Getter读取；volatile只防止编译器
 * 缓存跨上下文读写，不代表多个操作具有原子事务语义。
 */
static volatile uint8_t pfc_counters_started = 0U;
static volatile uint8_t pfc_outputs_enabled = 0U;

/**
 * @brief  启动Master、Timer A和Timer B，建立10 kHz控制与PWM时基。
 * @retval HAL_OK表示计数器已运行或此前已经启动；HAL_ERROR表示句柄无效或HAL启动失败。
 * @note   本函数不开放TA1/TA2/TB1/TB2，计数器运行不等于功率输出开放。
 */
HAL_StatusTypeDef PFC_HRTIM_StartSampling(void)
{
    HAL_StatusTypeDef status; /* HAL计数器启动结果，仅在本次调用内有效。 */

    if (hhrtim1.Instance != HRTIM1)
    {
        return HAL_ERROR;
    }
    if (pfc_counters_started != 0U)
    {
        return HAL_OK;
    }

#if PFC_USER_ENABLE_ADC
    status = HAL_HRTIM_WaveformCounterStart(&hhrtim1, PFC_HRTIM_TIMERS_ALL);
#else
    /*
     * Master RepetitionCounter=0，因此每个10 kHz Master周期产生一次MREP事件。
     * ADC停用后，该中断接替ADC1 DMA完整回调成为唯一快速控制入口。
     */
    HAL_NVIC_SetPriority(HRTIM1_Master_IRQn, 1U, 0U);
    HAL_NVIC_ClearPendingIRQ(HRTIM1_Master_IRQn);
    HAL_NVIC_EnableIRQ(HRTIM1_Master_IRQn);
    status = HAL_HRTIM_SimpleBaseStart_IT(&hhrtim1, HRTIM_TIMERINDEX_MASTER);
    if (status == HAL_OK)
    {
        status = HAL_HRTIM_WaveformCounterStart(&hhrtim1, PFC_HRTIM_TIMERS_AB);
    }
    if (status != HAL_OK)
    {
        (void)HAL_HRTIM_SimpleBaseStop_IT(&hhrtim1, HRTIM_TIMERINDEX_MASTER);
        HAL_NVIC_DisableIRQ(HRTIM1_Master_IRQn);
    }
#endif
    if (status == HAL_OK)
    {
        pfc_counters_started = 1U;
    }
    return status;
}

/**
 * @brief  开放Timer A/B四路波形输出引脚。
 * @retval HAL_OK表示输出已开放或此前已经开放；HAL_ERROR表示前置条件或HAL调用失败。
 * @note   必须先启动计数器并写入安全Compare；当前硬件没有独立MCU Gate Enable。
 */
HAL_StatusTypeDef PFC_HRTIM_StartOutputs(void)
{
    HAL_StatusTypeDef status; /* HAL输出启动结果，仅成功时更新软件镜像。 */

    if ((hhrtim1.Instance != HRTIM1) || (pfc_counters_started == 0U))
    {
        return HAL_ERROR;
    }
    if (pfc_outputs_enabled != 0U)
    {
        return HAL_OK;
    }

    status = HAL_HRTIM_WaveformOutputStart(&hhrtim1, PFC_HRTIM_OUTPUTS);
    if (status == HAL_OK)
    {
        pfc_outputs_enabled = 1U;
    }
    return status;
}

/**
 * @brief  关闭Timer A/B四路输出，同时保留Master/A/B计数器和ADC触发。
 * @note   供人工停机和普通软件故障使用；HAL返回值被有意忽略并强制清除软件镜像。
 */
void PFC_HRTIM_StopPower(void)
{
    if (hhrtim1.Instance == HRTIM1)
    {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, PFC_HRTIM_OUTPUTS);
    }
    pfc_outputs_enabled = 0U;
}

/**
 * @brief  关闭四路输出并停止Master、Timer A和Timer B计数器。
 * @note   用于不可恢复初始化错误；停止后HRTIM TRG1消失，ADC规则组不再更新。
 */
void PFC_HRTIM_StopAll(void)
{
    PFC_HRTIM_StopPower();
    if (hhrtim1.Instance == HRTIM1)
    {
#if PFC_USER_ENABLE_ADC
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, PFC_HRTIM_TIMERS_ALL);
#else
        HAL_NVIC_DisableIRQ(HRTIM1_Master_IRQn);
        (void)HAL_HRTIM_SimpleBaseStop_IT(&hhrtim1, HRTIM_TIMERINDEX_MASTER);
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, PFC_HRTIM_TIMERS_AB);
#endif
    }
    pfc_counters_started = 0U;
}

/**
 * @brief  写入Timer A/B Compare 1预装载值。
 * @param  cmp_a Timer A Compare 1计数值，有效范围1~33999。
 * @param  cmp_b Timer B Compare 1计数值，有效范围1~33999。
 * @retval HAL_OK表示两个寄存器写入完成，HAL_ERROR表示句柄或范围无效。
 * @note   IOC已启用Preload，写入值在配置的HRTIM更新边界统一生效；本函数不开放输出。
 */
HAL_StatusTypeDef PFC_HRTIM_SetCompare(uint16_t cmp_a, uint16_t cmp_b)
{
    if ((hhrtim1.Instance != HRTIM1) ||
        (cmp_a == 0U) || (cmp_a >= PFC_HRTIM_PERIOD_COUNTS) ||
        (cmp_b == 0U) || (cmp_b >= PFC_HRTIM_PERIOD_COUNTS))
    {
        return HAL_ERROR;
    }

    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_A,
                            HRTIM_COMPAREUNIT_1, cmp_a);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_B,
                            HRTIM_COMPAREUNIT_1, cmp_b);
    return HAL_OK;
}

/**
 * @brief  读取本模块的计数器启动命令镜像。
 * @retval 1表示本模块已成功发出启动且尚未发出停止，0表示未启动或已停止。
 * @note   该值不是硬件运行状态寄存器的实时回读。
 */
uint8_t PFC_HRTIM_CountersStarted(void)
{
    return pfc_counters_started;
}

/**
 * @brief  读取本模块的四路输出开放命令镜像。
 * @retval 1表示软件已开放且尚未软件关断，0表示软件认为输出已关闭。
 * @note   HRTIM Fault可先于ISR异步关闭引脚，因此该值不能证明引脚正在输出或功率管导通。
 */
uint8_t PFC_HRTIM_OutputsEnabled(void)
{
    return pfc_outputs_enabled;
}

/**
 * @brief  使用HRTIM输出禁止寄存器执行最小、非阻塞的四路PWM关断。
 * @note   可在ADC/HRTIM ISR、NMI和HardFault中调用；不停止计数器，不调用HAL，不清Fault源。
 */
void PFC_HRTIM_EmergencyOff(void)
{
    pfc_outputs_enabled = 0U;
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TA1ODIS | HRTIM_ODISR_TA2ODIS |
                                HRTIM_ODISR_TB1ODIS | HRTIM_ODISR_TB2ODIS;
}
