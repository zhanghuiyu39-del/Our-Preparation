#ifndef PFC_USER_CONFIG_H
#define PFC_USER_CONFIG_H

/* 兼容旧PFC模块的包装头；联合工程的日常配置统一放在pfc_inv_user_config.h。 */
#include "pfc_inv_user_config.h"

/* pfc_params.h使用的模式名称。 */
#define PFC_USER_MODE_RAW_ADC          0U
#define PFC_USER_MODE_ENGINEERING_CHECK 1U
#define PFC_USER_MODE_CLOSED_LOOP      2U

#endif /* PFC_USER_CONFIG_H */
