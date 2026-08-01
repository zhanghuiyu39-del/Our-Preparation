#ifndef PFC_PARAMS_H
#define PFC_PARAMS_H

/* 编译期参数档及板级标定契约；本接口不提供在线修改或Flash保存。 */

#include <stdint.h>
#include "pfc_user_config.h"

/** @brief 单相PWM整流器编译期参数档编号。 */
typedef enum
{
    PFC_PROFILE_5V_TEST = 0,  /* 5 V RMS、30 ohm负载的隔离低压闭环试验。 */
    PFC_PROFILE_36V_LIGHT_100R, /* 36 V RMS、60 V母线、100 ohm轻载调试档。 */
    PFC_PROFILE_CONTEST_36V     /* 36 V RMS赛题满功率档骨架，完成标定和保护前禁止启用。 */
} PFC_ProfileId;

/** @brief 编译期运行模式；模式只决定工程量与功率许可，不改变ADC/HRTIM时基。 */
typedef enum
{
    PFC_RUN_MODE_RAW_ADC = PFC_USER_MODE_RAW_ADC,
    PFC_RUN_MODE_ENGINEERING_CHECK = PFC_USER_MODE_ENGINEERING_CHECK,
    PFC_RUN_MODE_CLOSED_LOOP = PFC_USER_MODE_CLOSED_LOOP
} PFC_RunMode;

/** @brief 参数检查错误位；可组合显示，0表示全部数值自洽。 */
typedef enum
{
    PFC_PARAM_ERROR_NONE        = 0U,
    PFC_PARAM_ERROR_MODE        = 1U << 0,
    PFC_PARAM_ERROR_ELECTRICAL  = 1U << 1,
    PFC_PARAM_ERROR_CONTROL     = 1U << 2,
    PFC_PARAM_ERROR_CALIBRATION = 1U << 3,
    PFC_PARAM_ERROR_POLARITY    = 1U << 4,
    PFC_PARAM_ERROR_PROTECTION  = 1U << 5
} PFC_ParamValidationError;

/*
 * 所有物理量使用SI单位。该结构体由pfc_params.c持有，其他模块只保存const指针。
 * ADC比例、零点和极性必须来自实际板卡标定；标定未确认或非闭环模式时，软件只能
 * 执行对应的采样/工程量复核流程，不能开放HRTIM功率输出。
 */
