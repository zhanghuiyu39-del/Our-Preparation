#ifndef PFC_PARAMS_H
#define PFC_PARAMS_H

/* 编译期参数档及板级标定契约；本接口不提供在线修改或Flash保存。 */

#include <stdint.h>

/** @brief 单相PWM整流器编译期参数档编号。 */
typedef enum
{
    PFC_PROFILE_5V_TEST = 0,  /* 5 V RMS、30 ohm负载的隔离低压闭环试验。 */
    PFC_PROFILE_CONTEST_36V   /* 36 V RMS赛题档骨架，完成标定和闭环前禁止启用。 */
} PFC_ProfileId;

/*
 * 所有物理量使用SI单位。该结构体由pfc_params.c持有，其他模块只保存const指针。
 * ADC比例、零点和极性必须来自实际板卡标定；calibration_valid为0时，软件只能
 * 运行ADC/DMA、OLED和VOFA标定流程，不能开放HRTIM功率输出。
 */
typedef struct
{
    PFC_ProfileId profile_id;

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

    float ipfc_amp_per_count; /* A/count，带符号换算前的正比例系数。 */
    float vac_volt_per_count; /* V/count，带符号换算前的正比例系数。 */
    float vbus_volt_per_count;/* V/count，单极性母线采样比例。 */
    uint16_t ipfc_zero_count; /* ADC count，确认零电流时的IPFC码。 */
    uint16_t vac_zero_count;  /* ADC count，确认零输入电压时的VAC码。 */
    int8_t ipfc_polarity;     /* +1或-1，使换算后的正电流方向符合定义。 */
    int8_t vac_polarity;      /* +1或-1，使换算后的VAC极性符合定义。 */
    int8_t bridge_polarity;   /* +1或-1，匹配全桥接线与调制正方向。 */
    uint8_t calibration_valid;/* 1表示比例、零点、极性和阈值均已实测确认。 */
} PFC_Params;

/**
 * @brief  返回当前编译选择的PFC参数档。
 * @retval 常驻只读参数地址，不会返回NULL。
 * @note   活动参数档只允许在pfc_params.c中编译期选择，运行中不得切换。
 */
const PFC_Params *PFC_Params_GetActive(void);

/**
 * @brief  检查参数范围和板级标定确认标志。
 * @param  params 待检查的常驻参数地址。
 * @retval 1表示允许进入带功率状态，0表示只能停留在标定状态。
 * @note   只检查软件数值自洽性，不能代替板级实测。
 */
uint8_t PFC_Params_IsValid(const PFC_Params *params);

#endif
