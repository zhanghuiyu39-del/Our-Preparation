#include "menu.h"
#include "oled.h"
#include "key.h"
#include "pid.h"
#include "arm_math.h"

int8_t selectItem = 0;    // 当前菜单在当前菜单页的索引，最大为3，一页最多显示四个菜单
int8_t scrollBar = 0;     // 滚动条
struct Menu_t *menuPoint; // 当前菜单
u16 flagpwm = 0, flagloop = 0;
extern float32_t deta_theta, x;
extern PID_TypeDef pid_voltage_loop,pid_current_loop;
extern PI_CTRL_DATA_DEF vloop_pi;

// 一些菜单的功能函数
void KpAdd(void);
void KpSub(void);
void KiAdd(void);
void KiSub(void);
void UdAdd(void);
void UdSub(void);
void PWM0(void);
void PWM1(void);
void thetasub(void);
void thetaadd(void);
void PWM_ON(void);
void PWM_OFF(void);
void voltsub();
void voltadd();
void kisub();
void kiadd();
void kpsub();
void kpadd();
void ILOOP_ON();
void ILOOP_OFF();
void ikpsub();
void ikpadd();
void ikisub();
void ikiadd();
void isetsub();
void isetadd();
// 主菜单
struct MenuProperty_t menuMainProperty = {4, 0};
struct Menu_t menuMain[4] =
    {
        {&menuMainProperty, "mode          ", ILOOP_OFF, ILOOP_ON, NULL, NULL},
        {&menuMainProperty, "PWM           ", PWM_OFF, PWM_ON, NULL, NULL},
        {&menuMainProperty, "iset          ",  isetsub, isetadd,NULL, NULL},
        {&menuMainProperty, "vset          ", voltsub, voltadd, NULL, NULL},
};
// animal的子菜单
struct MenuProperty_t setMenu1Property = {6, 2};
struct Menu_t setMenu1[6] =
    {
        {&setMenu1Property, "last menu     ", NULL, NULL, menuMain, NULL},
        {&setMenu1Property, "Ud            ", NULL, NULL, menuMain, NULL},
        {&setMenu1Property, "Id_ref        ", NULL, NULL, menuMain, NULL},
        {&setMenu1Property, "              ", NULL, NULL, menuMain, NULL},
        {&setMenu1Property, "              ", NULL, NULL, menuMain, NULL},
        {&setMenu1Property, "              ", NULL, NULL, menuMain, NULL}};
// Pid的子菜单
struct MenuProperty_t setMenu2Property = {4, 0};
struct Menu_t setMenu2[4] =
    {
        {&setMenu2Property, "last menu     ", NULL, NULL, menuMain, NULL},
        {&setMenu2Property, "KP   0        ", ikpsub, ikpadd, menuMain, NULL},
        {&setMenu2Property, "KI   0        ", ikisub, ikiadd, menuMain, NULL},
        {&setMenu2Property, "iset 0        ", isetsub, isetadd, menuMain, NULL},
};
	
struct MenuProperty_t setMenu3Property = {4, 0};
struct Menu_t setMenu3[4] =
    {
        {&setMenu2Property, "last menu     ", NULL, NULL, menuMain, NULL},
        {&setMenu2Property, "KP   0        ", kpsub, kpadd, menuMain, NULL},
        {&setMenu2Property, "KI   0        ", kisub, kiadd, menuMain, NULL},
		{&setMenu2Property, "volt 0        ", voltsub, voltadd, menuMain, NULL},

};

void PWM_ON()
{

    TIM_BDTRInitTypeDef TIM_BDTRStruct;
    TIM_BDTRStruct.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRStruct.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRStruct.TIM_LOCKLevel = TIM_LOCKLevel_1;
    TIM_BDTRStruct.TIM_DeadTime = 0x50;
    TIM_BDTRStruct.TIM_Break = TIM_Break_Disable;
    TIM_BDTRStruct.TIM_BreakPolarity = TIM_BreakPolarity_High;
    TIM_BDTRStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    TIM_BDTRConfig(TIM1, &TIM_BDTRStruct);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    flagpwm = 1;
}
void PWM_OFF()
{

    TIM_BDTRInitTypeDef TIM_BDTRStruct;
    TIM_BDTRStruct.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRStruct.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRStruct.TIM_LOCKLevel = TIM_LOCKLevel_1;
    TIM_BDTRStruct.TIM_DeadTime = 0x50;
    TIM_BDTRStruct.TIM_Break = TIM_Break_Disable;
    TIM_BDTRStruct.TIM_BreakPolarity = TIM_BreakPolarity_High;
    TIM_BDTRStruct.TIM_AutomaticOutput = TIM_AutomaticOutput_Disable;
    TIM_BDTRConfig(TIM1, &TIM_BDTRStruct);
    TIM_CtrlPWMOutputs(TIM1, DISABLE);
    flagpwm = 0;
}

