#ifndef PFC_MEASURE_H
#define PFC_MEASURE_H

/* 双ADC测量公共契约：DMA原始数组只供驱动层使用，应用层读取一致性快照。 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_params.h"

/*
 * DMA写入的12位右对齐原始数组。volatile不提供跨ADC同步或结构体原子性；
 * 应用层必须通过PFC_Measure_GetSnapshot()读取已经由ADC1回调发布的一致性快照。
 */
extern volatile uint16_t PFC_Adc1Dma[2]; /* [0]=IPFC，[1]=VBUS */
extern volatile uint16_t PFC_Adc2Dma[1]; /* [0]=VAC */

/** @brief 可按位组合并永久锁存到下一次复位的PFC故障集合。 */
typedef enum
{
    PFC_FAULT_NONE             = 0U,       /* 尚未记录故障。 */
    PFC_FAULT_ADC_SYNC         = 1U << 0,  /* ADC1/2序列失步或DMA停滞。 */
    PFC_FAULT_ADC_ERROR        = 1U << 1,  /* HAL ADC/DMA错误。 */
    PFC_FAULT_ADC_RANGE        = 1U << 2,  /* 原始码连续贴近ADC量程边界。 */
    PFC_FAULT_HRTIM            = 1U << 3,  /* HRTIM启动/写入失败或Fault 3事件。 */
    PFC_FAULT_OVERCURRENT      = 1U << 4,  /* 标定电流超过软件过流阈值。 */
    PFC_FAULT_VBUS_OV          = 1U << 5,  /* 母线超过软件过压阈值。 */
    PFC_FAULT_VAC_LOST         = 1U << 6,  /* VAC同步丢失或运行条件越界。 */
    PFC_FAULT_PARAM            = 1U << 7,  /* 参数地址、范围或标定状态无效。 */
    PFC_FAULT_MODULATION       = 1U << 8,  /* SPWM输入、计算或Compare写入失败。 */
    PFC_FAULT_STATE            = 1U << 9,  /* 状态机进入未定义分支。 */
    PFC_FAULT_IWDG_RESET       = 1U << 10, /* 保留的IWDG复位诊断位。 */
    PFC_FAULT_TARGET_TIMEOUT   = 1U << 11, /* 最大电流指令下仍未按时建立母线目标。 */
    PFC_FAULT_AWD_IPFC         = 1U << 12, /* ADC1 AWD1检测IPFC越窗。 */
    PFC_FAULT_AWD_VBUS         = 1U << 13, /* ADC1 AWD2检测VBUS越窗。 */
    PFC_FAULT_AWD_VAC          = 1U << 14, /* ADC2 AWD1检测VAC越窗。 */
    PFC_FAULT_CONTROL          = 1U << 15, /* PI/PR未初始化、数值异常或控制路径失败。 */
    PFC_FAULT_CURRENT_TRACKING = 1U << 16, /* 电流内环在规定时间内未通过误差检查。 */
    PFC_FAULT_CONTROL_SATURATION = 1U << 17 /* PR或调制度连续限幅超过允许时间。 */
} PFC_FaultMask;

/** @brief HAL模拟看门狗回调转换成的明确测量来源。 */
typedef enum
{
    PFC_AWD_SOURCE_IPFC = 0, /* ADC1 AWD1，监视ADC1_IN2/IPFC。 */
    PFC_AWD_SOURCE_VBUS,     /* ADC1 AWD2，监视ADC1_IN7/VBUS。 */
    PFC_AWD_SOURCE_VAC       /* ADC2 AWD1，监视ADC2_IN6/VAC。 */
} PFC_AwdSource;

/**
 * @brief  一个完整统计窗口的ADC原始码结果。
 * @note   ADC1完整DMA ISR写，主循环必须通过Getter读取；mean/min/max均不使用标定系数。
 */
typedef struct
{
    uint16_t ipfc_latest; /* count，窗口发布时的ADC1 Rank 1样本。 */
    uint16_t ipfc_mean;   /* count，窗口内IPFC算术平均值。 */
    uint16_t ipfc_min;    /* count，窗口内IPFC最小值。 */
    uint16_t ipfc_max;    /* count，窗口内IPFC最大值。 */
    uint16_t vac_latest;  /* count，窗口发布时的ADC2 Rank 1样本。 */
    uint16_t vac_mean;    /* count，窗口内VAC算术平均值。 */
    uint16_t vac_min;     /* count，窗口内VAC最小值。 */
    uint16_t vac_max;     /* count，窗口内VAC最大值。 */
    uint16_t vbus_latest; /* count，窗口发布时的ADC1 Rank 2样本。 */
    uint16_t vbus_mean;   /* count，窗口内VBUS算术平均值。 */
    uint16_t vbus_min;    /* count，窗口内VBUS最小值。 */
    uint16_t vbus_max;    /* count，窗口内VBUS最大值。 */
    uint32_t window_sequence; /* 已发布统计窗口数，允许自然回绕。 */
    uint32_t sample_count;    /* 本结果实际包含的同步ADC帧数。 */
    uint8_t valid;            /* 1表示至少完成并发布过一个完整窗口。 */
} PFC_AdcCalibrationStats;

