#include "inv_open_loop.h"

#include <float.h>
#include <stddef.h>

#include "stm32g4xx.h"

/*
 * 本文件只负责“输出频率和线电压目标 -> 三相开环相电压指令”的计算，
 * 不负责ADC采样、CBSVPWM零序注入、HRTIM占空比更新或板级GPIO控制。
 * main.c在每个新的ADC同步帧到来后调用INV_OpenLoop_Step()，再把本模块生成的
 * vu/vv/vw_command交给CBSVPWM_Calc3Leg()计算三桥臂占空比。
 *
 * 数据流如下：
 * 用户配置的母线和线电压目标 -> 相电压峰值/目标调制度
 * 30/60 Hz频率参数 -> 32位DDS相位 -> 四分之一波正弦表插值
 *                  -> U/V/W三相单位正弦 -> 1 s软启动
 *                  -> U/V/W三相相电压指令 -> main.c/CBSVPWM
 *
 * 10 kHz下60 Hz和30 Hz都不是整数采样点周期，因此不能再使用固定点数清零法。
 * 本模块把uint32_t的0~2^32看成0~360度电角度：每调用一次Step就增加phase_step，
 * 超过uint32_t最大值后自然回到0。这样可以保持10 kHz控制节拍，同时得到准确的
 * 30 Hz或60 Hz平均输出频率，不需要在某个采样点强制把正弦状态清零。
 */

/* 2^32分别乘1/3和2/3，表示相差120度和240度的DDS相位偏移。 */
#define INV_OPEN_LOOP_PHASE_120_DEG       (0x55555555UL)
#define INV_OPEN_LOOP_PHASE_240_DEG       (0xAAAAAAABUL)

/* ======================== 四分之一波查表参数 ======================== */
/* 0x40000000和0x80000000分别对应90度和180度，用于把四个象限折叠到第一象限。 */
#define INV_OPEN_LOOP_QUARTER_PHASE       (0x40000000UL)
#define INV_OPEN_LOOP_HALF_PHASE          (0x80000000UL)

/* 第一象限划分为64段：高6位选表项，低24位计算相邻表项之间的插值比例。 */
#define INV_OPEN_LOOP_LUT_INDEX_SHIFT     (24U)
#define INV_OPEN_LOOP_LUT_FRACTION_MASK   (0x00FFFFFFUL)
#define INV_OPEN_LOOP_LUT_FRACTION_SCALE  (1.0f / 16777216.0f)

/* ======================== 电压换算与调制度限制 ======================== */
/* 三相对称三线制中Vphase_peak=Vline_rms*sqrt(2/3)。 */
#define INV_OPEN_LOOP_SQRT_TWO_THIRDS     (0.8164965809f)
/*
 * 0~pi/2四分之一波共64个等间隔区间。利用象限折叠后只需65个float，
 * 再对低24位相位做线性插值；表项无单位，范围0~1。
 */
static const float inv_sine_quarter_lut[65] =
{
    0.000000000f, 0.024541229f, 0.049067674f, 0.073564564f,
    0.098017140f, 0.122410675f, 0.146730474f, 0.170961889f,
    0.195090322f, 0.219101240f, 0.242980180f, 0.266712757f,
    0.290284677f, 0.313681740f, 0.336889853f, 0.359895037f,
    0.382683432f, 0.405241314f, 0.427555093f, 0.449611330f,
    0.471396737f, 0.492898192f, 0.514102744f, 0.534997620f,
    0.555570233f, 0.575808191f, 0.595699304f, 0.615231591f,
    0.634393284f, 0.653172843f, 0.671558955f, 0.689540545f,
    0.707106781f, 0.724247083f, 0.740951125f, 0.757208847f,
    0.773010453f, 0.788346428f, 0.803207531f, 0.817584813f,
    0.831469612f, 0.844853565f, 0.857728610f, 0.870086991f,
    0.881921264f, 0.893224301f, 0.903989293f, 0.914209756f,
    0.923879533f, 0.932992799f, 0.941544065f, 0.949528181f,
    0.956940336f, 0.963776066f, 0.970031253f, 0.975702130f,
    0.980785280f, 0.985277642f, 0.989176510f, 0.992479535f,
    0.995184727f, 0.997290457f, 0.998795456f, 0.999698819f,
    1.000000000f
};