void PWM0()
{

    TIM_CtrlPWMOutputs(TIM1, DISABLE);
    flagpwm = 0;
}
void PWM1()
{

    TIM_CtrlPWMOutputs(TIM1, ENABLE);

    flagpwm = 1;
}
void thetaadd()
{
    x += 0.05;
    if (x >= 1)
        x = 1;
    deta_theta = acosf(x);
}
void thetasub()
{
    x -= 0.05;
    if (x <= 0.5)
        x = 0.5;
    deta_theta = acosf(x);
}
void voltadd()
{
    pid_voltage_loop.initial_value += 0.05;
}
void voltsub()
{
    pid_voltage_loop.initial_value -= 0.05;
}
void kisub()
{
    pid_voltage_loop.ki -= 0.01;
}
void kiadd()
{
    pid_voltage_loop.ki += 0.01;
}
void kpsub()
{
    pid_voltage_loop.kp -= 0.05;
}
void kpadd()
{
    pid_voltage_loop.kp += 0.05;
}
void ILOOP_ON()
{
    flagloop = 1;
}
void ILOOP_OFF()
{
    flagloop = 0;
}
void ikpsub()
{
    pid_current_loop.kp -= 0.05;
}
void ikpadd()
{
    pid_current_loop.kp += 0.05;
}
void ikisub()
{
    pid_current_loop.ki -= 0.01;
}
void ikiadd()
{
   pid_current_loop.ki += 0.01;
}
void isetsub()
{
    pid_current_loop.initial_value -= 0.2;
}
void isetadd()
{
    pid_current_loop.initial_value += 0.2;
}



/**
 * @Name    DisplayRefreash
 * @brief   刷新当前菜单页的所有菜单项
 * @param   nowMenu: 当前菜单指针
 **			 selectItem: 当前菜单在当前菜单页的索引，指小光标在第几行
 **           scrollBar：滚动条所在位置
 */
void DisplayRefreash(struct Menu_t *nowMenu, u8 selectItem, u8 scrollBar)
{
    int i = 0;
    static u8 lastSelectItem = 0; // 记录上次索引
    if (nowMenu == menuMain)
        OLED_Clear();
    OLED_ShowChar(0, lastSelectItem, ' '); // 清除上次索引
    OLED_ShowChar(0, selectItem, '>');     // 画出这次索引

    for (i = 0; i < (nowMenu->MenuProperty->MenuLen - nowMenu->MenuProperty->scrollBarLen); i++)
    {
        OLED_ShowString(2, i, nowMenu[i + scrollBar].displayString);
    }

    lastSelectItem = selectItem;
}
/**
 * @Name    DisplayRefreashDate
 * @brief   刷新当前菜单页的所有菜单项,主要拿来更新数据
 * @param   nowMenu:    当前菜单指针
 **			 selectItem: 当前菜单在当前菜单页的索引，指小光标在第几行
 **           scrollBar： 滚动条所在位置
 */
void DisplayRefreashData(struct Menu_t *nowMenu, u8 selectItem, u8 scrollBar)
{
    int i = 0;
    for (i = 0; i < (nowMenu->MenuProperty->MenuLen - nowMenu->MenuProperty->scrollBarLen); i++)
    {
        OLED_ShowString(2, i, nowMenu[i + scrollBar].displayString);
    }
}

/**
 * @name    GuiInit
 * @brief   一些菜单初始化主要是主菜单页的子菜单确定，由于父菜单结构体初始化时不能初始化子菜单
 * 		 所以在这里初始化
 */
void MenuInit()
{
    // menuMain[2].childrenMenu = setMenu1;
	menuPoint = menuMain;
//    menuMain[2].childrenMenu = setMenu2;
//	menuMain[3].childrenMenu = setMenu3;
    DisplayRefreash(menuPoint, selectItem, scrollBar);
}
/**
 * @name    GuiDataDisplayRefresh
 * @brief   菜单页的数据更新，记得一定要字符串占满15个字符
 */
