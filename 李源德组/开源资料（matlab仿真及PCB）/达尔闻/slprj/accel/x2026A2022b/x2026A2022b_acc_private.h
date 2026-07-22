#ifndef RTW_HEADER_x2026A2022b_acc_private_h_
#define RTW_HEADER_x2026A2022b_acc_private_h_
#include "rtwtypes.h"
#include "multiword_types.h"
#include "x2026A2022b_acc.h"
#include "x2026A2022b_acc_types.h"
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
boolean_T x2026A2022b_acc_rt_TDelayUpdateTailOrGrowBuf ( int_T * bufSzPtr ,
int_T * tailPtr , int_T * headPtr , int_T * lastPtr , real_T tMinusDelay ,
real_T * * uBufPtr , boolean_T isfixedbuf , boolean_T istransportdelay ,
int_T * maxNewBufSzPtr ) ; real_T x2026A2022b_acc_rt_TDelayInterpolate (
real_T tMinusDelay , real_T tStart , real_T * uBuf , int_T bufSz , int_T *
lastIdx , int_T oldestIdx , int_T newIdx , real_T initOutput , boolean_T
discrete , boolean_T minorStepAndTAtLastMajorOutput ) ; extern real_T
look1_binlxpw ( real_T u0 , const real_T bp0 [ ] , const real_T table [ ] ,
uint32_T maxIndex ) ; void x2026A2022b_RMS_Init ( SimStruct * S ,
B_RMS_x2026A2022b_T * localB , DW_RMS_x2026A2022b_T * localDW ,
P_RMS_x2026A2022b_T * localP , X_RMS_x2026A2022b_T * localX ) ; void
x2026A2022b_RMS_Deriv ( SimStruct * S , B_RMS_x2026A2022b_T * localB ,
DW_RMS_x2026A2022b_T * localDW , XDot_RMS_x2026A2022b_T * localXdot ) ; void
x2026A2022b_RMS_Disable ( SimStruct * S , DW_RMS_x2026A2022b_T * localDW ) ;
void x2026A2022b_RMS_Update ( SimStruct * S , B_RMS_x2026A2022b_T * localB ,
DW_RMS_x2026A2022b_T * localDW , P_RMS_x2026A2022b_T * localP ) ; void
x2026A2022b_RMS ( SimStruct * S , boolean_T rtu_Enable , real_T rtu_In ,
real_T rtu_In_e , real_T rtu_In_l , B_RMS_x2026A2022b_T * localB ,
DW_RMS_x2026A2022b_T * localDW , P_RMS_x2026A2022b_T * localP ,
X_RMS_x2026A2022b_T * localX , XDis_RMS_x2026A2022b_T * localXdis ) ; void
x2026A2022b_TrueRMS_Init ( SimStruct * S , B_TrueRMS_x2026A2022b_T * localB ,
DW_TrueRMS_x2026A2022b_T * localDW , P_TrueRMS_x2026A2022b_T * localP ,
X_TrueRMS_x2026A2022b_T * localX ) ; void x2026A2022b_TrueRMS_Deriv (
SimStruct * S , B_TrueRMS_x2026A2022b_T * localB , DW_TrueRMS_x2026A2022b_T *
localDW , XDot_TrueRMS_x2026A2022b_T * localXdot ) ; void
x2026A2022b_TrueRMS_ZC ( SimStruct * S , B_TrueRMS_x2026A2022b_T * localB ,
DW_TrueRMS_x2026A2022b_T * localDW , P_TrueRMS_x2026A2022b_T * localP ,
ZCV_TrueRMS_x2026A2022b_T * localZCSV ) ; void x2026A2022b_TrueRMS_Disable (
SimStruct * S , DW_TrueRMS_x2026A2022b_T * localDW ) ; void
x2026A2022b_TrueRMS_Update ( SimStruct * S , B_TrueRMS_x2026A2022b_T * localB
, DW_TrueRMS_x2026A2022b_T * localDW , P_TrueRMS_x2026A2022b_T * localP ) ;
void x2026A2022b_TrueRMS ( SimStruct * S , boolean_T rtu_Enable , real_T
rtu_In , real_T rtu_In_f , real_T rtu_In_b , B_TrueRMS_x2026A2022b_T * localB
, DW_TrueRMS_x2026A2022b_T * localDW , P_TrueRMS_x2026A2022b_T * localP ,
X_TrueRMS_x2026A2022b_T * localX , XDis_TrueRMS_x2026A2022b_T * localXdis ) ;
void x2026A2022b_RMS_o_Init ( SimStruct * S , B_RMS_x2026A2022b_a_T * localB
, DW_RMS_x2026A2022b_e_T * localDW , P_RMS_x2026A2022b_a_T * localP ,
X_RMS_x2026A2022b_a_T * localX ) ; void x2026A2022b_RMS_k_Deriv ( SimStruct *
S , B_RMS_x2026A2022b_a_T * localB , DW_RMS_x2026A2022b_e_T * localDW ,
XDot_RMS_x2026A2022b_e_T * localXdot ) ; void x2026A2022b_RMS_k_Disable (
SimStruct * S , DW_RMS_x2026A2022b_e_T * localDW ) ; void
x2026A2022b_RMS_o_Update ( SimStruct * S , B_RMS_x2026A2022b_a_T * localB ,
DW_RMS_x2026A2022b_e_T * localDW , P_RMS_x2026A2022b_a_T * localP ) ; void
x2026A2022b_RMS_l ( SimStruct * S , boolean_T rtu_Enable , real_T rtu_In ,
B_RMS_x2026A2022b_a_T * localB , DW_RMS_x2026A2022b_e_T * localDW ,
P_RMS_x2026A2022b_a_T * localP , X_RMS_x2026A2022b_a_T * localX ,
XDis_RMS_x2026A2022b_j_T * localXdis ) ; void x2026A2022b_TrueRMS_e_Init (
SimStruct * S , B_TrueRMS_x2026A2022b_d_T * localB ,
DW_TrueRMS_x2026A2022b_c_T * localDW , P_TrueRMS_x2026A2022b_d_T * localP ,
X_TrueRMS_x2026A2022b_h_T * localX ) ; void x2026A2022b_TrueRMS_o_Deriv (
SimStruct * S , B_TrueRMS_x2026A2022b_d_T * localB ,
DW_TrueRMS_x2026A2022b_c_T * localDW , XDot_TrueRMS_x2026A2022b_m_T *
localXdot ) ; void x2026A2022b_TrueRMS_n_ZC ( SimStruct * S ,
B_TrueRMS_x2026A2022b_d_T * localB , DW_TrueRMS_x2026A2022b_c_T * localDW ,
P_TrueRMS_x2026A2022b_d_T * localP , ZCV_TrueRMS_x2026A2022b_n_T * localZCSV
) ; void x2026A2022b_TrueRMS_m_Disable ( SimStruct * S ,
DW_TrueRMS_x2026A2022b_c_T * localDW ) ; void x2026A2022b_TrueRMS_i_Update (
SimStruct * S , B_TrueRMS_x2026A2022b_d_T * localB ,
DW_TrueRMS_x2026A2022b_c_T * localDW , P_TrueRMS_x2026A2022b_d_T * localP ) ;
void x2026A2022b_TrueRMS_f ( SimStruct * S , boolean_T rtu_Enable , real_T
rtu_In , B_TrueRMS_x2026A2022b_d_T * localB , DW_TrueRMS_x2026A2022b_c_T *
localDW , P_TrueRMS_x2026A2022b_d_T * localP , X_TrueRMS_x2026A2022b_h_T *
localX , XDis_TrueRMS_x2026A2022b_e_T * localXdis ) ; void
x2026A2022b_RMS_Term ( SimStruct * const S ) ; void x2026A2022b_TrueRMS_Term
( SimStruct * const S ) ; void x2026A2022b_RMS_f_Term ( SimStruct * const S )
; void x2026A2022b_TrueRMS_d_Term ( SimStruct * const S ) ;
#endif
