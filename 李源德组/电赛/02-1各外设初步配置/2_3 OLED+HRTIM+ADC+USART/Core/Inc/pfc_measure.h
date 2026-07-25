#ifndef __PFC_MEASURE_H
#define __PFC_MEASURE_H

#include <stdint.h>

/* ADC1 Rank1=IPFC、Rank2=VBUS；ADC2 Rank1=VAC。DMA 缓冲顺序不可调整。 */
extern volatile uint16_t PFC_Adc1Dma[2];
extern volatile uint16_t PFC_Adc2Dma[1];

typedef enum
{
    PFC_FAULT_NONE       = 0U,
    PFC_FAULT_ADC_SYNC   = 1U << 0,
    PFC_FAULT_ADC_ERROR  = 1U << 1,
    PFC_FAULT_ADC_RANGE  = 1U << 2,
    PFC_FAULT_HRTIM      = 1U << 3,
    PFC_FAULT_DRIVER     = 1U << 4
} PFC_FaultMask;

typedef struct
{
    uint16_t ipfc_raw;
    uint16_t vbus_raw;
    uint16_t vac_raw;
    float ipfc;                 /* A，比例系数需按实际传感器填写 */
    float vac;                  /* V，比例系数需按实际分压填写 */
    float vbus;                 /* V，比例系数需按实际分压填写 */
    uint32_t adc1_sequence;
    uint32_t adc2_sequence;
    uint32_t fast_heartbeat;
    uint32_t fault_bits;
    uint8_t offset_ready;
    uint8_t valid;
} PFC_Measurement;

void PFC_Measure_Init(void);
void PFC_Measure_OnAdc1Complete(void);
void PFC_Measure_OnAdc2Complete(void);
void PFC_Measure_AdcError(void);
void PFC_Measure_Trip(uint32_t fault_bits);
void PFC_Measure_GetSnapshot(PFC_Measurement *measurement);
uint32_t PFC_Measure_GetFault(void);

#endif
