/*
 * DITASY_ADC.c
 *
 *  Created on: 2026年3月31日
 *      Author: DITASY
 *      模拟部分基础初始化文件
 */

#include "DITASY_ADC.h"



void DI_ADCA_Init()
{
    //F28379仅有外部基准，因此无需软件设置基准源，保证基准外部电容符合参数
    EALLOW;
        //声明中断
        PieVectTable.ADCA1_INT = &ADCA_INIT_HANDLE;
        //系统时钟树设置
        CpuSysRegs.PCLKCR13.bit.ADC_A=1;//启用ADCA时钟

        //ADCA模块内全局设置
        AdcaRegs.ADCCTL2.bit.PRESCALE=6;//0110对应四分频
        AdcaRegs.ADCCTL2.bit.RESOLUTION=1;//16位精度采样
        AdcaRegs.ADCCTL2.bit.SIGNALMODE=1;//0单端1差分，差分和16位是唯一组合

        AdcaRegs.ADCCTL1.bit.INTPULSEPOS=1;//1转换结束后生成中断脉冲,0则在采样窗口期间中断
        AdcaRegs.ADCCTL1.bit.ADCPWDNZ=1;//ADC模块上电
        //中断配置部分 [used AI]
        AdcaRegs.ADCINTSEL1N2.bit.INT1SEL = 1; // SOC1结束时触发ADCINT1
        AdcaRegs.ADCINTSEL1N2.bit.INT1E   = 1; // 使能ADCINT1
        AdcaRegs.ADCINTFLGCLR.bit.ADCINT1 = 1; // 清除上电时的脏标志

    EDIS;

    DELAY_US(1000);//初始化上电完成至少等待一毫秒启动

    //EPWM1脉冲输出部分
    EALLOW;
        //EPwm1Regs.ETSEL.bit.SOCAEN=1;//启用SOCA脉冲
        //EPwm1Regs.ETSEL.bit.SOCASEL=1;//001当计数到0时触发一次,011则是0和TBPRD都触发一次
        //EPwm1Regs.ETPS.bit.SOCAPRD=1;//每周期第一次事件就触发
        //EPwm1Regs.ETCLR.bit.SOCA = 1;//清零标志位
        EPwm9Regs.ETSEL.bit.SOCAEN=1;//启用SOCA脉冲
        EPwm9Regs.ETSEL.bit.SOCASEL=3;//001当计数到0时触发一次,011则是0和TBPRD都触发一次
        EPwm9Regs.ETPS.bit.SOCAPRD=1;//每周期第一次事件就触发
        EPwm9Regs.ETCLR.bit.SOCA = 1;//清零标志位
    EDIS;

    EALLOW;//采样触发部分
        AdcaRegs.ADCSOC0CTL.bit.CHSEL=0;//差分下对应使用AIN0-AIN1
        AdcaRegs.ADCSOC0CTL.bit.TRIGSEL=0x15;//选择EPWM1SOCA触发源
        AdcaRegs.ADCSOC0CTL.bit.ACQPS=127;//保持63+1个ADC时钟周期
        AdcaRegs.ADCSOC1CTL.bit.CHSEL=2;//差分下对应使用AIN2-AIN3
        AdcaRegs.ADCSOC1CTL.bit.TRIGSEL=0x15;//选择EPWM1SOCA触发源
        AdcaRegs.ADCSOC1CTL.bit.ACQPS=63;//保持63+1个ADC时钟周期
    EDIS;

    // 使能CPUPIE X1和Y1
    PieCtrlRegs.PIEIER1.bit.INTx1 = 1;  // 使能ADCA1 (INT1.1)
        IER |= M_INT1;      // 使能CPU INT1

}