/* ======================== 模块内部运行状态 ======================== */
/* 以下状态由10 kHz控制ISR独占更新；初始化阶段只在PWM输出关闭时写入。 */
static float inv_phase_peak_v;       /* 目标相电压峰值，单位V；由线电压有效值换算。 */
static float inv_target_modulation;  /* 最终目标调制度，不得超过用户配置上限。 */
static float inv_modulation_limit;   /* 配置允许的最大桥臂调制度，范围(0, 1]。 */
static float inv_dc_bus_v;           /* 当前开环和CBSVPWM共同使用的母线电压，单位V。 */
static float inv_ramp;               /* 软启动进度，范围0～1。 */
static float inv_ramp_increment;     /* 每个控制周期的软启动增量，无量纲。 */
static uint32_t inv_phase_accumulator; /* U相当前DDS相位，0~2^32对应0~360度。 */
static uint32_t inv_phase_step;      /* 每个控制周期增加的DDS相位量。 */
static uint32_t inv_control_frequency_hz; /* Step调用频率，单位Hz。 */
static uint16_t inv_frequency_hz;    /* 当前选择的输出频率，只允许30或60 Hz。 */
static uint8_t inv_initialized;      /* 电压参数和调制度检查通过后才置1。 */

/*
 * Step在ISR中完整覆盖该结构体，OLED/VOFA只通过GetSnapshot复制后读取。
 * volatile保证编译器每次实际访问内存，但不保证整个结构体复制具有原子性，
 * 因此GetSnapshot仍需使用短临界区防止读到一半新、一半旧的数据。
 */
static volatile INV_OpenLoopOutput inv_published_output;

/**
 * @brief  检查一个单精度浮点数是否为有限值。
 * @param  value 待检查的参数或中间计算结果。
 * @retval 1 数值有限，可以继续参与电压和调制度计算；0 为NaN或正负无穷。
 * @note   NaN具有value!=value的特征，正负无穷会超出FLT_MAX；函数不修改模块状态。
 */
static uint8_t INV_OpenLoop_IsFinite(float value)
{
    return ((value == value) && (value <= FLT_MAX) && (value >= -FLT_MAX)) ? 1U : 0U;
}

/** 根据输出频率和控制频率计算32位DDS步进，计算只发生在低速配置路径。 */
static uint32_t INV_OpenLoop_CalculatePhaseStep(INV_OutputFrequency frequency)
{
    uint64_t numerator;

    numerator = ((uint64_t)(uint32_t)frequency << 32U) +
                ((uint64_t)inv_control_frequency_hz / 2ULL);
    return (uint32_t)(numerator / (uint64_t)inv_control_frequency_hz);
}

/**
 * @brief  使用四分之一波查表和线性插值计算DDS相位对应的正弦值。
 * @param  phase 32位无符号DDS相位；0~2^32对应0~360度并允许自然回绕。
 * @retval 对应电角度的单位正弦值，范围约-1~1，无量纲。
 * @note   该函数由10 kHz Step路径调用，不使用三角函数库、不阻塞、不修改DDS相位。
 *
 * 处理步骤：
 * 1. 去掉符号半周，把第三、第四象限先映射到0~180度；
 * 2. 将90~180度镜像到0~90度，因此只需要保存第一象限正弦表；
 * 3. 使用相位高位选择相邻表项，低24位在线性区间内插值；
 * 4. 根据原始相位是否位于负半周恢复正负号。
 *
 * 相位刚好位于90度时index=64，直接读取最后一个表项1.0，避免访问index+1越界。
 */
