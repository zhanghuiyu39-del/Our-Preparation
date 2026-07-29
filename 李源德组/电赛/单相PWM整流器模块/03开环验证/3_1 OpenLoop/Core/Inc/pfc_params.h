#ifndef PFC_PARAMS_H
#define PFC_PARAMS_H

#include <stdint.h>

typedef enum
{
    PFC_PROFILE_5V_TEST = 0,  /* 当前5 V RMS隔离低压试验档。 */
    PFC_PROFILE_CONTEST_36V  /* 赛题36 V档仅保留接口，尚未建立可运行参数。 */
} PFC_ProfileId;

/*
 * 所有物理参数使用SI单位；ADC比例和极性必须来自实际模拟前端标定。
 * 结构体由参数模块持有，测量、状态机和SPWM只保存const指针，不在运行期间修改。
 */
typedef struct
{
    PFC_ProfileId profile_id;
    float vac_nominal_rms;       /* V RMS */
    float grid_frequency_hz;     /* Hz */
    float vbus_target;           /* V */
    float vbus_start_min;        /* V，被动预充最低启动门槛 */
    float vbus_overvoltage_warn; /* V，仅监测 */
    float vbus_overvoltage_trip; /* V，软件关断阈值 */
    float load_resistance;       /* ohm */
    float input_inductance;      /* H */
    float bus_capacitance;       /* F */
    float control_frequency;     /* Hz，与ADC1完整DMA回调频率一致 */
    float modulation_limit;      /* 归一化调制量绝对值上限，0~1 */
    float current_command_start; /* A peak */
    float current_command_max;   /* A peak */
    float current_trip;          /* A peak，软件过流阈值 */
    float ipfc_amp_per_count;    /* A/count */
    float vac_volt_per_count;    /* V/count */
    float vbus_volt_per_count;   /* V/count */
    uint16_t ipfc_zero_count;    /* ADC count，确认无输入电流时标定 */
    uint16_t vac_zero_count;     /* ADC count，断开交流输入时标定 */
    int8_t ipfc_polarity;        /* 只能为+1或-1 */
    int8_t vac_polarity;         /* 只能为+1或-1 */
    int8_t bridge_polarity;      /* 只能为+1或-1，匹配实际桥臂接线 */
    uint8_t calibration_valid;   /* 1表示板级比例、零点、极性和阈值均已实测确认 */
} PFC_Params;

/**
 * @brief  返回当前编译使用的低压试验参数。
 * @retval 常驻只读参数地址，不会返回NULL。
 * @note   板级ADC比例未实测前calibration_valid必须保持0，状态机不会开放Gate Enable。
 */
const PFC_Params *PFC_Params_GetActive(void);

/**
 * @brief  检查标定确认标志、SI参数范围、ADC零点和三个极性字段。
 * @param  params 待检查参数地址。
 * @retval 1表示允许状态机继续，0表示只能停留在标定状态。
 */
uint8_t PFC_Params_IsValid(const PFC_Params *params);

#endif
