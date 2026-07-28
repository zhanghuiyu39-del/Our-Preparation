#ifndef INV_HRTIM_H
#define INV_HRTIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "stm32g4xx_hal.h"

/*
 * 本头文件提供三相逆变HRTIM C/D/E的采样时基、占空比更新、功率级启停接口。
 * 调用者负责CBSVPWM计算、安全状态机及故障恢复，本模块不自动重新启动输出。
 */

/* 当前01.ioc固定使用10 kHz、中心对齐、Period=34000的HRTIM C/D/E。 */
#define INV_HRTIM_PERIOD_COUNTS (34000U)
#define INV_HRTIM_DUTY_MIN      (0.05f)
#define INV_HRTIM_DUTY_MAX      (0.95f)

/**
 * 启动Master与Timer C/D/E计数器，为ADC Trigger 2提供10 kHz时基。
 * 返回值：HAL_OK表示计数器已启动，否则返回HAL_ERROR/HAL_BUSY。
 * 调用位置：ADC3/4/5 DMA启动后、功率输出使能前；不建议在ISR中调用。
 * 副作用：强制PE1为低并关闭C/D/E六路输出，不启动功率级。
 */
HAL_StatusTypeDef INV_HRTIM_StartSampling(void);

/**
 * 将CBSVPWM输出的U/V/W占空比写入HRTIM C/D/E CMP1预装载寄存器。
 * duty_u/v/w：无量纲，占空比必须为有限数且位于0.05～0.95。
 * 返回值：输入有效并完成写入时返回HAL_OK，否则返回HAL_ERROR且不写寄存器。
 * 调用位置：10 kHz控制ISR；函数无等待，允许在ISR中调用。
 */
HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w);

/**
 * 直接写入U/V/W三相的CMP1计数值，允许范围为1700～32300。
 * cmp_u/v/w：HRTIM计数值，无物理单位。
 * 返回值：全部参数有效时返回HAL_OK，否则返回HAL_ERROR且不写寄存器。
 * 调用位置：底层调试或10 kHz控制ISR；允许在ISR中调用。
 */
HAL_StatusTypeDef INV_HRTIM_SetCompare(uint16_t cmp_u,
                                       uint16_t cmp_v,
                                       uint16_t cmp_w);

/**
 * 以50%中性占空比启动Timer C/D/E六路PWM，确认FLT3正常后拉高PE1。
 * 返回值：功率输出已开放返回HAL_OK；计数器未启动或检测到Fault时返回HAL_ERROR。
 * 调用位置：零点标定、自检和驱动器状态均正常后的状态机；禁止在ISR中调用。
 * 副作用：可能启动PWM引脚并拉高Gate Enable，调用前必须满足全部带功率条件。
 */
HAL_StatusTypeDef INV_HRTIM_EnablePowerStage(void);

/**
 * 紧急关闭逆变功率级：先拉低PE1，再关闭C/D/E输出和计数器。
 * 调用位置：Fault、ADC/DMA错误、停机和Error_Handler；允许在ISR中调用。
 * 副作用：保留Master运行，避免破坏可能共享Master的PFC时基。
 */
void INV_HRTIM_StopAll(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_HRTIM_H */