static float INV_OpenLoop_Sine(uint32_t phase)
{
    uint32_t folded_phase = phase & (INV_OPEN_LOOP_HALF_PHASE - 1UL);
    uint32_t index;
    float fraction;
    float value;

    if (folded_phase > INV_OPEN_LOOP_QUARTER_PHASE) {
        folded_phase = INV_OPEN_LOOP_HALF_PHASE - folded_phase;
    }

    index = folded_phase >> INV_OPEN_LOOP_LUT_INDEX_SHIFT;
    if (index >= 64U) {
        value = inv_sine_quarter_lut[64];
    } else {
        fraction = (float)(folded_phase & INV_OPEN_LOOP_LUT_FRACTION_MASK) *
                   INV_OPEN_LOOP_LUT_FRACTION_SCALE;
        value = inv_sine_quarter_lut[index] +
                ((inv_sine_quarter_lut[index + 1U] - inv_sine_quarter_lut[index]) *
                 fraction);
    }

    return ((phase & INV_OPEN_LOOP_HALF_PHASE) != 0UL) ? -value : value;
}

/**
 * @brief  根据统一配置对象初始化开环参考发生器。
 * @param  config 直流母线、目标线电压、频率、限幅、软启动和控制频率配置。
 * @retval true 全部参数有效；false 地址为空、参数非法或目标调制度超限。
 * @note   在ADC DMA和HRTIM输出启动前调用，只更新软件状态，不操作外设。
 *
 * 电压换算关系：
 * Vphase_peak = Vline_rms * sqrt(2/3)
 * modulation  = 2 * Vphase_peak / Vdc
 * 当前36 V档的60 V母线、32 Vrms线电压得到Vphase_peak约26.13 V、
 * 目标调制度约0.871；最终仍受用户配置的0.90上限约束。
 */
bool INV_OpenLoop_Init(const INV_OpenLoopConfig *config)
{
    inv_initialized = 0U;
    inv_phase_peak_v = 0.0f;
    inv_target_modulation = 0.0f;
    inv_modulation_limit = 0.0f;
    inv_dc_bus_v = 0.0f;
    inv_phase_accumulator = 0U;
    inv_phase_step = 0U;
    inv_control_frequency_hz = 0U;
    inv_frequency_hz = 0U;
    inv_ramp = 0.0f;
    inv_ramp_increment = 0.0f;
    inv_published_output = (INV_OpenLoopOutput){0};

    if ((config == NULL) ||
        (INV_OpenLoop_IsFinite(config->dc_bus_v) == 0U) ||
        (INV_OpenLoop_IsFinite(config->line_rms_target_v) == 0U) ||
        (INV_OpenLoop_IsFinite(config->modulation_limit) == 0U) ||
        (config->dc_bus_v <= 0.0f) ||
        (config->line_rms_target_v <= 0.0f) ||
        (config->modulation_limit <= 0.0f) ||
        (config->modulation_limit > 1.0f) ||
        (config->soft_start_ms == 0U) ||
        (config->control_frequency_hz == 0U)) {
        return false;
    }

    inv_modulation_limit = config->modulation_limit;
    inv_control_frequency_hz = config->control_frequency_hz;
    inv_ramp_increment = 1000.0f /
                         ((float)config->soft_start_ms *
                          (float)config->control_frequency_hz);
    if ((INV_OpenLoop_IsFinite(inv_ramp_increment) == 0U) ||
        (inv_ramp_increment <= 0.0f)) {
        return false;
    }

    inv_phase_peak_v = config->line_rms_target_v * INV_OPEN_LOOP_SQRT_TWO_THIRDS;
    inv_target_modulation = (2.0f * inv_phase_peak_v) / config->dc_bus_v;
    if ((INV_OpenLoop_IsFinite(inv_target_modulation) == 0U) ||
        (inv_target_modulation <= 0.0f) ||
        (inv_target_modulation > inv_modulation_limit)) {
        return false;
    }

    inv_dc_bus_v = config->dc_bus_v;
    inv_initialized = 1U;
    if (!INV_OpenLoop_SetFrequency(config->frequency)) {
        inv_initialized = 0U;
        return false;
    }

    return true;
}

