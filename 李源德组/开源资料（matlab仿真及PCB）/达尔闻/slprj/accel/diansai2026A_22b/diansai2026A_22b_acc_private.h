#ifndef RTW_HEADER_diansai2026A_22b_acc_private_h_
#define RTW_HEADER_diansai2026A_22b_acc_private_h_
#include "rtwtypes.h"
#include "multiword_types.h"
#include "diansai2026A_22b_acc.h"
#include "diansai2026A_22b_acc_types.h"
#if !defined(ss_VALIDATE_MEMORY)
#define ss_VALIDATE_MEMORY(S, ptr)     if(!(ptr)) {\
    ssSetErrorStatus(S, RT_MEMORY_ALLOCATION_ERROR);\
    }
#endif
#if !defined(rt_FREE)
#if !defined(_WIN32)
#define rt_FREE(ptr)     if((ptr) != (NULL)) {\
    free((ptr));\
    (ptr) = (NULL);\
    }
#else
#define rt_FREE(ptr)     if((ptr) != (NULL)) {\
    free((void *)(ptr));\
    (ptr) = (NULL);\
    }
#endif
#endif
#ifndef __RTW_UTFREE__
extern void * utMalloc ( size_t ) ; extern void utFree ( void * ) ;
#endif
boolean_T diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( int_T *
bufSzPtr , int_T * tailPtr , int_T * headPtr , int_T * lastPtr , real_T
tMinusDelay , real_T * * uBufPtr , boolean_T isfixedbuf , boolean_T
istransportdelay , int_T * maxNewBufSzPtr ) ; real_T
diansai2026A_22b_acc_rt_TDelayInterpolate ( real_T tMinusDelay , real_T
tStart , real_T * uBuf , int_T bufSz , int_T * lastIdx , int_T oldestIdx ,
int_T newIdx , real_T initOutput , boolean_T discrete , boolean_T
minorStepAndTAtLastMajorOutput ) ; extern real_T look1_binlxpw ( real_T u0 ,
const real_T bp0 [ ] , const real_T table [ ] , uint32_T maxIndex ) ; void
diansai2026A_22b_PR_Init ( SimStruct * S , X_PR_diansai2026A_22b_T * localX )
; void diansai2026A_22b_PR_Deriv ( SimStruct * S , B_PR_diansai2026A_22b_T *
localB , P_PR_diansai2026A_22b_T * localP , X_PR_diansai2026A_22b_T * localX
, XDot_PR_diansai2026A_22b_T * localXdot ) ; void diansai2026A_22b_PR_ZC (
SimStruct * S , B_PR_diansai2026A_22b_T * localB , P_PR_diansai2026A_22b_T *
localP , ZCV_PR_diansai2026A_22b_T * localZCSV ) ; void diansai2026A_22b_PR (
SimStruct * S , real_T rtu_err , B_PR_diansai2026A_22b_T * localB ,
DW_PR_diansai2026A_22b_T * localDW , P_PR_diansai2026A_22b_T * localP ,
X_PR_diansai2026A_22b_T * localX ) ; void diansai2026A_22b_PR_c_Init (
SimStruct * S , X_PR_diansai2026A_22b_c_T * localX ) ; void
diansai2026A_22b_PR_i_Deriv ( SimStruct * S , B_PR_diansai2026A_22b_a_T *
localB , P_PR_diansai2026A_22b_a_T * localP , X_PR_diansai2026A_22b_c_T *
localX , XDot_PR_diansai2026A_22b_a_T * localXdot ) ; void
diansai2026A_22b_PR_e_ZC ( SimStruct * S , B_PR_diansai2026A_22b_a_T * localB
, P_PR_diansai2026A_22b_a_T * localP , ZCV_PR_diansai2026A_22b_o_T *
localZCSV ) ; void diansai2026A_22b_PR_j ( SimStruct * S , real_T rtu_err ,
B_PR_diansai2026A_22b_a_T * localB , DW_PR_diansai2026A_22b_f_T * localDW ,
P_PR_diansai2026A_22b_a_T * localP , X_PR_diansai2026A_22b_c_T * localX ) ;
void diansai2026A_22b_RMS_Init ( SimStruct * S , B_RMS_diansai2026A_22b_T *
localB , DW_RMS_diansai2026A_22b_T * localDW , P_RMS_diansai2026A_22b_T *
localP , X_RMS_diansai2026A_22b_T * localX ) ; void
diansai2026A_22b_RMS_Deriv ( SimStruct * S , B_RMS_diansai2026A_22b_T *
localB , DW_RMS_diansai2026A_22b_T * localDW , XDot_RMS_diansai2026A_22b_T *
localXdot ) ; void diansai2026A_22b_RMS_Disable ( SimStruct * S ,
DW_RMS_diansai2026A_22b_T * localDW ) ; void diansai2026A_22b_RMS_Update (
SimStruct * S , B_RMS_diansai2026A_22b_T * localB , DW_RMS_diansai2026A_22b_T
* localDW , P_RMS_diansai2026A_22b_T * localP ) ; void diansai2026A_22b_RMS (
SimStruct * S , boolean_T rtu_Enable , real_T rtu_In , real_T rtu_In_e ,
real_T rtu_In_l , B_RMS_diansai2026A_22b_T * localB ,
DW_RMS_diansai2026A_22b_T * localDW , P_RMS_diansai2026A_22b_T * localP ,
X_RMS_diansai2026A_22b_T * localX , XDis_RMS_diansai2026A_22b_T * localXdis )
; void diansai2026A_22b_TrueRMS_Init ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_T * localB , DW_TrueRMS_diansai2026A_22b_T *
localDW , P_TrueRMS_diansai2026A_22b_T * localP ,
X_TrueRMS_diansai2026A_22b_T * localX ) ; void diansai2026A_22b_TrueRMS_Deriv
( SimStruct * S , B_TrueRMS_diansai2026A_22b_T * localB ,
DW_TrueRMS_diansai2026A_22b_T * localDW , XDot_TrueRMS_diansai2026A_22b_T *
localXdot ) ; void diansai2026A_22b_TrueRMS_ZC ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_T * localB , DW_TrueRMS_diansai2026A_22b_T *
localDW , P_TrueRMS_diansai2026A_22b_T * localP ,
ZCV_TrueRMS_diansai2026A_22b_T * localZCSV ) ; void
diansai2026A_22b_TrueRMS_Disable ( SimStruct * S ,
DW_TrueRMS_diansai2026A_22b_T * localDW ) ; void
diansai2026A_22b_TrueRMS_Update ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_T * localB , DW_TrueRMS_diansai2026A_22b_T *
localDW , P_TrueRMS_diansai2026A_22b_T * localP ) ; void
diansai2026A_22b_TrueRMS ( SimStruct * S , boolean_T rtu_Enable , real_T
rtu_In , real_T rtu_In_f , real_T rtu_In_b , B_TrueRMS_diansai2026A_22b_T *
localB , DW_TrueRMS_diansai2026A_22b_T * localDW ,
P_TrueRMS_diansai2026A_22b_T * localP , X_TrueRMS_diansai2026A_22b_T * localX
, XDis_TrueRMS_diansai2026A_22b_T * localXdis ) ; void
diansai2026A_22b_RMS_d_Init ( SimStruct * S , B_RMS_diansai2026A_22b_a_T *
localB , DW_RMS_diansai2026A_22b_e_T * localDW , P_RMS_diansai2026A_22b_a_T *
localP , X_RMS_diansai2026A_22b_a_T * localX ) ; void
diansai2026A_22b_RMS_b_Deriv ( SimStruct * S , B_RMS_diansai2026A_22b_a_T *
localB , DW_RMS_diansai2026A_22b_e_T * localDW ,
XDot_RMS_diansai2026A_22b_e_T * localXdot ) ; void
diansai2026A_22b_RMS_m_Disable ( SimStruct * S , DW_RMS_diansai2026A_22b_e_T
* localDW ) ; void diansai2026A_22b_RMS_k_Update ( SimStruct * S ,
B_RMS_diansai2026A_22b_a_T * localB , DW_RMS_diansai2026A_22b_e_T * localDW ,
P_RMS_diansai2026A_22b_a_T * localP ) ; void diansai2026A_22b_RMS_l (
SimStruct * S , boolean_T rtu_Enable , real_T rtu_In ,
B_RMS_diansai2026A_22b_a_T * localB , DW_RMS_diansai2026A_22b_e_T * localDW ,
P_RMS_diansai2026A_22b_a_T * localP , X_RMS_diansai2026A_22b_a_T * localX ,
XDis_RMS_diansai2026A_22b_j_T * localXdis ) ; void
diansai2026A_22b_TrueRMS_o_Init ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ,
X_TrueRMS_diansai2026A_22b_h_T * localX ) ; void
diansai2026A_22b_TrueRMS_d_Deriv ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , XDot_TrueRMS_diansai2026A_22b_m_T * localXdot ) ; void
diansai2026A_22b_TrueRMS_e_ZC ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ,
ZCV_TrueRMS_diansai2026A_22b_n_T * localZCSV ) ; void
diansai2026A_22b_TrueRMS_j_Disable ( SimStruct * S ,
DW_TrueRMS_diansai2026A_22b_c_T * localDW ) ; void
diansai2026A_22b_TrueRMS_g_Update ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ) ; void
diansai2026A_22b_TrueRMS_f ( SimStruct * S , boolean_T rtu_Enable , real_T
rtu_In , B_TrueRMS_diansai2026A_22b_d_T * localB ,
DW_TrueRMS_diansai2026A_22b_c_T * localDW , P_TrueRMS_diansai2026A_22b_d_T *
localP , X_TrueRMS_diansai2026A_22b_h_T * localX ,
XDis_TrueRMS_diansai2026A_22b_e_T * localXdis ) ; void
diansai2026A_22b_PR_Term ( SimStruct * const S ) ; void
diansai2026A_22b_PR_l_Term ( SimStruct * const S ) ; void
diansai2026A_22b_RMS_Term ( SimStruct * const S ) ; void
diansai2026A_22b_TrueRMS_Term ( SimStruct * const S ) ; void
diansai2026A_22b_RMS_c_Term ( SimStruct * const S ) ; void
diansai2026A_22b_TrueRMS_e_Term ( SimStruct * const S ) ;
#endif
