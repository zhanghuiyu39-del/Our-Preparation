#ifndef PFC_CONTROL_H
#define PFC_CONTROL_H

/* PI+PR双闭环公共契约：10 kHz ISR写控制状态，主循环只读取遥测快照。 */

#include <stdint.h>
#include "stm32g4xx_hal.h"
#include "pfc_measure.h"
#include "pfc_params.h"

/** @brief 双闭环控制器的运行阶段，由PFC应用状态机显式切换。 */
typedef enum
{
    PFC_CONTROL_IDLE = 0,       /* 控制器已复位，不写HRTIM Compare。 */
    PFC_CONTROL_CURRENT_RAMP = 1, /* PR电流环探测，RMS指令由0按参数斜率增加。 */
    PFC_CONTROL_VBUS_RAMP = 2,    /* PI外环已投入，VBUS参考斜坡上升。 */
    PFC_CONTROL_VBUS_RUN = 3,     /* 参数档母线目标附近的稳态双闭环。 */
    PFC_CONTROL_PRIME = 4         /* 输出关闭时跟踪VAC/VBUS前馈，等待正向过零投入。 */
} PFC_ControlMode;

/**
 * @brief F=08000等控制故障的细分原因，供OLED、VOFA和Keil Watch定位首个失败点。
 * @note  数值固定为两位十六进制诊断码D；该枚举只说明软件失败位置，不替代fault_bits。
 */
typedef enum
{
    PFC_CONTROL_FAIL_NONE = 0x00,              /* 尚未发生控制故障，正常运行时D显示00。 */
    PFC_CONTROL_FAIL_INIT_PARAM = 0x01,        /* 控制频率、工频、外环频率或参数指针无效。 */
    PFC_CONTROL_FAIL_INIT_CONTROLLER = 0x02,   /* PI、PR、陷波器系数或SPWM初始化失败。 */
    PFC_CONTROL_FAIL_NOT_INITIALIZED = 0x03,   /* 10 kHz路径开始时控制模块或参数尚未初始化。 */
    PFC_CONTROL_FAIL_MEASUREMENT = 0x04,       /* 测量指针为空或一致性快照valid为0。 */
    PFC_CONTROL_FAIL_CONTROLLER_STATE = 0x05,  /* PI或PR内部initialized标志异常。 */
    PFC_CONTROL_FAIL_MODE = 0x06,              /* 控制模式为IDLE或超出已定义范围。 */
    PFC_CONTROL_FAIL_NOTCH_NONFINITE = 0x07,   /* 100 Hz陷波器输出出现NaN或Inf。 */
    PFC_CONTROL_FAIL_PI_NONFINITE = 0x08,      /* 1 kHz PI输出出现NaN或Inf。 */
    PFC_CONTROL_FAIL_VBUS_DIVISION = 0x09,     /* VBUS不大于2 V，禁止继续作调制度除法。 */
    PFC_CONTROL_FAIL_CURRENT_REFERENCE = 0x0A, /* 瞬时电流参考出现NaN或Inf。 */
    PFC_CONTROL_FAIL_CURRENT_ERROR = 0x0B,     /* 电流误差出现NaN或Inf。 */
    PFC_CONTROL_FAIL_PR_RAW = 0x0C,            /* PR限幅前原始输出出现NaN或Inf。 */
    PFC_CONTROL_FAIL_PR_OUTPUT = 0x0D,         /* PR限幅后输出出现NaN或Inf。 */
    PFC_CONTROL_FAIL_BRIDGE_REFERENCE = 0x0E,  /* 桥侧电压参考出现NaN或Inf。 */
    PFC_CONTROL_FAIL_MODULATION = 0x0F,        /* 归一化调制度出现NaN或Inf。 */
    PFC_CONTROL_FAIL_SPWM_WRITE = 0x10,        /* SPWM限幅或HRTIM Compare写入返回失败。 */
    PFC_CONTROL_FAIL_ENTER_PRECONDITION = 0x11,/* 模式切换时初始化、测量或valid条件不满足。 */
    PFC_CONTROL_FAIL_PI_PRESET = 0x12,         /* 切入母线外环时PI无扰预置失败。 */
    PFC_CONTROL_FAIL_ENTER_MODE = 0x13,        /* 请求了未定义的控制模式。 */
    PFC_CONTROL_FAIL_CONTINUOUS_SATURATION = 0x14, /* 连续限幅达到参数档规定样本数。 */
    PFC_CONTROL_FAIL_PERIOD_SATURATION = 0x15, /* 工频周期削顶比例连续超限。 */
    PFC_CONTROL_FAIL_VBUS_REGULATION = 0x16    /* PI限幅且母线稳压误差持续超时。 */
} PFC_ControlFailureReason;