void DI_ADCB_Init()
{
    //F28379仅有外部基准，因此无需软件设置基准源，保证基准外部电容符合参数
    EALLOW;
        //系统时钟树设置
        CpuSysRegs.PCLKCR13.bit.ADC_B=1;//启用ADCD时钟

        //ADCA模块内全局设置
        AdcbRegs.ADCCTL2.bit.PRESCALE=6;//0110对应四分频
        AdcbRegs.ADCCTL2.bit.RESOLUTION=1;//16位精度采样
        AdcbRegs.ADCCTL2.bit.SIGNALMODE=1;//0单端1差分，差分和16位是唯一组合
        AdcbRegs.ADCCTL1.bit.ADCPWDNZ=1;//ADC模块上电

    EDIS;

    DELAY_US(1000);//初始化上电完成至少等待一毫秒启动

    EALLOW;//采样触发部分
        AdcbRegs.ADCSOC0CTL.bit.CHSEL=0;//差分下对应使用AIN0-AIN1
        AdcbRegs.ADCSOC0CTL.bit.TRIGSEL=0x15;//选择EPWM1SOCA触发源
        AdcbRegs.ADCSOC0CTL.bit.ACQPS=63;//保持63+1个ADC时钟周期
    EDIS;

}

void DI_ADCD_Init()
{
    //F28379仅有外部基准，因此无需软件设置基准源，保证基准外部电容符合参数
    EALLOW;
        //系统时钟树设置
        CpuSysRegs.PCLKCR13.bit.ADC_D=1;//启用ADCD时钟

        //ADCA模块内全局设置
        AdcdRegs.ADCCTL2.bit.PRESCALE=6;//0110对应四分频
        AdcdRegs.ADCCTL2.bit.RESOLUTION=1;//16位精度采样
        AdcdRegs.ADCCTL2.bit.SIGNALMODE=1;//0单端1差分，差分和16位是唯一组合
        AdcdRegs.ADCCTL1.bit.ADCPWDNZ=1;//ADC模块上电

    EDIS;

    DELAY_US(1000);//初始化上电完成至少等待一毫秒启动

    EALLOW;//采样触发部分
        AdcdRegs.ADCSOC0CTL.bit.CHSEL=0;//差分下对应使用AIN0-AIN1
        AdcdRegs.ADCSOC0CTL.bit.TRIGSEL=0x15;//选择EPWM1SOCA触发源
        AdcdRegs.ADCSOC0CTL.bit.ACQPS=63;//保持63+1个ADC时钟周期
        AdcdRegs.ADCSOC1CTL.bit.CHSEL=2;//差分下对应使用AIN0-AIN1
        AdcdRegs.ADCSOC1CTL.bit.TRIGSEL=0x15;//选择EPWM1SOCA触发源
        AdcdRegs.ADCSOC1CTL.bit.ACQPS=63;//保持63+1个ADC时钟周期
    EDIS;

}

