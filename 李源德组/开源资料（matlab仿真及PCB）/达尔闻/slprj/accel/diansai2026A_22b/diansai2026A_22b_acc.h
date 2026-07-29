#ifndef RTW_HEADER_diansai2026A_22b_acc_h_
#define RTW_HEADER_diansai2026A_22b_acc_h_
#ifndef diansai2026A_22b_acc_COMMON_INCLUDES_
#define diansai2026A_22b_acc_COMMON_INCLUDES_
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
#include "diansai2026A_22b_acc_types.h"
#include <stddef.h>
#include <float.h>
#include "mwmathutil.h"
#include <string.h>
#include "rt_defines.h"
#include "rt_nonfinite.h"
typedef struct { real_T B_0_0_0 ; real_T B_0_1_0 ; real_T B_0_2_0 ; real_T
B_0_3_0 ; real_T B_0_4_0 ; } B_PR_diansai2026A_22b_T ; typedef struct {
int32_T PR_sysIdxToRun ; int_T Saturation_MODE ; } DW_PR_diansai2026A_22b_T ;
typedef struct { real_T TransferFcn_CSTATE_n [ 2 ] ; }
X_PR_diansai2026A_22b_T ; typedef struct { real_T TransferFcn_CSTATE_n [ 2 ]
; } XDot_PR_diansai2026A_22b_T ; typedef struct { boolean_T
TransferFcn_CSTATE_n [ 2 ] ; } XDis_PR_diansai2026A_22b_T ; typedef struct {
real_T TransferFcn_CSTATE_n [ 2 ] ; } XAbsTol_PR_diansai2026A_22b_T ; typedef
struct { real_T TransferFcn_CSTATE_n [ 2 ] ; } XPtMin_PR_diansai2026A_22b_T ;
typedef struct { real_T TransferFcn_CSTATE_n [ 2 ] ; }
XPtMax_PR_diansai2026A_22b_T ; typedef struct { real_T Saturation_UprLim_ZC_h
; real_T Saturation_LwrLim_ZC_a ; } ZCV_PR_diansai2026A_22b_T ; typedef
struct { ZCSigState Saturation_UprLim_ZCE_p ; ZCSigState
Saturation_LwrLim_ZCE_n ; } ZCE_PR_diansai2026A_22b_T ; typedef struct {
real_T B_2_0_0 ; real_T B_2_1_0 ; real_T B_2_2_0 ; real_T B_2_3_0 ; real_T
B_2_4_0 ; } B_PR_diansai2026A_22b_a_T ; typedef struct { int32_T
PR_sysIdxToRun ; int_T Saturation_MODE ; } DW_PR_diansai2026A_22b_f_T ;
typedef struct { real_T TransferFcn_CSTATE_e [ 2 ] ; }
X_PR_diansai2026A_22b_c_T ; typedef struct { real_T TransferFcn_CSTATE_e [ 2
] ; } XDot_PR_diansai2026A_22b_a_T ; typedef struct { boolean_T
TransferFcn_CSTATE_e [ 2 ] ; } XDis_PR_diansai2026A_22b_j_T ; typedef struct
{ real_T TransferFcn_CSTATE_e [ 2 ] ; } XAbsTol_PR_diansai2026A_22b_b_T ;
typedef struct { real_T TransferFcn_CSTATE_e [ 2 ] ; }
XPtMin_PR_diansai2026A_22b_p_T ; typedef struct { real_T TransferFcn_CSTATE_e
[ 2 ] ; } XPtMax_PR_diansai2026A_22b_b_T ; typedef struct { real_T
Saturation_UprLim_ZC ; real_T Saturation_LwrLim_ZC ; }
ZCV_PR_diansai2026A_22b_o_T ; typedef struct { ZCSigState
Saturation_UprLim_ZCE ; ZCSigState Saturation_LwrLim_ZCE ; }
ZCE_PR_diansai2026A_22b_l_T ; typedef struct { creal_T B_10_16_0 [ 3 ] ;
real_T B_10_0_0 [ 3 ] ; real_T B_10_1_0 [ 3 ] ; real_T B_10_3_0 ; real_T
B_10_5_0 [ 3 ] ; real_T B_10_7_0 [ 3 ] ; real_T B_10_8_0 [ 3 ] ; real_T
B_10_9_0 [ 3 ] ; real_T B_10_11_0 ; real_T B_10_13_0 [ 3 ] ; real_T B_10_15_0
[ 3 ] ; real_T B_10_19_0 [ 3 ] ; real_T B_10_21_0 [ 3 ] ; real_T B_10_22_0 [
3 ] ; real_T B_9_0_0 [ 3 ] ; real_T B_9_1_0 [ 3 ] ; real_T B_8_0_0 [ 3 ] ;
real_T B_8_1_0 [ 3 ] ; } B_RMS_diansai2026A_22b_T ; typedef struct { real_T
Memory_PreviousInput [ 3 ] ; real_T Memory_PreviousInput_o [ 3 ] ; struct {
real_T modelTStart ; } TransportDelay_RWORK ; struct { real_T modelTStart ; }
TransportDelay_RWORK_m ; struct { void * TUbufferPtrs [ 6 ] ; }
TransportDelay_PWORK ; struct { void * TUbufferPtrs [ 6 ] ; }
TransportDelay_PWORK_j ; int32_T RMS_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_h ; struct { int_T Tail [ 3 ] ;
int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK ; struct { int_T Tail [ 3 ] ; int_T
Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK_c ; int8_T RMS_SubsysRanBC ; boolean_T
RMS_MODE ; char_T pad_RMS_MODE [ 2 ] ; } DW_RMS_diansai2026A_22b_T ; typedef
struct { real_T integrator_CSTATE_a [ 3 ] ; real_T integrator_CSTATE_h [ 3 ]
; } X_RMS_diansai2026A_22b_T ; typedef struct { real_T integrator_CSTATE_a [
3 ] ; real_T integrator_CSTATE_h [ 3 ] ; } XDot_RMS_diansai2026A_22b_T ;
typedef struct { boolean_T integrator_CSTATE_a [ 3 ] ; boolean_T
integrator_CSTATE_h [ 3 ] ; } XDis_RMS_diansai2026A_22b_T ; typedef struct {
real_T integrator_CSTATE_a [ 3 ] ; real_T integrator_CSTATE_h [ 3 ] ; }
XAbsTol_RMS_diansai2026A_22b_T ; typedef struct { real_T integrator_CSTATE_a
[ 3 ] ; real_T integrator_CSTATE_h [ 3 ] ; } XPtMin_RMS_diansai2026A_22b_T ;
typedef struct { real_T integrator_CSTATE_a [ 3 ] ; real_T
integrator_CSTATE_h [ 3 ] ; } XPtMax_RMS_diansai2026A_22b_T ; typedef struct
{ real_T B_12_1_0 [ 3 ] ; real_T B_12_2_0 [ 3 ] ; real_T B_12_3_0 ; real_T
B_12_4_0 [ 3 ] ; real_T B_12_7_0 [ 3 ] ; real_T B_12_8_0 [ 3 ] ; real_T
B_12_9_0 [ 3 ] ; real_T B_12_10_0 [ 3 ] ; real_T B_11_0_0 [ 3 ] ; real_T
B_11_1_0 [ 3 ] ; } B_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T
Memory_PreviousInput [ 3 ] ; struct { real_T modelTStart ; }
TransportDelay_RWORK ; struct { void * TUbufferPtrs [ 6 ] ; }
TransportDelay_PWORK ; int32_T TrueRMS_sysIdxToRun ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; struct { int_T Tail [ 3 ] ;
int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T CircularBufSize [ 3 ] ; int_T
MaxNewBufSize ; } TransportDelay_IWORK ; int_T
Saturationtoavoidnegativesqrt_MODE [ 3 ] ; int8_T TrueRMS_SubsysRanBC ;
int8_T Sqrt_DWORK1 ; boolean_T TrueRMS_MODE ; char_T pad_TrueRMS_MODE [ 5 ] ;
} DW_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T integrator_CSTATE_o
[ 3 ] ; } X_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T
integrator_CSTATE_o [ 3 ] ; } XDot_TrueRMS_diansai2026A_22b_T ; typedef
struct { boolean_T integrator_CSTATE_o [ 3 ] ; }
XDis_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T integrator_CSTATE_o
[ 3 ] ; } XAbsTol_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T
integrator_CSTATE_o [ 3 ] ; } XPtMin_TrueRMS_diansai2026A_22b_T ; typedef
struct { real_T integrator_CSTATE_o [ 3 ] ; }
XPtMax_TrueRMS_diansai2026A_22b_T ; typedef struct { real_T
Saturationtoavoidnegativesqrt_UprLim_ZC_p [ 3 ] ; real_T
Saturationtoavoidnegativesqrt_LwrLim_ZC_b [ 3 ] ; }
ZCV_TrueRMS_diansai2026A_22b_T ; typedef struct { ZCSigState
Saturationtoavoidnegativesqrt_UprLim_ZCE_i [ 3 ] ; ZCSigState
Saturationtoavoidnegativesqrt_LwrLim_ZCE_e [ 3 ] ; }
ZCE_TrueRMS_diansai2026A_22b_T ; typedef struct { creal_T B_32_16_0 ; real_T
B_32_0_0 ; real_T B_32_1_0 ; real_T B_32_3_0 ; real_T B_32_5_0 ; real_T
B_32_7_0 ; real_T B_32_8_0 ; real_T B_32_9_0 ; real_T B_32_11_0 ; real_T
B_32_13_0 ; real_T B_32_15_0 ; real_T B_32_19_0 ; real_T B_32_21_0 ; real_T
B_32_22_0 ; real_T B_31_0_0 ; real_T B_31_1_0 ; real_T B_30_0_0 ; real_T
B_30_1_0 ; } B_RMS_diansai2026A_22b_a_T ; typedef struct { real_T
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
DW_RMS_diansai2026A_22b_e_T ; typedef struct { real_T integrator_CSTATE_l ;
real_T integrator_CSTATE_cu ; } X_RMS_diansai2026A_22b_a_T ; typedef struct {
real_T integrator_CSTATE_l ; real_T integrator_CSTATE_cu ; }
XDot_RMS_diansai2026A_22b_e_T ; typedef struct { boolean_T
integrator_CSTATE_l ; boolean_T integrator_CSTATE_cu ; }
XDis_RMS_diansai2026A_22b_j_T ; typedef struct { real_T integrator_CSTATE_l ;
real_T integrator_CSTATE_cu ; } XAbsTol_RMS_diansai2026A_22b_d_T ; typedef
struct { real_T integrator_CSTATE_l ; real_T integrator_CSTATE_cu ; }
XPtMin_RMS_diansai2026A_22b_e_T ; typedef struct { real_T integrator_CSTATE_l
; real_T integrator_CSTATE_cu ; } XPtMax_RMS_diansai2026A_22b_i_T ; typedef
struct { real_T B_34_1_0 ; real_T B_34_2_0 ; real_T B_34_3_0 ; real_T
B_34_4_0 ; real_T B_34_7_0 ; real_T B_34_8_0 ; real_T B_34_9_0 ; real_T
B_34_10_0 ; real_T B_33_0_0 ; real_T B_33_1_0 ; }
B_TrueRMS_diansai2026A_22b_d_T ; typedef struct { real_T Memory_PreviousInput
; struct { real_T modelTStart ; } TransportDelay_RWORK ; struct { void *
TUbufferPtrs [ 2 ] ; } TransportDelay_PWORK ; int32_T TrueRMS_sysIdxToRun ;
int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun ; struct { int_T Tail ;
int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
TransportDelay_IWORK ; int_T Saturationtoavoidnegativesqrt_MODE ; int8_T
TrueRMS_SubsysRanBC ; int8_T Sqrt_DWORK1 ; boolean_T TrueRMS_MODE ; char_T
pad_TrueRMS_MODE [ 5 ] ; } DW_TrueRMS_diansai2026A_22b_c_T ; typedef struct {
real_T integrator_CSTATE_ic ; } X_TrueRMS_diansai2026A_22b_h_T ; typedef
struct { real_T integrator_CSTATE_ic ; } XDot_TrueRMS_diansai2026A_22b_m_T ;
typedef struct { boolean_T integrator_CSTATE_ic ; }
XDis_TrueRMS_diansai2026A_22b_e_T ; typedef struct { real_T
integrator_CSTATE_ic ; } XAbsTol_TrueRMS_diansai2026A_22b_o_T ; typedef
struct { real_T integrator_CSTATE_ic ; } XPtMin_TrueRMS_diansai2026A_22b_p_T
; typedef struct { real_T integrator_CSTATE_ic ; }
XPtMax_TrueRMS_diansai2026A_22b_g_T ; typedef struct { real_T
Saturationtoavoidnegativesqrt_UprLim_ZC ; real_T
Saturationtoavoidnegativesqrt_LwrLim_ZC ; } ZCV_TrueRMS_diansai2026A_22b_n_T
; typedef struct { ZCSigState Saturationtoavoidnegativesqrt_UprLim_ZCE ;
ZCSigState Saturationtoavoidnegativesqrt_LwrLim_ZCE ; }
ZCE_TrueRMS_diansai2026A_22b_b_T ; typedef struct { real_T B_40_0_0 ; real_T
B_40_1_0 [ 17 ] ; real_T B_40_1_1 [ 10 ] ; real_T B_40_2_0 ; real_T B_40_3_0
; real_T B_40_4_0 ; real_T B_40_11_0 ; real_T B_40_15_0 ; real_T B_40_21_0 ;
real_T B_40_25_0 ; real_T B_40_42_0 [ 2 ] ; real_T B_40_45_0 ; real_T
B_40_46_0 ; real_T B_40_47_0 ; real_T B_40_49_0 ; real_T B_40_50_0 ; real_T
B_40_51_0 ; real_T B_40_52_0 ; real_T B_40_54_0 ; real_T B_40_55_0 ; real_T
B_40_57_0 ; real_T B_40_60_0 ; real_T B_40_62_0 ; real_T B_40_64_0 [ 2 ] ;
real_T B_40_71_0 [ 2 ] ; real_T B_40_72_0 [ 2 ] ; real_T B_40_75_0 [ 2 ] ;
real_T B_40_76_0 [ 2 ] ; real_T B_40_85_0 ; real_T B_40_87_0 ; real_T
B_40_90_0 ; real_T B_40_93_0 ; real_T B_40_96_0 [ 4 ] ; real_T B_40_103_0 ;
real_T B_40_104_0 [ 2 ] ; real_T B_40_105_0 ; real_T B_40_111_0 [ 3 ] ;
real_T B_40_113_0 ; real_T B_40_116_0 ; real_T B_40_123_0 [ 6 ] ; real_T
B_40_130_0 ; real_T B_40_137_0 ; real_T B_40_138_0 ; real_T B_40_140_0 ;
real_T B_40_142_0 ; real_T B_40_144_0 ; real_T B_40_149_0 ; real_T B_40_152_0
; real_T B_40_154_0 ; real_T B_40_156_0 ; real_T B_40_166_0 ; real_T
B_40_174_0 [ 3 ] ; real_T B_40_177_0 ; real_T B_40_179_0 [ 3 ] ; real_T
B_40_181_0 [ 3 ] ; real_T B_40_195_0 ; real_T B_40_198_0 ; real_T B_40_200_0
; real_T B_40_202_0 ; real_T B_40_204_0 ; real_T B_40_205_0 ; real_T
B_40_212_0 ; real_T B_40_213_0 ; real_T B_40_215_0 ; real_T B_40_217_0 ;
real_T B_40_218_0 ; real_T B_40_220_0 ; real_T B_40_222_0 ; real_T B_40_226_0
; real_T B_40_227_0 [ 3 ] ; real_T B_40_0_0_m [ 4 ] ; real_T B_40_1_0_c [ 6 ]
; real_T B_40_4_0_k ; real_T B_40_5_0 ; real_T B_40_6_0 ; real_T B_40_7_0 ;
real_T B_40_8_0 ; real_T B_40_10_0 ; real_T B_40_11_0_c ; real_T B_40_12_0 ;
real_T B_40_13_0 ; real_T B_40_14_0 ; real_T B_40_33_0 ; real_T B_40_34_0 ;
real_T B_6_0_1 ; real_T B_5_0_0 ; real_T B_5_1_0 ; real_T B_4_0_1 ; real_T
B_40_7_0_b [ 3 ] ; real_T B_40_110_0 [ 3 ] ; real_T B_40_133_0 [ 3 ] ; real_T
B_40_191_0 [ 3 ] ; boolean_T B_40_117_0 ; boolean_T B_40_16_0 ; boolean_T
B_40_17_0 ; boolean_T B_40_19_0 ; boolean_T B_40_20_0 ; boolean_T B_40_22_0 ;
boolean_T B_40_23_0 ; boolean_T B_40_25_0_p ; boolean_T B_40_26_0 ; boolean_T
B_40_28_0 ; boolean_T B_40_29_0 ; boolean_T B_40_31_0 ; boolean_T B_40_32_0 ;
char_T pad_B_40_32_0 [ 3 ] ; B_TrueRMS_diansai2026A_22b_d_T TrueRMS_a ;
B_RMS_diansai2026A_22b_a_T RMS_d ; B_TrueRMS_diansai2026A_22b_d_T TrueRMS_fw
; B_RMS_diansai2026A_22b_a_T RMS_l ; B_TrueRMS_diansai2026A_22b_T TrueRMS_e ;
B_RMS_diansai2026A_22b_T RMS_f ; B_TrueRMS_diansai2026A_22b_T TrueRMS_f ;
B_RMS_diansai2026A_22b_T RMS_e ; B_TrueRMS_diansai2026A_22b_T TrueRMS_g ;
B_RMS_diansai2026A_22b_T RMS_n ; B_TrueRMS_diansai2026A_22b_T TrueRMS ;
B_RMS_diansai2026A_22b_T RMS ; B_PR_diansai2026A_22b_a_T PR1_o ;
B_PR_diansai2026A_22b_a_T PR_j ; B_PR_diansai2026A_22b_T PR1 ;
B_PR_diansai2026A_22b_T PR ; } B_diansai2026A_22b_T ; typedef struct { real_T
StateSpace_DSTATE [ 10 ] ; real_T Memory_PreviousInput ; real_T
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
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_a ; int32_T
TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_m ; int32_T PF_sysIdxToRun ;
int32_T TmpAtomicSubsysAtSwitchInport1_sysIdxToRun_o ; int32_T
DiscretePR_sysIdxToRun ; int_T StateSpace_IWORK [ 11 ] ; struct { int_T Tail
; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
T_IWORK ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } T1_IWORK ; struct { int_T Tail ;
int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
T_IWORK_d ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } T1_IWORK_g ; struct { int_T Tail ;
int_T Head ; int_T Last ; int_T CircularBufSize ; int_T MaxNewBufSize ; }
TransportDelay_IWORK ; struct { int_T Tail ; int_T Head ; int_T Last ; int_T
CircularBufSize ; int_T MaxNewBufSize ; } TransportDelay_IWORK_i ; struct {
int_T Tail [ 3 ] ; int_T Head [ 3 ] ; int_T Last [ 3 ] ; int_T
CircularBufSize [ 3 ] ; int_T MaxNewBufSize ; } TransportDelay_IWORK_k ;
struct { int_T Tail ; int_T Head ; int_T Last ; int_T CircularBufSize ; int_T
MaxNewBufSize ; } TransportDelay_IWORK_ib ; int_T MinMax_MODE ; int_T
MinMax2_MODE ; int8_T Sqrt_DWORK1 ; int8_T Sqrt_DWORK1_i ; int8_T
Sqrt_DWORK1_e ; int8_T Sqrt_DWORK1_n ; boolean_T RelationalOperator_Mode ;
boolean_T RelationalOperator1_Mode ; boolean_T RelationalOperator7_Mode ;
boolean_T RelationalOperator6_Mode ; boolean_T RelationalOperator2_Mode ;
char_T pad_RelationalOperator2_Mode [ 3 ] ; DW_TrueRMS_diansai2026A_22b_c_T
TrueRMS_a ; DW_RMS_diansai2026A_22b_e_T RMS_d ;
DW_TrueRMS_diansai2026A_22b_c_T TrueRMS_fw ; DW_RMS_diansai2026A_22b_e_T
RMS_l ; DW_TrueRMS_diansai2026A_22b_T TrueRMS_e ; DW_RMS_diansai2026A_22b_T
RMS_f ; DW_TrueRMS_diansai2026A_22b_T TrueRMS_f ; DW_RMS_diansai2026A_22b_T
RMS_e ; DW_TrueRMS_diansai2026A_22b_T TrueRMS_g ; DW_RMS_diansai2026A_22b_T
RMS_n ; DW_TrueRMS_diansai2026A_22b_T TrueRMS ; DW_RMS_diansai2026A_22b_T RMS
; DW_PR_diansai2026A_22b_f_T PR1_o ; DW_PR_diansai2026A_22b_f_T PR_j ;
DW_PR_diansai2026A_22b_T PR1 ; DW_PR_diansai2026A_22b_T PR ; }
DW_diansai2026A_22b_T ; typedef struct { real_T integ1_CSTATE ; real_T
Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ; real_T
TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
X_TrueRMS_diansai2026A_22b_h_T TrueRMS_a ; X_RMS_diansai2026A_22b_a_T RMS_d ;
X_TrueRMS_diansai2026A_22b_h_T TrueRMS_fw ; X_RMS_diansai2026A_22b_a_T RMS_l
; X_TrueRMS_diansai2026A_22b_T TrueRMS_e ; X_RMS_diansai2026A_22b_T RMS_f ;
X_TrueRMS_diansai2026A_22b_T TrueRMS_f ; X_RMS_diansai2026A_22b_T RMS_e ;
X_TrueRMS_diansai2026A_22b_T TrueRMS_g ; X_RMS_diansai2026A_22b_T RMS_n ;
X_TrueRMS_diansai2026A_22b_T TrueRMS ; X_RMS_diansai2026A_22b_T RMS ;
X_PR_diansai2026A_22b_c_T PR1_o ; X_PR_diansai2026A_22b_c_T PR_j ;
X_PR_diansai2026A_22b_T PR1 ; X_PR_diansai2026A_22b_T PR ; }
X_diansai2026A_22b_T ; typedef int_T PeriodicIndX_diansai2026A_22b_T [ 1 ] ;
typedef real_T PeriodicRngX_diansai2026A_22b_T [ 2 ] ; typedef struct {
real_T integ1_CSTATE ; real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T
Integ2_CSTATE_m ; real_T TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE
; real_T Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T
integrator_CSTATE_n ; real_T integrator_CSTATE_c [ 3 ] ; real_T
integrator_CSTATE_i ; XDot_TrueRMS_diansai2026A_22b_m_T TrueRMS_a ;
XDot_RMS_diansai2026A_22b_e_T RMS_d ; XDot_TrueRMS_diansai2026A_22b_m_T
TrueRMS_fw ; XDot_RMS_diansai2026A_22b_e_T RMS_l ;
XDot_TrueRMS_diansai2026A_22b_T TrueRMS_e ; XDot_RMS_diansai2026A_22b_T RMS_f
; XDot_TrueRMS_diansai2026A_22b_T TrueRMS_f ; XDot_RMS_diansai2026A_22b_T
RMS_e ; XDot_TrueRMS_diansai2026A_22b_T TrueRMS_g ;
XDot_RMS_diansai2026A_22b_T RMS_n ; XDot_TrueRMS_diansai2026A_22b_T TrueRMS ;
XDot_RMS_diansai2026A_22b_T RMS ; XDot_PR_diansai2026A_22b_a_T PR1_o ;
XDot_PR_diansai2026A_22b_a_T PR_j ; XDot_PR_diansai2026A_22b_T PR1 ;
XDot_PR_diansai2026A_22b_T PR ; } XDot_diansai2026A_22b_T ; typedef struct {
boolean_T integ1_CSTATE ; boolean_T Integ2_CSTATE ; boolean_T integ1_CSTATE_h
; boolean_T Integ2_CSTATE_m ; boolean_T TransferFcn_CSTATE [ 2 ] ; boolean_T
Integrator_CSTATE ; boolean_T Integrator_CSTATE_p ; boolean_T
integrator_CSTATE ; boolean_T integrator_CSTATE_n ; boolean_T
integrator_CSTATE_c [ 3 ] ; boolean_T integrator_CSTATE_i ;
XDis_TrueRMS_diansai2026A_22b_e_T TrueRMS_a ; XDis_RMS_diansai2026A_22b_j_T
RMS_d ; XDis_TrueRMS_diansai2026A_22b_e_T TrueRMS_fw ;
XDis_RMS_diansai2026A_22b_j_T RMS_l ; XDis_TrueRMS_diansai2026A_22b_T
TrueRMS_e ; XDis_RMS_diansai2026A_22b_T RMS_f ;
XDis_TrueRMS_diansai2026A_22b_T TrueRMS_f ; XDis_RMS_diansai2026A_22b_T RMS_e
; XDis_TrueRMS_diansai2026A_22b_T TrueRMS_g ; XDis_RMS_diansai2026A_22b_T
RMS_n ; XDis_TrueRMS_diansai2026A_22b_T TrueRMS ; XDis_RMS_diansai2026A_22b_T
RMS ; XDis_PR_diansai2026A_22b_j_T PR1_o ; XDis_PR_diansai2026A_22b_j_T PR_j
; XDis_PR_diansai2026A_22b_T PR1 ; XDis_PR_diansai2026A_22b_T PR ; }
XDis_diansai2026A_22b_T ; typedef struct { real_T integ1_CSTATE ; real_T
Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ; real_T
TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
XAbsTol_TrueRMS_diansai2026A_22b_o_T TrueRMS_a ;
XAbsTol_RMS_diansai2026A_22b_d_T RMS_d ; XAbsTol_TrueRMS_diansai2026A_22b_o_T
TrueRMS_fw ; XAbsTol_RMS_diansai2026A_22b_d_T RMS_l ;
XAbsTol_TrueRMS_diansai2026A_22b_T TrueRMS_e ; XAbsTol_RMS_diansai2026A_22b_T
RMS_f ; XAbsTol_TrueRMS_diansai2026A_22b_T TrueRMS_f ;
XAbsTol_RMS_diansai2026A_22b_T RMS_e ; XAbsTol_TrueRMS_diansai2026A_22b_T
TrueRMS_g ; XAbsTol_RMS_diansai2026A_22b_T RMS_n ;
XAbsTol_TrueRMS_diansai2026A_22b_T TrueRMS ; XAbsTol_RMS_diansai2026A_22b_T
RMS ; XAbsTol_PR_diansai2026A_22b_b_T PR1_o ; XAbsTol_PR_diansai2026A_22b_b_T
PR_j ; XAbsTol_PR_diansai2026A_22b_T PR1 ; XAbsTol_PR_diansai2026A_22b_T PR ;
} CStateAbsTol_diansai2026A_22b_T ; typedef struct { real_T integ1_CSTATE ;
real_T Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ;
real_T TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
XPtMin_TrueRMS_diansai2026A_22b_p_T TrueRMS_a ;
XPtMin_RMS_diansai2026A_22b_e_T RMS_d ; XPtMin_TrueRMS_diansai2026A_22b_p_T
TrueRMS_fw ; XPtMin_RMS_diansai2026A_22b_e_T RMS_l ;
XPtMin_TrueRMS_diansai2026A_22b_T TrueRMS_e ; XPtMin_RMS_diansai2026A_22b_T
RMS_f ; XPtMin_TrueRMS_diansai2026A_22b_T TrueRMS_f ;
XPtMin_RMS_diansai2026A_22b_T RMS_e ; XPtMin_TrueRMS_diansai2026A_22b_T
TrueRMS_g ; XPtMin_RMS_diansai2026A_22b_T RMS_n ;
XPtMin_TrueRMS_diansai2026A_22b_T TrueRMS ; XPtMin_RMS_diansai2026A_22b_T RMS
; XPtMin_PR_diansai2026A_22b_p_T PR1_o ; XPtMin_PR_diansai2026A_22b_p_T PR_j
; XPtMin_PR_diansai2026A_22b_T PR1 ; XPtMin_PR_diansai2026A_22b_T PR ; }
CXPtMin_diansai2026A_22b_T ; typedef struct { real_T integ1_CSTATE ; real_T
Integ2_CSTATE ; real_T integ1_CSTATE_h ; real_T Integ2_CSTATE_m ; real_T
TransferFcn_CSTATE [ 2 ] ; real_T Integrator_CSTATE ; real_T
Integrator_CSTATE_p ; real_T integrator_CSTATE ; real_T integrator_CSTATE_n ;
real_T integrator_CSTATE_c [ 3 ] ; real_T integrator_CSTATE_i ;
XPtMax_TrueRMS_diansai2026A_22b_g_T TrueRMS_a ;
XPtMax_RMS_diansai2026A_22b_i_T RMS_d ; XPtMax_TrueRMS_diansai2026A_22b_g_T
TrueRMS_fw ; XPtMax_RMS_diansai2026A_22b_i_T RMS_l ;
XPtMax_TrueRMS_diansai2026A_22b_T TrueRMS_e ; XPtMax_RMS_diansai2026A_22b_T
RMS_f ; XPtMax_TrueRMS_diansai2026A_22b_T TrueRMS_f ;
XPtMax_RMS_diansai2026A_22b_T RMS_e ; XPtMax_TrueRMS_diansai2026A_22b_T
TrueRMS_g ; XPtMax_RMS_diansai2026A_22b_T RMS_n ;
XPtMax_TrueRMS_diansai2026A_22b_T TrueRMS ; XPtMax_RMS_diansai2026A_22b_T RMS
; XPtMax_PR_diansai2026A_22b_b_T PR1_o ; XPtMax_PR_diansai2026A_22b_b_T PR_j
; XPtMax_PR_diansai2026A_22b_T PR1 ; XPtMax_PR_diansai2026A_22b_T PR ; }
CXPtMax_diansai2026A_22b_T ; typedef struct { real_T
RelationalOperator_RelopInput_ZC ; real_T RelationalOperator1_RelopInput_ZC ;
real_T MinMax_MinmaxInput_ZC ; real_T MinMax2_MinmaxInput_ZC ; real_T
RelationalOperator7_RelopInput_ZC ; real_T RelationalOperator6_RelopInput_ZC
; real_T RelationalOperator2_RelopInput_ZC ; ZCV_TrueRMS_diansai2026A_22b_n_T
TrueRMS_a ; ZCV_TrueRMS_diansai2026A_22b_n_T TrueRMS_fw ;
ZCV_TrueRMS_diansai2026A_22b_T TrueRMS_e ; ZCV_TrueRMS_diansai2026A_22b_T
TrueRMS_f ; ZCV_TrueRMS_diansai2026A_22b_T TrueRMS_g ;
ZCV_TrueRMS_diansai2026A_22b_T TrueRMS ; ZCV_PR_diansai2026A_22b_o_T PR1_o ;
ZCV_PR_diansai2026A_22b_o_T PR_j ; ZCV_PR_diansai2026A_22b_T PR1 ;
ZCV_PR_diansai2026A_22b_T PR ; } ZCV_diansai2026A_22b_T ; typedef struct {
ZCSigState RelationalOperator_RelopInput_ZCE ; ZCSigState
RelationalOperator1_RelopInput_ZCE ; ZCSigState MinMax_MinmaxInput_ZCE ;
ZCSigState MinMax2_MinmaxInput_ZCE ; ZCSigState
RelationalOperator7_RelopInput_ZCE ; ZCSigState
RelationalOperator6_RelopInput_ZCE ; ZCSigState
RelationalOperator2_RelopInput_ZCE ; ZCE_TrueRMS_diansai2026A_22b_b_T
TrueRMS_a ; ZCE_TrueRMS_diansai2026A_22b_b_T TrueRMS_fw ;
ZCE_TrueRMS_diansai2026A_22b_T TrueRMS_e ; ZCE_TrueRMS_diansai2026A_22b_T
TrueRMS_f ; ZCE_TrueRMS_diansai2026A_22b_T TrueRMS_g ;
ZCE_TrueRMS_diansai2026A_22b_T TrueRMS ; ZCE_PR_diansai2026A_22b_l_T PR1_o ;
ZCE_PR_diansai2026A_22b_l_T PR_j ; ZCE_PR_diansai2026A_22b_T PR1 ;
ZCE_PR_diansai2026A_22b_T PR ; } PrevZCX_diansai2026A_22b_T ; struct
P_PR_diansai2026A_22b_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 [ 2 ] ;
real_T P_3 [ 2 ] ; real_T P_4 ; real_T P_5 ; } ; struct
P_PR_diansai2026A_22b_a_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 [ 2 ] ;
real_T P_3 [ 2 ] ; real_T P_4 ; real_T P_5 ; } ; struct
P_RMS_diansai2026A_22b_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ; real_T P_3
; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 ; real_T P_9
; real_T P_10 ; real_T P_11 ; real_T P_12 ; real_T P_13 ; real_T P_14 ;
real_T P_15 ; real_T P_16 ; real_T P_17 ; real_T P_18 ; real_T P_19 ; real_T
P_20 ; real_T P_21 ; } ; struct P_TrueRMS_diansai2026A_22b_T_ { real_T P_0 ;
real_T P_1 ; real_T P_2 ; real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ;
real_T P_7 ; real_T P_8 ; } ; struct P_RMS_diansai2026A_22b_a_T_ { real_T P_0
; real_T P_1 ; real_T P_2 ; real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6
; real_T P_7 ; real_T P_8 ; real_T P_9 ; real_T P_10 ; real_T P_11 ; real_T
P_12 ; real_T P_13 ; real_T P_14 ; real_T P_15 ; real_T P_16 ; real_T P_17 ;
real_T P_18 ; real_T P_19 ; real_T P_20 ; real_T P_21 ; } ; struct
P_TrueRMS_diansai2026A_22b_d_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ;
real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 ;
} ; struct P_diansai2026A_22b_T_ { real_T P_0 ; real_T P_1 ; real_T P_2 ;
real_T P_3 ; real_T P_4 ; real_T P_5 ; real_T P_6 ; real_T P_7 ; real_T P_8 [
2 ] ; real_T P_9 [ 100 ] ; real_T P_10 [ 2 ] ; real_T P_11 [ 110 ] ; real_T
P_12 [ 2 ] ; real_T P_13 [ 170 ] ; real_T P_14 [ 2 ] ; real_T P_15 [ 187 ] ;
real_T P_16 [ 2 ] ; real_T P_17 [ 10 ] ; real_T P_18 ; real_T P_19 ; real_T
P_20 ; real_T P_21 ; real_T P_22 ; real_T P_23 ; real_T P_24 ; real_T P_25 ;
real_T P_26 ; real_T P_27 ; real_T P_28 ; real_T P_29 ; real_T P_30 ; real_T
P_31 ; real_T P_32 ; real_T P_33 ; real_T P_34 ; real_T P_35 ; real_T P_36 ;
real_T P_37 [ 2 ] ; real_T P_38 [ 2 ] ; real_T P_39 ; real_T P_40 ; real_T
P_41 ; real_T P_42 ; real_T P_43 ; real_T P_44 ; real_T P_45 ; real_T P_46 ;
real_T P_47 ; real_T P_48 ; real_T P_49 ; real_T P_50 ; real_T P_51 ; real_T
P_52 ; real_T P_53 ; real_T P_54 ; real_T P_55 [ 3 ] ; real_T P_56 [ 3 ] ;
real_T P_57 ; real_T P_58 ; real_T P_59 [ 3 ] ; real_T P_60 [ 3 ] ; real_T
P_61 ; real_T P_62 ; real_T P_63 ; real_T P_64 ; real_T P_65 ; real_T P_66 ;
real_T P_67 ; real_T P_68 ; real_T P_69 ; real_T P_70 ; real_T P_71 ; real_T
P_72 ; real_T P_73 ; real_T P_74 ; real_T P_75 ; real_T P_76 ; real_T P_77 ;
real_T P_78 ; real_T P_79 ; real_T P_80 ; real_T P_81 ; real_T P_82 ; real_T
P_83 ; real_T P_84 ; real_T P_85 ; real_T P_86 ; real_T P_87 ; real_T P_88 ;
real_T P_89 ; real_T P_90 ; real_T P_91 ; real_T P_92 ; real_T P_93 ; real_T
P_94 ; real_T P_95 ; real_T P_96 ; real_T P_97 ; real_T P_98 ; real_T P_99 ;
real_T P_100 ; real_T P_101 ; real_T P_102 ; real_T P_103 ; real_T P_104 ;
real_T P_105 ; real_T P_106 ; real_T P_107 ; real_T P_108 [ 4 ] ; real_T
P_109 [ 6 ] ; real_T P_110 ; real_T P_111 ; real_T P_112 ; real_T P_113 ;
real_T P_114 ; real_T P_115 ; real_T P_116 ; real_T P_117 ; real_T P_118 ;
real_T P_119 ; real_T P_120 ; real_T P_121 ; real_T P_122 ; real_T P_123 ;
real_T P_124 ; real_T P_125 ; real_T P_126 ; real_T P_127 ; real_T P_128 ;
real_T P_129 ; P_TrueRMS_diansai2026A_22b_d_T TrueRMS_a ;
P_RMS_diansai2026A_22b_a_T RMS_d ; P_TrueRMS_diansai2026A_22b_d_T TrueRMS_fw
; P_RMS_diansai2026A_22b_a_T RMS_l ; P_TrueRMS_diansai2026A_22b_T TrueRMS_e ;
P_RMS_diansai2026A_22b_T RMS_f ; P_TrueRMS_diansai2026A_22b_T TrueRMS_f ;
P_RMS_diansai2026A_22b_T RMS_e ; P_TrueRMS_diansai2026A_22b_T TrueRMS_g ;
P_RMS_diansai2026A_22b_T RMS_n ; P_TrueRMS_diansai2026A_22b_T TrueRMS ;
P_RMS_diansai2026A_22b_T RMS ; P_PR_diansai2026A_22b_a_T PR1_o ;
P_PR_diansai2026A_22b_a_T PR_j ; P_PR_diansai2026A_22b_T PR1 ;
P_PR_diansai2026A_22b_T PR ; } ; extern P_diansai2026A_22b_T
diansai2026A_22b_rtDefaultP ;
#endif
