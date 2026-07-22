/*
 * DITASY.c
 *
 *  Created on: 2026年3月27日
 *      Author: DITASY
 *      EPWMxSCI等部分基础初始化的文件
 */
#include "DITASY.h"
#include "string.h"
SCI_ARRAY DI_SCIA_ARRAY;

void DI_Timer0_Init(float Freq,float Period)
{
    ConfigCpuTimer(&CpuTimer0, Freq, Period);//定时器配置(选择定时器，时钟源频率，定时器周期)可输入浮点
    EALLOW;//解锁配置权限
    CpuTimer0Regs.TCR.all=0x4000;//强制设置为仅使用中断
    EDIS;//锁定配置权限

}

void DI_Send_Float(float Input1,float Input2)
{
    FTChar Fdata;
    Fdata.f_data=Input1;
    DI_SCIA_ARRAY.SCI_DATA[3]=(Fdata.c_data[1]>>8)& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[2]=Fdata.c_data[1]& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[1]=(Fdata.c_data[0]>>8)& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[0]=Fdata.c_data[0]& 0x00FF;
    Fdata.f_data=Input2;
    DI_SCIA_ARRAY.SCI_DATA[7]=(Fdata.c_data[1]>>8)& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[6]=Fdata.c_data[1]& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[5]=(Fdata.c_data[0]>>8)& 0x00FF;
    DI_SCIA_ARRAY.SCI_DATA[4]=Fdata.c_data[0]& 0x00FF;

    DI_SCIA_ARRAY.SCI_DATA[8] = 0x00;
    DI_SCIA_ARRAY.SCI_DATA[9] = 0x00;
    DI_SCIA_ARRAY.SCI_DATA[10] = 0x80;
    DI_SCIA_ARRAY.SCI_DATA[11] = 0x7F;
    SCIA_TRANSMIT_ARRAY(12);
}

void DI_SCIA_Init()
{
    //清空队列数组内容
    DI_SCIA_ARRAY.ARR=0;
    DI_SCIA_ARRAY.FULL=0;
    //GPIO设定
    // GPIO28 29初始化
    EALLOW;
    GPIO_SetupPinMux(28, GPIO_MUX_CPU1, 1);  // GPIO28 = SCIRXDA
    GPIO_SetupPinMux(29, GPIO_MUX_CPU1, 1);  // GPIO29 = SCITXDA

    // 配置输入限定（可选）
    GPIO_SetupPinOptions(28, GPIO_INPUT, GPIO_ASYNC); // 异步输入
    GPIO_SetupPinOptions(29, GPIO_OUTPUT, GPIO_PUSHPULL); // 推挽输出
    EDIS;

    // 使能SCI外设时钟
    EALLOW;
    CpuSysRegs.PCLKCR7.bit.SCI_A = 1;
    EDIS;



    // 配置通信格式
    SciaRegs.SCICCR.all = 0x0007;    // 1停止位，无奇偶校验，8位数据，异步模式
    ClkCfgRegs.LOSPCP.bit.LSPCLKDIV = 2;//2=低速时钟四分频最终为200MHZ/4=50Mhz
    // 设置波特率（LSPCLK=50MHz，115200波特率）
    // BRR = (LSPCLK/(SCI波特率×8))-1 = (50,000,000/(115200×8))-1 ≈ 53 ERR=0.47%
    SciaRegs.SCIHBAUD.all = 0x0000;      // 高位波特率寄存器
    SciaRegs.SCILBAUD.all = 0x0035;      // 低位波特率寄存器


    /*
    // 配置通信格式
    SciaRegs.SCICCR.all = 0x0007;    // 1停止位，无奇偶校验，8位数据，异步模式
    ClkCfgRegs.LOSPCP.bit.LSPCLKDIV = 0;//低速时钟四分频最终为200MHZ
    // 设置波特率（LSPCLK=200MHz，1000000波特率）
    // BRR = (LSPCLK/(SCI波特率×8))-1 = (200,000,000/(500000×8))-1 ≈ 49 ERR=0%
    SciaRegs.SCIHBAUD.all = 0x0000;      // 高位波特率寄存器
    SciaRegs.SCILBAUD.all = 24;      // 低位波特率寄存器
    SciaRegs.SCICTL1.bit.SWRESET = 1;
    */

    SciaRegs.SCICCR.bit.LOOPBKENA = 0; // 使能回环，即是否需要内部连接TXRX来测试程序，0为不使用

    // 配置控制寄存器
    SciaRegs.SCICTL1.all = 0x0003;   // 置零其它控制位并使能TX和RX，禁用休眠模式
    SciaRegs.SCICTL2.bit.TXINTENA = 1; // 使能发送中断
    //SciaRegs.SCICTL2.bit.RXBKINTENA = 1; // 使能接收中断

    // 4. 使能SCI
    SciaRegs.SCICTL1.all = 0x0023;   // 重新使能SCI

    // 初始化PIE向量表
    EALLOW;
    //PieVectTable.SCIA_RX_INT = &SCI_RXINT;  // 接收中断服务程序
    PieVectTable.SCIA_TX_INT = &SCI_TXINT;  // 发送中断服务程序
    EDIS;


    // 使能PIE组9中断（SCIA在INT9.1和INT9.2）
    //PieCtrlRegs.PIEIER9.bit.INTx1 = 1;  // 使能SCIA_RX_INT (INT9.1)
    PieCtrlRegs.PIEIER9.bit.INTx2 = 1;  // 使能SCIA_TX_INT (INT9.2)

    // 使能CPU INT9和全局中断
    IER |= M_INT9;      // 使能CPU INT9

}