void MenuDataDisplayRefresh()
{
    if (menuPoint == menuMain)
    {
        sprintf((char *)menuMain[0].displayString, "mode   %1d  ", flagloop);
        sprintf((char *)menuMain[1].displayString, "PWM    %1d  ", flagpwm);
        sprintf((char *)menuMain[2].displayString, "iset   %.2f ", pid_current_loop.initial_value);
		    sprintf((char *)menuMain[3].displayString, "vset   %.2f ", pid_voltage_loop.initial_value);

        DisplayRefreashData(menuPoint, selectItem, scrollBar);
    }
    
    else if (menuPoint == setMenu2)
    {
        sprintf((char *)setMenu2[1].displayString, "KP   %.3f    ", pid_current_loop.kp);
        sprintf((char *)setMenu2[2].displayString, "KI   %.3f    ", pid_current_loop.ki);
        sprintf((char *)setMenu2[3].displayString, "Iset %.2f    ", pid_current_loop.initial_value);

        DisplayRefreashData(menuPoint, selectItem, scrollBar);
    }
	else if (menuPoint == setMenu3)
    {
        sprintf((char *)setMenu3[1].displayString, "KP   %.3f    ", pid_voltage_loop.kp);
        sprintf((char *)setMenu3[2].displayString, "KI   %.3f    ", pid_voltage_loop.ki);
        sprintf((char *)setMenu3[3].displayString, "volt %.2f    ", pid_voltage_loop.initial_value );

        DisplayRefreashData(menuPoint, selectItem, scrollBar);
    }
}
/**
 * @Name    GuiControl
 * @brief   按键控制，按键控制的确定变量在keyscan.c中,按键扫描实现又是放在定时器里面的
 * @param   : [输入/出]
 * @retval  void
 * @author  LQH
 * @Data    2022-01-27
 */
void MenuControl()
{
    u16 key = 0;
    key = KEY_Scan(0);

    if (key == 1) // 上键按下
    {
        selectItem--;                         // 当前菜单在当前菜单页的索引--
        if (selectItem < 0 && scrollBar != 0) // 小于0,但是滚动条不在0，就减滚动条
        {
            selectItem = 0;
            scrollBar--;
        }
        else if (selectItem < 0 && scrollBar == 0) // 小于0,滚动条也在0，就将索引移到最后，滚动条到最大
        {
            selectItem = menuPoint->MenuProperty->MenuLen - 1 - menuPoint->MenuProperty->scrollBarLen;
            scrollBar = menuPoint->MenuProperty->scrollBarLen;
        }
        DisplayRefreash(menuPoint, selectItem, scrollBar); // 刷新显示
    }
    else if (key == 2) // 和上键差不多
    {

        selectItem++;
        // 假如索引大于最大值，但是滚动条不在最大值，保持索引最大值，滚动条++
        if (selectItem > (menuPoint->MenuProperty->MenuLen - 1 - menuPoint->MenuProperty->scrollBarLen) && scrollBar != menuPoint->MenuProperty->scrollBarLen)
        {
            selectItem = menuPoint->MenuProperty->MenuLen - 1 - menuPoint->MenuProperty->scrollBarLen;
            scrollBar++;
        }
        // 假如索引大于最大值，滚动条在最大值，移动到第一个位置
        else if (selectItem > (menuPoint->MenuProperty->MenuLen - 1 - menuPoint->MenuProperty->scrollBarLen) && scrollBar == menuPoint->MenuProperty->scrollBarLen)
        {
            selectItem = 0;
            scrollBar = 0;
        }
        DisplayRefreash(menuPoint, selectItem, scrollBar);
    }
    else if (key == 4)
    {
        // 假如当前菜单的func1不为空，执行相关函数
        if (menuPoint[selectItem + scrollBar].func1 != NULL)
        {
            menuPoint[selectItem + scrollBar].func1();
        }

        DisplayRefreash(menuPoint, selectItem, scrollBar);
    }
    else if (key == 3)
    {
        if (selectItem == 0 && scrollBar == 0 && menuPoint[selectItem].fatherMenu != NULL) // 假如索引为零而且父菜单不为空，指向父指针
        {
            menuPoint = menuPoint[selectItem].fatherMenu;
        }
        else if (menuPoint[selectItem + scrollBar].childrenMenu != NULL) // 假如该索引子菜单页不为空，指向子菜单
        {
            if (menuPoint[selectItem + scrollBar].func2 != NULL) // 假如当前菜单的func2不为空，执行相关函数
            {
                menuPoint[selectItem + scrollBar].func2();
            }
            menuPoint = menuPoint[selectItem + scrollBar].childrenMenu;
            selectItem = 0;
        }
        else if (menuPoint[selectItem + scrollBar].func2 != NULL) // 假如当前菜单的func2不为空，执行相关函数
        {
            menuPoint[selectItem + scrollBar].func2();
        }

        DisplayRefreash(menuPoint, selectItem, scrollBar);
    }
    else if (key == 5)
    {
        DisplayRefreash(menuPoint, selectItem, scrollBar);
    }
    if (key != 0)
        MenuDataDisplayRefresh();
}