/**
 * @brief Keil Watch可直接读取的首个控制失败现场。
 * @note  由10 kHz控制ISR写；故障后保持不变，人工停机或重新初始化时清零。
 */
typedef struct
{
    uint32_t fault_bits;              /* PFC_FaultMask组合，应与OLED的F公共故障码对应。 */
    PFC_ControlFailureReason reason;  /* 两位D诊断码，先用它确定失败分支。 */
    PFC_ControlMode mode;             /* 失败时控制模式：1=电流斜坡，2=母线斜坡，3=稳态。 */
    float vbus_v;                     /* V，失败帧的母线电压；D=09时重点检查是否<=2 V。 */
    float vac_v;                      /* V，失败帧的瞬时交流输入电压。 */
    float ipfc_a;                     /* A，失败帧的瞬时输入电流。 */
    float current_rms_command_a;      /* A RMS，失败时PI输出或PR探测幅值指令。 */
    float current_reference_a;        /* A，失败帧的瞬时电流参考。 */
    float current_error_a;            /* A，current_reference_a-ipfc_a。 */
    float pr_raw_output_v;            /* V，PR限幅前输出；D=0C时通常为NaN或Inf。 */
    float pr_output_v;                /* V，PR限幅后输出。 */
    float bridge_reference_v;         /* V，VAC-PR输出得到的桥侧电压参考。 */
    float modulation;                 /* 归一化调制度；D=0F/10时重点检查该值。 */
    float vbus_used_v;                 /* V，本次控制公式实际采用的VBUS；正常时等于vbus_v。 */
    float last_rejected_vbus_v;        /* V，放宽模式最近一次被替代的异常VBUS样本。 */
    uint32_t vbus_fallback_count;      /* 放宽模式累计替代次数；只诊断，不触发故障或停PWM。 */
    uint32_t prime_start_cross_sequence;  /* PD0启动命令到达时的VAC正向过零序列号。 */
    uint32_t output_enable_cross_sequence;/* 四路PWM实际开放时的VAC正向过零序列号。 */
    float startup_vac_v;               /* V，PRIME最近一次预装载使用的瞬时VAC。 */
    float startup_vbus_v;              /* V，PRIME最近一次预装载使用的真实VBUS。 */
    float startup_modulation;          /* 归一化量，开放PWM前已提交的VAC/VBUS前馈调制度。 */
    uint8_t prime_waiting;             /* 1=仍等待合格正向过零，0=首周期Compare已经就绪。 */
    uint8_t output_enable_count;       /* 本次启动中成功开放四路PWM的次数，正常为0或1。 */
} PFC_ControlFailureSnapshot;

/** @brief 公开诊断快照，只允许在Keil Watch中读取，应用代码不得写入。 */
extern volatile PFC_ControlFailureSnapshot PFC_ControlDiagnostic;

/** @brief ISR发布给主循环、状态机和Keil Watch的一致性控制遥测。 */
typedef struct
{
    float vbus_reference;       /* V，当前母线软启动参考。 */
    float vbus_filtered;        /* V，100 Hz陷波后的母线反馈。 */
    float current_rms_command;  /* A RMS，PI输出或电流环探测斜坡值。 */
    float current_reference;    /* A，当前10 kHz瞬时电流参考。 */
    float current_error;        /* A，current_reference-IPFC。 */
    float current_error_rms;    /* A RMS，最近完整工频周期的电流误差。 */
    float pr_output_v;          /* V，PR输出的电感压降校正量。 */
    float modulation;           /* 归一化全桥调制量，范围受参数档限制。 */
    float vac_bus_ratio;        /* 0~正无穷，当前|VAC|/VBUS前馈占比；越接近调制上限，接管裕量越小。 */
    float modulation_headroom;  /* 归一化量，modulation_limit-|VAC|/VBUS；负值表示仅前馈已超出可用桥压。 */
    uint32_t fast_heartbeat;    /* 10 kHz控制成功执行次数，允许自然回绕。 */
    uint32_t outer_heartbeat;   /* 1 kHz陷波与PI执行次数，允许自然回绕。 */
    uint32_t fault_bits;        /* 控制模块发现的PFC_FaultMask组合。 */
    uint16_t saturation_count;  /* PR或调制度连续限幅的10 kHz样本数。 */
    uint16_t period_saturation_samples; /* 当前工频窗口内已经出现的限幅样本数。 */
    float period_saturation_ratio; /* 上一个完整工频周期的限幅样本占比，0~1。 */
    uint16_t vbus_regulation_error_ms; /* PI限幅且母线失调已连续保持的时间。 */
    PFC_ControlMode mode;       /* 当前控制阶段。 */
    uint8_t pi_limited;         /* 1表示本次1 kHz PI输出触及上下限。 */
    uint8_t pr_limited;         /* 1表示本次10 kHz PR输出触及上下限。 */
    uint8_t modulation_limited; /* 1表示本次调制量被SPWM限幅。 */
    uint8_t period_saturation_bad_cycles; /* 连续严重削顶的完整工频周期数。 */
    uint8_t current_loop_qualified; /* 1表示电流误差RMS已满足切外环条件。 */
    uint8_t vbus_reference_reached; /* 1表示参考已爬升到最终VBUS目标。 */
    uint8_t healthy;            /* 1表示控制器及最近一次快速路径均正常。 */
    PFC_ControlFailureReason failure_reason; /* D诊断码；首次失败后保持到人工停机或重新初始化。 */
    uint32_t prime_start_cross_sequence;   /* PD0启动时记录的VAC正向过零序列号。 */
    uint32_t output_enable_cross_sequence; /* PWM实际开放时对应的VAC正向过零序列号。 */
    float startup_vac_v;        /* V，首周期前馈预装载所用瞬时VAC。 */
    float startup_vbus_v;       /* V，首周期前馈预装载所用真实VBUS。 */
    float startup_modulation;   /* 归一化量，首周期前馈调制度。 */
    uint8_t prime_waiting;      /* 1表示PRIME仍在等过零，0表示可以提交并开放输出。 */
    uint8_t output_enable_count;/* 本次启动成功开放输出的次数。 */
} PFC_ControlTelemetry;

