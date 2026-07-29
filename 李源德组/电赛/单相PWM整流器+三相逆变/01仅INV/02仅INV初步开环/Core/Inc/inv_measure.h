#ifndef INV_MEASURE_H
#define INV_MEASURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * 本头文件公开ADC3/4/5循环DMA缓冲区、六通道测量快照及故障处理接口。
 * DMA数组只供HAL DMA写入；应用层应使用INV_Measure_GetSnapshot()读取一致数据。
 */

/* DMA数组顺序必须与02OpenLoop.ioc的ADC3/4/5 Regular Rank完全一致。 */
extern volatile uint16_t INV_Adc3Dma[2]; /* Rank1=IU，Rank2=VUV。 */
extern volatile uint16_t INV_Adc4Dma[2]; /* Rank1=IV，Rank2=VVW。 */
extern volatile uint16_t INV_Adc5Dma[2]; /* Rank1=IW，Rank2=VWU。 */

/* 故障位使用位掩码，可同时记录首发故障和后续故障。 */
typedef enum
{
    INV_FAULT_NONE        = 0U,      /* 当前未锁存软件故障。 */
    INV_FAULT_ADC_SYNC    = 1U << 0, /* ADC3/4/5序列失步或采样心跳停止。 */
    INV_FAULT_ADC_ERROR   = 1U << 1, /* ADC或其DMA报告HAL错误。 */
    INV_FAULT_ADC_RANGE   = 1U << 2, /* 原始码超出12位ADC合法范围。 */
    INV_FAULT_HRTIM       = 1U << 3, /* HRTIM计数器或输出启动失败。 */
    INV_FAULT_DRIVER      = 1U << 4, /* 驱动器nFAULT、FLT3或安全GPIO异常。 */
    INV_FAULT_PWM_COMMAND = 1U << 5, /* 占空比/比较值非法或写入失败。 */
    INV_FAULT_CBSVPWM     = 1U << 6, /* CBSVPWM初始化或本周期计算失败。 */
    INV_FAULT_PARAMETER   = 1U << 7, /* 开环母线、线电压或30/60 Hz参数非法。 */
    INV_FAULT_CLOCK       = 1U << 8  /* CSS检测到HSE时钟故障。 */
} INV_FaultMask;

typedef struct
{
    /* ADC原始值，便于在Keil Watch中核对Rank顺序和模拟前端。 */
    uint16_t iu_raw;
    uint16_t iv_raw;
    uint16_t iw_raw;
    uint16_t vuv_raw;
    uint16_t vvw_raw;
    uint16_t vwu_raw;

    /* 完成零点扣除和比例换算后的物理量。 */
    float iu; /* U相电流，单位A。 */
    float iv; /* V相电流，单位A。 */
    float iw; /* W相电流，单位A。 */
    float vuv; /* U-V线电压，单位V。 */
    float vvw; /* V-W线电压，单位V。 */
    float vwu; /* W-U线电压，单位V。 */

    /* 三相三线一致性诊断量；标定完成后更新，首版只观测、不作为关断阈值。 */
    float current_sum;      /* IU+IV+IW，单位A，理想三线负载下接近0。 */
    float line_voltage_sum; /* VUV+VVW+VWU，单位V，按定义应接近0。 */

    /* 三路DMA序列用于诊断，fast_heartbeat每个完整六通道帧只增加一次。 */
    uint32_t adc3_sequence;
    uint32_t adc4_sequence;
    uint32_t adc5_sequence;
    uint32_t fast_heartbeat;
    uint32_t fault_bits;   /* INV_FaultMask组合；故障锁存后不会自动清零。 */
    uint8_t offset_ready;  /* 六路零点平均完成时为1。 */
    uint8_t valid;         /* 当前快照已完成零点换算且无锁存故障时为1。 */
} INV_Measurement;

/**
 * 清空DMA缓冲、同步序号、零点累加器和锁存故障。
 * 调用位置：ADC校准及HAL_ADC_Start_DMA()之前；禁止在采样运行期间调用。
 * 副作用：丢弃全部旧测量和故障记录；不访问HAL，可在PE1保持低时调用。
 */
void INV_Measure_Init(void);

/**
 * 分别通知ADC3/4/5的一次完整规则序列已由DMA写入。
 * 调用位置：HAL_ADC_ConvCpltCallback()对应ADC分支；允许且仅应在ISR中调用。
 * 副作用：三个序列均更新后发布一次快照，必要时锁存同步或范围故障。
 */
void INV_Measure_OnAdc3Complete(void);
void INV_Measure_OnAdc4Complete(void);
void INV_Measure_OnAdc5Complete(void);

/**
 * ADC或DMA错误统一入口，无参数。
 * 调用位置：HAL_ADC_ErrorCallback()；允许在ISR中调用。
 * 副作用：锁存INV_FAULT_ADC_ERROR并立即关闭逆变功率级。
 */
void INV_Measure_AdcError(void);

/**
 * 锁存一个或多个INV_FaultMask故障位并关闭逆变功率级。
 * fault_bits：位掩码，无单位；函数不自动清除故障。
 * 调用位置：任意Fault或控制错误路径；无等待，允许在ISR中调用。
 */
void INV_Measure_Trip(uint32_t fault_bits);

/**
 * 将ISR维护的完整测量快照复制到调用者结构体。
 * measurement：输出地址，不得为NULL；可在主循环或ADC完成ISR中调用。
 * 副作用：短暂屏蔽中断以保证结构体一致，随后恢复进入函数前的中断状态。
 * ISR用法：三个ADC分支均可复制后比较fast_heartbeat，仅新帧分支运行一次控制。
 */
void INV_Measure_GetSnapshot(INV_Measurement *measurement);

/**
 * 返回当前锁存故障位，0表示本模块未记录软件故障。
 * 调用位置：主循环或安全监督；只读、无阻塞，允许在ISR中调用。
 */
uint32_t INV_Measure_GetFault(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_MEASURE_H */
