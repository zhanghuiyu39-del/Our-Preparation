#include "F2837xD_device.h"
#include "F28x_Project.h"
#include "DITASY.h"
#include "math.h"
#include "SFO_V8.h"
#include "DITASY_ADC.h"
#include "PR.h"
/**
 * main.c
 */
#define OSCLEGTH 1600
struct PR_PATH PRA;
float OSC_CH1[OSCLEGTH],OSC_CH2[OSCLEGTH];
float Vac,Iac,Udc,Udc1,Udc2,Udcout,ADCGET[5];
float PIout=0,ITG=0,DCerr=0,Iref=0,ITG2=0,OUTerr=0,PIout2=0;
int PSout=0,CHBout=0;
int OSC_num=0,SEND_CD=2000,OSC_LOAD=0,num=0;
char TXD_FLG=0,GO_PR=0,GO_PS=0;
__interrupt void CPUTIMER0_HANDLE(void);//定时器0中断服务函数


int main(void)
{
	InitSysCtrl();//启用系统内核及其初始化
    DELAY_US(300);//内外核供电有具体的时序，延迟以保证时序正确
    InitCpuTimers();//初始化CPU定时器
	InitGpio();//GPIO外设初始化
    InitPieCtrl();//中断向量表初始化
    DINT;
    IER=0x0000;//清空向量表Y
    IFR=0x0000;
    InitPieVectTable();

    EALLOW;//解锁配置权限
    PieVectTable.TIMER0_INT= &CPUTIMER0_HANDLE;//注册中断函数，名字可自选
    EDIS;//锁定配置权限

	//Timer中断初始化部分
    DI_Timer0_Init(200,1000);
	IER |=M_INT1;//选中使能中断向量表Y=1部分，具体查PIE Channel Mapping
	PieCtrlRegs.PIEIER1.bit.INTx7=1;//选中使能中断向量表X=7部分

	//CMPSS初始化
	DI_CMPSS1_Init(500);
	//EPWM初始化
	//注意,EPWMA端的占空比是反向的,这和中央对其方式的计数有关
	DI_EPWM1_Init();
	DI_EPWM2_SYNC_Init();
	DI_EPWM3_SYNC_Init();
	DI_EPWM4_SYNC1_Init();
    DI_EPWM5_SYNC_Init();
    DI_EPWM6_SYNC_Init();
    DI_EPWM7_SYNC1_Init();
    DI_EPWM8_SYNC_Init();
    DI_NEPWM9_SYNC_Init();
    DI_NEPWM10_SYNC1_Init();
    DI_NEPWM11_SYNC_Init();
    DI_NEPWM12_SYNC_Init();
	//SCI初始化
	DI_SCIA_Init();
	//ADC初始化
	DI_ADCA_Init();
	DI_ADCB_Init();
    DI_ADCD_Init();
    GPIO_SetupPinOptions(58, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(58, GPIO_MUX_CPU1, 0);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GpioDataRegs.GPBCLEAR.bit.GPIO58=1;
    EINT;//使能全局中断
    ERTM;//使能实时中断

	//GPIO初始化部分
    //运行状态闪烁灯
	GPIO_SetupPinMux(41, GPIO_MUX_CPU1, 0);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
	GPIO_SetupPinOptions(41, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
	//故障指示灯
    GPIO_SetupPinMux(48, GPIO_MUX_CPU1, 0);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(48, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    //中断占用指示信号
    GPIO_SetupPinMux(46, GPIO_MUX_CPU1, 0);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(46, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    //采样正负校准信号
    GPIO_SetupPinMux(42, GPIO_MUX_CPU1, 0);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(42, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);

	GPIO_WritePin(41, 0);
    GpioDataRegs.GPBCLEAR.bit.GPIO41=1;//寄存器方式置低
	GpioDataRegs.GPBSET.bit.GPIO41=1;//寄存器方式置高
	//N=GpioDataRegs.GPCDAT.bit.GPIO64;//寄存器方式读取
	//50占空比
    EPwm1Regs.CMPA.bit.CMPA=500;
    EPwm2Regs.CMPA.bit.CMPA=500;
    EPwm3Regs.CMPA.bit.CMPA=500;
    EPwm4Regs.CMPA.bit.CMPA=500;
	//PR初始化
    PR_init(&PRA, 0.000025, 7.5, 95.0, 5.0, 314.15926535);
	//清空保护(默认OUTPUT-XBAR保护状态)
	while(1)
	{
	    //GpioDataRegs.GPCSET.bit.GPIO64=1;
	    //DELAY_US(500000);
	    //GpioDataRegs.GPCCLEAR.bit.GPIO64=1;
        //DELAY_US(500000);
	    if(GO_PR==0&&Udc>40.0) DELAY_US(200000),DI_CMPSS_CLR(),GO_PR=1;
        if(GO_PS==0&&Udc>60.0) DELAY_US(20000),GO_PS=1;

	    if(EPwm1Regs.TZFLG.bit.DCAEVT1) GpioDataRegs.GPBSET.bit.GPIO48=1;//寄存器方式置高
	    else GpioDataRegs.GPBCLEAR.bit.GPIO48=1;//寄存器方式置低
	    if(TXD_FLG==1&&SEND_CD==0)
	    {
	        if(OSC_num<OSCLEGTH)
	        {
	            DI_Send_Float(OSC_CH1[OSC_num],OSC_CH2[OSC_num]),
	            DELAY_US(1200),
	            OSC_num++;
	            //if(num<OSCLEGTH) num++;
	            //else num=0;
	        }
	        else SEND_CD=500,OSC_num=0,TXD_FLG=0;
	    }
	}
}




__interrupt void CPUTIMER0_HANDLE(void)//定时器0中断服务函数
{
    static uint64_t senddata=1;
    static uint32_t cnt=0,LED=0;
    if(SEND_CD<=0) SEND_CD=0;
    else SEND_CD--;
    if(cnt>=500)
    {
        cnt=0,LED=!LED;
        if(LED==1)
            {
                GpioDataRegs.GPBSET.bit.GPIO41=1;

                //SciaRegs.SCICTL2.bit.TXINTENA = 0; // 关闭发送中断

            }
            else
            {
                GpioDataRegs.GPBCLEAR.bit.GPIO41=1;
                //DI_Send_Float((float)senddata);
                //DI_Send_Float((float)AdcaResultRegs.ADCRESULT0);
                senddata++;


            }
    }
    else cnt++;

    CpuTimer0Regs.TCR.bit.TIF=1;//清除溢出位
    PieCtrlRegs.PIEACK.bit.ACK1=1;//清空中断组
}


__interrupt void ADCA_INIT_HANDLE(void)
{

    /*
    static float wt=0;
    static int Duty=0;

    //SPWM测试
    wt+=314.15926535/20000.0;
    if(wt>=(2*3.1415926535)) wt-=2*3.1415926535;
    Duty=1250+(int)(0.8*1250*sinf(wt));
    if(Duty>=2500) Duty=2500;
    else if(Duty<=0) Duty=0;
                EPwm9Regs.CMPA.bit.CMPA=Duty;
                EPwm10Regs.CMPA.bit.CMPA=2500-Duty;
                EPwm11Regs.CMPA.bit.CMPA=Duty;
                EPwm12Regs.CMPA.bit.CMPA=2500-Duty;
    */

    GpioDataRegs.GPBSET.bit.GPIO46=1;
    //数值校准
    ADCGET[0]=AdcaResultRegs.ADCRESULT0;
    ADCGET[1]=AdcaResultRegs.ADCRESULT1;
    ADCGET[2]=AdcdResultRegs.ADCRESULT0;
    ADCGET[3]=AdcdResultRegs.ADCRESULT1;
    ADCGET[4]=AdcbResultRegs.ADCRESULT0;
    Vac=(ADCGET[0]-32761.0)*0.033886745;
    Iac=(ADCGET[1]-32763.0)*0.006004581;
    Udc1=(ADCGET[2]-32779.0)*0.027934214;
    Udc2=(ADCGET[3]-32771.0)*0.027934214;
    Udcout=(ADCGET[4]-32798.0)*0.01816727;
    Udc=Udc1+Udc2;
    if(Udc==0) Udc=0.01;
    //正负校准
    if(Vac>0) GpioDataRegs.GPBSET.bit.GPIO42=1;
    else      GpioDataRegs.GPBCLEAR.bit.GPIO42=1;
    //CHB部分
    if(EPwm1Regs.TZFLG.bit.DCAEVT1!=1&&GO_PR==1)
    {
    DCerr=100.0-Udc;
    ITG+=0.000025*1.0*DCerr;
    PIout=0.01*DCerr+ITG;
    Iref=PIout*Vac*0.01;
    PR_update(&PRA, Iref-Iac);
    }
    else PRA.Vo=0,PRA.Vo1=0,PRA.Vo2=0,PRA.Vi1=0,PRA.Vi2=0,ITG=0;
    //前馈
    CHBout=(int)(1250.0+1250.0*(Vac-PRA.Vo)/Udc);
    if(CHBout>2500) CHBout=2500;
    else if(CHBout<0) CHBout=0;
            EPwm9Regs.CMPA.bit.CMPA=2500-CHBout;
            EPwm10Regs.CMPA.bit.CMPA=CHBout;
            EPwm11Regs.CMPA.bit.CMPA=2500-CHBout;
            EPwm12Regs.CMPA.bit.CMPA=CHBout;
    //PSFB部分
    if(GO_PS)
    {
        OUTerr=20.0-Udcout;
        ITG2+=0.000025*50*OUTerr;
        PIout2=0.01*OUTerr+ITG2;
        PSout=(int)(PIout2*500.0);
        if(PSout>500)PSout=500;
        else if(PSout<0)PSout=0;
    }
    else PSout=0;
    EPwm4Regs.TBPHS.bit.TBPHS=500+PSout;//移相寄存器
    EPwm2Regs.TBPHS.bit.TBPHS=PSout;//移相寄存器
    //发送采集数据给PC

    if(num<OSCLEGTH&&TXD_FLG==0)
        OSC_CH1[num]=Udc1,
        OSC_CH2[num]=Udc2,
        //OSC_CH1[num]=Duty,
        //OSC_CH2[num]=-0.0925609*((float)AdcbResultRegs.ADCRESULT0-32793.54),
        //OSC_CH1[num]=((float)AdcdResultRegs.ADCRESULT0-32779),
        //OSC_CH2[num]=((float)AdcdResultRegs.ADCRESULT1-32771),
        num++;
    else num=0,TXD_FLG=1;

    /*
    if(TXD_FLG==0)
    {
        OSC_CH1[num]=Vac;
        OSC_CH2[num]=CHBout;
        if(num<OSCLEGTH) num++;
        else num=0;
    }
    if(EPwm1Regs.TZFLG.bit.DCAEVT1&&GO_PR)
    {
    if(OSC_LOAD<=800) OSC_LOAD++;
    else TXD_FLG=1;
    }
    else TXD_FLG=0;
    */

    GpioDataRegs.GPBCLEAR.bit.GPIO46=1;
    AdcaRegs.ADCINTFLGCLR.bit.ADCINT1=1; // 清除内部中断禁止标志
    PieCtrlRegs.PIEACK.bit.ACK1=1;//清空中断组
}