/**
 * @brief  初始化PI、PR、100 Hz陷波器、SPWM底层和遥测状态。
 * @param  params 常驻只读参数地址，函数返回后必须继续有效。
 * @retval HAL_OK表示所有控制器及中性Compare初始化成功。
 * @note   在ADC/DMA启动前调用；不启动HRTIM计数器或PWM输出。
 */
HAL_StatusTypeDef PFC_Control_Init(const PFC_Params *params);

/**
 * @brief  切换双闭环阶段，并完成进入该阶段所需的状态预置。
 * @param  mode 目标控制模式。
 * @param  measurement 当前一致性有效测量快照。
 * @retval HAL_OK表示切换成功，HAL_ERROR表示前置条件或参数无效。
 * @note   状态机在1 ms路径调用；函数使用短临界区避免与10 kHz ISR并发修改状态。
 */
HAL_StatusTypeDef PFC_Control_EnterMode(PFC_ControlMode mode,
                                        const PFC_Measurement *measurement);

/**
 * @brief  执行一次10 kHz PR内环，并按1/10分频执行陷波器和PI外环。
 * @param  measurement ADC1完整DMA回调刚发布的一致性测量快照。
 * @retval HAL_OK表示本周期控制与Compare写入成功，HAL_ERROR表示必须安全停机。
 * @note   只允许在ADC1 DMA ISR中调用；禁止阻塞、显示、串口和HAL_Delay()。
 */
HAL_StatusTypeDef PFC_Control_Step10k(const PFC_Measurement *measurement);

/**
 * @brief  记录HRTIM四路输出已经成功开放的启动现场。
 * @param  measurement 开放输出时使用的ADC一致性测量快照。
 * @note   只由PFC应用层在PFC_HRTIM_StartOutputs()返回HAL_OK后调用；不计算控制量，
 *         不操作HRTIM，也不新增故障或关断条件。
 */
void PFC_Control_ConfirmOutputsEnabled(const PFC_Measurement *measurement);

/**
 * @brief  清除PI/PR、陷波器、分频、RMS统计、斜坡和遥测运行状态。
 * @note   人工停机和任意故障路径调用；不关闭HRTIM输出，调用者必须先关断功率。
 */
void PFC_Control_Reset(void);

/**
 * @brief  关断故障后复位PI/PR内部状态，但保留失败瞬间遥测和D诊断码。
 * @note   只供PFC_AppTrip()使用；人工PD0停机仍调用PFC_Control_Reset()清除旧诊断。
 */
void PFC_Control_ResetPreserveFault(void);

/**
 * @brief  在短临界区内复制控制遥测快照。
 * @param  telemetry 接收快照的非空地址。
 * @note   主循环不得直接读取模块内部ISR状态；临界区不会冻结ADC DMA硬件。
 */
void PFC_Control_GetTelemetry(PFC_ControlTelemetry *telemetry);

/** @brief 返回最近一次控制路径健康状态；1为正常，0为禁止继续带功率。 */
uint8_t PFC_Control_IsHealthy(void);

#endif