typedef struct
{
    PFC_ProfileId profile_id;
    PFC_RunMode run_mode;             /* 编译期运行模式，运行中不可切换。 */
    uint8_t calibration_confirmed;    /* 1表示当前档零点、比例、极性和阈值已实测。 */

    float vac_nominal_rms;          /* V RMS，输入交流额定有效值。 */
    float grid_frequency_hz;        /* Hz，输入交流额定频率。 */
    float grid_frequency_tolerance; /* Hz，允许建立VAC同步的频率偏差。 */
    float vac_peak_trip;            /* V peak，VAC严重越界的绝对值阈值。 */

    float vbus_target;               /* V，PI外环最终母线电压参考。 */
    float vbus_start_min;            /* V，允许开放PWM前的被动预充最低值。 */
    float vbus_run_min;              /* V，运行时母线跌落关断阈值。 */
    float vbus_overvoltage_warn;     /* V，仅用于显示和遥测。 */
    float vbus_overvoltage_trip;     /* V，软件与ADC AWD2过压关断阈值。 */
    float vbus_target_tolerance;     /* V，判断开环目标是否建立的容差。 */

    float load_resistance;       /* ohm，当前低压试验的直流负载。 */
    float input_inductance;      /* H，交流侧电感。 */
    float bus_capacitance;       /* F，直流母线电容。 */
    float control_frequency;     /* Hz，ADC1完整DMA回调与快速控制频率。 */
    float modulation_limit;      /* 归一化差分调制量绝对值上限，范围0~1。 */
    float current_command_target;/* A peak，换算PI最大RMS电流指令的峰值上限。 */
    float current_ramp_a_per_s;  /* A/s，启动时电流峰值指令爬升速度。 */
    float current_trip;          /* A peak，软件与ADC AWD1过流阈值。 */

    float current_pr_kp;              /* V/A，PR比例增益。 */
    float current_pr_kr;              /* V/A，PR谐振增益。 */
    float current_pr_bandwidth_rad_s; /* rad/s，准PR谐振带宽Wc。 */
    float current_pr_output_limit_v;  /* V，PR电感压降校正量绝对值上限。 */
    float voltage_pi_kp;              /* A/V，母线PI比例增益，输出为A RMS。 */
    float voltage_pi_ki;              /* A/(V*s)，母线PI连续域积分增益。 */
    float voltage_loop_frequency_hz;  /* Hz，PI与100 Hz陷波器实际调用频率。 */
    float vbus_notch_damping_rad_s;   /* rad/s，100 Hz陷波器分母一次项系数。 */
    float vbus_reference_slew_v_s;    /* V/s，母线参考软启动斜率。 */
    float current_probe_rms;          /* A RMS，投入外环前的电流环探测指令。 */
    float current_probe_slew_a_s;     /* A RMS/s，探测电流指令爬升速度。 */
    float current_error_rms_max;      /* A RMS，允许切入外环的电流误差上限。 */
    uint16_t current_probe_min_ms;    /* ms，电流环投入后的最短观察时间。 */
    uint16_t current_loop_timeout_ms; /* ms，电流环未通过检查的超时。 */
    uint16_t vbus_build_timeout_ms;   /* ms，母线闭环未建立目标的超时。 */
    uint16_t saturation_trip_samples;/* 10 kHz样本数，连续限幅关断门槛。 */
    float period_saturation_ratio;   /* 0~1，一个工频周期允许的限幅样本占比。 */
    uint8_t period_saturation_cycles;/* 个，连续严重削顶多少个工频周期后关断。 */
    uint16_t vbus_regulation_timeout_ms; /* ms，稳态PI限幅且母线失调的关断超时。 */

    float ipfc_amp_per_count; /* A/count，带符号换算前的正比例系数。 */
    float vac_volt_per_count; /* V/count，带符号换算前的正比例系数。 */
    float vbus_volt_per_count;/* V/count，单极性母线采样比例。 */
    uint16_t ipfc_zero_count; /* ADC count，确认零电流时的IPFC码。 */
    uint16_t vac_zero_count;  /* ADC count，确认零输入电压时的VAC码。 */
    int8_t ipfc_polarity;     /* +1或-1，使换算后的正电流方向符合定义。 */
    int8_t vac_polarity;      /* +1或-1，使换算后的VAC极性符合定义。 */
    int8_t bridge_polarity;   /* +1或-1，匹配全桥接线与调制正方向。 */
} PFC_Params;

/**
 * @brief  返回当前编译选择的PFC参数档。
 * @retval 常驻只读参数地址，不会返回NULL。
 * @note   活动参数档只允许在pfc_params.c中编译期选择，运行中不得切换。
 */
const PFC_Params *PFC_Params_GetActive(void);

/**
 * @brief  检查参数数值是否自洽，不把标定确认或运行模式当成功率许可。
 * @param  params 待检查的常驻参数地址。
 * @retval 1表示所有数值范围自洽，0表示至少存在一个参数类别错误。
 * @note   只检查软件数值自洽性，不能代替板级实测。
 */
uint8_t PFC_Params_Validate(const PFC_Params *params);

/**
 * @brief  检查ADC工程量换算是否允许使用。
 * @param  params 待检查的常驻参数地址。
 * @retval 1表示参数自洽且当前档标定已确认，否则返回0。
 */
uint8_t PFC_Params_CalibrationReady(const PFC_Params *params);

/**
 * @brief  检查状态机是否允许评估并开放PWM输出。
 * @param  params 待检查的常驻参数地址。
 * @retval 1仅表示闭环模式、标定确认和参数检查全部通过。
 * @note   返回1仍不代表VAC、VBUS、Fault和人工启动等实时条件已经满足。
 */
uint8_t PFC_Params_PowerAllowed(const PFC_Params *params);

/**
 * @brief  返回参数数值检查的错误类别位图。
 * @param  params 待检查的常驻参数地址。
 * @retval PFC_ParamValidationError按位组合；0表示数值自洽。
 */
uint32_t PFC_Params_GetValidationError(const PFC_Params *params);

#endif
