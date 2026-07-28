#include "inv_hrtim.h"

#include <float.h>

#include "gpio.h"
#include "hrtim.h"

/* U/V/W分别使用Timer C/D/E，每相的Output 2由HRTIM死区单元生成互补波形。 */
#define INV_HRTIM_OUTPUTS (HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2 | \
                           HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2 | \
                           HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2)

#define INV_HRTIM_COUNTERS (HRTIM_TIMERID_MASTER | HRTIM_TIMERID_TIMER_C | \
                            HRTIM_TIMERID_TIMER_D | HRTIM_TIMERID_TIMER_E)

#define INV_HRTIM_INV_COUNTERS (HRTIM_TIMERID_TIMER_C | \
                                HRTIM_TIMERID_TIMER_D | \
                                HRTIM_TIMERID_TIMER_E)

/* 0.05～0.95占空比按反向映射对应CMP1=32300～1700。 */
#define INV_HRTIM_COMPARE_MIN (1700U)
#define INV_HRTIM_COMPARE_MAX (32300U)
#define INV_HRTIM_NEUTRAL_CMP (17000U)

static uint8_t inv_sampling_started = 0U;

/* 避免依赖不同ARM C库的isfinite()实现。 */
static uint8_t INV_HRTIM_IsFinite(float value)
{
    if ((value != value) || (value > FLT_MAX) || (value < -FLT_MAX)) {
        return 0U;
    }

    return 1U;
}

/* FLT3事件标志由硬件置位；本模块只读取，不擅自清除公共故障记录。 */
static uint8_t INV_HRTIM_IsFault3Set(void)
{
    return (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET) ? 1U : 0U;
}

/* 当前Valley/Set-Reset极性已经确定为CMP=Period*(1-duty)。 */
static uint16_t INV_HRTIM_DutyToCompare(float duty)
{
    float compare = (float)INV_HRTIM_PERIOD_COUNTS * (1.0f - duty);
    return (uint16_t)(compare + 0.5f);
}

HAL_StatusTypeDef INV_HRTIM_StartSampling(void)
{
    HAL_StatusTypeDef status;

    /* 计数器启动前再次建立硬件安全态，防止旧调试状态遗留输出使能。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if (hhrtim1.Instance != HRTIM1) {
        inv_sampling_started = 0U;
        return HAL_ERROR;
    }

    (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
    status = HAL_HRTIM_WaveformCounterStart(&hhrtim1, INV_HRTIM_COUNTERS);
    inv_sampling_started = (status == HAL_OK) ? 1U : 0U;

    return status;
}

HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w)
{
    /* 先验证全部输入，再统一写寄存器，避免只更新部分桥臂。 */
    if ((INV_HRTIM_IsFinite(duty_u) == 0U) ||
        (INV_HRTIM_IsFinite(duty_v) == 0U) ||
        (INV_HRTIM_IsFinite(duty_w) == 0U) ||
        (duty_u < INV_HRTIM_DUTY_MIN) || (duty_u > INV_HRTIM_DUTY_MAX) ||
        (duty_v < INV_HRTIM_DUTY_MIN) || (duty_v > INV_HRTIM_DUTY_MAX) ||
        (duty_w < INV_HRTIM_DUTY_MIN) || (duty_w > INV_HRTIM_DUTY_MAX)) {
        return HAL_ERROR;
    }

    return INV_HRTIM_SetCompare(INV_HRTIM_DutyToCompare(duty_u),
                                INV_HRTIM_DutyToCompare(duty_v),
                                INV_HRTIM_DutyToCompare(duty_w));
}

HAL_StatusTypeDef INV_HRTIM_SetCompare(uint16_t cmp_u,
                                       uint16_t cmp_v,
                                       uint16_t cmp_w)
{
    if ((hhrtim1.Instance != HRTIM1) ||
        (cmp_u < INV_HRTIM_COMPARE_MIN) || (cmp_u > INV_HRTIM_COMPARE_MAX) ||
        (cmp_v < INV_HRTIM_COMPARE_MIN) || (cmp_v > INV_HRTIM_COMPARE_MAX) ||
        (cmp_w < INV_HRTIM_COMPARE_MIN) || (cmp_w > INV_HRTIM_COMPARE_MAX)) {
        return HAL_ERROR;
    }

    /* 三个CMP1均启用预装载，将在CubeMX配置的Valley边界同步生效。 */
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C,
                           HRTIM_COMPAREUNIT_1, cmp_u);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D,
                           HRTIM_COMPAREUNIT_1, cmp_v);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E,
                           HRTIM_COMPAREUNIT_1, cmp_w);

    return HAL_OK;
}

HAL_StatusTypeDef INV_HRTIM_EnablePowerStage(void)
{
    HAL_StatusTypeDef status;

    /* PE1最后才允许置位；任何前置检查失败都保持驱动关闭。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if ((hhrtim1.Instance != HRTIM1) ||
        (inv_sampling_started == 0U) ||
        (INV_HRTIM_IsFault3Set() != 0U)) {
        return HAL_ERROR;
    }

    if (INV_HRTIM_SetCompare(INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP) != HAL_OK) {
        return HAL_ERROR;
    }

    status = HAL_HRTIM_WaveformOutputStart(&hhrtim1, INV_HRTIM_OUTPUTS);
    if (status != HAL_OK) {
        INV_HRTIM_StopAll();
        return status;
    }

    /* 输出开放后复查FLT3，覆盖启动边界恰好到来的驱动器故障。 */
    if (INV_HRTIM_IsFault3Set() != 0U) {
        INV_HRTIM_StopAll();
        return HAL_ERROR;
    }

    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

void INV_HRTIM_StopAll(void)
{
    /* 安全顺序固定为先关驱动器，再撤销PWM输出和逆变专用计数器。 */
    HAL_GPIO_WritePin(INV_GATE_EN_GPIO_Port, INV_GATE_EN_Pin, GPIO_PIN_RESET);

    if (hhrtim1.Instance == HRTIM1) {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, INV_HRTIM_INV_COUNTERS);
    }

    inv_sampling_started = 0U;
}