/**
 * @brief  选择30 Hz或60 Hz输出频率，并保持切换前后的电角度连续。
 * @param  frequency INV_FREQ_30HZ或INV_FREQ_60HZ，其他枚举值均非法。
 * @retval true 已更新DDS步进值；false 尚未初始化或频率参数不受支持。
 * @note   初始化阶段和运行阶段均可调用。函数只改变“以后每周期前进多少相位”，
 *         不清零inv_phase_accumulator、不重新执行软启动，也不直接操作HRTIM。
 *         运行时若从60 Hz切换到30 Hz，当前正弦位置不跳变，只是后续转速减半。
 */
bool INV_OpenLoop_SetFrequency(INV_OutputFrequency frequency)
{
    uint32_t phase_step;
    uint32_t primask;

    if (inv_initialized == 0U) {
        return false;
    }

    if ((frequency != INV_FREQ_30HZ) && (frequency != INV_FREQ_60HZ)) {
        return false;
    }

    /* 64位除法只在配置路径执行，10 kHz Step仍只进行32位加法。 */
    phase_step = INV_OpenLoop_CalculatePhaseStep(frequency);
    if (phase_step == 0U) {
        return false;
    }

    /* 主循环修改频率时，禁止10 kHz ISR看到新旧步进和频率的混合状态。 */
    primask = __get_PRIMASK();
    __disable_irq();
    inv_phase_step = phase_step;
    inv_frequency_hz = (uint16_t)frequency;
    __set_PRIMASK(primask);
    return true;
}

