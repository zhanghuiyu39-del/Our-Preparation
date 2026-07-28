#ifndef SPWM_H
#define SPWM_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 初始化单极性倍频SPWM模块。
 * open_loop_amplitude是开环正弦调制度，允许范围为0.0f～0.90f。
 * 本函数只写HRTIM预装载比较值，不启动计数器、PWM输出或门极使能。
 */
void SPWM_Init(float open_loop_amplitude);

/* 修改开环试波调制度，不改变当前正弦相位。 */
void SPWM_SetOpenLoopAmplitude(float amplitude);

/*
 * 推进一步50 Hz开环正弦调制。
 * 当前工程必须以10 kHz固定频率调用本函数一次，不能重复调用。
 */
void SPWM_OpenLoopStep(void);

/*
 * 将瞬时归一化调制量写入HRTIM Timer A/B Compare 1。
 * modulation会限制到-0.90f～+0.90f，返回值是实际应用的调制量。
 * 后续闭环控制直接调用本接口，不再调用SPWM_OpenLoopStep()。
 */
float SPWM_ApplyModulation(float modulation);

/* 清零开环相位，并把两个桥臂恢复到50%中性比较值。 */
void SPWM_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* SPWM_H */
