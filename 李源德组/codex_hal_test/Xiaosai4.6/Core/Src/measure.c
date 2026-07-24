#include "measure.h"
#include "main.h"
#include "math.h"
#include "arm_math.h"

/*
ADC2 CH0 PA0 采集交流电压输入
ADC2 CH1 PA1 采集交流电流输入
*/



#define SAMPLE_LEN       100    // 单周期采样点数
#define NUM_CHANNELS     2      // 通道数：0=电压, 1=电流
/************************ 配置参数 ************************/
#define BUFFER_DEPTH     400     // 保留最新400组数据
#define ADC_BITS         12      // 你的ADC位数（12位/10位自行修改）


rms_type AC_rms={0};
__ALIGNED(4) float32_t AC_sample_mat[NUM_CHANNELS][SAMPLE_LEN]; // 采样缓冲区，用于有效值计算
extern __IO uint16_t AD_Value[2];
extern float inverter_duty;
uint16_t AC_vref, AC_iref;
uint8_t jisuan_status;
/************************ 静态缓存区 ************************/
static uint16_t volt_buf[BUFFER_DEPTH] = {0};  // 电压缓存400点
static uint16_t curr_buf[BUFFER_DEPTH] = {0};  // 电流缓存400点
static uint32_t buf_index = 0;                 // 循环写入索引



/**
 * @brief  50us周期调用：把最新ADC值存入循环缓冲区
 * @note   自动覆盖最旧数据，只保留最新400组
 */
void ADC_Data_Update(void)
{
    // 循环写入：最新→覆盖最旧
    volt_buf[buf_index] = AD_Value[0];
    curr_buf[buf_index] = AD_Value[1];

    // 索引自增，到400自动归零
    buf_index++;
    if (buf_index >= BUFFER_DEPTH) {
        buf_index = 0;
    }
}

/**
 * @brief  计算 电压 算术平均值
 * @retval 均值（uint32_t，避免溢出）
 */
uint32_t Voltage_Get_Average(void)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < BUFFER_DEPTH; i++) {
        sum += volt_buf[i];
    }
    return sum / BUFFER_DEPTH;
}

/**
 * @brief  计算 电流 算术平均值
 * @retval 均值（uint32_t）
 */
uint32_t Current_Get_Average(void)
{
    uint32_t sum = 0;
    for (uint32_t i = 0; i < BUFFER_DEPTH; i++) {
        sum += curr_buf[i];
    }
    return sum / BUFFER_DEPTH;
}


// 获取电压采样值 (通道0)得到的是0-26V的
float get_voltage_sample(void) {
    uint16_t raw = AD_Value[0];  // 电压通道
    const float VOLTAGE_RATIO = 20.07f; // 电压变比系数 (根据实际硬件调整)
    AC_vref = Voltage_Get_Average();
    // 去除直流偏置并应用比例系数
    return (raw > AC_vref) ? 
           (raw - AC_vref) * VOLTAGE_RATIO : 
           (AC_vref - raw) * VOLTAGE_RATIO;
}


// 获取电流采样值 (通道1)得到的是0-3A的
float get_current_sample(void) {
    uint16_t raw = AD_Value[1];  // 电流通道
    const float CURRENT_RATIO = 0.05f; // 电流互感器比例 (根据实际硬件调整)
    AC_iref = Current_Get_Average();
    // 去除直流偏置并应用比例系数
    return (raw > AC_iref) ? 
           (raw - AC_iref) * CURRENT_RATIO : 
           (AC_iref - raw) * CURRENT_RATIO;
}


// 双通道同步采样控制
void calculate_rms(void) {
	static uint16_t index = 0;
	
	// 同步采样电压和电流
	AC_sample_mat[0][index] = get_voltage_sample(); // 电压采样
	AC_sample_mat[1][index] = get_current_sample(); // 电流采样
	
	index++;
	if(index >= SAMPLE_LEN) index = 0;
	
	// 当完成一个完整周期采样时
	if (index == SAMPLE_LEN - 1) 
	{       
		// 计算电压和电流有效值
		AC_rms.V_rms = measure_rms(0, SAMPLE_LEN);
		AC_rms.I_rms = measure_rms(1, SAMPLE_LEN);
	}
}

// 使用CMSIS-DSP库计算指定通道的有效值
float measure_rms(uint8_t channel, uint8_t len) 
{
    float32_t result;
    arm_rms_f32(AC_sample_mat[channel], len, &result);
    return result;
}



////采集输入直流电压
//uint16_t get_acv2_in(void)
//{
//	uint16_t temp=0;
//	temp = AD_Value[0];
//	if(temp>=AC_vref)return (temp-AC_vref);//返回直流电压大小，倍数乘以20.07倍（可测试得出)	
//	else return (AC_vref-temp);//返回直流电压大小，倍数乘以20.07倍（可测试得出)	
//}


//void calculate_rms(void) //计算采样函数
//{
//	static uint32_t count=0;
//	uint32_t temp=count %SAMPLE_LEN;
//	count++;
//	AC_sample_mat[temp]= get_acv2_in(); //PA0
//	if(SAMPLE_LEN-1==temp)//如果检测到检测周期内的最后一个采样点
//	{
//		 count=0;
//		 jisuan_status=1;
////     AC_rms.V1_rms=measure_rms(SAMPLE_LEN)/4095*3.3*AC_V1_ratio;//采集输入电压的均方根
//	}
//}


//float measure_rms(uint8_t len) 
//{
//	float32_t result;
//    arm_rms_f32(AC_sample_mat, len, &result);
//    return result;
//}


//float measure_rms(uint8_t i)//均方根算法
//{
//	float sum=0,result;
//  int j;
//  for(j=0;j<i;j++)
//	{
//		sum+=(AC_sample_mat[j]*AC_sample_mat[j]);
//	}
//	sum=sum/i;
//	result=sqrt(sum);
//	return result;
//	
//}


////采集输入多路交流电压
//float get_acv1_in(void)
//{
//	float temp=0;
//	u32 sum=0;
//	int i;
//	for(i=0;i<ROWS;i++)
//	{
//		sum+=adc_result[i*COLS +0];//采集第一个通道，也就是第一个数据
//	}
//	temp =(double)sum/ROWS/4095*3.3;//将总值变成平均值
//	return (temp-AC_V1_vref)*AC_V1_ratio;//返回直流电压大小，倍数乘以20.07倍（可测试得出)
//	
//}



//float measure_rms(uint8_t i,uint8_t size)//均方根算法
//{
//	float sum=0,result;
//  int j;
//  for(j=0;j<i;j++)
//	{
//		sum+=(float)(AC_sample_mat[size][j]*AC_sample_mat[size][j]);
//	}
//	sum=(double)(sum/i);
//	result=(float)(1.0*sqrt(sum));
//	return result;
//	
//}