void SCIA_TRANSMIT_ARRAY(uint16_t ARRAY_length)
{
    DI_SCIA_ARRAY.ARR=1;
    if(ARRAY_length>64) DI_SCIA_ARRAY.FULL=63;
    else DI_SCIA_ARRAY.FULL=ARRAY_length-1;
    SciaRegs.SCICTL2.bit.TXINTENA = 1; // 使能发送中断
    SciaRegs.SCITXBUF.all =DI_SCIA_ARRAY.SCI_DATA[0];
}

__interrupt void SCI_TXINT(void)
{
    if(DI_SCIA_ARRAY.ARR>DI_SCIA_ARRAY.FULL)
    {
    SciaRegs.SCICTL2.bit.TXINTENA = 0; // 关闭发送中断
    }
    else
    {
        SciaRegs.SCITXBUF.all =DI_SCIA_ARRAY.SCI_DATA[DI_SCIA_ARRAY.ARR];
        DI_SCIA_ARRAY.ARR++;
    }
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP9;
}








void DI_EPWM1_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM1=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm1Gpio();//打开EPWM1引脚
    //TB部分
    EPwm1Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm1Regs.TBCTL.bit.SWFSYNC=0;//0为不接收同步脉冲1反之
    EPwm1Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm1Regs.TBCTL.bit.PHSEN=TB_DISABLE;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm1Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm1Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm1Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm1Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm1Regs.TBPRD=1000;//100Mhz/(2500*2)=20khz
    EPwm1Regs.TBCTR=0x0000;//清零
    EPwm1Regs.TBCTL.bit.SYNCOSEL=1;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    //EPwm1Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm1Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm1Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm1Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm1Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA，只使用了部分基本功能，更多AQ功能查阅手册AQCTL寄存器
    EPwm1Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm1Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB，同上
    EPwm1Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm1Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm1Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，有外部硬件死区这里不使用，更详细的控制查阅手册


    EALLOW;

    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm1Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm1Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm1Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm1Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm1Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm1Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm1Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm1Regs.TZCLR.bit.DCAEVT1=1;//清空单次触发保护，在程序中使用
    //EPwm1Regs.TZCLR.bit.OST=1;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;
}

void DI_EPWM2_SYNC_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM2=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm2Gpio();//打开EPWM1引脚
    //TB部分
    EPwm2Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm2Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm2Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm2Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm2Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm2Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm2Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm2Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm2Regs.TBPRD=1000;//100Mhz/(2500*2)=20khz
    EPwm2Regs.TBCTR=0x0000;//清零
    EPwm2Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm2Regs.TBPHS.bit.TBPHS=300;//移相寄存器
    //EPwm2Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm2Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm2Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm2Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm2Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm2Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm2Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB
    EPwm2Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm2Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm2Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm2Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm2Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm2Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm2Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm2Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm2Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm2Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm2Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm2Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;
}