bool INV_OpenLoop_GetFrequency(INV_OutputFrequency *frequency)
{
    uint32_t primask;
    uint16_t current_frequency;

    if ((frequency == NULL) || (inv_initialized == 0U)) {
        return false;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    current_frequency = inv_frequency_hz;
    __set_PRIMASK(primask);
    if ((current_frequency != (uint16_t)INV_FREQ_30HZ) &&
        (current_frequency != (uint16_t)INV_FREQ_60HZ)) {
        return false;
    }
    *frequency = (INV_OutputFrequency)current_frequency;
    return true;
}

bool INV_OpenLoop_SetVoltageCommand(float dc_bus_v, float line_rms_v)
{
    float phase_peak_v;
    float target_modulation;

    if ((inv_initialized == 0U) ||
        (inv_ramp > 0.0f) ||
        (INV_OpenLoop_IsFinite(dc_bus_v) == 0U) ||
        (INV_OpenLoop_IsFinite(line_rms_v) == 0U) ||
        (dc_bus_v <= 0.0f) || (line_rms_v <= 0.0f)) {
        return false;
    }

    phase_peak_v = line_rms_v * INV_OPEN_LOOP_SQRT_TWO_THIRDS;
    target_modulation = (2.0f * phase_peak_v) / dc_bus_v;
    if ((INV_OpenLoop_IsFinite(target_modulation) == 0U) ||
        (target_modulation <= 0.0f) ||
        (target_modulation > inv_modulation_limit)) {
        return false;
    }

    /* 只允许软启动尚未开始时提交；失败时保留旧的可运行参数。 */
    inv_dc_bus_v = dc_bus_v;
    inv_phase_peak_v = phase_peak_v;
    inv_target_modulation = target_modulation;
    return true;
}

/**
 * @brief  推进一个100 us控制周期，生成U/V/W三相开环相电压指令。
 * @param  output 调用者提供的结果结构体；成功后包含三相单位正弦、电压指令、
 *                软启动进度、调制度、DDS相位和当前频率。
 * @retval true 本周期参考有效并已发布；false 输出指针为空、模块未初始化或
 *         计算得到NaN/无穷。main.c收到false后应关闭输出并锁存参数故障。
 * @note   由ADC3/4/5形成一个完整同步帧后，在10 kHz DMA回调路径中调用一次。
 *         本函数无阻塞、无动态内存，不调用HAL，也不直接更新CBSVPWM/HRTIM。
 *
 * 每次调用依次完成：软启动推进 -> 三相正弦查表 -> 相电压指令计算
 * -> 发布一致性快照 -> DDS相位前进一步。三相定义为：
 * U=sin(theta)，V=sin(theta-120度)，W=sin(theta+120度)。
 */
bool INV_OpenLoop_Step(INV_OpenLoopOutput *output)
{
    /* 本周期三相相电压指令的共同峰值，单位V，随软启动从0增加到目标值。 */
    float voltage_amplitude;

    if ((output == NULL) || (inv_initialized == 0U)) {
        return false;
    }

    /* 增量由soft_start_ms和control_frequency_hz计算，不依赖固定10 kHz常量。 */
    if (inv_ramp < 1.0f) {
        inv_ramp += inv_ramp_increment;
        if (inv_ramp > 1.0f) {
            inv_ramp = 1.0f;
        }
    }

    /* V相使用+240度等价于-120度；W相使用+120度，形成U->V->W正相序。 */
    output->sine_u = INV_OpenLoop_Sine(inv_phase_accumulator);
    output->sine_v = INV_OpenLoop_Sine(inv_phase_accumulator +
                                       INV_OPEN_LOOP_PHASE_240_DEG);
    output->sine_w = INV_OpenLoop_Sine(inv_phase_accumulator +
                                       INV_OPEN_LOOP_PHASE_120_DEG);
    /* 单位正弦乘当前峰值，得到送入CBSVPWM的三相虚拟相电压指令。 */
    voltage_amplitude = inv_phase_peak_v * inv_ramp;
    output->vu_command = voltage_amplitude * output->sine_u;
    output->vv_command = voltage_amplitude * output->sine_v;
    output->vw_command = voltage_amplitude * output->sine_w;
    output->dc_bus_v = inv_dc_bus_v;
    output->modulation = inv_target_modulation * inv_ramp;
    output->ramp = inv_ramp;
    output->phase_accumulator = inv_phase_accumulator;
    output->frequency_hz = inv_frequency_hz;
    output->valid = 1U;

    /* 最后再次检查浮点结果，防止异常参数传播到占空比和HRTIM比较寄存器。 */
    if ((INV_OpenLoop_IsFinite(output->vu_command) == 0U) ||
        (INV_OpenLoop_IsFinite(output->vv_command) == 0U) ||
        (INV_OpenLoop_IsFinite(output->vw_command) == 0U)) {
        output->valid = 0U;
        return false;
    }

    /* 先发布当前完整结果，再推进下一周期相位；快照中的相位对应本次三相指令。 */
    inv_published_output = *output;
    inv_phase_accumulator += inv_phase_step;
    return true;
}

/**
 * @brief  清零DDS相位、软启动进度和已发布快照。
 * @note   故障停机或准备重新启动时调用。函数保留母线、线电压、目标调制度和
 *         30/60 Hz选择，因此重新进入Step后会从U相零度和0幅值重新软启动。
 *         本函数不关闭PWM，安全停机由应用层调用INV_HRTIM_DisableOutputs()。
 */
void INV_OpenLoop_Reset(void)
{
    inv_phase_accumulator = 0U;
    inv_ramp = 0.0f;
    inv_published_output = (INV_OpenLoopOutput){0};
}

/**
 * @brief  把ISR最近发布的一整组开环参考复制给主循环。
 * @param  output 主循环提供的目标结构体地址；NULL输入直接返回。
 * @note   OLED和VOFA调用本函数，不应直接读取inv_published_output。结构体包含多个
 *         32位字段，复制期间若被10 kHz ISR打断，可能混入两个控制周期的数据，
 *         因此这里短暂屏蔽中断，并在完成后恢复调用前的PRIMASK状态。
 *         函数不操作PWM、GPIO或故障状态。
 */
void INV_OpenLoop_GetSnapshot(INV_OpenLoopOutput *output)
{
    /* 保存进入函数前的中断屏蔽状态，避免误开启调用者原本已经关闭的中断。 */
    uint32_t primask;

    if (output == NULL) {
        return;
    }

    /* 临界区只包含一次结构体复制，不执行OLED、USART或其他阻塞操作。 */
    primask = __get_PRIMASK();
    __disable_irq();
    *output = inv_published_output;
    __set_PRIMASK(primask);
}