void DI_CMPSS1_Init(uint16_t DAC_VAL)
{

    //一下是初始化部分，会列出所有CMPSS1寄存器操作
    //DAC比较值装载

    Cmpss1Regs.DACHVALS.bit.DACVAL=1861+DAC_VAL;//12位有效，DAC寄存器H值
    Cmpss1Regs.DACLVALS.bit.DACVAL=1861-DAC_VAL;//12位有效，DAC寄存器L值

    EALLOW;
        //COMPCTL控制寄存器部分
        Cmpss1Regs.COMPCTL.bit.COMPDACE=1;//使能DAC比较0则具体看手册功能图
        /*
        Cmpss1Regs.COMPCTL.bit.CTRIPOUTHSEL=0;//高低俩比较器异步输到OUTPUT的X-BAR选项
        Cmpss1Regs.COMPCTL.bit.CTRIPOUTLSEL=0;//选0直接输出到XBAR达到最快反应速度

        Cmpss1Regs.COMPCTL.bit.CTRIPHSEL=0;//高低俩比较器输出到EPWM X-BAR的选项
        Cmpss1Regs.COMPCTL.bit.CTRIPHSEL=0;//选0直接异步输出达到最快反应速度
        */
        Cmpss1Regs.COMPCTL.bit.CTRIPOUTHSEL=3;//高低俩比较器异步输到OUTPUT的X-BAR选项,具体使用看Figure 13-1
        Cmpss1Regs.COMPCTL.bit.CTRIPOUTLSEL=3;//选3经过数字滤波器并经过锁存器

        Cmpss1Regs.COMPCTL.bit.CTRIPHSEL=3;//高低俩比较器输出到EPWM X-BAR的选项
        Cmpss1Regs.COMPCTL.bit.CTRIPHSEL=3;//选3经过数字滤波器并经过锁存器

        Cmpss1Regs.COMPCTL.bit.COMPHINV=0;//0为不反向，1为反向
        Cmpss1Regs.COMPCTL.bit.COMPLINV=1;//比较器的CMP输入大于DAC输出时为拉高

        Cmpss1Regs.COMPCTL.bit.COMPHSOURCE=0;//比较器负端输入的选择，0为内部DAC,1为CMPIN1N
        Cmpss1Regs.COMPCTL.bit.COMPLSOURCE=0;//比较器正端固定为CMPIN1P

        //数字滤波器部分
        Cmpss1Regs.COMPCTL.bit.ASYNCHEN=0;//0为不连接到与锁存数字滤波器输出相连的逻辑门
        Cmpss1Regs.COMPCTL.bit.ASYNCLEN=0;//同上，看结构框图决定是否使用

        //滤波器设置,参考13.5 Digital Filter
        Cmpss1Regs.CTRIPHFILCTL.bit.SAMPWIN=15;//采样窗口大小
        Cmpss1Regs.CTRIPHFILCTL.bit.THRESH=10;//置高阈值，当采样窗内有多少个高电平后置1
        Cmpss1Regs.CTRIPHFILCTL.bit.FILINIT=1;//初始化用，置1后将滤波器所有位内容设为当前比较器输出
        Cmpss1Regs.CTRIPHFILCLKCTL.bit.CLKPRESCALE=1;//2分频,时钟源为系统时钟
        Cmpss1Regs.CTRIPLFILCTL.bit.SAMPWIN=15;
        Cmpss1Regs.CTRIPLFILCTL.bit.THRESH=10;
        Cmpss1Regs.CTRIPLFILCTL.bit.FILINIT=1;
        Cmpss1Regs.CTRIPLFILCLKCTL.bit.CLKPRESCALE=1;//低边同上

        Cmpss1Regs.COMPSTSCLR.bit.HSYNCCLREN=1;//锁存器清零使能
        Cmpss1Regs.COMPSTSCLR.bit.LSYNCCLREN=1;//锁存器清零使能
        Cmpss1Regs.COMPSTSCLR.bit.HLATCHCLR=1;//清零数字滤波后面的SR输出锁存器
        Cmpss1Regs.COMPSTSCLR.bit.LLATCHCLR=1;//低边同上
        Cmpss1Regs.COMPSTSCLR.bit.HSYNCCLREN=0;//锁存器清零使能
        Cmpss1Regs.COMPSTSCLR.bit.LSYNCCLREN=0;//锁存器清零使能
        //COMPLOCK用于锁定各CMPSS寄存器设置，仅复位能解锁，这里不使用

        //COMPHYSCTL/COMPHYSCTL寄存器:迟滞控制器
        //COMPSTSCLR状态清除寄存器,需要打开EALLOW,暂时不使用

        //COMPDACCTL  DAC控制寄存器部分
        Cmpss1Regs.COMPDACCTL.bit.FREESOFT=3;//斜坡生成器的调试状态选择，自由运行，本程序没使用斜坡生成器
        Cmpss1Regs.COMPDACCTL.bit.SWLOADSEL=0;//DACVAL影子寄存器什么时候装载，0为每次系统时钟
        Cmpss1Regs.COMPDACCTL.bit.SELREF=0;//1选用VDAC做参考，0选用VDDA
        //↑↑这里选用VDDA，狗操的TI工程师把28379D的ADCINB0和VDAC绑在一个脚上💧
        Cmpss1Regs.COMPDACCTL.bit.RAMPLOADSEL=0;//没有使用到斜坡源
        Cmpss1Regs.COMPDACCTL.bit.RAMPSOURCE=0;//没有使用到斜坡源
        Cmpss1Regs.COMPDACCTL.bit.DACSOURCE=0;//不使用斜坡源，从DACVALS更新比较电压

        //XBAR初始化，用于输出TRIP4和GPIO
        EPwmXbarRegs.TRIP4MUX0TO15CFG.bit.MUX0=1;//TRIP4的XBAR输入源选择,=1即CMPSS1俩比较器输出相或,详情查询用户手册Table 9-2
        EPwmXbarRegs.TRIP4MUXENABLE.bit.MUX0=1;//打开该XBAR通道
        EPwmXbarRegs.TRIPOUTINV.bit.TRIP4=0;//输出反向

        OutputXbarRegs.OUTPUT1MUX0TO15CFG.bit.MUX0=1;//XBAROUTPUT1输入源选择，MUX0=1为CMP1高低侧输出相或，=0为仅高侧
        OutputXbarRegs.OUTPUT1MUXENABLE.bit.MUX0=1;//打开该XBAR通道,详情查询Table 9-4
        //输出XBAR锁存器
        OutputXbarRegs.OUTPUTLATCHENABLE.bit.OUTPUT1=1;//使能锁存器，为0则直接输出到反向选择器
        OutputXbarRegs.OUTPUTINV.bit.OUTPUT1=0;//反向选择器，为0输出不反向
        //OutputXbarRegs.OUTPUTLATCHCLR.bit.OUTPUT1=1;//强制清零,放到程序中使用
        OutputXbarRegs.OUTPUTLATCHFRC.bit.OUTPUT1=1;//强制拉高,这里刚好用于连接到 UCC21520 DIS端，高电平关闭发波，所以建议初始化上拉

        //OutputXbarRegs.OUTPUTINV.bit.OUTPUT1=1;//输出反向
        //GPIO初始化部分
        GPIO_SetupPinOptions(58, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
        GPIO_SetupPinMux(58, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
        //状态寄存器COMPSTS是只读寄存器，可读取数字滤波器输出和锁存器输出

    EDIS;
    //RAMPMAXREFA RAMPMAXREFS RAMPDECVALA RAMPDECVALS RAMPSTS RAMPDLYA RAMPDLYS
    //皆为斜坡源相关内容，不使用

}

void DI_CMPSS_CLR()
{
    EALLOW;
    Cmpss1Regs.COMPSTSCLR.bit.HSYNCCLREN=1;//锁存器清零使能
    Cmpss1Regs.COMPSTSCLR.bit.LSYNCCLREN=1;//锁存器清零使能
    Cmpss1Regs.COMPSTSCLR.bit.HLATCHCLR=1;//清零数字滤波后面的SR输出锁存器
    Cmpss1Regs.COMPSTSCLR.bit.LLATCHCLR=1;//低边同上
    Cmpss1Regs.COMPSTSCLR.bit.HSYNCCLREN=0;//锁存器清零使能
    Cmpss1Regs.COMPSTSCLR.bit.LSYNCCLREN=0;//锁存器清零使能
    EPwm1Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm1Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm2Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm2Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm3Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm3Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm4Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm4Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm5Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm5Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm6Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm6Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm7Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm7Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm8Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm8Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm9Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm9Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm10Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm10Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm11Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm11Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EPwm12Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    EPwm12Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EDIS;
    OutputXbarRegs.OUTPUTLATCHCLR.bit.OUTPUT1=1;//OUTXBAR强制清零;
}









