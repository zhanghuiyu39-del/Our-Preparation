#include "inv_hrtim.h"

#include <float.h>

#include "hrtim.h"

/*
 * 本文件只负责“占空比 -> CMP1预装载 -> HRTIM C/D/E互补PWM输出”。
 * 数据流如下：
 * CBSVPWM duty_u/v/w -> 反向换算CMP1 -> C/D/E预装载寄存器
 *                    -> IOC指定的更新边界同步生效 -> 六路互补PWM
 *
 * Master提供ADC Trigger 2公共时基，Timer C/D/E分别对应U/V/W桥臂。
 * 当前PCB没有独立Gate Enable，因此软件只能区分“计数器运行”和
 * “MCU PWM引脚开放”两个状态；本模块不访问任何普通GPIO。
 */

/** U/V/W三桥臂的六路HRTIM物理输出，不包含HRTIM F。 */
#define INV_HRTIM_OUTPUTS (HRTIM_OUTPUT_TC1 | HRTIM_OUTPUT_TC2 | \
                           HRTIM_OUTPUT_TD1 | HRTIM_OUTPUT_TD2 | \
                           HRTIM_OUTPUT_TE1 | HRTIM_OUTPUT_TE2)

/** 建立逆变采样时基时同时启动Master和C/D/E。 */
#define INV_HRTIM_TIMEBASE_COUNTERS (HRTIM_TIMERID_MASTER | \
                                     HRTIM_TIMERID_TIMER_C | \
                                     HRTIM_TIMERID_TIMER_D | \
                                     HRTIM_TIMERID_TIMER_E)

/** 故障停机只停止逆变专用C/D/E，避免影响后续与PFC共享的Master。 */
#define INV_HRTIM_INVERTER_COUNTERS (HRTIM_TIMERID_TIMER_C | \
                                     HRTIM_TIMERID_TIMER_D | \
                                     HRTIM_TIMERID_TIMER_E)

/** 5%～95%占空比按反向映射对应CMP1=32300～1700。 */
#define INV_HRTIM_COMPARE_MIN (1700U)
#define INV_HRTIM_COMPARE_MAX (32300U)
#define INV_HRTIM_NEUTRAL_CMP (17000U)

/* 仅由初始化/故障状态机写入，高频ISR只读取，表示C/D/E时基是否已启动。 */
static volatile uint8_t inv_timebase_started = 0U;
/* 输出开放命令的软件镜像；Fault硬件可在ISR到来前异步关闭实际引脚。 */
static volatile uint8_t inv_outputs_enabled = 0U;

/** 检查单精度参数是否为有限数，避免NaN或无穷转换成比较寄存器值。 */
static uint8_t INV_HRTIM_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX)) ? 1U : 0U;
}

/** 读取FLT3硬件标志；本模块只判断，不清除公共故障记录。 */
static uint8_t INV_HRTIM_IsFault3Set(void)
{
    return (__HAL_HRTIM_GET_FLAG(&hhrtim1, HRTIM_FLAG_FLT3) != RESET) ? 1U : 0U;
}

/** 当前Set/Reset极性下，桥臂占空比使用CMP=Period*(1-duty)反向映射。 */
static uint16_t INV_HRTIM_DutyToCompare(float duty)
{
    float compare = (float)INV_HRTIM_PERIOD_COUNTS * (1.0f - duty);
    return (uint16_t)(compare + 0.5f);
}

HAL_StatusTypeDef INV_HRTIM_StartTimeBase(void)
{
    HAL_StatusTypeDef status;

    if (hhrtim1.Instance != HRTIM1) {
        inv_timebase_started = 0U;
        return HAL_ERROR;
    }

    /* 先撤销输出，再启动计数器，避免复位前残留的输出使能状态泄漏到本次启动。 */
    (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
    status = HAL_HRTIM_WaveformCounterStart(&hhrtim1, INV_HRTIM_TIMEBASE_COUNTERS);
    inv_timebase_started = (status == HAL_OK) ? 1U : 0U;
    return status;
}

void INV_HRTIM_AttachSharedTimeBase(void)
{
    inv_timebase_started = (hhrtim1.Instance == HRTIM1) ? 1U : 0U;
    inv_outputs_enabled = 0U;
}

HAL_StatusTypeDef INV_HRTIM_EnableOutputs(void)
{
    HAL_StatusTypeDef status;

    if ((hhrtim1.Instance != HRTIM1) ||
        (inv_timebase_started == 0U) ||
        (INV_HRTIM_IsFault3Set() != 0U)) {
        return HAL_ERROR;
    }

    /* 六路输出开放前统一写入50%，避免使用上一次调试遗留的比较值。 */
    if (INV_HRTIM_SetCompare(INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP,
                             INV_HRTIM_NEUTRAL_CMP) != HAL_OK) {
        return HAL_ERROR;
    }

    status = HAL_HRTIM_WaveformOutputStart(&hhrtim1, INV_HRTIM_OUTPUTS);
    if (status != HAL_OK) {
        INV_HRTIM_DisableOutputs();
        return status;
    }

    /* 覆盖输出开放边界附近到来的Fault，失败后不允许自动重试。 */
    if (INV_HRTIM_IsFault3Set() != 0U) {
        INV_HRTIM_DisableOutputs();
        return HAL_ERROR;
    }

    inv_outputs_enabled = 1U;

    return HAL_OK;
}

HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w)
{
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

    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_C,
                           HRTIM_COMPAREUNIT_1, cmp_u);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_D,
                           HRTIM_COMPAREUNIT_1, cmp_v);
    __HAL_HRTIM_SETCOMPARE(&hhrtim1, HRTIM_TIMERINDEX_TIMER_E,
                           HRTIM_COMPAREUNIT_1, cmp_w);
    return HAL_OK;
}

void INV_HRTIM_DisableOutputs(void)
{
    if (hhrtim1.Instance == HRTIM1) {
        (void)HAL_HRTIM_WaveformOutputStop(&hhrtim1, INV_HRTIM_OUTPUTS);
    }
    inv_outputs_enabled = 0U;
}

uint8_t INV_HRTIM_OutputsEnabled(void)
{
    return inv_outputs_enabled;
}

void INV_HRTIM_EmergencyOff(void)
{
    inv_outputs_enabled = 0U;
    HRTIM1->sCommonRegs.ODISR = HRTIM_ODISR_TC1ODIS | HRTIM_ODISR_TC2ODIS |
                                HRTIM_ODISR_TD1ODIS | HRTIM_ODISR_TD2ODIS |
                                HRTIM_ODISR_TE1ODIS | HRTIM_ODISR_TE2ODIS;
}

void INV_HRTIM_StopInverterTimers(void)
{
    INV_HRTIM_DisableOutputs();

    if (hhrtim1.Instance == HRTIM1) {
        (void)HAL_HRTIM_WaveformCounterStop(&hhrtim1, INV_HRTIM_INVERTER_COUNTERS);
    }

    inv_timebase_started = 0U;
}