void DI_EPWM3_SYNC_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM3=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm3Gpio();//打开EPWM1引脚
    //TB部分
    EPwm3Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm3Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm3Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm3Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm3Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm3Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm3Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm3Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm3Regs.TBPRD=1000;//100Mhz/(2500*2)=20khz
    EPwm3Regs.TBCTR=0x0000;//清零
    EPwm3Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm3Regs.TBPHS.bit.TBPHS=500;//移相寄存器
    //EPwm3Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm3Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm3Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm3Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm3Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm3Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm3Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB
    EPwm3Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm3Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm3Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm3Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm3Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm3Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm3Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm3Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm3Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm3Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm3Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm3Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;
}

void DI_EPWM4_SYNC1_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM4=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm4Gpio();//打开EPWM1引脚
    SyncSocRegs.SYNCSELECT.bit.EPWM4SYNCIN=0;//EPWM4/7/10需要另外选择同步源，详情见表Figure 15-7
    //TB部分
    EPwm4Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm4Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm4Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm4Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm4Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm4Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm4Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm4Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm4Regs.TBPRD=1000;//100Mhz/(2500*2)=20khz
    EPwm4Regs.TBCTR=0x0000;//清零
    EPwm4Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm4Regs.TBPHS.bit.TBPHS=800;//移相寄存器
    //EPwm4Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm4Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm4Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm4Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm4Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm4Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm4Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB
    EPwm4Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm4Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm4Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm4Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm4Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm4Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm4Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm4Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm4Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm4Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm4Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm4Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;
}

void DI_EPWM5_SYNC_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM5=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm5Gpio();//打开EPWM1引脚
    //TB部分
    EPwm5Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm5Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm5Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm5Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm5Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm5Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm5Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm5Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm5Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm5Regs.TBCTR=0x0000;//清零
    EPwm5Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm5Regs.TBPHS.bit.TBPHS=300;//移相寄存器
    //EPwm5Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm5Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm5Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm5Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm5Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm5Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm5Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm5Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm5Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm5Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm5Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm5Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm5Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm5Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm5Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm5Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm5Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm5Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm5Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

