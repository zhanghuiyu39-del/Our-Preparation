#ifndef PFC_MEASURE_H
#define PFC_MEASURE_H

#include <stdint.h>
#include "pfc_params.h"

/* DMA写入的原始数组；应用层不得直接读取，必须通过PFC_Measure_GetSnapshot()。 */
extern volatile uint16_t PFC_Adc1Dma[2]; /* [0]=IPFC，[1]=VBUS */
extern volatile uint16_t PFC_Adc2Dma[1]; /* [0]=VAC */

typedef enum
{
    PFC_FAULT_NONE        = 0U,
    PFC_FAULT_ADC_SYNC    = 1U << 0,  /* ADC2样本连续3次未随ADC1更新 */
    PFC_FAULT_ADC_ERROR   = 1U << 1,  /* HAL报告ADC或DMA错误 */
    PFC_FAULT_ADC_RANGE   = 1U << 2,  /* 双极采样通道靠近ADC电源轨 */
    PFC_FAULT_HRTIM       = 1U << 3,  /* HRTIM启动失败或Fault 3回调 */
    PFC_FAULT_DRIVER      = 1U << 4,  /* PE5驱动器诊断故障 */
    PFC_FAULT_OVERCURRENT = 1U << 5,  /* IPFC超过标定后的软件阈值 */
    PFC_FAULT_VBUS_OV     = 1U << 6,  /* VBUS超过软件过压阈值 */
    PFC_FAULT_VAC_LOST    = 1U << 7,  /* 运行中VAC过零锁定丢失 */
    PFC_FAULT_PARAM       = 1U << 8,  /* 保留给运行期参数校验失败 */
    PFC_FAULT_MODULATION  = 1U << 9,  /* SPWM输入非有限或计算无效 */
    PFC_FAULT_STATE       = 1U << 10, /* 状态枚举进入非法分支 */
    PFC_FAULT_IWDG_RESET  = 1U << 11  /* 保留给复位原因记录 */
} PFC_FaultMask;

/* ISR发布、主循环读取的一致性测量快照；SI物理量来自PFC_Params中的板级标定。 */
typedef struct
{
    uint16_t ipfc_raw;
    uint16_t vbus_raw;
    uint16_t vac_raw;
    float ipfc;             /* A，瞬时值 */
    float ipfc_rms;         /* A，一个标称50 Hz窗口更新一次 */
    float vac;              /* V，瞬时值 */
    float vac_rms;          /* V，一个标称50 Hz窗口更新一次 */
    float vac_frequency_hz; /* Hz，由相邻VAC正向过零计算 */
    float vbus;             /* V */
    uint32_t adc1_sequence; /* 每次成功发布快照加1 */
    uint32_t adc2_sequence; /* 最近被ADC1采用的VAC序列 */
    uint32_t fast_heartbeat;
    uint32_t vac_zero_cross_sequence;
    uint32_t fault_bits;
    uint8_t offset_ready;   /* 已装载参数档零点，不代表calibration_valid */
    uint8_t vac_locked;
    uint8_t valid;
} PFC_Measurement;

void PFC_Measure_Init(const PFC_Params *params);
void PFC_Measure_OnAdc1Complete(void);
void PFC_Measure_OnAdc2Complete(void);
void PFC_Measure_AdcError(void);
void PFC_Measure_Trip(uint32_t fault_bits);
void PFC_Measure_GetSnapshot(PFC_Measurement *measurement);
uint32_t PFC_Measure_GetFault(void);

#endif