/* ISR发布、主循环读取的一致性快照；物理量只有valid非0时才可用于控制。 */
typedef struct
{
    uint16_t ipfc_raw;       /* ADC count，ADC1 Rank 1原始码。 */
    uint16_t vbus_raw;       /* ADC count，ADC1 Rank 2原始码。 */
    uint16_t vac_raw;        /* ADC count，ADC2 Rank 1原始码。 */
    float ipfc;             /* A，瞬时输入电感电流。 */
    float ipfc_rms;         /* A，一个标称工频周期更新一次。 */
    float vac;              /* V，瞬时交流输入电压。 */
    float vac_rms;          /* V，一个标称工频周期更新一次。 */
    float vac_frequency_hz; /* Hz，由相邻VAC正向过零计算。 */
    float vbus;             /* V，直流母线电压。 */
    uint32_t adc1_sequence;  /* ADC1完整序列发布次数，10 kHz递增并允许自然回绕。 */
    uint32_t adc2_sequence;  /* 当前快照接受的ADC2完整序列号。 */
    uint32_t fast_heartbeat; /* 同步原始快照发布次数，10 kHz递增。 */
    uint32_t vac_zero_cross_sequence; /* 有效VAC正向过零次数。 */
    uint32_t fault_bits;     /* PFC_FaultMask按位锁存结果。 */
    uint8_t raw_valid;      /* 三个DMA原始码来自同一触发周期。 */
    uint8_t offset_ready;   /* 参数自洽且板级零点、比例和极性已经确认。 */
    uint8_t vac_locked;      /* 1表示相邻正向过零周期处于允许频率范围。 */
    uint8_t valid;          /* 工程量有效，可用于状态机和调制。 */
} PFC_Measurement;

/**
 * @brief  初始化DMA缓冲区、同步序列和测量统计状态。
 * @param  params 常驻只读参数地址，函数返回后必须继续有效。
 * @note   在HAL_ADC_Start_DMA()前调用；不校准ADC、不启动DMA或PWM。
 */
void PFC_Measure_Init(const PFC_Params *params);

/**
 * @brief  根据已确认参数重写ADC1/2模拟看门狗窗口。
 * @param  hadc1_handle ADC1句柄，监视IPFC和VBUS。
 * @param  hadc2_handle ADC2句柄，监视VAC。
 * @retval HAL状态；标定无效时保持CubeMX宽窗口并返回HAL_OK。
 * @note   只能在ADC校准后、HAL_ADC_Start_DMA()之前调用。
 */
HAL_StatusTypeDef PFC_Measure_ConfigureWatchdogs(ADC_HandleTypeDef *hadc1_handle,
                                                 ADC_HandleTypeDef *hadc2_handle);

/**
 * @brief  ADC1完整DMA回调入口，核对ADC2序列并发布同步快照。
 * @note   以10 kHz在DMA ISR中调用；禁止阻塞、显示和串口发送。
 */
void PFC_Measure_OnAdc1Complete(void);

/**
 * @brief  ADC2完整DMA回调入口，只推进VAC样本序列号。
 * @note   DMA已经写入VAC原始数组；本函数不复制样本或执行控制。
 */
void PFC_Measure_OnAdc2Complete(void);

/**
 * @brief  将HAL ADC/DMA错误转换为锁存故障并关闭HRTIM输出。
 * @note   可在错误ISR中调用，不尝试自动恢复ADC或功率输出。
 */
void PFC_Measure_AdcError(void);

/**
 * @brief  将模拟看门狗来源转换为明确故障位并关断输出。
 * @param  source IPFC、VBUS或VAC看门狗来源。
 * @note   可在ADC1_2 ISR中调用；不执行显示、通信或延时。
 */
void PFC_Measure_AnalogWatchdog(PFC_AwdSource source);

/**
 * @brief  按位锁存故障并执行ISR安全的HRTIM紧急关断。
 * @param  fault_bits 一个或多个PFC_FaultMask位。
 * @note   可由ISR或主循环调用；故障位只置位，不在运行中清除。
 */
void PFC_Measure_Trip(uint32_t fault_bits);

/**
 * @brief  在短临界区内复制双缓冲发布的完整测量快照。
 * @param  measurement 接收快照的非空地址。
 * @note   写入者先完整填充非活动缓冲区，再翻转发布索引，因此优先级更高的INV ISR
 *         抢占PFC DMA ISR时仍只能读到上一帧完整数据。主循环和控制模块不得绕过
 *         本接口直接读取DMA数组；临界区不冻结DMA硬件。
 */
void PFC_Measure_GetSnapshot(PFC_Measurement *measurement);

/**
 * @brief  在短临界区内复制最近一个完整ADC原始码统计窗口。
 * @param  stats 接收统计结果的非空地址。
 * @note   默认窗口约1秒；主循环不得为统计目的直接读取DMA原始数组。
 */
void PFC_Measure_GetCalibrationStats(PFC_AdcCalibrationStats *stats);

/**
 * @brief  返回当前锁存故障位。
 * @retval PFC_FaultMask按位组合；0表示尚未记录故障。
 */
uint32_t PFC_Measure_GetFault(void);

#endif