void DI_EPWM6_SYNC_Init()
{

    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM6=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm6Gpio();//打开EPWM1引脚
    //TB部分
    EPwm6Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm6Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm6Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm6Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm6Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm6Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm6Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm6Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm6Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm6Regs.TBCTR=0x0000;//清零
    EPwm6Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm6Regs.TBPHS.bit.TBPHS=300;//移相寄存器
    //EPwm6Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm6Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm6Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm6Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm6Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm6Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm6Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm6Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm6Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm6Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm6Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm6Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm6Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm6Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm6Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm6Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm6Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm6Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm6Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

void DI_EPWM7_SYNC1_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM7=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm7Gpio();//打开EPWM1引脚
    SyncSocRegs.SYNCSELECT.bit.EPWM7SYNCIN=0;//EPWM4/7/10需要另外选择同步源，详情见表Figure 15-7选0和EPWM1输出同步，选1和4同步
    //TB部分
    EPwm7Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm7Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm7Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm7Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm7Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm7Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm7Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm7Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm7Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm7Regs.TBCTR=0x0000;//清零
    EPwm7Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm7Regs.TBPHS.bit.TBPHS=300;//移相寄存器
    //EPwm7Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm7Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm7Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm7Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm7Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm7Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm7Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB
    EPwm7Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm7Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm7Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm7Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm7Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm7Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm7Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm7Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm7Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm7Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm7Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm7Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

void DI_EPWM8_SYNC_Init()
{

    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM8=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    InitEPwm8Gpio();//打开EPWM1引脚
    //TB部分
    EPwm8Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm8Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm8Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm8Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm8Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm8Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm8Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm8Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm8Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm8Regs.TBCTR=0x0000;//清零
    EPwm8Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm8Regs.TBPHS.bit.TBPHS=300;//移相寄存器
    //EPwm8Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm8Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm8Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm8Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm8Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm8Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm8Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm8Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm8Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm8Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm8Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm8Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm8Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm8Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm8Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm8Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm8Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm8Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm8Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

void DI_NEPWM9_SYNC_Init()
{

    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM9=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    GPIO_SetupPinOptions(16, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(16, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(17, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(17, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    //TB部分
    EPwm9Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm9Regs.TBCTL.bit.SWFSYNC=0;//0为不接收同步脉冲1反之
    EPwm9Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm9Regs.TBCTL.bit.PHSEN=0;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm9Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm9Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm9Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm9Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm9Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm9Regs.TBCTR=0x0000;//清零
    EPwm9Regs.TBCTL.bit.SYNCOSEL=1;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm9Regs.TBPHS.bit.TBPHS=0;//移相寄存器
    //EPwm9Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm9Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm9Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm9Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm9Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm9Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm9Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm9Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm9Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm9Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm9Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm9Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm9Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm9Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm9Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm9Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm9Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm9Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm9Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

void DI_NEPWM10_SYNC1_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM10=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    GPIO_SetupPinOptions(18, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(18, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(19, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(19, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    SyncSocRegs.SYNCSELECT.bit.EPWM10SYNCIN=0;//EPWM4/7/10需要另外选择同步源，详情见表Figure 15-7
    //TB部分
    EPwm10Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm10Regs.TBCTL.bit.SWFSYNC=0;//0为不接收同步脉冲1反之
    EPwm10Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm10Regs.TBCTL.bit.PHSEN=0;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm10Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm10Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm10Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm10Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm10Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm10Regs.TBCTR=0x0000;//清零
    EPwm9Regs.TBCTR=0x0000;//清零
    EPwm10Regs.TBCTL.bit.SYNCOSEL=1;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm10Regs.TBPHS.bit.TBPHS=0;//移相寄存器
    //EPwm10Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm10Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm10Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm10Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm10Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm10Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm10Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

        //COMPB
    EPwm10Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm10Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm10Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm10Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm10Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm10Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm10Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm10Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm10Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm10Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm10Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm10Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;
}

void DI_NEPWM11_SYNC_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM11=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    GPIO_SetupPinOptions(20, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(20, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(21, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(21, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    //TB部分
    EPwm11Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm11Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm11Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm11Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm11Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm11Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm11Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm11Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm11Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm11Regs.TBCTR=0x0000;//清零
    EPwm11Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm11Regs.TBPHS.bit.TBPHS=1250;//移相寄存器
    //EPwm11Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm11Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm11Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm11Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm11Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm11Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm11Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm11Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm11Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm11Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm11Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm11Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm11Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm11Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm11Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm11Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm11Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm11Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm11Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}


void DI_NEPWM12_SYNC_Init()
{
    //初始化准备
    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=0;//关闭时间基准模块时钟
    CpuSysRegs.PCLKCR2.bit.EPWM12=1;//使能EPWM1外设
    EDIS;
    //EPWM-GPIO初始化
    GPIO_SetupPinOptions(22, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(22, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    GPIO_SetupPinOptions(23, GPIO_OUTPUT, GPIO_PUSHPULL);//初始化GPIO功能(gpio口，1输出0输入，0推挽，其它选项查看定义GPIO_PUSHPULL的位置以及后面内容);
    GPIO_SetupPinMux(23, GPIO_MUX_CPU1, 5);//初始化GPIO引脚(GPIO口，选择CPU1还是2，0为普通GPIO其它功能查复用表);
    //TB部分
    EPwm12Regs.TBCTL.bit.FREE_SOFT=2;//2为仿真下自由运行
    EPwm12Regs.TBCTL.bit.SWFSYNC=1;//0为不接收同步脉冲1反之
    EPwm12Regs.TBCTL.bit.PHSDIR=1;//同步后计数方向，上下计数专用，1上0下
    EPwm12Regs.TBCTL.bit.PHSEN=1;//是否使用相位寄存器，0不使用1使用TBPHS寄存器
    EPwm12Regs.TBCTL.bit.CTRMODE=TB_COUNT_UPDOWN;//上下计数
    EPwm12Regs.TBCTL.bit.HSPCLKDIV=TB_DIV1;//高速时钟源不分频
    EPwm12Regs.TBCTL.bit.CLKDIV=TB_DIV1;//时钟源不分频
    EPwm12Regs.TBCTL.bit.PRDLD=0;//TBPRD影子寄存器0为启用
    EPwm12Regs.TBPRD=2500;//100Mhz/(2500*2)=20khz
    EPwm12Regs.TBCTR=0x0000;//清零
    EPwm12Regs.TBCTL.bit.SYNCOSEL=0;//同步信号输出选择，0为输出与输入相接，1为计数为0时，2为等于CMPB，3由TBCTL2[SYNCOSELX]选择
    EPwm12Regs.TBPHS.bit.TBPHS=1250;//移相寄存器
    //EPwm12Regs.TBCTL2.bit.SYNCOSELX=0;//0禁用同步输出，1为CMPC，2为CMPD
    //比较器部分
    EPwm12Regs.CMPCTL.bit.SHDWAMODE=CC_SHADOW;//使用影子寄存器
    EPwm12Regs.CMPCTL.bit.SHDWBMODE=CC_SHADOW;//使用影子寄存器
    EPwm12Regs.CMPCTL.bit.LOADAMODE=CC_CTR_ZERO;//选择影子装载时机
    EPwm12Regs.CMPCTL.bit.LOADBMODE=CC_CTR_ZERO;//选择影子装载时机

    //AQ动作限位寄存器
        //COMPA
    EPwm12Regs.AQCTLA.bit.CAU=AQ_SET;//指定EPWMA当COMPA=TBCTR且在向上计数时的动作
    EPwm12Regs.AQCTLA.bit.CAD=AQ_CLEAR;//指定EPWMA当COMPA=TBCTR且在向下计数时的动作

    //COMPB
    EPwm12Regs.AQCTLB.bit.CAU=AQ_CLEAR;//指定EPWMB当COMPA=TBCTR且在向上计数时的动作
    EPwm12Regs.AQCTLB.bit.CAD=AQ_SET;//指定EPWMB当COMPA=TBCTR且在向下计数时的动作

    //DB死区控制
    EPwm12Regs.DBCTL.bit.OUT_MODE=DB_DISABLE;//可选择死区是使用在A还是B或者全选，更详细的控制查阅手册

    EALLOW;
    //DC数字比较器部分DCTRIPSEL/TZDCSEL/DCACTL
    //DC输入判决器EVENT QUAL有A/B两个模块，各模块输入有高位和低位，有两个不同输出,由DCTRIPSEL决定输入通道，由TZDCSEL决定输入和输出的关系
    EPwm12Regs.DCTRIPSEL.bit.DCAHCOMPSEL=3;//选择输入到DCAH的TRIP通道，0=trip1,1=trip2,2=trip3...
    EPwm12Regs.TZDCSEL.bit.DCAEVT1=2;//=2对应DCAH为高时触发DCAEVT1，无视DCAL，输出通道默认=0不触发事件
    //QUAL后级的事件触发器
    EPwm12Regs.DCACTL.bit.EVT1FRCSYNCSEL=1;//异步触发DCAEVT1.force，不经过时钟
    EPwm12Regs.DCACTL.bit.EVT1SRCSEL=0;//选用DCAEVT1作为输入源
    //TZFLG是由EVT1SRCSEL选择的信号经过一次时钟延迟后经过RS锁存器直接触发,通过TZFLG[DCAEVT1]读取,通过TZCLR[DCAEVT1]清零
    //EVT1SOCE,EVT1SYNCE触发采样和EPWM同步，这里不使用

    //TZ错误跳闸部分TZSEL/TZCTL(这里暂时不使用TZ中断)
    EPwm12Regs.TZSEL.bit.DCAEVT1=1;//选用DCAEVT1作为输入源
    EPwm12Regs.TZCTL.bit.TZA=2;//tz触发后EPWMA口的状态，0高阻1拉高2拉低3不动作
    EPwm12Regs.TZCTL.bit.TZB=2;//tz触发后EPWMB口的状态，0高阻1拉高2拉低3不动作
    //EPwm12Regs.TZCLR.bit.DCAEVT1=0;//清空单次触发保护，在程序中使用
    //EPwm12Regs.TZCLR.bit.OST=0;//清空单次触发保护，在程序中使用
    EDIS;

    EALLOW;
    CpuSysRegs.PCLKCR0.bit.TBCLKSYNC=1;//恢复时间基准模块时钟
    EDIS;

}

