#ifndef RTW_HEADER_x2026A2022b_acc_h_
#define RTW_HEADER_x2026A2022b_acc_h_
#ifndef x2026A2022b_acc_COMMON_INCLUDES_
#define x2026A2022b_acc_COMMON_INCLUDES_
#include <stdlib.h>
#define S_FUNCTION_NAME simulink_only_sfcn
#define S_FUNCTION_LEVEL 2
#ifndef RTW_GENERATED_S_FUNCTION
#define RTW_GENERATED_S_FUNCTION
#endif
#include "rtwtypes.h"
#include "simstruc.h"
#include "fixedpoint.h"
#endif
#include "x2026A2022b_acc_types.h"
#include <stddef.h>
#include <float.h>
#include "mwmathutil.h"
#include <string.h>
#include "rt_defines.h"
#include "rt_nonfinite.h"
typedef struct { creal_T B_8_16_0 [ 3 ] ; real_T B_8_0_0 [ 3 ] ; real_T
B_8_1_0 [ 3 ] ; real_T B_8_3_0 ; real_T B_8_5_0 [ 3 ] ; real_T B_8_7_0 [ 3 ]
; real_T B_8_8_0 [ 3 ] ; real_T B_8_9_0 [ 3 ] ; real_T B_8_11_0 ; real_T
B_8_13_0 [ 3 ] ; real_T B_8_15_0 [ 3 ] ; real_T B_8_19_0 [ 3 ] ; real_T
B_8_21_0 [ 3 ] ; real_T B_8_22_0 [ 3 ] ; real_T B_7_0_0 [ 3 ] ; real_T
B_7_1_0 [ 3 ] ; real_T B_6_0_0 [ 3 ] ; real_T B_6_1_0 [ 3 ] ; }
B_RMS_x2026A2022b_T ; typedef struct { real_T Memory_PreviousInput [ 3 ] ;
real_T Memory_PreviousInput_o [ 3 ] ; struct { real_T modelTStart ; }
TransportDelay_RWORK ; struct { real_T modelTStart ; } TransportDelay_RWORK_m
; struct { void * TUbufferPtrs [ 6 ] ; } TransportDelay_PWORK ; struct { void
* TUbufferPtrs [ 6 ] ; } TransportDelay_PWORK_j ; int32_T RMS_sysIdxToRun ;
int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_h ; struct { int_T Tail [ 3 ] ;
int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK ; struct { int_T Tail [ 3 ] ; int_T
Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK_c ; int8_T RMS_SubsysRanBC ; boolean_T
RMS_MODE ; char_T pad_RMS_MODE [ 2 ] ; } DW_RMS_x2026A2022b_T ; typedef
struct { real_T integrator_CSTATE_a [ 3 ] ; real_T integrator_CSTATE_h [ 3 ]
; } X_RMS_x2026A2022b_T ; typedef struct { real_T integrator_CSTATE_a [ 3 ] ;
real_T integrator_CSTATE_h [ 3 ] ; } XDot_RMS_x2026A2022b_T ; typedef struct
{ boolean_T integrator_CSTATE_a [ 3 ] ; boolean_T integrator_CSTATE_h [ 3 ] ;
} XDis_RMS_x2026A2022b_T ; typedef struct { real_T integrator_CSTATE_a [ 3 ]
; real_T integrator_CSTATE_h [ 3 ] ; } XAbsTol_RMS_x2026A2022b_T ; typedef
struct { real_T integrator_CSTATE_a [ 3 ] ; real_T integrator_CSTATE_h [ 3 ]
; } XPtMin_RMS_x2026A2022b_T ; typedef struct { real_T integrator_CSTATE_a [
3 ] ; real_T integrator_CSTATE_h [ 3 ] ; } XPtMax_RMS_x2026A2022b_T ; typedef
struct { real_T B_10_1_0 [ 3 ] ; real_T B_10_2_0 [ 3 ] ; real_T B_10_3_0 ;
real_T B_10_4_0 [ 3 ] ; real_T B_10_7_0 [ 3 ] ; real_T B_10_8_0 [ 3 ] ;
real_T B_10_9_0 [ 3 ] ; real_T B_10_10_0 [ 3 ] ; real_T B_9_0_0 [ 3 ] ;
real_T B_9_1_0 [ 3 ] ; } B_TrueRMS_x2026A2022b_T ; typedef struct { real_T
Memory_PreviousInput [ 3 ] ; struct { real_T modelTStart ; }
TransportDelay_RWORK ; struct { void * TUbufferPtrs [ 6 ] ; }
TransportDelay_PWORK ; int32_T TrueRMS_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; struct { int_T Tail [ 3 ] ;
int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK ; int_T
Saturationtoavoidnegativesqrt_MODE [ 3 ] ; int8_T TrueRMS_SubsysRanBC ;
int8_T Sqrt_DWORK1 ; boolean_T TrueRMS_MODE ; char_T pad_TrueRMS_MODE [ 5 ] ;
} DW_TrueRMS_x2026A2022b_T ; typedef struct { real_T integrator_CSTATE_o [ 3
] ; } X_TrueRMS_x2026A2022b_T ; typedef struct { real_T integrator_CSTATE_o [
3 ] ; } XDot_TrueRMS_x2026A2022b_T ; typedef struct { boolean_T
integrator_CSTATE_o [ 3 ] ; } XDis_TrueRMS_x2026A2022b_T ; typedef struct {
real_T integrator_CSTATE_o [ 3 ] ; } XAbsTol_TrueRMS_x2026A2022b_T ; typedef
struct { real_T integrator_CSTATE_o [ 3 ] ; } XPtMin_TrueRMS_x2026A2022b_T ;
typedef struct { real_T integrator_CSTATE_o [ 3 ] ; }
XPtMax_TrueRMS_x2026A2022b_T ; typedef struct { real_T
Saturationtoavoidnegativesqrt_UprLim_ZC [ 3 ] ; real_T
Saturationtoavoidnegativesqrt_LwrLim_ZC [ 3 ] ; } ZCV_TrueRMS_x2026A2022b_T ;
typedef struct { ZCSigState Saturationtoavoidnegativesqrt_UprLim_ZCE [ 3 ] ;
ZCSigState Saturationtoavoidnegativesqrt_LwrLim_ZCE [ 3 ] ; }
ZCE_TrueRMS_x2026A2022b_T ; typedef struct { creal_T B_19_16_0 ; real_T
B_19_0_0 ; real_T B_19_1_0 ; real_T B_19_3_0 ; real_T B_19_5_0 ; real_T
B_19_7_0 ; real_T B_19_8_0 ; real_T B_19_9_0 ; real_T B_19_11_0 ; real_T
B_19_13_0 ; real_T B_19_15_0 ; real_T B_19_19_0 ; real_T B_19_21_0 ; real_T
B_19_22_0 ; real_T B_18_0_0 ; real_T B_18_1_0 ; real_T B_17_0_0 ; real_T
B_17_1_0 ; } B_RMS_x2026A2022b_a_T ; typedef struct { real_T
Memory_PreviousInput ; real_T Memory_PreviousInput_a ; struct { real_T
modelTStart ; } TransportDelay_RWORK ; struct { real_T modelTStart ; }
TransportDelay_RWORK_p ; struct { void * TUbufferPtrs [ 2 ] ; }
TransportDelay_PWORK ; struct { void * TUbufferPtrs [ 2 ] ; }
TransportDelay_PWORK_m ; int32_T RMS_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_e ; struct { int_T Tail ; int_T
Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
TransportDelay_IWORK ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } TransportDelay_IWORK_d ; int8_T
RMS_SubsysRanBC ; boolean_T RMS_MODE ; char_T pad_RMS_MODE [ 2 ] ; }
DW_RMS_x2026A2022b_e_T ; typedef struct { real_T integrator_CSTATE_l ; real_T
integrator_CSTATE_cu ; } X_RMS_x2026A2022b_a_T ; typedef struct { real_T
integrator_CSTATE_l ; real_T integrator_CSTATE_cu ; }
XDot_RMS_x2026A2022b_e_T ; typedef struct { boolean_T integrator_CSTATE_l ;
boolean_T integrator_CSTATE_cu ; } XDis_RMS_x2026A2022b_j_T ; typedef struct
{ real_T integrator_CSTATE_l ; real_T integrator_CSTATE_cu ; }
XAbsTol_RMS_x2026A2022b_d_T ; typedef struct { real_T integrator_CSTATE_l ;
real_T integrator_CSTATE_cu ; } XPtMin_RMS_x2026A2022b_e_T ; typedef struct {
real_T integrator_CSTATE_l ; real_T integrator_CSTATE_cu ; }
XPtMax_RMS_x2026A2022b_i_T ; typedef struct { real_T B_21_1_0 ; real_T
B_21_2_0 ; real_T B_21_3_0 ; real_T B_21_4_0 ; real_T B_21_7_0 ; real_T
B_21_8_0 ; real_T B_21_9_0 ; real_T B_21_10_0 ; real_T B_20_0_0 ; real_T
B_20_1_0 ; } B_TrueRMS_x2026A2022b_d_T ; typedef struct { real_T
Memory_PreviousInput ; struct { real_T modelTStart ; } TransportDelay_RWORK ;
struct { void * TUbufferPtrs [ 2 ] ; } TransportDelay_PWORK ; int32_T
TrueRMS_sysIdxToRun ; int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ;
struct { int_T Tail ; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T
MaxNewBufSize ; } TransportDelay_IWORK ; int_T
Saturationtoavoidnegativesqrt_MODE ; int8_T TrueRMS_SubsysRanBC ; int8_T
Sqrt_DWORK1 ; boolean_T TrueRMS_MODE ; char_T pad_TrueRMS_MODE [ 5 ] ; }
DW_TrueRMS_x2026A2022b_c_T ; typedef struct { real_T integrator_CSTATE_ic ; }
X_TrueRMS_x2026A2022b_h_T ; typedef struct { real_T integrator_CSTATE_ic ; }
XDot_TrueRMS_x2026A2022b_m_T ; typedef struct { boolean_T
integrator_CSTATE_ic ; } XDis_TrueRMS_x2026A2022b_e_T ; typedef struct {
real_T integrator_CSTATE_ic ; } XAbsTol_TrueRMS_x2026A2022b_o_T ; typedef
struct { real_T integrator_CSTATE_ic ; } XPtMin_TrueRMS_x2026A2022b_p_T ;
typedef struct { real_T integrator_CSTATE_ic ; }
XPtMax_TrueRMS_x2026A2022b_g_T ; typedef struct { real_T
Saturationtoavoidnegativesqrt_UprLim_ZC_h ; real_T
Saturationtoavoidnegativesqrt_LwrLim_ZC_g ; } ZCV_TrueRMS_x2026A2022b_n_T ;
typedef struct { ZCSigState Saturationtoavoidnegativesqrt_UprLim_ZCE_d ;
ZCSigState Saturationtoavoidnegativesqrt_LwrLim_ZCE_h ; }
ZCE_TrueRMS_x2026A2022b_b_T ; typedef struct { real_T B_38_0_0 ; real_T
B_38_1_0 [ 21 ] ; real_T B_38_1_1 [ 12 ] ; real_T B_38_2_0 ; real_T B_38_3_0
; real_T B_38_4_0 ; real_T B_38_11_0 ; real_T B_38_15_0 ; real_T B_38_21_0 ;
real_T B_38_25_0 ; real_T B_38_42_0 [ 2 ] ; real_T B_38_45_0 ; real_T
B_38_46_0 ; real_T B_38_47_0 ; real_T B_38_49_0 ; real_T B_38_50_0 ; real_T
B_38_51_0 ; real_T B_38_52_0 ; real_T B_38_54_0 ; real_T B_38_55_0 ; real_T
B_38_57_0 ; real_T B_38_60_0 ; real_T B_38_67_0 ; real_T B_38_68_0 ; real_T
B_38_69_0 ; real_T B_38_70_0 [ 3 ] ; real_T B_38_71_0 [ 3 ] ; real_T
B_38_73_0 ; real_T B_38_74_0 ; real_T B_38_75_0 ; real_T B_38_76_0 [ 3 ] ;
real_T B_38_78_0 ; real_T B_38_79_0 ; real_T B_38_80_0 ; real_T B_38_86_0 ;
real_T B_38_90_0 [ 3 ] ; real_T B_38_96_0 [ 3 ] ; real_T B_38_98_0 ; real_T
B_38_101_0 ; real_T B_38_108_0 [ 6 ] ; real_T B_38_116_0 ; real_T B_38_119_0
[ 2 ] ; real_T B_38_132_0 ; real_T B_38_134_0 ; real_T B_38_137_0 ; real_T
B_38_140_0 ; real_T B_38_143_0 [ 4 ] ; real_T B_38_155_0 ; real_T B_38_156_0
; real_T B_38_158_0 ; real_T B_38_160_0 ; real_T B_38_162_0 ; real_T
B_38_167_0 ; real_T B_38_170_0 ; real_T B_38_172_0 ; real_T B_38_174_0 ;
real_T B_38_184_0 ; real_T B_38_192_0 [ 3 ] ; real_T B_38_195_0 ; real_T
B_38_197_0 [ 3 ] ; real_T B_38_199_0 [ 3 ] ; real_T B_38_213_0 ; real_T
B_38_216_0 ; real_T B_38_218_0 ; real_T B_38_220_0 ; real_T B_38_222_0 ;
real_T B_38_223_0 ; real_T B_38_230_0 ; real_T B_38_231_0 ; real_T B_38_233_0
; real_T B_38_235_0 ; real_T B_38_236_0 ; real_T B_38_238_0 ; real_T
B_38_240_0 ; real_T B_38_244_0 ; real_T B_38_0_0_m [ 6 ] ; real_T B_38_1_0_c
[ 2 ] ; real_T B_38_2_0_k [ 4 ] ; real_T B_38_5_0 ; real_T B_38_6_0 ; real_T
B_38_7_0 ; real_T B_38_8_0 ; real_T B_38_9_0 ; real_T B_38_11_0_c ; real_T
B_38_12_0 ; real_T B_38_13_0 ; real_T B_38_14_0 ; real_T B_38_16_0 ; real_T
B_38_17_0 ; real_T B_38_18_0 ; real_T B_38_19_0 ; real_T B_38_21_0_b ; real_T
B_38_22_0 ; real_T B_38_23_0 ; real_T B_38_24_0 ; real_T B_38_25_0_p [ 3 ] ;
real_T B_38_44_0 ; real_T B_38_45_0_c ; real_T B_4_0_1 ; real_T B_4_0_2 ;
real_T B_4_0_3 ; real_T B_3_0_1 ; real_T B_3_0_2 ; real_T B_3_0_3 ; real_T
B_2_0_1 ; real_T B_1_0_0 ; real_T B_1_1_0 ; real_T B_0_0_1 ; real_T
B_38_7_0_f [ 3 ] ; real_T B_38_95_0 [ 3 ] ; real_T B_38_151_0 [ 3 ] ; real_T
B_38_209_0 [ 3 ] ; boolean_T B_38_102_0 ; boolean_T B_38_27_0 ; boolean_T
B_38_28_0 ; boolean_T B_38_30_0 ; boolean_T B_38_31_0 ; boolean_T B_38_33_0 ;
boolean_T B_38_34_0 ; boolean_T B_38_36_0 ; boolean_T B_38_37_0 ; boolean_T
B_38_39_0 ; boolean_T B_38_40_0 ; boolean_T B_38_42_0_g ; boolean_T B_38_43_0
; char_T pad_B_38_43_0 [ 3 ] ; B_TrueRMS_x2026A2022b_T TrueRMS_e ;
B_RMS_x2026A2022b_T RMS_f ; B_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
B_RMS_x2026A2022b_T RMS_e ; B_TrueRMS_x2026A2022b_d_T TrueRMS_a ;
B_RMS_x2026A2022b_a_T RMS_d ; B_TrueRMS_x2026A2022b_d_T TrueRMS_f ;
B_RMS_x2026A2022b_a_T RMS_l ; B_TrueRMS_x2026A2022b_T TrueRMS_g ;
B_RMS_x2026A2022b_T RMS_n ; B_TrueRMS_x2026A2022b_T TrueRMS ;
B_RMS_x2026A2022b_T RMS ; } B_x2026A2022b_T ; typedef struct { real_T
StateSpace_DSTATE [ 12 ] ; real_T Memory_PreviousInput ; real_T
Memory_PreviousInput_i ; real_T Memory_PreviousInput_a [ 3 ] ; real_T
Memory_PreviousInput_as ; struct { real_T modelTStart ; } T_RWORK ; struct {
real_T modelTStart ; } T1_RWORK ; struct { real_T modelTStart ; } T_RWORK_k ;
struct { real_T modelTStart ; } T1_RWORK_k ; struct { real_T modelTStart ; }
TransportDelay_RWORK ; struct { real_T modelTStart ; } TransportDelay_RWORK_p
; struct { real_T modelTStart ; } TransportDelay_RWORK_p4 ; struct { real_T
modelTStart ; } TransportDelay_RWORK_i ; struct { void * AS ; void * BS ;
void * CS ; void * DS ; void * DX_COL ; void * BD_COL ; void * TMP1 ; void *
TMP2 ; void * XTMP ; void * SWITCH_STATUS ; void * SWITCH_STATUS_INIT ; void
* SW_CHG ; void * G_STATE ; void * USWLAST ; void * XKM12 ; void * XKP12 ;
void * XLAST ; void * ULAST ; void * IDX_SW_CHG ; void * Y_SWITCH ; void *
SWITCH_TYPES ; void * IDX_OUT_SW ; void * SWITCH_TOPO_SAVED_IDX ; void *
SWITCH_MAP ; } StateSpace_PWORK ; struct { void * TUbufferPtrs [ 2 ] ; }
T_PWORK ; struct { void * TUbufferPtrs [ 2 ] ; } T1_PWORK ; struct { void *
TUbufferPtrs [ 2 ] ; } T_PWORK_e ; struct { void * TUbufferPtrs [ 2 ] ; }
T1_PWORK_h ; void * Scope1_PWORK [ 2 ] ; void * Scope2_PWORK [ 2 ] ; void *
Scope3_PWORK [ 2 ] ; void * Scope4_PWORK [ 3 ] ; void * Scope5_PWORK [ 2 ] ;
void * Scope6_PWORK [ 2 ] ; struct { void * TUbufferPtrs [ 2 ] ; }
TransportDelay_PWORK ; struct { void * TUbufferPtrs [ 2 ] ; }
TransportDelay_PWORK_e ; struct { void * TUbufferPtrs [ 6 ] ; }
TransportDelay_PWORK_b ; struct { void * TUbufferPtrs [ 2 ] ; }
TransportDelay_PWORK_o ; void * Scope2_PWORK_f [ 2 ] ; void * Scope2_PWORK_a
[ 2 ] ; int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_k ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_m ; int32_T
DiscreteQPR3ph_sysIdxToRun ; int32_T DiscreteQPR3ph_sysIdxToRun_k ; int32_T
PF_sysIdxToRun ; int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_o ;
int32_T DiscretePR_sysIdxToRun ; int_T StateSpace_IWORK [ 11 ] ; struct {
int_T Tail ; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T
MaxNewBufSize ; } T_IWORK ; struct { int_T Tail ; int_T Head ; int_T Last ;
int_T CircularBufSize ; int_T MaxNewBufSize ; } T1_IWORK ; struct { int_T
Tail ; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize
; } T_IWORK_d ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } T1_IWORK_g ; struct { int_T Tail ;
int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
TransportDelay_IWORK ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } TransportDelay_IWORK_i ; struct {
int_T Tail [ 3 ] ; int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T
CircularBufSize [ 3 ] ; int_T MaxNewBufSize ; } TransportDelay_IWORK_k ;
struct { int_T Tail ; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T
MaxNewBufSize ; } TransportDelay_IWORK_ib ; int_T MinMax_MODE ; int_T
MinMax2_MODE ; int8_T Sqrt_DWORK1 ; int8_T Sqrt_DWORK1_i ; int8_T
Sqrt_DWORK1_e ; int8_T Sqrt_DWORK1_n ; boolean_T RelationalOperator7_Mode ;
boolean_T RelationalOperator6_Mode ; boolean_T RelationalOperator2_Mode ;
boolean_T RelationalOperator1_Mode ; boolean_T RelationalOperator_Mode ;
boolean_T RelationalOperator1_Mode_d ; char_T pad_RelationalOperator1_Mode_d
[ 2 ] ; DW_TrueRMS_x2026A2022b_T TrueRMS_e ; DW_RMS_x2026A2022b_T RMS_f ;
DW_TrueRMS_x2026A2022b_T TrueRMS_f5 ; DW_RMS_x2026A2022b_T RMS_e ;
DW_TrueRMS_x2026A2022b_c_T TrueRMS_a ; DW_RMS_x2026A2022b_e_T RMS_d ;
DW_TrueRMS_x2026A2022b_c_T TrueRMS_f ; DW_RMS_x2026A2022b_e_T RMS_l ;
DW_TrueRMS_x2026A2022b_T TrueRMS_g ; DW_RMS_x2026A2022b_T RMS_n ;
DW_TrueRMS_x2026A2022b_T TrueRMS ; DW_RMS_x2026A2022b_T RMS ; }
DW_x2026A2022b_T ; typedef struct { real_T integ1_CSTATE ; real_T
Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ; real_T
TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
X_TrueRMS_x2026A2022b_T TrueRMS_e ; X_RMS_x2026A2022b_T RMS_f ;
X_TrueRMS_x2026A2022b_T TrueRMS_f5 ; X_RMS_x2026A2022b_T RMS_e ;
X_TrueRMS_x2026A2022b_h_T TrueRMS_a ; X_RMS_x2026A2022b_a_T RMS_d ;
X_TrueRMS_x2026A2022b_h_T TrueRMS_f ; X_RMS_x2026A2022b_a_T RMS_l ;
X_TrueRMS_x2026A2022b_T TrueRMS_g ; X_RMS_x2026A2022b_T RMS_n ;
X_TrueRMS_x2026A2022b_T TrueRMS ; X_RMS_x2026A2022b_T RMS ; } X_x2026A2022b_T
; typedef int_T PeriodicIndX_x2026A2022b_T [ 1 ] ; typedef real_T
PeriodicRngX_x2026A2022b_T [ 2 ] ; typedef struct { real_T integ1_CSTATE ;
real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ;
real_T TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
XDot_TrueRMS_x2026A2022b_T TrueRMS_e ; XDot_RMS_x2026A2022b_T RMS_f ;
XDot_TrueRMS_x2026A2022b_T TrueRMS_f5 ; XDot_RMS_x2026A2022b_T RMS_e ;
XDot_TrueRMS_x2026A2022b_m_T TrueRMS_a ; XDot_RMS_x2026A2022b_e_T RMS_d ;
XDot_TrueRMS_x2026A2022b_m_T TrueRMS_f ; XDot_RMS_x2026A2022b_e_T RMS_l ;
XDot_TrueRMS_x2026A2022b_T TrueRMS_g ; XDot_RMS_x2026A2022b_T RMS_n ;
XDot_TrueRMS_x2026A2022b_T TrueRMS ; XDot_RMS_x2026A2022b_T RMS ; }
XDot_x2026A2022b_T ; typedef struct { boolean_T integ1_CSTATE ; boolean_T
Integ2_CSTATE ; boolean_T integ1_CSTATE_h ; boolean_T Integ2_CSTATE_m ;
boolean_T TransferFcn_CSTATE [ 2 ] ; boolean_T Integrator_CSTATE ; boolean_T
Integrator_CSTATE_p ; boolean_T integrator_CSTATE ; boolean_T
integrator_CSTATE_n ; boolean_T integrator_CSTATE_c [ 3 ] ; boolean_T
integrator_CSTATE_i ; XDis_TrueRMS_x2026A2022b_T TrueRMS_e ;
XDis_RMS_x2026A2022b_T RMS_f ; XDis_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
XDis_RMS_x2026A2022b_T RMS_e ; XDis_TrueRMS_x2026A2022b_e_T TrueRMS_a ;
XDis_RMS_x2026A2022b_j_T RMS_d ; XDis_TrueRMS_x2026A2022b_e_T TrueRMS_f ;
XDis_RMS_x2026A2022b_j_T RMS_l ; XDis_TrueRMS_x2026A2022b_T TrueRMS_g ;
XDis_RMS_x2026A2022b_T RMS_n ; XDis_TrueRMS_x2026A2022b_T TrueRMS ;
XDis_RMS_x2026A2022b_T RMS ; } XDis_x2026A2022b_T ; typedef struct { real_T
integ1_CSTATE ; real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T
Integ2_CSTATE_m ; real_T TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE
; real_T Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T
integrator_CSTATE_n ; real_T integrator_CSTATE_c [ 3 ] ; real_T
integrator_CSTATE_i ; XAbsTol_TrueRMS_x2026A2022b_T TrueRMS_e ;
XAbsTol_RMS_x2026A2022b_T RMS_f ; XAbsTol_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
XAbsTol_RMS_x2026A2022b_T RMS_e ; XAbsTol_TrueRMS_x2026A2022b_o_T TrueRMS_a ;
XAbsTol_RMS_x2026A2022b_d_T RMS_d ; XAbsTol_TrueRMS_x2026A2022b_o_T TrueRMS_f
; XAbsTol_RMS_x2026A2022b_d_T RMS_l ; XAbsTol_TrueRMS_x2026A2022b_T TrueRMS_g
; XAbsTol_RMS_x2026A2022b_T RMS_n ; XAbsTol_TrueRMS_x2026A2022b_T TrueRMS ;
XAbsTol_RMS_x2026A2022b_T RMS ; } CStateAbsTol_x2026A2022b_T ; typedef struct
{ real_T integ1_CSTATE ; real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ;
real_T Integ2_CSTATE_m ; real_T TransferFcn_CSTATE [ 2 ] ; real_T
Integrator_CSTATE ; real_T Integrator_CSTATE_p ; real_T integrator_CSTATE ;
real_T integrator_CSTATE_n ; real_T integrator_CSTATE_c [ 3 ] ; real_T
integrator_CSTATE_i ; XPtMin_TrueRMS_x2026A2022b_T TrueRMS_e ;
XPtMin_RMS_x2026A2022b_T RMS_f ; XPtMin_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
XPtMin_RMS_x2026A2022b_T RMS_e ; XPtMin_TrueRMS_x2026A2022b_p_T TrueRMS_a ;
XPtMin_RMS_x2026A2022b_e_T RMS_d ; XPtMin_TrueRMS_x2026A2022b_p_T TrueRMS_f ;
XPtMin_RMS_x2026A2022b_e_T RMS_l ; XPtMin_TrueRMS_x2026A2022b_T TrueRMS_g ;
XPtMin_RMS_x2026A2022b_T RMS_n ; XPtMin_TrueRMS_x2026A2022b_T TrueRMS ;
XPtMin_RMS_x2026A2022b_T RMS ; } CXPtMin_x2026A2022b_T ; typedef struct {
real_T integ1_CSTATE ; real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T
Integ2_CSTATE_m ; real_T TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE
; real_T Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T
integrator_CSTATE_n ; real_T integrator_CSTATE_c [ 3 ] ; real_T
integrator_CSTATE_i ; XPtMax_TrueRMS_x2026A2022b_T TrueRMS_e ;
XPtMax_RMS_x2026A2022b_T RMS_f ; XPtMax_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
XPtMax_RMS_x2026A2022b_T RMS_e ; XPtMax_TrueRMS_x2026A2022b_g_T TrueRMS_a ;
XPtMax_RMS_x2026A2022b_i_T RMS_d ; XPtMax_TrueRMS_x2026A2022b_g_T TrueRMS_f ;
XPtMax_RMS_x2026A2022b_i_T RMS_l ; XPtMax_TrueRMS_x2026A2022b_T TrueRMS_g ;
XPtMax_RMS_x2026A2022b_T RMS_n ; XPtMax_TrueRMS_x2026A2022b_T TrueRMS ;
XPtMax_RMS_x2026A2022b_T RMS ; } CXPtMax_x2026A2022b_T ; typedef struct {
real_T MinMax_MinmaxInput_ZC ; real_T MinMax2_MinmaxInput_ZC ; real_T
RelationalOperator7_RelopInput_ZC ; real_T RelationalOperator6_RelopInput_ZC
; real_T RelationalOperator2_RelopInput_ZC ; real_T
RelationalOperator1_RelopInput_ZC ; real_T RelationalOperator_RelopInput_ZC ;
real_T RelationalOperator1_RelopInput_ZC_o ; ZCV_TrueRMS_x2026A2022b_T
TrueRMS_e ; ZCV_TrueRMS_x2026A2022b_T TrueRMS_f5 ;
ZCV_TrueRMS_x2026A2022b_n_T TrueRMS_a ; ZCV_TrueRMS_x2026A2022b_n_T TrueRMS_f
; ZCV_TrueRMS_x2026A2022b_T TrueRMS_g ; ZCV_TrueRMS_x2026A2022b_T TrueRMS ; }
ZCV_x2026A2022b_T ; typedef struct { ZCSigState MinMax_MinmaxInput_ZCE ;
ZCSigState MinMax2_MinmaxInput_ZCE ; ZCSigState
RelationalOperator7_RelopInput_ZCE ; ZCSigState
RelationalOperator6_RelopInput_ZCE ; ZCSigState
RelationalOperator2_RelopInput_ZCE ; ZCSigState
RelationalOperator1_RelopInput_ZCE ; ZCSigState
RelationalOperator_RelopInput_ZCE ; ZCSigState
RelationalOperator1_RelopInput_ZCE_f ; ZCE_TrueRMS_x2026A2022b_T TrueRMS_e ;
ZCE_TrueRMS_x2026A2022b_T TrueRMS_f5 ; ZCE_TrueRMS_x2026A2022b_b_T TrueRMS_a
; ZCE_TrueRMS_x2026A2022b_b_T TrueRMS_f ; ZCE_TrueRMS_x2026A2022b_T TrueRMS_g
; ZCE_TrueRMS_x2026A2022b_T TrueRMS ; } PrevZCX_x2026A2022b_T ; struct
P_RMS_x2026A2022b_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ; real_T P_3 ;
real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 ; real_T P_9 ;
real_T P_10 ; real_T P_11 ; real_T P_12 ; real_T P_13 ; real_T P_14 ; real_T
P_15 ; real_T P_16 ; real_T P_17 ; real_T P_18 ; real_T P_19 ; real_T P_20 ;
real_T P_21 ; } ; struct P_TrueRMS_x2026A2022b_T_ { real_T P_0 ; real_T P_1 ;
real_T P_2 ; real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ;
real_T P_8 ; } ; struct P_RMS_x2026A2022b_a_T_ { real_T P_0 ; real_T P_1 ;
real_T P_2 ; real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ;
real_T P_8 ; real_T P_9 ; real_T P_10 ; real_T P_11 ; real_T P_12 ; real_T
P_13 ; real_T P_14 ; real_T P_15 ; real_T P_16 ; real_T P_17 ; real_T P_18 ;
real_T P_19 ; real_T P_20 ; real_T P_21 ; } ; struct
P_TrueRMS_x2026A2022b_d_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ; real_T
P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 ; } ;
struct P_x2026A2022b_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ; real_T P_3 ;
real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 [ 2 ] ; real_T
P_9 [ 144 ] ; real_T P_10 [ 2 ] ; real_T P_11 [ 156 ] ; real_T P_12 [ 2 ] ;
real_T P_13 [ 252 ] ; real_T P_14 [ 2 ] ; real_T P_15 [ 273 ] ; real_T P_16 [
2 ] ; real_T P_17 [ 12 ] ; real_T P_18 ; real_T P_19 ; real_T P_20 ; real_T
P_21 ; real_T P_22 ; real_T P_23 ; real_T P_24 ; real_T P_25 ; real_T P_26 ;
real_T P_27 ; real_T P_28 ; real_T P_29 ; real_T P_30 ; real_T P_31 ; real_T
P_32 ; real_T P_33 ; real_T P_34 ; real_T P_35 ; real_T P_36 ; real_T P_37 ;
real_T P_38 [ 2 ] ; real_T P_39 [ 2 ] ; real_T P_40 ; real_T P_41 ; real_T
P_42 ; real_T P_43 ; real_T P_44 ; real_T P_45 ; real_T P_46 ; real_T P_47 ;
real_T P_48 ; real_T P_49 ; real_T P_50 ; real_T P_51 ; real_T P_52 ; real_T
P_53 ; real_T P_54 ; real_T P_55 ; real_T P_56 ; real_T P_57 ; real_T P_58 ;
real_T P_59 ; real_T P_60 ; real_T P_61 ; real_T P_62 ; real_T P_63 ; real_T
P_64 ; real_T P_65 ; real_T P_66 ; real_T P_67 ; real_T P_68 ; real_T P_69 [
3 ] ; real_T P_70 [ 3 ] ; real_T P_71 [ 3 ] ; real_T P_72 [ 3 ] ; real_T P_73
; real_T P_74 ; real_T P_75 ; real_T P_76 ; real_T P_77 ; real_T P_78 ;
real_T P_79 ; real_T P_80 ; real_T P_81 ; real_T P_82 ; real_T P_83 ; real_T
P_84 ; real_T P_85 ; real_T P_86 ; real_T P_87 ; real_T P_88 ; real_T P_89 ;
real_T P_90 ; real_T P_91 ; real_T P_92 ; real_T P_93 ; real_T P_94 ; real_T
P_95 ; real_T P_96 ; real_T P_97 ; real_T P_98 ; real_T P_99 ; real_T P_100 ;
real_T P_101 ; real_T P_102 ; real_T P_103 ; real_T P_104 ; real_T P_105 ;
real_T P_106 ; real_T P_107 ; real_T P_108 ; real_T P_109 ; real_T P_110 ;
real_T P_111 ; real_T P_112 ; real_T P_113 ; real_T P_114 ; real_T P_115 ;
real_T P_116 ; real_T P_117 ; real_T P_118 ; real_T P_119 ; real_T P_120 [ 6
] ; real_T P_121 [ 2 ] ; real_T P_122 [ 4 ] ; real_T P_123 ; real_T P_124 [ 3
] ; real_T P_125 ; real_T P_126 ; real_T P_127 ; real_T P_128 ; real_T P_129
; real_T P_130 ; real_T P_131 ; real_T P_132 ; real_T P_133 ; real_T P_134 ;
real_T P_135 ; real_T P_136 ; real_T P_137 ; real_T P_138 ; real_T P_139 ;
real_T P_140 ; real_T P_141 ; real_T P_142 ; real_T P_143 ; real_T P_144 ;
real_T P_145 ; real_T P_146 ; real_T P_147 ; real_T P_148 ; real_T P_149 ;
real_T P_150 ; P_TrueRMS_x2026A2022b_T TrueRMS_e ; P_RMS_x2026A2022b_T RMS_f
; P_TrueRMS_x2026A2022b_T TrueRMS_f5 ; P_RMS_x2026A2022b_T RMS_e ;
P_TrueRMS_x2026A2022b_d_T TrueRMS_a ; P_RMS_x2026A2022b_a_T RMS_d ;
P_TrueRMS_x2026A2022b_d_T TrueRMS_f ; P_RMS_x2026A2022b_a_T RMS_l ;
P_TrueRMS_x2026A2022b_T TrueRMS_g ; P_RMS_x2026A2022b_T RMS_n ;
P_TrueRMS_x2026A2022b_T TrueRMS ; P_RMS_x2026A2022b_T RMS ; } ; extern
P_x2026A2022b_T x2026A2022b_rtDefaultP ;
#endif
