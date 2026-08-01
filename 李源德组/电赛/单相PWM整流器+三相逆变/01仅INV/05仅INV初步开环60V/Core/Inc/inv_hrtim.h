#ifndef INV_HRTIM_H
#define INV_HRTIM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "stm32g4xx_hal.h"

/*
 * 本模块只负责三相逆变HRTIM C/D/E的时基、比较值和六路PWM输出。
 * 它不生成调制指令、不处理ADC，也不假定板上存在独立Gate Enable引脚。
 */

/** 当前IOC中Timer C/D/E的周期计数，MUL4、Up-Down时对应10 kHz。 */
#define INV_HRTIM_PERIOD_COUNTS (34000U)

/** 低压初调的软件占空比边界，防止比较值进入周期端点附近。 */
#define INV_HRTIM_DUTY_MIN      (0.05f)
#define INV_HRTIM_DUTY_MAX      (0.95f)

/**
 * @brief 启动Master和Timer C/D/E计数器，建立10 kHz ADC Trigger 2时基。
 * @retval HAL_OK 计数器启动成功；其他值表示HRTIM句柄或HAL启动失败。
 * @note 在ADC3/4/5 DMA均已等待外部触发后调用。函数会关闭六路PWM输出，
 *       但“计数器运行”不等于“PWM引脚已经开放”。禁止在高频ISR中调用。
 */
HAL_StatusTypeDef INV_HRTIM_StartTimeBase(void);

/**
 * @brief 在50%初始占空比下开放Timer C/D/E六路互补PWM输出。
 * @retval HAL_OK 输出已开放；HAL_ERROR表示时基未启动、FLT3有效或参数异常。
 * @note 当前板没有PE1 Gate Enable，调用成功即表示MCU PWM引脚开始输出。
 *       只能在ADC标定、PB10和全部启动检查通过后由应用状态机调用。
 */
HAL_StatusTypeDef INV_HRTIM_EnableOutputs(void);

/**
 * @brief 把U/V/W占空比换算并写入Timer C/D/E CMP1预装载寄存器。
 * @param duty_u U相占空比，无量纲，合法范围0.05～0.95。
 * @param duty_v V相占空比，无量纲，合法范围0.05～0.95。
 * @param duty_w W相占空比，无量纲，合法范围0.05～0.95。
 * @retval HAL_OK 三相参数有效并完成写入；HAL_ERROR表示参数非法。
 * @note 无等待、无阻塞，允许在10 kHz ADC DMA完成中断路径调用。
 */
HAL_StatusTypeDef INV_HRTIM_SetDuty(float duty_u,
                                    float duty_v,
                                    float duty_w);

/**
 * @brief 直接写入U/V/W三相CMP1计数值。
 * @param cmp_u Timer C比较值，允许范围1700～32300。
 * @param cmp_v Timer D比较值，允许范围1700～32300。
 * @param cmp_w Timer E比较值，允许范围1700～32300。
 * @retval HAL_OK 参数有效并完成写入；HAL_ERROR表示句柄或范围非法。
 * @note 三个值先统一校验再写入，并在IOC配置的更新边界同步生效。
 */
HAL_StatusTypeDef INV_HRTIM_SetCompare(uint16_t cmp_u,
                                       uint16_t cmp_v,
                                       uint16_t cmp_w);

/**
 * @brief 立即关闭Timer C/D/E六路PWM输出，但保留计数器和ADC采样时基。
 * @note 用于ADC、AWD、FLT3和控制计算故障的第一步关断。函数无等待，
 *       允许在ISR中调用；保留采样便于故障锁存后的低速诊断。
 */
void INV_HRTIM_DisableOutputs(void);

/**
 * @brief 关闭六路PWM并停止Timer C/D/E计数器，保留共享Master。
 * @note 用于HardFault、CSS等不可恢复异常或系统完全停机路径。
 */
void INV_HRTIM_StopInverterTimers(void);

#ifdef __cplusplus
}
#endif

#endif /* INV_HRTIM_H */
