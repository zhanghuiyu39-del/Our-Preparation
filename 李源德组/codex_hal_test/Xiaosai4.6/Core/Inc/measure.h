#include <stdint.h>

#ifndef __MEASURE_H
#define __MEASURE_H

//typedef struct _AC_RMS  //定义有效值结构体
//{
//	float V1_rms;   //输入交流电压有效值  
//}rms_type;


//float measure_rms(uint8_t i);//均方根算法,i代表我们采样的点数，size代表我们要采样的数据行
//float measure_rms(uint8_t len);


typedef struct {
    float V_rms;  // 电压有效值
    float I_rms;  // 电流有效值
} rms_type;

extern const float VOLTAGE_RATIO;
extern const float CURRENT_RATIO;
extern rms_type AC_rms;
void calculate_rms(void); 
float measure_rms(uint8_t channel, uint8_t len);
float get_voltage_sample(void);
float get_current_sample(void);
void ADC_Data_Update(void);
uint32_t Voltage_Get_Average(void);
uint32_t Current_Get_Average(void);



#endif
