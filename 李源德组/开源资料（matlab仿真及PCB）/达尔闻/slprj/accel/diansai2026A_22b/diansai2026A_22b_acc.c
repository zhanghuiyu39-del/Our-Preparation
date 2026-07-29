#include "diansai2026A_22b_acc.h"
#include "rtwtypes.h"
#include "diansai2026A_22b_acc_private.h"
#include "mwmathutil.h"
#include "multiword_types.h"
#include <stdio.h>
#include "slexec_vm_simstruct_bridge.h"
#include "slexec_vm_zc_functions.h"
#include "slexec_vm_lookup_functions.h"
#include "slsv_diagnostic_codegen_c_api.h"
#include "simtarget/slSimTgtMdlrefSfcnBridge.h"
#include "simstruc.h"
#include "fixedpoint.h"
#define CodeFormat S-Function
#define AccDefine1 Accelerator_S-Function
#include "simtarget/slAccSfcnBridge.h"
#ifndef __RTW_UTFREE__  
extern void * utMalloc ( size_t ) ; extern void utFree ( void * ) ;
#endif
boolean_T diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( int_T *
bufSzPtr , int_T * tailPtr , int_T * headPtr , int_T * lastPtr , real_T
tMinusDelay , real_T * * uBufPtr , boolean_T isfixedbuf , boolean_T
istransportdelay , int_T * maxNewBufSzPtr ) { int_T testIdx ; int_T tail = *
tailPtr ; int_T bufSz = * bufSzPtr ; real_T * tBuf = * uBufPtr + bufSz ;
real_T * xBuf = ( NULL ) ; int_T numBuffer = 2 ; if ( istransportdelay ) {
numBuffer = 3 ; xBuf = * uBufPtr + 2 * bufSz ; } testIdx = ( tail < ( bufSz -
1 ) ) ? ( tail + 1 ) : 0 ; if ( ( tMinusDelay <= tBuf [ testIdx ] ) && !
isfixedbuf ) { int_T j ; real_T * tempT ; real_T * tempU ; real_T * tempX = (
NULL ) ; real_T * uBuf = * uBufPtr ; int_T newBufSz = bufSz + 1024 ; if (
newBufSz > * maxNewBufSzPtr ) { * maxNewBufSzPtr = newBufSz ; } tempU = (
real_T * ) utMalloc ( numBuffer * newBufSz * sizeof ( real_T ) ) ; if ( tempU
== ( NULL ) ) { return ( false ) ; } tempT = tempU + newBufSz ; if (
istransportdelay ) tempX = tempT + newBufSz ; for ( j = tail ; j < bufSz ; j
++ ) { tempT [ j - tail ] = tBuf [ j ] ; tempU [ j - tail ] = uBuf [ j ] ; if
( istransportdelay ) tempX [ j - tail ] = xBuf [ j ] ; } for ( j = 0 ; j <
tail ; j ++ ) { tempT [ j + bufSz - tail ] = tBuf [ j ] ; tempU [ j + bufSz -
tail ] = uBuf [ j ] ; if ( istransportdelay ) tempX [ j + bufSz - tail ] =
xBuf [ j ] ; } if ( * lastPtr > tail ) { * lastPtr -= tail ; } else { *
lastPtr += ( bufSz - tail ) ; } * tailPtr = 0 ; * headPtr = bufSz ; utFree (
uBuf ) ; * bufSzPtr = newBufSz ; * uBufPtr = tempU ; } else { * tailPtr =
testIdx ; } return ( true ) ; } real_T
diansai2026A_22b_acc_rt_TDelayInterpolate ( real_T tMinusDelay , real_T
tStart , real_T * uBuf , int_T bufSz , int_T * lastIdx , int_T oldestIdx ,
int_T newIdx , real_T initOutput , boolean_T discrete , boolean_T
minorStepAndTAtLastMajorOutput ) { int_T i ; real_T yout , t1 , t2 , u1 , u2
; real_T * tBuf = uBuf + bufSz ; if ( ( newIdx == 0 ) && ( oldestIdx == 0 )
&& ( tMinusDelay > tStart ) ) return initOutput ; if ( tMinusDelay <= tStart
) return initOutput ; if ( ( tMinusDelay <= tBuf [ oldestIdx ] ) ) { if (
discrete ) { return ( uBuf [ oldestIdx ] ) ; } else { int_T tempIdx =
oldestIdx + 1 ; if ( oldestIdx == bufSz - 1 ) tempIdx = 0 ; t1 = tBuf [
oldestIdx ] ; t2 = tBuf [ tempIdx ] ; u1 = uBuf [ oldestIdx ] ; u2 = uBuf [
tempIdx ] ; if ( t2 == t1 ) { if ( tMinusDelay >= t2 ) { yout = u2 ; } else {
yout = u1 ; } } else { real_T f1 = ( t2 - tMinusDelay ) / ( t2 - t1 ) ;
real_T f2 = 1.0 - f1 ; yout = f1 * u1 + f2 * u2 ; } return yout ; } } if (
minorStepAndTAtLastMajorOutput ) { if ( newIdx != 0 ) { if ( * lastIdx ==
newIdx ) { ( * lastIdx ) -- ; } newIdx -- ; } else { if ( * lastIdx == newIdx
) { * lastIdx = bufSz - 1 ; } newIdx = bufSz - 1 ; } } i = * lastIdx ; if (
tBuf [ i ] < tMinusDelay ) { while ( tBuf [ i ] < tMinusDelay ) { if ( i ==
newIdx ) break ; i = ( i < ( bufSz - 1 ) ) ? ( i + 1 ) : 0 ; } } else { while
( tBuf [ i ] >= tMinusDelay ) { i = ( i > 0 ) ? i - 1 : ( bufSz - 1 ) ; } i =
( i < ( bufSz - 1 ) ) ? ( i + 1 ) : 0 ; } * lastIdx = i ; if ( discrete ) {
double tempEps = ( DBL_EPSILON ) * 128.0 ; double localEps = tempEps *
muDoubleScalarAbs ( tBuf [ i ] ) ; if ( tempEps > localEps ) { localEps =
tempEps ; } localEps = localEps / 2.0 ; if ( tMinusDelay >= ( tBuf [ i ] -
localEps ) ) { yout = uBuf [ i ] ; } else { if ( i == 0 ) { yout = uBuf [
bufSz - 1 ] ; } else { yout = uBuf [ i - 1 ] ; } } } else { if ( i == 0 ) {
t1 = tBuf [ bufSz - 1 ] ; u1 = uBuf [ bufSz - 1 ] ; } else { t1 = tBuf [ i -
1 ] ; u1 = uBuf [ i - 1 ] ; } t2 = tBuf [ i ] ; u2 = uBuf [ i ] ; if ( t2 ==
t1 ) { if ( tMinusDelay >= t2 ) { yout = u2 ; } else { yout = u1 ; } } else {
real_T f1 = ( t2 - tMinusDelay ) / ( t2 - t1 ) ; real_T f2 = 1.0 - f1 ; yout
= f1 * u1 + f2 * u2 ; } } return ( yout ) ; } real_T look1_binlxpw ( real_T
u0 , const real_T bp0 [ ] , const real_T table [ ] , uint32_T maxIndex ) {
real_T frac ; real_T yL_0d0 ; uint32_T bpIdx ; uint32_T iLeft ; uint32_T
iRght ; if ( u0 <= bp0 [ 0U ] ) { iLeft = 0U ; frac = ( u0 - bp0 [ 0U ] ) / (
bp0 [ 1U ] - bp0 [ 0U ] ) ; } else if ( u0 < bp0 [ maxIndex ] ) { bpIdx =
maxIndex >> 1U ; iLeft = 0U ; iRght = maxIndex ; while ( iRght - iLeft > 1U )
{ if ( u0 < bp0 [ bpIdx ] ) { iRght = bpIdx ; } else { iLeft = bpIdx ; }
bpIdx = ( iRght + iLeft ) >> 1U ; } frac = ( u0 - bp0 [ iLeft ] ) / ( bp0 [
iLeft + 1U ] - bp0 [ iLeft ] ) ; } else { iLeft = maxIndex - 1U ; frac = ( u0
- bp0 [ maxIndex - 1U ] ) / ( bp0 [ maxIndex ] - bp0 [ maxIndex - 1U ] ) ; }
yL_0d0 = table [ iLeft ] ; return ( table [ iLeft + 1U ] - yL_0d0 ) * frac +
yL_0d0 ; } void rt_ssGetBlockPath ( SimStruct * S , int_T sysIdx , int_T
blkIdx , char_T * * path ) { _ssGetBlockPath ( S , sysIdx , blkIdx , path ) ;
} void rt_ssSet_slErrMsg ( void * S , void * diag ) { SimStruct * castedS = (
SimStruct * ) S ; if ( ! _ssIsErrorStatusAslErrMsg ( castedS ) ) {
_ssSet_slErrMsg ( castedS , diag ) ; } else { _ssDiscardDiagnostic ( castedS
, diag ) ; } } void rt_ssReportDiagnosticAsWarning ( void * S , void * diag )
{ _ssReportDiagnosticAsWarning ( ( SimStruct * ) S , diag ) ; } void
rt_ssReportDiagnosticAsInfo ( void * S , void * diag ) {
_ssReportDiagnosticAsInfo ( ( SimStruct * ) S , diag ) ; } void
diansai2026A_22b_PR_Init ( SimStruct * S , X_PR_diansai2026A_22b_T * localX )
{ localX -> TransferFcn_CSTATE_n [ 0 ] = 0.0 ; localX -> TransferFcn_CSTATE_n
[ 1 ] = 0.0 ; } void diansai2026A_22b_PR ( SimStruct * S , real_T rtu_err ,
B_PR_diansai2026A_22b_T * localB , DW_PR_diansai2026A_22b_T * localDW ,
P_PR_diansai2026A_22b_T * localP , X_PR_diansai2026A_22b_T * localX ) {
int32_T isHit ; isHit = ssIsSampleHit ( S , 3 , 0 ) ; if ( isHit != 0 ) {
localB -> B_0_0_0 = localP -> P_0 * rtu_err ; localB -> B_0_1_0 = localP ->
P_1 * rtu_err ; } localB -> B_0_2_0 = 0.0 ; localB -> B_0_2_0 += localP ->
P_3 [ 0 ] * localX -> TransferFcn_CSTATE_n [ 0 ] ; localB -> B_0_2_0 +=
localP -> P_3 [ 1 ] * localX -> TransferFcn_CSTATE_n [ 1 ] ; localB ->
B_0_3_0 = localB -> B_0_0_0 + localB -> B_0_2_0 ; if ( ssIsModeUpdateTimeStep
( S ) ) { localDW -> Saturation_MODE = localB -> B_0_3_0 >= localP -> P_4 ? 1
: localB -> B_0_3_0 > localP -> P_5 ? 0 : - 1 ; } localB -> B_0_4_0 = localDW
-> Saturation_MODE == 1 ? localP -> P_4 : localDW -> Saturation_MODE == - 1 ?
localP -> P_5 : localB -> B_0_3_0 ; } void diansai2026A_22b_PR_Deriv (
SimStruct * S , B_PR_diansai2026A_22b_T * localB , P_PR_diansai2026A_22b_T *
localP , X_PR_diansai2026A_22b_T * localX , XDot_PR_diansai2026A_22b_T *
localXdot ) { localXdot -> TransferFcn_CSTATE_n [ 0 ] = 0.0 ; localXdot ->
TransferFcn_CSTATE_n [ 0 ] += localP -> P_2 [ 0 ] * localX ->
TransferFcn_CSTATE_n [ 0 ] ; localXdot -> TransferFcn_CSTATE_n [ 1 ] = 0.0 ;
localXdot -> TransferFcn_CSTATE_n [ 0 ] += localP -> P_2 [ 1 ] * localX ->
TransferFcn_CSTATE_n [ 1 ] ; localXdot -> TransferFcn_CSTATE_n [ 1 ] +=
localX -> TransferFcn_CSTATE_n [ 0 ] ; localXdot -> TransferFcn_CSTATE_n [ 0
] += localB -> B_0_1_0 ; } void diansai2026A_22b_PR_ZC ( SimStruct * S ,
B_PR_diansai2026A_22b_T * localB , P_PR_diansai2026A_22b_T * localP ,
ZCV_PR_diansai2026A_22b_T * localZCSV ) { localZCSV -> Saturation_UprLim_ZC_h
= localB -> B_0_3_0 - localP -> P_4 ; localZCSV -> Saturation_LwrLim_ZC_a =
localB -> B_0_3_0 - localP -> P_5 ; } void diansai2026A_22b_PR_Term (
SimStruct * const S ) { } void diansai2026A_22b_PR_c_Init ( SimStruct * S ,
X_PR_diansai2026A_22b_c_T * localX ) { localX -> TransferFcn_CSTATE_e [ 0 ] =
0.0 ; localX -> TransferFcn_CSTATE_e [ 1 ] = 0.0 ; } void
diansai2026A_22b_PR_j ( SimStruct * S , real_T rtu_err ,
B_PR_diansai2026A_22b_a_T * localB , DW_PR_diansai2026A_22b_f_T * localDW ,
P_PR_diansai2026A_22b_a_T * localP , X_PR_diansai2026A_22b_c_T * localX ) {
localB -> B_2_0_0 = localP -> P_0 * rtu_err ; localB -> B_2_1_0 = localP ->
P_1 * rtu_err ; localB -> B_2_2_0 = 0.0 ; localB -> B_2_2_0 += localP -> P_3
[ 0 ] * localX -> TransferFcn_CSTATE_e [ 0 ] ; localB -> B_2_2_0 += localP ->
P_3 [ 1 ] * localX -> TransferFcn_CSTATE_e [ 1 ] ; localB -> B_2_3_0 = localB
-> B_2_0_0 + localB -> B_2_2_0 ; if ( ssIsModeUpdateTimeStep ( S ) ) {
localDW -> Saturation_MODE = localB -> B_2_3_0 >= localP -> P_4 ? 1 : localB
-> B_2_3_0 > localP -> P_5 ? 0 : - 1 ; } localB -> B_2_4_0 = localDW ->
Saturation_MODE == 1 ? localP -> P_4 : localDW -> Saturation_MODE == - 1 ?
localP -> P_5 : localB -> B_2_3_0 ; } void diansai2026A_22b_PR_i_Deriv (
SimStruct * S , B_PR_diansai2026A_22b_a_T * localB ,
P_PR_diansai2026A_22b_a_T * localP , X_PR_diansai2026A_22b_c_T * localX ,
XDot_PR_diansai2026A_22b_a_T * localXdot ) { localXdot ->
TransferFcn_CSTATE_e [ 0 ] = 0.0 ; localXdot -> TransferFcn_CSTATE_e [ 0 ] +=
localP -> P_2 [ 0 ] * localX -> TransferFcn_CSTATE_e [ 0 ] ; localXdot ->
TransferFcn_CSTATE_e [ 1 ] = 0.0 ; localXdot -> TransferFcn_CSTATE_e [ 0 ] +=
localP -> P_2 [ 1 ] * localX -> TransferFcn_CSTATE_e [ 1 ] ; localXdot ->
TransferFcn_CSTATE_e [ 1 ] += localX -> TransferFcn_CSTATE_e [ 0 ] ;
localXdot -> TransferFcn_CSTATE_e [ 0 ] += localB -> B_2_1_0 ; } void
diansai2026A_22b_PR_e_ZC ( SimStruct * S , B_PR_diansai2026A_22b_a_T * localB
, P_PR_diansai2026A_22b_a_T * localP , ZCV_PR_diansai2026A_22b_o_T *
localZCSV ) { localZCSV -> Saturation_UprLim_ZC = localB -> B_2_3_0 - localP
-> P_4 ; localZCSV -> Saturation_LwrLim_ZC = localB -> B_2_3_0 - localP ->
P_5 ; } void diansai2026A_22b_PR_l_Term ( SimStruct * const S ) { } void
diansai2026A_22b_RMS_Init ( SimStruct * S , B_RMS_diansai2026A_22b_T * localB
, DW_RMS_diansai2026A_22b_T * localDW , P_RMS_diansai2026A_22b_T * localP ,
X_RMS_diansai2026A_22b_T * localX ) { localX -> integrator_CSTATE_a [ 0 ] =
localP -> P_3 ; localDW -> Memory_PreviousInput [ 0 ] = localP -> P_7 ;
localX -> integrator_CSTATE_h [ 0 ] = localP -> P_8 ; localDW ->
Memory_PreviousInput_o [ 0 ] = localP -> P_12 ; localB -> B_10_22_0 [ 0 ] =
localP -> P_2 ; localX -> integrator_CSTATE_a [ 1 ] = localP -> P_3 ; localDW
-> Memory_PreviousInput [ 1 ] = localP -> P_7 ; localX -> integrator_CSTATE_h
[ 1 ] = localP -> P_8 ; localDW -> Memory_PreviousInput_o [ 1 ] = localP ->
P_12 ; localB -> B_10_22_0 [ 1 ] = localP -> P_2 ; localX ->
integrator_CSTATE_a [ 2 ] = localP -> P_3 ; localDW -> Memory_PreviousInput [
2 ] = localP -> P_7 ; localX -> integrator_CSTATE_h [ 2 ] = localP -> P_8 ;
localDW -> Memory_PreviousInput_o [ 2 ] = localP -> P_12 ; localB ->
B_10_22_0 [ 2 ] = localP -> P_2 ; } void diansai2026A_22b_RMS_Disable (
SimStruct * S , DW_RMS_diansai2026A_22b_T * localDW ) { localDW -> RMS_MODE =
false ; } void diansai2026A_22b_RMS ( SimStruct * S , boolean_T rtu_Enable ,
real_T rtu_In , real_T rtu_In_e , real_T rtu_In_l , B_RMS_diansai2026A_22b_T
* localB , DW_RMS_diansai2026A_22b_T * localDW , P_RMS_diansai2026A_22b_T *
localP , X_RMS_diansai2026A_22b_T * localX , XDis_RMS_diansai2026A_22b_T *
localXdis ) { real_T rtb_B_10_18_0 ; int32_T isHit ; boolean_T rtb_B_10_4_0 ;
isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( ( isHit != 0 ) &&
ssIsModeUpdateTimeStep ( S ) ) { if ( rtu_Enable ) { if ( ! localDW ->
RMS_MODE ) { if ( ssGetTaskTime ( S , 1 ) != ssGetTStart ( S ) ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; } ( void ) memset ( & (
localXdis -> integrator_CSTATE_a ) , 0 , 6 * sizeof ( boolean_T ) ) ; localDW
-> RMS_MODE = true ; } } else { if ( ssGetTaskTime ( S , 1 ) == ssGetTStart (
S ) ) { ( void ) memset ( & ( localXdis -> integrator_CSTATE_a ) , 1 , 6 *
sizeof ( boolean_T ) ) ; } if ( localDW -> RMS_MODE ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; ( void ) memset ( & (
localXdis -> integrator_CSTATE_a ) , 1 , 6 * sizeof ( boolean_T ) ) ;
diansai2026A_22b_RMS_Disable ( S , localDW ) ; } } } if ( localDW -> RMS_MODE
) { localB -> B_10_0_0 [ 0 ] = localX -> integrator_CSTATE_a [ 0 ] ; localB
-> B_10_0_0 [ 1 ] = localX -> integrator_CSTATE_a [ 1 ] ; localB -> B_10_0_0
[ 2 ] = localX -> integrator_CSTATE_a [ 2 ] ; { real_T * * uBuffer = ( real_T
* * ) & localDW -> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime
= ssGetT ( S ) ; real_T tMinusDelay ; { int_T i1 ; real_T * y0 = & localB ->
B_10_1_0 [ 0 ] ; int_T * iw_Tail = & localDW -> TransportDelay_IWORK . Tail [
0 ] ; int_T * iw_Head = & localDW -> TransportDelay_IWORK . Head [ 0 ] ;
int_T * iw_Last = & localDW -> TransportDelay_IWORK . Last [ 0 ] ; int_T *
iw_CircularBufSize = & localDW -> TransportDelay_IWORK . CircularBufSize [ 0
] ; for ( i1 = 0 ; i1 < 3 ; i1 ++ ) { tMinusDelay = ( ( localP -> P_4 > 0.0 )
? localP -> P_4 : 0.0 ) ; tMinusDelay = simTime - tMinusDelay ; y0 [ i1 ] =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
iw_CircularBufSize [ i1 ] , & iw_Last [ i1 ] , iw_Tail [ i1 ] , iw_Head [ i1
] , localP -> P_5 , 0 , ( boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( *
uBuffer + iw_CircularBufSize [ i1 ] ) [ iw_Head [ i1 ] ] == ssGetT ( S ) ) )
) ; uBuffer ++ ; } } } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { localB -> B_10_3_0 = localP -> P_6 ; } rtb_B_10_4_0 = ( ssGetT ( S ) >=
localB -> B_10_3_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { localB -> B_10_5_0 [ 0 ] = localDW -> Memory_PreviousInput [ 0 ] ; localB
-> B_10_5_0 [ 1 ] = localDW -> Memory_PreviousInput [ 1 ] ; localB ->
B_10_5_0 [ 2 ] = localDW -> Memory_PreviousInput [ 2 ] ; } if ( rtb_B_10_4_0
) { rtb_B_10_18_0 = localB -> B_10_0_0 [ 0 ] - localB -> B_10_1_0 [ 0 ] ;
localB -> B_9_0_0 [ 0 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *= localP -> P_1 ;
localB -> B_9_1_0 [ 0 ] = rtb_B_10_18_0 ; localB -> B_10_7_0 [ 0 ] =
rtb_B_10_18_0 ; rtb_B_10_18_0 = localB -> B_10_0_0 [ 1 ] - localB -> B_10_1_0
[ 1 ] ; localB -> B_9_0_0 [ 1 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *= localP ->
P_1 ; localB -> B_9_1_0 [ 1 ] = rtb_B_10_18_0 ; localB -> B_10_7_0 [ 1 ] =
rtb_B_10_18_0 ; rtb_B_10_18_0 = localB -> B_10_0_0 [ 2 ] - localB -> B_10_1_0
[ 2 ] ; localB -> B_9_0_0 [ 2 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *= localP ->
P_1 ; localB -> B_9_1_0 [ 2 ] = rtb_B_10_18_0 ; localB -> B_10_7_0 [ 2 ] =
rtb_B_10_18_0 ; } else { localB -> B_10_7_0 [ 0 ] = localB -> B_10_5_0 [ 0 ]
; localB -> B_10_7_0 [ 1 ] = localB -> B_10_5_0 [ 1 ] ; localB -> B_10_7_0 [
2 ] = localB -> B_10_5_0 [ 2 ] ; } localB -> B_10_8_0 [ 0 ] = localX ->
integrator_CSTATE_h [ 0 ] ; localB -> B_10_8_0 [ 1 ] = localX ->
integrator_CSTATE_h [ 1 ] ; localB -> B_10_8_0 [ 2 ] = localX ->
integrator_CSTATE_h [ 2 ] ; { real_T * * uBuffer = ( real_T * * ) & localDW
-> TransportDelay_PWORK_j . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S
) ; real_T tMinusDelay ; { int_T i1 ; real_T * y0 = & localB -> B_10_9_0 [ 0
] ; int_T * iw_Tail = & localDW -> TransportDelay_IWORK_c . Tail [ 0 ] ;
int_T * iw_Head = & localDW -> TransportDelay_IWORK_c . Head [ 0 ] ; int_T *
iw_Last = & localDW -> TransportDelay_IWORK_c . Last [ 0 ] ; int_T *
iw_CircularBufSize = & localDW -> TransportDelay_IWORK_c . CircularBufSize [
0 ] ; for ( i1 = 0 ; i1 < 3 ; i1 ++ ) { tMinusDelay = ( ( localP -> P_9 > 0.0
) ? localP -> P_9 : 0.0 ) ; tMinusDelay = simTime - tMinusDelay ; y0 [ i1 ] =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
iw_CircularBufSize [ i1 ] , & iw_Last [ i1 ] , iw_Tail [ i1 ] , iw_Head [ i1
] , localP -> P_10 , 0 , ( boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( *
uBuffer + iw_CircularBufSize [ i1 ] ) [ iw_Head [ i1 ] ] == ssGetT ( S ) ) )
) ; uBuffer ++ ; } } } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { localB -> B_10_11_0 = localP -> P_11 ; } rtb_B_10_4_0 = ( ssGetT ( S ) >=
localB -> B_10_11_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { localB -> B_10_13_0 [ 0 ] = localDW -> Memory_PreviousInput_o [ 0 ] ;
localB -> B_10_13_0 [ 1 ] = localDW -> Memory_PreviousInput_o [ 1 ] ; localB
-> B_10_13_0 [ 2 ] = localDW -> Memory_PreviousInput_o [ 2 ] ; } if (
rtb_B_10_4_0 ) { rtb_B_10_18_0 = localB -> B_10_8_0 [ 0 ] - localB ->
B_10_9_0 [ 0 ] ; localB -> B_8_0_0 [ 0 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *=
localP -> P_0 ; localB -> B_8_1_0 [ 0 ] = rtb_B_10_18_0 ; localB -> B_10_15_0
[ 0 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 = localB -> B_10_8_0 [ 1 ] - localB ->
B_10_9_0 [ 1 ] ; localB -> B_8_0_0 [ 1 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *=
localP -> P_0 ; localB -> B_8_1_0 [ 1 ] = rtb_B_10_18_0 ; localB -> B_10_15_0
[ 1 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 = localB -> B_10_8_0 [ 2 ] - localB ->
B_10_9_0 [ 2 ] ; localB -> B_8_0_0 [ 2 ] = rtb_B_10_18_0 ; rtb_B_10_18_0 *=
localP -> P_0 ; localB -> B_8_1_0 [ 2 ] = rtb_B_10_18_0 ; localB -> B_10_15_0
[ 2 ] = rtb_B_10_18_0 ; } else { localB -> B_10_15_0 [ 0 ] = localB ->
B_10_13_0 [ 0 ] ; localB -> B_10_15_0 [ 1 ] = localB -> B_10_13_0 [ 1 ] ;
localB -> B_10_15_0 [ 2 ] = localB -> B_10_13_0 [ 2 ] ; } rtb_B_10_18_0 =
muDoubleScalarSin ( localP -> P_15 * ssGetTaskTime ( S , 0 ) + localP -> P_16
) * localP -> P_13 + localP -> P_14 ; localB -> B_10_19_0 [ 0 ] = rtu_In *
rtb_B_10_18_0 ; localB -> B_10_19_0 [ 1 ] = rtu_In_e * rtb_B_10_18_0 ; localB
-> B_10_19_0 [ 2 ] = rtu_In_l * rtb_B_10_18_0 ; rtb_B_10_18_0 =
muDoubleScalarSin ( localP -> P_19 * ssGetTaskTime ( S , 0 ) + localP -> P_20
) * localP -> P_17 + localP -> P_18 ; localB -> B_10_21_0 [ 0 ] = rtu_In *
rtb_B_10_18_0 ; localB -> B_10_21_0 [ 1 ] = rtu_In_e * rtb_B_10_18_0 ; localB
-> B_10_21_0 [ 2 ] = rtu_In_l * rtb_B_10_18_0 ; localB -> B_10_16_0 [ 0 ] .
re = localB -> B_10_7_0 [ 0 ] ; localB -> B_10_16_0 [ 0 ] . im = localB ->
B_10_15_0 [ 0 ] ; localB -> B_10_22_0 [ 0 ] = localP -> P_21 *
muDoubleScalarHypot ( localB -> B_10_7_0 [ 0 ] , localB -> B_10_15_0 [ 0 ] )
; localB -> B_10_16_0 [ 1 ] . re = localB -> B_10_7_0 [ 1 ] ; localB ->
B_10_16_0 [ 1 ] . im = localB -> B_10_15_0 [ 1 ] ; localB -> B_10_22_0 [ 1 ]
= localP -> P_21 * muDoubleScalarHypot ( localB -> B_10_7_0 [ 1 ] , localB ->
B_10_15_0 [ 1 ] ) ; localB -> B_10_16_0 [ 2 ] . re = localB -> B_10_7_0 [ 2 ]
; localB -> B_10_16_0 [ 2 ] . im = localB -> B_10_15_0 [ 2 ] ; localB ->
B_10_22_0 [ 2 ] = localP -> P_21 * muDoubleScalarHypot ( localB -> B_10_7_0 [
2 ] , localB -> B_10_15_0 [ 2 ] ) ; if ( ssIsModeUpdateTimeStep ( S ) ) {
srUpdateBC ( localDW -> RMS_SubsysRanBC ) ; } } } void
diansai2026A_22b_RMS_Update ( SimStruct * S , B_RMS_diansai2026A_22b_T *
localB , DW_RMS_diansai2026A_22b_T * localDW , P_RMS_diansai2026A_22b_T *
localP ) { int32_T isHit ; if ( localDW -> RMS_MODE ) { { real_T * * uBuffer
= ( real_T * * ) & localDW -> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ;
real_T simTime = ssGetT ( S ) ; localDW -> TransportDelay_IWORK . Head [ 0 ]
= ( ( localDW -> TransportDelay_IWORK . Head [ 0 ] < ( localDW ->
TransportDelay_IWORK . CircularBufSize [ 0 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK . Head [ 0 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK . Head [ 0 ] == localDW -> TransportDelay_IWORK . Tail [
0 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW
-> TransportDelay_IWORK . CircularBufSize [ 0 ] , & localDW ->
TransportDelay_IWORK . Tail [ 0 ] , & localDW -> TransportDelay_IWORK . Head
[ 0 ] , & localDW -> TransportDelay_IWORK . Last [ 0 ] , simTime - localP ->
P_4 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 0 ] ) [ localDW -> TransportDelay_IWORK . Head [ 0 ] ] =
simTime ; ( * uBuffer ++ ) [ localDW -> TransportDelay_IWORK . Head [ 0 ] ] =
localB -> B_10_0_0 [ 0 ] ; localDW -> TransportDelay_IWORK . Head [ 1 ] = ( (
localDW -> TransportDelay_IWORK . Head [ 1 ] < ( localDW ->
TransportDelay_IWORK . CircularBufSize [ 1 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK . Head [ 1 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK . Head [ 1 ] == localDW -> TransportDelay_IWORK . Tail [
1 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW
-> TransportDelay_IWORK . CircularBufSize [ 1 ] , & localDW ->
TransportDelay_IWORK . Tail [ 1 ] , & localDW -> TransportDelay_IWORK . Head
[ 1 ] , & localDW -> TransportDelay_IWORK . Last [ 1 ] , simTime - localP ->
P_4 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 1 ] ) [ localDW -> TransportDelay_IWORK . Head [ 1 ] ] =
simTime ; ( * uBuffer ++ ) [ localDW -> TransportDelay_IWORK . Head [ 1 ] ] =
localB -> B_10_0_0 [ 1 ] ; localDW -> TransportDelay_IWORK . Head [ 2 ] = ( (
localDW -> TransportDelay_IWORK . Head [ 2 ] < ( localDW ->
TransportDelay_IWORK . CircularBufSize [ 2 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK . Head [ 2 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK . Head [ 2 ] == localDW -> TransportDelay_IWORK . Tail [
2 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW
-> TransportDelay_IWORK . CircularBufSize [ 2 ] , & localDW ->
TransportDelay_IWORK . Tail [ 2 ] , & localDW -> TransportDelay_IWORK . Head
[ 2 ] , & localDW -> TransportDelay_IWORK . Last [ 2 ] , simTime - localP ->
P_4 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 2 ] ) [ localDW -> TransportDelay_IWORK . Head [ 2 ] ] =
simTime ; ( * uBuffer ) [ localDW -> TransportDelay_IWORK . Head [ 2 ] ] =
localB -> B_10_0_0 [ 2 ] ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit
!= 0 ) { localDW -> Memory_PreviousInput [ 0 ] = localB -> B_10_7_0 [ 0 ] ;
localDW -> Memory_PreviousInput [ 1 ] = localB -> B_10_7_0 [ 1 ] ; localDW ->
Memory_PreviousInput [ 2 ] = localB -> B_10_7_0 [ 2 ] ; } { real_T * *
uBuffer = ( real_T * * ) & localDW -> TransportDelay_PWORK_j . TUbufferPtrs [
0 ] ; real_T simTime = ssGetT ( S ) ; localDW -> TransportDelay_IWORK_c .
Head [ 0 ] = ( ( localDW -> TransportDelay_IWORK_c . Head [ 0 ] < ( localDW
-> TransportDelay_IWORK_c . CircularBufSize [ 0 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK_c . Head [ 0 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK_c . Head [ 0 ] == localDW -> TransportDelay_IWORK_c .
Tail [ 0 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( &
localDW -> TransportDelay_IWORK_c . CircularBufSize [ 0 ] , & localDW ->
TransportDelay_IWORK_c . Tail [ 0 ] , & localDW -> TransportDelay_IWORK_c .
Head [ 0 ] , & localDW -> TransportDelay_IWORK_c . Last [ 0 ] , simTime -
localP -> P_9 , uBuffer , ( boolean_T ) 0 , false , & localDW ->
TransportDelay_IWORK_c . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + localDW ->
TransportDelay_IWORK_c . CircularBufSize [ 0 ] ) [ localDW ->
TransportDelay_IWORK_c . Head [ 0 ] ] = simTime ; ( * uBuffer ++ ) [ localDW
-> TransportDelay_IWORK_c . Head [ 0 ] ] = localB -> B_10_8_0 [ 0 ] ; localDW
-> TransportDelay_IWORK_c . Head [ 1 ] = ( ( localDW ->
TransportDelay_IWORK_c . Head [ 1 ] < ( localDW -> TransportDelay_IWORK_c .
CircularBufSize [ 1 ] - 1 ) ) ? ( localDW -> TransportDelay_IWORK_c . Head [
1 ] + 1 ) : 0 ) ; if ( localDW -> TransportDelay_IWORK_c . Head [ 1 ] ==
localDW -> TransportDelay_IWORK_c . Tail [ 1 ] ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK_c . CircularBufSize [ 1 ] , & localDW ->
TransportDelay_IWORK_c . Tail [ 1 ] , & localDW -> TransportDelay_IWORK_c .
Head [ 1 ] , & localDW -> TransportDelay_IWORK_c . Last [ 1 ] , simTime -
localP -> P_9 , uBuffer , ( boolean_T ) 0 , false , & localDW ->
TransportDelay_IWORK_c . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + localDW ->
TransportDelay_IWORK_c . CircularBufSize [ 1 ] ) [ localDW ->
TransportDelay_IWORK_c . Head [ 1 ] ] = simTime ; ( * uBuffer ++ ) [ localDW
-> TransportDelay_IWORK_c . Head [ 1 ] ] = localB -> B_10_8_0 [ 1 ] ; localDW
-> TransportDelay_IWORK_c . Head [ 2 ] = ( ( localDW ->
TransportDelay_IWORK_c . Head [ 2 ] < ( localDW -> TransportDelay_IWORK_c .
CircularBufSize [ 2 ] - 1 ) ) ? ( localDW -> TransportDelay_IWORK_c . Head [
2 ] + 1 ) : 0 ) ; if ( localDW -> TransportDelay_IWORK_c . Head [ 2 ] ==
localDW -> TransportDelay_IWORK_c . Tail [ 2 ] ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK_c . CircularBufSize [ 2 ] , & localDW ->
TransportDelay_IWORK_c . Tail [ 2 ] , & localDW -> TransportDelay_IWORK_c .
Head [ 2 ] , & localDW -> TransportDelay_IWORK_c . Last [ 2 ] , simTime -
localP -> P_9 , uBuffer , ( boolean_T ) 0 , false , & localDW ->
TransportDelay_IWORK_c . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + localDW ->
TransportDelay_IWORK_c . CircularBufSize [ 2 ] ) [ localDW ->
TransportDelay_IWORK_c . Head [ 2 ] ] = simTime ; ( * uBuffer ) [ localDW ->
TransportDelay_IWORK_c . Head [ 2 ] ] = localB -> B_10_8_0 [ 2 ] ; } isHit =
ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { localDW ->
Memory_PreviousInput_o [ 0 ] = localB -> B_10_15_0 [ 0 ] ; localDW ->
Memory_PreviousInput_o [ 1 ] = localB -> B_10_15_0 [ 1 ] ; localDW ->
Memory_PreviousInput_o [ 2 ] = localB -> B_10_15_0 [ 2 ] ; } } } void
diansai2026A_22b_RMS_Deriv ( SimStruct * S , B_RMS_diansai2026A_22b_T *
localB , DW_RMS_diansai2026A_22b_T * localDW , XDot_RMS_diansai2026A_22b_T *
localXdot ) { if ( localDW -> RMS_MODE ) { localXdot -> integrator_CSTATE_a [
0 ] = localB -> B_10_19_0 [ 0 ] ; localXdot -> integrator_CSTATE_h [ 0 ] =
localB -> B_10_21_0 [ 0 ] ; localXdot -> integrator_CSTATE_a [ 1 ] = localB
-> B_10_19_0 [ 1 ] ; localXdot -> integrator_CSTATE_h [ 1 ] = localB ->
B_10_21_0 [ 1 ] ; localXdot -> integrator_CSTATE_a [ 2 ] = localB ->
B_10_19_0 [ 2 ] ; localXdot -> integrator_CSTATE_h [ 2 ] = localB ->
B_10_21_0 [ 2 ] ; } else { { real_T * dx ; int_T i ; dx = & ( localXdot ->
integrator_CSTATE_a [ 0 ] ) ; for ( i = 0 ; i < 6 ; i ++ ) { dx [ i ] = 0.0 ;
} } } } void diansai2026A_22b_RMS_Term ( SimStruct * const S ) { } void
diansai2026A_22b_TrueRMS_Init ( SimStruct * S , B_TrueRMS_diansai2026A_22b_T
* localB , DW_TrueRMS_diansai2026A_22b_T * localDW ,
P_TrueRMS_diansai2026A_22b_T * localP , X_TrueRMS_diansai2026A_22b_T * localX
) { localX -> integrator_CSTATE_o [ 0 ] = localP -> P_2 ; localDW ->
Memory_PreviousInput [ 0 ] = localP -> P_6 ; localB -> B_12_10_0 [ 0 ] =
localP -> P_1 ; localX -> integrator_CSTATE_o [ 1 ] = localP -> P_2 ; localDW
-> Memory_PreviousInput [ 1 ] = localP -> P_6 ; localB -> B_12_10_0 [ 1 ] =
localP -> P_1 ; localX -> integrator_CSTATE_o [ 2 ] = localP -> P_2 ; localDW
-> Memory_PreviousInput [ 2 ] = localP -> P_6 ; localB -> B_12_10_0 [ 2 ] =
localP -> P_1 ; } void diansai2026A_22b_TrueRMS_Disable ( SimStruct * S ,
DW_TrueRMS_diansai2026A_22b_T * localDW ) { localDW -> TrueRMS_MODE = false ;
} void diansai2026A_22b_TrueRMS ( SimStruct * S , boolean_T rtu_Enable ,
real_T rtu_In , real_T rtu_In_f , real_T rtu_In_b ,
B_TrueRMS_diansai2026A_22b_T * localB , DW_TrueRMS_diansai2026A_22b_T *
localDW , P_TrueRMS_diansai2026A_22b_T * localP ,
X_TrueRMS_diansai2026A_22b_T * localX , XDis_TrueRMS_diansai2026A_22b_T *
localXdis ) { real_T B_11_0_0_0 ; int32_T isHit ; isHit = ssIsSampleHit ( S ,
1 , 0 ) ; if ( ( isHit != 0 ) && ssIsModeUpdateTimeStep ( S ) ) { if (
rtu_Enable ) { if ( ! localDW -> TrueRMS_MODE ) { if ( ssGetTaskTime ( S , 1
) != ssGetTStart ( S ) ) { ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
} ( void ) memset ( & ( localXdis -> integrator_CSTATE_o ) , 0 , 3 * sizeof (
boolean_T ) ) ; localDW -> TrueRMS_MODE = true ; } } else { if (
ssGetTaskTime ( S , 1 ) == ssGetTStart ( S ) ) { ( void ) memset ( & (
localXdis -> integrator_CSTATE_o ) , 1 , 3 * sizeof ( boolean_T ) ) ; } if (
localDW -> TrueRMS_MODE ) { ssSetBlockStateForSolverChangedAtMajorStep ( S )
; ( void ) memset ( & ( localXdis -> integrator_CSTATE_o ) , 1 , 3 * sizeof (
boolean_T ) ) ; diansai2026A_22b_TrueRMS_Disable ( S , localDW ) ; } } } if (
localDW -> TrueRMS_MODE ) { localB -> B_12_1_0 [ 0 ] = localX ->
integrator_CSTATE_o [ 0 ] ; localB -> B_12_1_0 [ 1 ] = localX ->
integrator_CSTATE_o [ 1 ] ; localB -> B_12_1_0 [ 2 ] = localX ->
integrator_CSTATE_o [ 2 ] ; { real_T * * uBuffer = ( real_T * * ) & localDW
-> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S )
; real_T tMinusDelay ; { int_T i1 ; real_T * y0 = & localB -> B_12_2_0 [ 0 ]
; int_T * iw_Tail = & localDW -> TransportDelay_IWORK . Tail [ 0 ] ; int_T *
iw_Head = & localDW -> TransportDelay_IWORK . Head [ 0 ] ; int_T * iw_Last =
& localDW -> TransportDelay_IWORK . Last [ 0 ] ; int_T * iw_CircularBufSize =
& localDW -> TransportDelay_IWORK . CircularBufSize [ 0 ] ; for ( i1 = 0 ; i1
< 3 ; i1 ++ ) { tMinusDelay = ( ( localP -> P_3 > 0.0 ) ? localP -> P_3 : 0.0
) ; tMinusDelay = simTime - tMinusDelay ; y0 [ i1 ] =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
iw_CircularBufSize [ i1 ] , & iw_Last [ i1 ] , iw_Tail [ i1 ] , iw_Head [ i1
] , localP -> P_4 , 0 , ( boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( *
uBuffer + iw_CircularBufSize [ i1 ] ) [ iw_Head [ i1 ] ] == ssGetT ( S ) ) )
) ; uBuffer ++ ; } } } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { localB -> B_12_3_0 = localP -> P_5 ; localB -> B_12_4_0 [ 0 ] = localDW
-> Memory_PreviousInput [ 0 ] ; localB -> B_12_4_0 [ 1 ] = localDW ->
Memory_PreviousInput [ 1 ] ; localB -> B_12_4_0 [ 2 ] = localDW ->
Memory_PreviousInput [ 2 ] ; } if ( ssGetT ( S ) >= localB -> B_12_3_0 ) {
B_11_0_0_0 = localB -> B_12_1_0 [ 0 ] - localB -> B_12_2_0 [ 0 ] ; localB ->
B_11_0_0 [ 0 ] = B_11_0_0_0 ; B_11_0_0_0 *= localP -> P_0 ; localB ->
B_11_1_0 [ 0 ] = B_11_0_0_0 ; localB -> B_12_7_0 [ 0 ] = B_11_0_0_0 ;
B_11_0_0_0 = localB -> B_12_1_0 [ 1 ] - localB -> B_12_2_0 [ 1 ] ; localB ->
B_11_0_0 [ 1 ] = B_11_0_0_0 ; B_11_0_0_0 *= localP -> P_0 ; localB ->
B_11_1_0 [ 1 ] = B_11_0_0_0 ; localB -> B_12_7_0 [ 1 ] = B_11_0_0_0 ;
B_11_0_0_0 = localB -> B_12_1_0 [ 2 ] - localB -> B_12_2_0 [ 2 ] ; localB ->
B_11_0_0 [ 2 ] = B_11_0_0_0 ; B_11_0_0_0 *= localP -> P_0 ; localB ->
B_11_1_0 [ 2 ] = B_11_0_0_0 ; localB -> B_12_7_0 [ 2 ] = B_11_0_0_0 ; } else
{ localB -> B_12_7_0 [ 0 ] = localB -> B_12_4_0 [ 0 ] ; localB -> B_12_7_0 [
1 ] = localB -> B_12_4_0 [ 1 ] ; localB -> B_12_7_0 [ 2 ] = localB ->
B_12_4_0 [ 2 ] ; } localB -> B_12_8_0 [ 0 ] = rtu_In * rtu_In ; localB ->
B_12_8_0 [ 1 ] = rtu_In_f * rtu_In_f ; localB -> B_12_8_0 [ 2 ] = rtu_In_b *
rtu_In_b ; if ( ssIsModeUpdateTimeStep ( S ) ) { localDW ->
Saturationtoavoidnegativesqrt_MODE [ 0 ] = localB -> B_12_7_0 [ 0 ] >= localP
-> P_7 ? 1 : localB -> B_12_7_0 [ 0 ] > localP -> P_8 ? 0 : - 1 ; localDW ->
Saturationtoavoidnegativesqrt_MODE [ 1 ] = localB -> B_12_7_0 [ 1 ] >= localP
-> P_7 ? 1 : localB -> B_12_7_0 [ 1 ] > localP -> P_8 ? 0 : - 1 ; localDW ->
Saturationtoavoidnegativesqrt_MODE [ 2 ] = localB -> B_12_7_0 [ 2 ] >= localP
-> P_7 ? 1 : localB -> B_12_7_0 [ 2 ] > localP -> P_8 ? 0 : - 1 ; } localB ->
B_12_9_0 [ 0 ] = localDW -> Saturationtoavoidnegativesqrt_MODE [ 0 ] == 1 ?
localP -> P_7 : localDW -> Saturationtoavoidnegativesqrt_MODE [ 0 ] == - 1 ?
localP -> P_8 : localB -> B_12_7_0 [ 0 ] ; localB -> B_12_9_0 [ 1 ] = localDW
-> Saturationtoavoidnegativesqrt_MODE [ 1 ] == 1 ? localP -> P_7 : localDW ->
Saturationtoavoidnegativesqrt_MODE [ 1 ] == - 1 ? localP -> P_8 : localB ->
B_12_7_0 [ 1 ] ; localB -> B_12_9_0 [ 2 ] = localDW ->
Saturationtoavoidnegativesqrt_MODE [ 2 ] == 1 ? localP -> P_7 : localDW ->
Saturationtoavoidnegativesqrt_MODE [ 2 ] == - 1 ? localP -> P_8 : localB ->
B_12_7_0 [ 2 ] ; if ( ssIsMajorTimeStep ( S ) != 0 ) { if ( localDW ->
Sqrt_DWORK1 != 0 ) { ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; localDW ->
Sqrt_DWORK1 = 0 ; } localB -> B_12_10_0 [ 0 ] = muDoubleScalarSqrt ( localB
-> B_12_9_0 [ 0 ] ) ; localB -> B_12_10_0 [ 1 ] = muDoubleScalarSqrt ( localB
-> B_12_9_0 [ 1 ] ) ; localB -> B_12_10_0 [ 2 ] = muDoubleScalarSqrt ( localB
-> B_12_9_0 [ 2 ] ) ; } else { if ( localB -> B_12_9_0 [ 0 ] < 0.0 ) { localB
-> B_12_10_0 [ 0 ] = - muDoubleScalarSqrt ( muDoubleScalarAbs ( localB ->
B_12_9_0 [ 0 ] ) ) ; } else { localB -> B_12_10_0 [ 0 ] = muDoubleScalarSqrt
( localB -> B_12_9_0 [ 0 ] ) ; } if ( localB -> B_12_9_0 [ 0 ] < 0.0 ) {
localDW -> Sqrt_DWORK1 = 1 ; } if ( localB -> B_12_9_0 [ 1 ] < 0.0 ) { localB
-> B_12_10_0 [ 1 ] = - muDoubleScalarSqrt ( muDoubleScalarAbs ( localB ->
B_12_9_0 [ 1 ] ) ) ; } else { localB -> B_12_10_0 [ 1 ] = muDoubleScalarSqrt
( localB -> B_12_9_0 [ 1 ] ) ; } if ( localB -> B_12_9_0 [ 1 ] < 0.0 ) {
localDW -> Sqrt_DWORK1 = 1 ; } if ( localB -> B_12_9_0 [ 2 ] < 0.0 ) { localB
-> B_12_10_0 [ 2 ] = - muDoubleScalarSqrt ( muDoubleScalarAbs ( localB ->
B_12_9_0 [ 2 ] ) ) ; } else { localB -> B_12_10_0 [ 2 ] = muDoubleScalarSqrt
( localB -> B_12_9_0 [ 2 ] ) ; } if ( localB -> B_12_9_0 [ 2 ] < 0.0 ) {
localDW -> Sqrt_DWORK1 = 1 ; } } if ( ssIsModeUpdateTimeStep ( S ) ) {
srUpdateBC ( localDW -> TrueRMS_SubsysRanBC ) ; } } } void
diansai2026A_22b_TrueRMS_Update ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_T * localB , DW_TrueRMS_diansai2026A_22b_T *
localDW , P_TrueRMS_diansai2026A_22b_T * localP ) { int32_T isHit ; if (
localDW -> TrueRMS_MODE ) { { real_T * * uBuffer = ( real_T * * ) & localDW
-> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S )
; localDW -> TransportDelay_IWORK . Head [ 0 ] = ( ( localDW ->
TransportDelay_IWORK . Head [ 0 ] < ( localDW -> TransportDelay_IWORK .
CircularBufSize [ 0 ] - 1 ) ) ? ( localDW -> TransportDelay_IWORK . Head [ 0
] + 1 ) : 0 ) ; if ( localDW -> TransportDelay_IWORK . Head [ 0 ] == localDW
-> TransportDelay_IWORK . Tail [ 0 ] ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK . CircularBufSize [ 0 ] , & localDW ->
TransportDelay_IWORK . Tail [ 0 ] , & localDW -> TransportDelay_IWORK . Head
[ 0 ] , & localDW -> TransportDelay_IWORK . Last [ 0 ] , simTime - localP ->
P_3 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 0 ] ) [ localDW -> TransportDelay_IWORK . Head [ 0 ] ] =
simTime ; ( * uBuffer ++ ) [ localDW -> TransportDelay_IWORK . Head [ 0 ] ] =
localB -> B_12_1_0 [ 0 ] ; localDW -> TransportDelay_IWORK . Head [ 1 ] = ( (
localDW -> TransportDelay_IWORK . Head [ 1 ] < ( localDW ->
TransportDelay_IWORK . CircularBufSize [ 1 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK . Head [ 1 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK . Head [ 1 ] == localDW -> TransportDelay_IWORK . Tail [
1 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW
-> TransportDelay_IWORK . CircularBufSize [ 1 ] , & localDW ->
TransportDelay_IWORK . Tail [ 1 ] , & localDW -> TransportDelay_IWORK . Head
[ 1 ] , & localDW -> TransportDelay_IWORK . Last [ 1 ] , simTime - localP ->
P_3 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 1 ] ) [ localDW -> TransportDelay_IWORK . Head [ 1 ] ] =
simTime ; ( * uBuffer ++ ) [ localDW -> TransportDelay_IWORK . Head [ 1 ] ] =
localB -> B_12_1_0 [ 1 ] ; localDW -> TransportDelay_IWORK . Head [ 2 ] = ( (
localDW -> TransportDelay_IWORK . Head [ 2 ] < ( localDW ->
TransportDelay_IWORK . CircularBufSize [ 2 ] - 1 ) ) ? ( localDW ->
TransportDelay_IWORK . Head [ 2 ] + 1 ) : 0 ) ; if ( localDW ->
TransportDelay_IWORK . Head [ 2 ] == localDW -> TransportDelay_IWORK . Tail [
2 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW
-> TransportDelay_IWORK . CircularBufSize [ 2 ] , & localDW ->
TransportDelay_IWORK . Tail [ 2 ] , & localDW -> TransportDelay_IWORK . Head
[ 2 ] , & localDW -> TransportDelay_IWORK . Last [ 2 ] , simTime - localP ->
P_3 , uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize [ 2 ] ) [ localDW -> TransportDelay_IWORK . Head [ 2 ] ] =
simTime ; ( * uBuffer ) [ localDW -> TransportDelay_IWORK . Head [ 2 ] ] =
localB -> B_12_1_0 [ 2 ] ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit
!= 0 ) { localDW -> Memory_PreviousInput [ 0 ] = localB -> B_12_7_0 [ 0 ] ;
localDW -> Memory_PreviousInput [ 1 ] = localB -> B_12_7_0 [ 1 ] ; localDW ->
Memory_PreviousInput [ 2 ] = localB -> B_12_7_0 [ 2 ] ; } } } void
diansai2026A_22b_TrueRMS_Deriv ( SimStruct * S , B_TrueRMS_diansai2026A_22b_T
* localB , DW_TrueRMS_diansai2026A_22b_T * localDW ,
XDot_TrueRMS_diansai2026A_22b_T * localXdot ) { if ( localDW -> TrueRMS_MODE
) { localXdot -> integrator_CSTATE_o [ 0 ] = localB -> B_12_8_0 [ 0 ] ;
localXdot -> integrator_CSTATE_o [ 1 ] = localB -> B_12_8_0 [ 1 ] ; localXdot
-> integrator_CSTATE_o [ 2 ] = localB -> B_12_8_0 [ 2 ] ; } else { { real_T *
dx ; int_T i ; dx = & ( localXdot -> integrator_CSTATE_o [ 0 ] ) ; for ( i =
0 ; i < 3 ; i ++ ) { dx [ i ] = 0.0 ; } } } } void
diansai2026A_22b_TrueRMS_ZC ( SimStruct * S , B_TrueRMS_diansai2026A_22b_T *
localB , DW_TrueRMS_diansai2026A_22b_T * localDW ,
P_TrueRMS_diansai2026A_22b_T * localP , ZCV_TrueRMS_diansai2026A_22b_T *
localZCSV ) { if ( localDW -> TrueRMS_MODE ) { localZCSV ->
Saturationtoavoidnegativesqrt_UprLim_ZC_p [ 0 ] = localB -> B_12_7_0 [ 0 ] -
localP -> P_7 ; localZCSV -> Saturationtoavoidnegativesqrt_LwrLim_ZC_b [ 0 ]
= localB -> B_12_7_0 [ 0 ] - localP -> P_8 ; localZCSV ->
Saturationtoavoidnegativesqrt_UprLim_ZC_p [ 1 ] = localB -> B_12_7_0 [ 1 ] -
localP -> P_7 ; localZCSV -> Saturationtoavoidnegativesqrt_LwrLim_ZC_b [ 1 ]
= localB -> B_12_7_0 [ 1 ] - localP -> P_8 ; localZCSV ->
Saturationtoavoidnegativesqrt_UprLim_ZC_p [ 2 ] = localB -> B_12_7_0 [ 2 ] -
localP -> P_7 ; localZCSV -> Saturationtoavoidnegativesqrt_LwrLim_ZC_b [ 2 ]
= localB -> B_12_7_0 [ 2 ] - localP -> P_8 ; } else { { real_T * zcsv = & (
localZCSV -> Saturationtoavoidnegativesqrt_UprLim_ZC_p [ 0 ] ) ; int_T i ;
for ( i = 0 ; i < 6 ; i ++ ) { zcsv [ i ] = 0.0 ; } } } } void
diansai2026A_22b_TrueRMS_Term ( SimStruct * const S ) { } void
diansai2026A_22b_RMS_d_Init ( SimStruct * S , B_RMS_diansai2026A_22b_a_T *
localB , DW_RMS_diansai2026A_22b_e_T * localDW , P_RMS_diansai2026A_22b_a_T *
localP , X_RMS_diansai2026A_22b_a_T * localX ) { localX ->
integrator_CSTATE_l = localP -> P_3 ; localDW -> Memory_PreviousInput =
localP -> P_7 ; localX -> integrator_CSTATE_cu = localP -> P_8 ; localDW ->
Memory_PreviousInput_a = localP -> P_12 ; localB -> B_32_22_0 = localP -> P_2
; } void diansai2026A_22b_RMS_m_Disable ( SimStruct * S ,
DW_RMS_diansai2026A_22b_e_T * localDW ) { localDW -> RMS_MODE = false ; }
void diansai2026A_22b_RMS_l ( SimStruct * S , boolean_T rtu_Enable , real_T
rtu_In , B_RMS_diansai2026A_22b_a_T * localB , DW_RMS_diansai2026A_22b_e_T *
localDW , P_RMS_diansai2026A_22b_a_T * localP , X_RMS_diansai2026A_22b_a_T *
localX , XDis_RMS_diansai2026A_22b_j_T * localXdis ) { int32_T isHit ;
boolean_T rtb_B_32_4_0 ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( ( isHit
!= 0 ) && ssIsModeUpdateTimeStep ( S ) ) { if ( rtu_Enable ) { if ( ! localDW
-> RMS_MODE ) { if ( ssGetTaskTime ( S , 1 ) != ssGetTStart ( S ) ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; } ( void ) memset ( & (
localXdis -> integrator_CSTATE_l ) , 0 , 2 * sizeof ( boolean_T ) ) ; localDW
-> RMS_MODE = true ; } } else { if ( ssGetTaskTime ( S , 1 ) == ssGetTStart (
S ) ) { ( void ) memset ( & ( localXdis -> integrator_CSTATE_l ) , 1 , 2 *
sizeof ( boolean_T ) ) ; } if ( localDW -> RMS_MODE ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; ( void ) memset ( & (
localXdis -> integrator_CSTATE_l ) , 1 , 2 * sizeof ( boolean_T ) ) ;
diansai2026A_22b_RMS_m_Disable ( S , localDW ) ; } } } if ( localDW ->
RMS_MODE ) { localB -> B_32_0_0 = localX -> integrator_CSTATE_l ; { real_T *
* uBuffer = ( real_T * * ) & localDW -> TransportDelay_PWORK . TUbufferPtrs [
0 ] ; real_T simTime = ssGetT ( S ) ; real_T tMinusDelay = simTime - localP
-> P_4 ; localB -> B_32_1_0 = diansai2026A_22b_acc_rt_TDelayInterpolate (
tMinusDelay , 0.0 , * uBuffer , localDW -> TransportDelay_IWORK .
CircularBufSize , & localDW -> TransportDelay_IWORK . Last , localDW ->
TransportDelay_IWORK . Tail , localDW -> TransportDelay_IWORK . Head , localP
-> P_5 , 0 , ( boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( * uBuffer +
localDW -> TransportDelay_IWORK . CircularBufSize ) [ localDW ->
TransportDelay_IWORK . Head ] == ssGetT ( S ) ) ) ) ; } isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { localB -> B_32_3_0 = localP -> P_6 ; }
rtb_B_32_4_0 = ( ssGetT ( S ) >= localB -> B_32_3_0 ) ; isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { localB -> B_32_5_0 = localDW ->
Memory_PreviousInput ; } if ( rtb_B_32_4_0 ) { localB -> B_31_0_0 = localB ->
B_32_0_0 - localB -> B_32_1_0 ; localB -> B_31_1_0 = localP -> P_1 * localB
-> B_31_0_0 ; localB -> B_32_7_0 = localB -> B_31_1_0 ; } else { localB ->
B_32_7_0 = localB -> B_32_5_0 ; } localB -> B_32_8_0 = localX ->
integrator_CSTATE_cu ; { real_T * * uBuffer = ( real_T * * ) & localDW ->
TransportDelay_PWORK_m . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ;
real_T tMinusDelay = simTime - localP -> P_9 ; localB -> B_32_9_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
localDW -> TransportDelay_IWORK_d . CircularBufSize , & localDW ->
TransportDelay_IWORK_d . Last , localDW -> TransportDelay_IWORK_d . Tail ,
localDW -> TransportDelay_IWORK_d . Head , localP -> P_10 , 0 , ( boolean_T )
( ssIsMinorTimeStep ( S ) && ( ( * uBuffer + localDW ->
TransportDelay_IWORK_d . CircularBufSize ) [ localDW ->
TransportDelay_IWORK_d . Head ] == ssGetT ( S ) ) ) ) ; } isHit =
ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { localB -> B_32_11_0 =
localP -> P_11 ; } rtb_B_32_4_0 = ( ssGetT ( S ) >= localB -> B_32_11_0 ) ;
isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { localB -> B_32_13_0
= localDW -> Memory_PreviousInput_a ; } if ( rtb_B_32_4_0 ) { localB ->
B_30_0_0 = localB -> B_32_8_0 - localB -> B_32_9_0 ; localB -> B_30_1_0 =
localP -> P_0 * localB -> B_30_0_0 ; localB -> B_32_15_0 = localB -> B_30_1_0
; } else { localB -> B_32_15_0 = localB -> B_32_13_0 ; } localB -> B_32_16_0
. re = localB -> B_32_7_0 ; localB -> B_32_16_0 . im = localB -> B_32_15_0 ;
localB -> B_32_19_0 = ( muDoubleScalarSin ( localP -> P_15 * ssGetTaskTime (
S , 0 ) + localP -> P_16 ) * localP -> P_13 + localP -> P_14 ) * rtu_In ;
localB -> B_32_21_0 = ( muDoubleScalarSin ( localP -> P_19 * ssGetTaskTime (
S , 0 ) + localP -> P_20 ) * localP -> P_17 + localP -> P_18 ) * rtu_In ;
localB -> B_32_22_0 = localP -> P_21 * muDoubleScalarHypot ( localB ->
B_32_16_0 . re , localB -> B_32_16_0 . im ) ; if ( ssIsModeUpdateTimeStep ( S
) ) { srUpdateBC ( localDW -> RMS_SubsysRanBC ) ; } } } void
diansai2026A_22b_RMS_k_Update ( SimStruct * S , B_RMS_diansai2026A_22b_a_T *
localB , DW_RMS_diansai2026A_22b_e_T * localDW , P_RMS_diansai2026A_22b_a_T *
localP ) { int32_T isHit ; if ( localDW -> RMS_MODE ) { { real_T * * uBuffer
= ( real_T * * ) & localDW -> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ;
real_T simTime = ssGetT ( S ) ; localDW -> TransportDelay_IWORK . Head = ( (
localDW -> TransportDelay_IWORK . Head < ( localDW -> TransportDelay_IWORK .
CircularBufSize - 1 ) ) ? ( localDW -> TransportDelay_IWORK . Head + 1 ) : 0
) ; if ( localDW -> TransportDelay_IWORK . Head == localDW ->
TransportDelay_IWORK . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK . CircularBufSize , & localDW -> TransportDelay_IWORK .
Tail , & localDW -> TransportDelay_IWORK . Head , & localDW ->
TransportDelay_IWORK . Last , simTime - localP -> P_4 , uBuffer , ( boolean_T
) 0 , false , & localDW -> TransportDelay_IWORK . MaxNewBufSize ) ) {
ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } ( *
uBuffer + localDW -> TransportDelay_IWORK . CircularBufSize ) [ localDW ->
TransportDelay_IWORK . Head ] = simTime ; ( * uBuffer ) [ localDW ->
TransportDelay_IWORK . Head ] = localB -> B_32_0_0 ; } isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { localDW -> Memory_PreviousInput = localB
-> B_32_7_0 ; } { real_T * * uBuffer = ( real_T * * ) & localDW ->
TransportDelay_PWORK_m . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ;
localDW -> TransportDelay_IWORK_d . Head = ( ( localDW ->
TransportDelay_IWORK_d . Head < ( localDW -> TransportDelay_IWORK_d .
CircularBufSize - 1 ) ) ? ( localDW -> TransportDelay_IWORK_d . Head + 1 ) :
0 ) ; if ( localDW -> TransportDelay_IWORK_d . Head == localDW ->
TransportDelay_IWORK_d . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK_d . CircularBufSize , & localDW ->
TransportDelay_IWORK_d . Tail , & localDW -> TransportDelay_IWORK_d . Head ,
& localDW -> TransportDelay_IWORK_d . Last , simTime - localP -> P_9 ,
uBuffer , ( boolean_T ) 0 , false , & localDW -> TransportDelay_IWORK_d .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + localDW -> TransportDelay_IWORK_d .
CircularBufSize ) [ localDW -> TransportDelay_IWORK_d . Head ] = simTime ; (
* uBuffer ) [ localDW -> TransportDelay_IWORK_d . Head ] = localB -> B_32_8_0
; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { localDW ->
Memory_PreviousInput_a = localB -> B_32_15_0 ; } } } void
diansai2026A_22b_RMS_b_Deriv ( SimStruct * S , B_RMS_diansai2026A_22b_a_T *
localB , DW_RMS_diansai2026A_22b_e_T * localDW ,
XDot_RMS_diansai2026A_22b_e_T * localXdot ) { if ( localDW -> RMS_MODE ) {
localXdot -> integrator_CSTATE_l = localB -> B_32_19_0 ; localXdot ->
integrator_CSTATE_cu = localB -> B_32_21_0 ; } else { { real_T * dx ; int_T i
; dx = & ( localXdot -> integrator_CSTATE_l ) ; for ( i = 0 ; i < 2 ; i ++ )
{ dx [ i ] = 0.0 ; } } } } void diansai2026A_22b_RMS_c_Term ( SimStruct *
const S ) { } void diansai2026A_22b_TrueRMS_o_Init ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ,
X_TrueRMS_diansai2026A_22b_h_T * localX ) { localX -> integrator_CSTATE_ic =
localP -> P_2 ; localDW -> Memory_PreviousInput = localP -> P_6 ; localB ->
B_34_10_0 = localP -> P_1 ; } void diansai2026A_22b_TrueRMS_j_Disable (
SimStruct * S , DW_TrueRMS_diansai2026A_22b_c_T * localDW ) { localDW ->
TrueRMS_MODE = false ; } void diansai2026A_22b_TrueRMS_f ( SimStruct * S ,
boolean_T rtu_Enable , real_T rtu_In , B_TrueRMS_diansai2026A_22b_d_T *
localB , DW_TrueRMS_diansai2026A_22b_c_T * localDW ,
P_TrueRMS_diansai2026A_22b_d_T * localP , X_TrueRMS_diansai2026A_22b_h_T *
localX , XDis_TrueRMS_diansai2026A_22b_e_T * localXdis ) { int32_T isHit ;
isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( ( isHit != 0 ) &&
ssIsModeUpdateTimeStep ( S ) ) { if ( rtu_Enable ) { if ( ! localDW ->
TrueRMS_MODE ) { if ( ssGetTaskTime ( S , 1 ) != ssGetTStart ( S ) ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; } localXdis ->
integrator_CSTATE_ic = 0 ; localDW -> TrueRMS_MODE = true ; } } else { if (
ssGetTaskTime ( S , 1 ) == ssGetTStart ( S ) ) { localXdis ->
integrator_CSTATE_ic = 1 ; } if ( localDW -> TrueRMS_MODE ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ; localXdis ->
integrator_CSTATE_ic = 1 ; diansai2026A_22b_TrueRMS_j_Disable ( S , localDW )
; } } } if ( localDW -> TrueRMS_MODE ) { localB -> B_34_1_0 = localX ->
integrator_CSTATE_ic ; { real_T * * uBuffer = ( real_T * * ) & localDW ->
TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ;
real_T tMinusDelay = simTime - localP -> P_3 ; localB -> B_34_2_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
localDW -> TransportDelay_IWORK . CircularBufSize , & localDW ->
TransportDelay_IWORK . Last , localDW -> TransportDelay_IWORK . Tail ,
localDW -> TransportDelay_IWORK . Head , localP -> P_4 , 0 , ( boolean_T ) (
ssIsMinorTimeStep ( S ) && ( ( * uBuffer + localDW -> TransportDelay_IWORK .
CircularBufSize ) [ localDW -> TransportDelay_IWORK . Head ] == ssGetT ( S )
) ) ) ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { localB ->
B_34_3_0 = localP -> P_5 ; localB -> B_34_4_0 = localDW ->
Memory_PreviousInput ; } if ( ssGetT ( S ) >= localB -> B_34_3_0 ) { localB
-> B_33_0_0 = localB -> B_34_1_0 - localB -> B_34_2_0 ; localB -> B_33_1_0 =
localP -> P_0 * localB -> B_33_0_0 ; localB -> B_34_7_0 = localB -> B_33_1_0
; } else { localB -> B_34_7_0 = localB -> B_34_4_0 ; } localB -> B_34_8_0 =
rtu_In * rtu_In ; if ( ssIsModeUpdateTimeStep ( S ) ) { localDW ->
Saturationtoavoidnegativesqrt_MODE = localB -> B_34_7_0 >= localP -> P_7 ? 1
: localB -> B_34_7_0 > localP -> P_8 ? 0 : - 1 ; } localB -> B_34_9_0 =
localDW -> Saturationtoavoidnegativesqrt_MODE == 1 ? localP -> P_7 : localDW
-> Saturationtoavoidnegativesqrt_MODE == - 1 ? localP -> P_8 : localB ->
B_34_7_0 ; if ( ssIsMajorTimeStep ( S ) != 0 ) { if ( localDW -> Sqrt_DWORK1
!= 0 ) { ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; localDW ->
Sqrt_DWORK1 = 0 ; } localB -> B_34_10_0 = muDoubleScalarSqrt ( localB ->
B_34_9_0 ) ; } else { if ( localB -> B_34_9_0 < 0.0 ) { localB -> B_34_10_0 =
- muDoubleScalarSqrt ( muDoubleScalarAbs ( localB -> B_34_9_0 ) ) ; } else {
localB -> B_34_10_0 = muDoubleScalarSqrt ( localB -> B_34_9_0 ) ; } if (
localB -> B_34_9_0 < 0.0 ) { localDW -> Sqrt_DWORK1 = 1 ; } } if (
ssIsModeUpdateTimeStep ( S ) ) { srUpdateBC ( localDW -> TrueRMS_SubsysRanBC
) ; } } } void diansai2026A_22b_TrueRMS_g_Update ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ) { int32_T isHit ; if (
localDW -> TrueRMS_MODE ) { { real_T * * uBuffer = ( real_T * * ) & localDW
-> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S )
; localDW -> TransportDelay_IWORK . Head = ( ( localDW ->
TransportDelay_IWORK . Head < ( localDW -> TransportDelay_IWORK .
CircularBufSize - 1 ) ) ? ( localDW -> TransportDelay_IWORK . Head + 1 ) : 0
) ; if ( localDW -> TransportDelay_IWORK . Head == localDW ->
TransportDelay_IWORK . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & localDW ->
TransportDelay_IWORK . CircularBufSize , & localDW -> TransportDelay_IWORK .
Tail , & localDW -> TransportDelay_IWORK . Head , & localDW ->
TransportDelay_IWORK . Last , simTime - localP -> P_3 , uBuffer , ( boolean_T
) 0 , false , & localDW -> TransportDelay_IWORK . MaxNewBufSize ) ) {
ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } ( *
uBuffer + localDW -> TransportDelay_IWORK . CircularBufSize ) [ localDW ->
TransportDelay_IWORK . Head ] = simTime ; ( * uBuffer ) [ localDW ->
TransportDelay_IWORK . Head ] = localB -> B_34_1_0 ; } isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { localDW -> Memory_PreviousInput = localB
-> B_34_7_0 ; } } } void diansai2026A_22b_TrueRMS_d_Deriv ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , XDot_TrueRMS_diansai2026A_22b_m_T * localXdot ) { if ( localDW ->
TrueRMS_MODE ) { localXdot -> integrator_CSTATE_ic = localB -> B_34_8_0 ; }
else { localXdot -> integrator_CSTATE_ic = 0.0 ; } } void
diansai2026A_22b_TrueRMS_e_ZC ( SimStruct * S ,
B_TrueRMS_diansai2026A_22b_d_T * localB , DW_TrueRMS_diansai2026A_22b_c_T *
localDW , P_TrueRMS_diansai2026A_22b_d_T * localP ,
ZCV_TrueRMS_diansai2026A_22b_n_T * localZCSV ) { if ( localDW -> TrueRMS_MODE
) { localZCSV -> Saturationtoavoidnegativesqrt_UprLim_ZC = localB -> B_34_7_0
- localP -> P_7 ; localZCSV -> Saturationtoavoidnegativesqrt_LwrLim_ZC =
localB -> B_34_7_0 - localP -> P_8 ; } else { { real_T * zcsv = & ( localZCSV
-> Saturationtoavoidnegativesqrt_UprLim_ZC ) ; int_T i ; for ( i = 0 ; i < 2
; i ++ ) { zcsv [ i ] = 0.0 ; } } } } void diansai2026A_22b_TrueRMS_e_Term (
SimStruct * const S ) { } static void mdlOutputs ( SimStruct * S , int_T tid
) { real_T B_40_12_0 ; real_T B_40_16_0 ; real_T B_40_22_0 ; real_T B_40_26_0
; real_T B_40_150_0 ; real_T B_40_175_0 [ 3 ] ; real_T B_40_196_0 ;
B_diansai2026A_22b_T * _rtB ; DW_diansai2026A_22b_T * _rtDW ;
P_diansai2026A_22b_T * _rtP ; X_diansai2026A_22b_T * _rtX ; real_T tmp [ 3 ]
; real_T B_40_172_0_0 ; real_T B_40_173_0_idx_0 ; real_T B_40_182_0_idx_0 ;
real_T rtb_B_40_13_0 ; real_T rtb_B_40_17_0 ; real_T rtb_B_40_19_0 ; real_T
rtb_B_40_20_0 ; real_T rtb_B_40_27_0 ; real_T rtb_B_40_29_0 ; real_T
rtb_B_40_84_0 ; int32_T isHit ; boolean_T rtb_B_40_91_0 ; boolean_T
rtb_B_40_92_0 ; _rtDW = ( ( DW_diansai2026A_22b_T * ) ssGetRootDWork ( S ) )
; _rtX = ( ( X_diansai2026A_22b_T * ) ssGetContStates ( S ) ) ; _rtP = ( (
P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) ; _rtB = ( (
B_diansai2026A_22b_T * ) _ssGetModelBlockIO ( S ) ) ; _rtB -> B_40_0_0 =
muDoubleScalarSin ( _rtP -> P_6 * ssGetTaskTime ( S , 0 ) + _rtP -> P_7 ) *
_rtP -> P_4 + _rtP -> P_5 ; isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit
!= 0 ) { ssCallAccelRunBlock ( S , 40 , 1 , SS_CALL_MDL_OUTPUTS ) ; _rtB ->
B_40_2_0 = _rtP -> P_18 * _rtB -> B_40_1_0 [ 15 ] ; _rtB -> B_40_3_0 = _rtP
-> P_19 * _rtB -> B_40_1_0 [ 16 ] ; _rtB -> B_40_4_0 = ( 0.0 - _rtB ->
B_40_2_0 ) - _rtB -> B_40_3_0 ; } diansai2026A_22b_TrueRMS ( S , _rtB ->
B_40_19_0 , _rtB -> B_40_2_0 , _rtB -> B_40_4_0 , _rtB -> B_40_3_0 , & _rtB
-> TrueRMS_g , & _rtDW -> TrueRMS_g , & _rtP -> TrueRMS_g , & _rtX ->
TrueRMS_g , & ( ( XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S ) )
-> TrueRMS_g ) ; diansai2026A_22b_RMS ( S , _rtB -> B_40_20_0 , _rtB ->
B_40_2_0 , _rtB -> B_40_4_0 , _rtB -> B_40_3_0 , & _rtB -> RMS_n , & _rtDW ->
RMS_n , & _rtP -> RMS_n , & _rtX -> RMS_n , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> RMS_n ) ; if ( _rtB -> B_40_19_0 ) { _rtB
-> B_40_7_0_b [ 0 ] = _rtB -> TrueRMS_g . B_12_10_0 [ 0 ] ; _rtB ->
B_40_7_0_b [ 1 ] = _rtB -> TrueRMS_g . B_12_10_0 [ 1 ] ; _rtB -> B_40_7_0_b [
2 ] = _rtB -> TrueRMS_g . B_12_10_0 [ 2 ] ; } else { _rtB -> B_40_7_0_b [ 0 ]
= _rtB -> RMS_n . B_10_22_0 [ 0 ] ; _rtB -> B_40_7_0_b [ 1 ] = _rtB -> RMS_n
. B_10_22_0 [ 1 ] ; _rtB -> B_40_7_0_b [ 2 ] = _rtB -> RMS_n . B_10_22_0 [ 2
] ; } ssCallAccelRunBlock ( S , 40 , 8 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 9 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 10 , SS_CALL_MDL_OUTPUTS ) ; _rtB -> B_40_11_0
= _rtX -> integ1_CSTATE ; { real_T * * uBuffer = ( real_T * * ) & _rtDW ->
T_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ; real_T
tMinusDelay = simTime - _rtP -> P_21 ; B_40_12_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
_rtDW -> T_IWORK . CircularBufSize , & _rtDW -> T_IWORK . Last , _rtDW ->
T_IWORK . Tail , _rtDW -> T_IWORK . Head , _rtP -> P_22 , 0 , ( boolean_T ) (
ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> T_IWORK . CircularBufSize
) [ _rtDW -> T_IWORK . Head ] == ssGetT ( S ) ) ) ) ; } rtb_B_40_13_0 = _rtB
-> B_40_11_0 - B_40_12_0 ; _rtB -> B_40_15_0 = _rtX -> Integ2_CSTATE ; {
real_T * * uBuffer = ( real_T * * ) & _rtDW -> T1_PWORK . TUbufferPtrs [ 0 ]
; real_T simTime = ssGetT ( S ) ; real_T tMinusDelay = simTime - _rtP -> P_24
; B_40_16_0 = diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 ,
* uBuffer , _rtDW -> T1_IWORK . CircularBufSize , & _rtDW -> T1_IWORK . Last
, _rtDW -> T1_IWORK . Tail , _rtDW -> T1_IWORK . Head , _rtP -> P_25 , 0 , (
boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> T1_IWORK .
CircularBufSize ) [ _rtDW -> T1_IWORK . Head ] == ssGetT ( S ) ) ) ) ; }
rtb_B_40_17_0 = _rtB -> B_40_15_0 - B_40_16_0 ; rtb_B_40_19_0 = rtb_B_40_13_0
* rtb_B_40_13_0 + rtb_B_40_17_0 * rtb_B_40_17_0 ; if ( ssIsMajorTimeStep ( S
) != 0 ) { if ( _rtDW -> Sqrt_DWORK1 != 0 ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; _rtDW ->
Sqrt_DWORK1 = 0 ; } rtb_B_40_20_0 = muDoubleScalarSqrt ( rtb_B_40_19_0 ) ; }
else if ( rtb_B_40_19_0 < 0.0 ) { rtb_B_40_20_0 = - muDoubleScalarSqrt (
muDoubleScalarAbs ( rtb_B_40_19_0 ) ) ; _rtDW -> Sqrt_DWORK1 = 1 ; } else {
rtb_B_40_20_0 = muDoubleScalarSqrt ( rtb_B_40_19_0 ) ; } _rtB -> B_40_21_0 =
_rtX -> integ1_CSTATE_h ; { real_T * * uBuffer = ( real_T * * ) & _rtDW ->
T_PWORK_e . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ; real_T
tMinusDelay = simTime - _rtP -> P_27 ; B_40_22_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
_rtDW -> T_IWORK_d . CircularBufSize , & _rtDW -> T_IWORK_d . Last , _rtDW ->
T_IWORK_d . Tail , _rtDW -> T_IWORK_d . Head , _rtP -> P_28 , 0 , ( boolean_T
) ( ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> T_IWORK_d .
CircularBufSize ) [ _rtDW -> T_IWORK_d . Head ] == ssGetT ( S ) ) ) ) ; }
rtb_B_40_19_0 = _rtB -> B_40_21_0 - B_40_22_0 ; _rtB -> B_40_25_0 = _rtX ->
Integ2_CSTATE_m ; { real_T * * uBuffer = ( real_T * * ) & _rtDW -> T1_PWORK_h
. TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ; real_T tMinusDelay =
simTime - _rtP -> P_30 ; B_40_26_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
_rtDW -> T1_IWORK_g . CircularBufSize , & _rtDW -> T1_IWORK_g . Last , _rtDW
-> T1_IWORK_g . Tail , _rtDW -> T1_IWORK_g . Head , _rtP -> P_31 , 0 , (
boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> T1_IWORK_g
. CircularBufSize ) [ _rtDW -> T1_IWORK_g . Head ] == ssGetT ( S ) ) ) ) ; }
rtb_B_40_27_0 = _rtB -> B_40_25_0 - B_40_26_0 ; rtb_B_40_29_0 = rtb_B_40_19_0
* rtb_B_40_19_0 + rtb_B_40_27_0 * rtb_B_40_27_0 ; if ( ssIsMajorTimeStep ( S
) != 0 ) { if ( _rtDW -> Sqrt_DWORK1_i != 0 ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; _rtDW ->
Sqrt_DWORK1_i = 0 ; } rtb_B_40_29_0 = muDoubleScalarSqrt ( rtb_B_40_29_0 ) ;
} else if ( rtb_B_40_29_0 < 0.0 ) { rtb_B_40_29_0 = - muDoubleScalarSqrt (
muDoubleScalarAbs ( rtb_B_40_29_0 ) ) ; _rtDW -> Sqrt_DWORK1_i = 1 ; } else {
rtb_B_40_29_0 = muDoubleScalarSqrt ( rtb_B_40_29_0 ) ; } rtb_B_40_20_0 *=
rtb_B_40_29_0 ; rtb_B_40_13_0 = ( _rtP -> P_32 * muDoubleScalarAtan2 (
rtb_B_40_17_0 , rtb_B_40_13_0 ) - _rtP -> P_33 * muDoubleScalarAtan2 (
rtb_B_40_27_0 , rtb_B_40_19_0 ) ) * _rtP -> P_34 ; _rtB -> B_40_42_0 [ 0 ] =
rtb_B_40_20_0 * muDoubleScalarCos ( rtb_B_40_13_0 ) * _rtP -> P_35 ; _rtB ->
B_40_42_0 [ 1 ] = rtb_B_40_20_0 * muDoubleScalarSin ( rtb_B_40_13_0 ) * _rtP
-> P_35 ; ssCallAccelRunBlock ( S , 40 , 43 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 44 , SS_CALL_MDL_OUTPUTS ) ; isHit =
ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 ) { _rtB -> B_40_45_0 = _rtP ->
P_36 * _rtB -> B_40_1_0 [ 11 ] ; } _rtB -> B_40_46_0 = 0.0 ; _rtB ->
B_40_46_0 += _rtP -> P_38 [ 0 ] * _rtX -> TransferFcn_CSTATE [ 0 ] ; _rtB ->
B_40_46_0 += _rtP -> P_38 [ 1 ] * _rtX -> TransferFcn_CSTATE [ 1 ] ; _rtB ->
B_40_46_0 += _rtP -> P_39 * _rtB -> B_40_45_0 ; _rtB -> B_40_47_0 = _rtB ->
B_40_4_0_k - _rtB -> B_40_46_0 ; _rtB -> B_40_49_0 = _rtX ->
Integrator_CSTATE ; _rtB -> B_40_50_0 = _rtP -> P_40 * _rtB -> B_40_47_0 +
_rtB -> B_40_49_0 ; isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 ) {
_rtB -> B_40_51_0 = _rtP -> P_42 * _rtB -> B_40_1_0 [ 10 ] ; _rtB ->
B_40_52_0 = _rtP -> P_43 * _rtB -> B_40_51_0 ; } isHit = ssIsSampleHit ( S ,
2 , 0 ) ; if ( isHit != 0 ) { _rtB -> B_40_54_0 = _rtP -> P_44 * _rtB ->
B_40_1_0 [ 14 ] ; } _rtB -> B_40_55_0 = _rtB -> B_40_50_0 * _rtB -> B_40_52_0
- _rtB -> B_40_54_0 ; isHit = ssIsSampleHit ( S , 3 , 0 ) ; if ( isHit != 0 )
{ ssCallAccelRunBlock ( S , 4 , 0 , SS_CALL_MDL_OUTPUTS ) ; if ( _rtB ->
B_4_0_1 > _rtP -> P_45 ) { _rtB -> B_40_57_0 = _rtP -> P_45 ; } else if (
_rtB -> B_4_0_1 < _rtP -> P_46 ) { _rtB -> B_40_57_0 = _rtP -> P_46 ; } else
{ _rtB -> B_40_57_0 = _rtB -> B_4_0_1 ; } } ssCallAccelRunBlock ( S , 40 , 58
, SS_CALL_MDL_OUTPUTS ) ; ssCallAccelRunBlock ( S , 40 , 59 ,
SS_CALL_MDL_OUTPUTS ) ; _rtB -> B_40_60_0 = _rtX -> Integrator_CSTATE_p ;
_rtB -> B_40_62_0 = _rtB -> B_40_60_0 - _rtB -> B_40_12_0 ; _rtB -> B_40_64_0
[ 0 ] = muDoubleScalarSin ( _rtB -> B_40_60_0 ) * _rtB -> B_40_14_0 ; _rtB ->
B_40_64_0 [ 1 ] = muDoubleScalarSin ( _rtB -> B_40_62_0 ) * _rtB -> B_40_14_0
; isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 ) { rtb_B_40_13_0 =
_rtP -> P_50 * _rtB -> B_40_1_0 [ 12 ] ; rtb_B_40_17_0 = _rtP -> P_52 * _rtB
-> B_40_1_0 [ 13 ] ; _rtB -> B_40_71_0 [ 0 ] = ( _rtP -> P_51 * rtb_B_40_13_0
+ rtb_B_40_17_0 ) * _rtP -> P_54 ; _rtB -> B_40_71_0 [ 1 ] = ( ( 0.0 - _rtP
-> P_53 * rtb_B_40_17_0 ) - rtb_B_40_13_0 ) * _rtP -> P_54 ; } _rtB ->
B_40_72_0 [ 0 ] = _rtB -> B_40_64_0 [ 0 ] - _rtB -> B_40_71_0 [ 0 ] ; _rtB ->
B_40_72_0 [ 1 ] = _rtB -> B_40_64_0 [ 1 ] - _rtB -> B_40_71_0 [ 1 ] ;
diansai2026A_22b_PR_j ( S , _rtB -> B_40_72_0 [ 0 ] , & _rtB -> PR_j , &
_rtDW -> PR_j , & _rtP -> PR_j , & _rtX -> PR_j ) ; diansai2026A_22b_PR_j ( S
, _rtB -> B_40_72_0 [ 1 ] , & _rtB -> PR1_o , & _rtDW -> PR1_o , & _rtP ->
PR1_o , & _rtX -> PR1_o ) ; _rtB -> B_40_75_0 [ 0 ] = _rtB -> PR_j . B_2_4_0
- _rtB -> B_40_2_0 ; _rtB -> B_40_75_0 [ 1 ] = _rtB -> PR1_o . B_2_4_0 - _rtB
-> B_40_3_0 ; isHit = ssIsSampleHit ( S , 3 , 0 ) ; if ( isHit != 0 ) { _rtB
-> B_40_76_0 [ 0 ] = _rtB -> B_40_75_0 [ 0 ] ; _rtB -> B_40_76_0 [ 1 ] = _rtB
-> B_40_75_0 [ 1 ] ; } diansai2026A_22b_PR ( S , _rtB -> B_40_76_0 [ 0 ] , &
_rtB -> PR , & _rtDW -> PR , & _rtP -> PR , & _rtX -> PR ) ;
diansai2026A_22b_PR ( S , _rtB -> B_40_76_0 [ 1 ] , & _rtB -> PR1 , & _rtDW
-> PR1 , & _rtP -> PR1 , & _rtX -> PR1 ) ; ssCallAccelRunBlock ( S , 40 , 79
, SS_CALL_MDL_OUTPUTS ) ; isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit !=
0 ) { ssCallAccelRunBlock ( S , 40 , 80 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 81 , SS_CALL_MDL_OUTPUTS ) ; }
ssCallAccelRunBlock ( S , 40 , 82 , SS_CALL_MDL_OUTPUTS ) ; isHit =
ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 ) { rtb_B_40_84_0 = ( _rtB ->
B_40_51_0 - _rtB -> B_40_57_0 ) / _rtB -> B_40_45_0 ; _rtB -> B_40_85_0 =
rtb_B_40_84_0 + _rtB -> B_40_33_0 ; } _rtB -> B_40_90_0 = look1_binlxpw (
muDoubleScalarRem ( ssGetT ( S ) - _rtB -> B_40_87_0 , _rtB -> B_40_34_0 ) ,
_rtP -> P_56 , _rtP -> P_55 , 2U ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if
( isHit != 0 ) { if ( ssIsModeUpdateTimeStep ( S ) ) { _rtDW ->
RelationalOperator_Mode = ( _rtB -> B_40_85_0 >= _rtB -> B_40_90_0 ) ; }
rtb_B_40_91_0 = _rtDW -> RelationalOperator_Mode ; rtb_B_40_92_0 = ! _rtDW ->
RelationalOperator_Mode ; } isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit
!= 0 ) { _rtB -> B_40_93_0 = _rtB -> B_40_33_0 - rtb_B_40_84_0 ; } isHit =
ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { if ( ssIsModeUpdateTimeStep
( S ) ) { _rtDW -> RelationalOperator1_Mode = ( _rtB -> B_40_93_0 >= _rtB ->
B_40_90_0 ) ; } _rtB -> B_40_96_0 [ 0 ] = rtb_B_40_91_0 ; _rtB -> B_40_96_0 [
1 ] = rtb_B_40_92_0 ; _rtB -> B_40_96_0 [ 2 ] = _rtDW ->
RelationalOperator1_Mode ; _rtB -> B_40_96_0 [ 3 ] = ! _rtDW ->
RelationalOperator1_Mode ; } isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit
!= 0 ) { _rtB -> B_40_103_0 = _rtP -> P_57 * _rtB -> B_40_45_0 ; } _rtB ->
B_40_104_0 [ 0 ] = ( _rtB -> B_40_64_0 [ 0 ] + _rtB -> PR . B_0_4_0 ) / _rtB
-> B_40_103_0 ; _rtB -> B_40_104_0 [ 1 ] = ( _rtB -> B_40_64_0 [ 1 ] + _rtB
-> PR1 . B_0_4_0 ) / _rtB -> B_40_103_0 ; _rtB -> B_40_105_0 = ( 0.0 - _rtB
-> B_40_104_0 [ 0 ] ) - _rtB -> B_40_104_0 [ 1 ] ; if (
ssIsModeUpdateTimeStep ( S ) ) { rtb_B_40_84_0 = _rtB -> B_40_104_0 [ 0 ] ;
_rtDW -> MinMax_MODE = 0 ; rtb_B_40_13_0 = _rtB -> B_40_104_0 [ 0 ] ; _rtDW
-> MinMax2_MODE = 0 ; if ( ( _rtB -> B_40_104_0 [ 0 ] != _rtB -> B_40_104_0 [
0 ] ) || ( _rtB -> B_40_105_0 < _rtB -> B_40_104_0 [ 0 ] ) ) { rtb_B_40_84_0
= _rtB -> B_40_105_0 ; _rtDW -> MinMax_MODE = 1 ; } if ( ( _rtB -> B_40_104_0
[ 0 ] != _rtB -> B_40_104_0 [ 0 ] ) || ( _rtB -> B_40_105_0 > _rtB ->
B_40_104_0 [ 0 ] ) ) { rtb_B_40_13_0 = _rtB -> B_40_105_0 ; _rtDW ->
MinMax2_MODE = 1 ; } if ( ( rtb_B_40_84_0 != rtb_B_40_84_0 ) || ( _rtB ->
B_40_104_0 [ 1 ] < rtb_B_40_84_0 ) ) { rtb_B_40_84_0 = _rtB -> B_40_104_0 [ 1
] ; _rtDW -> MinMax_MODE = 2 ; } if ( ( rtb_B_40_13_0 != rtb_B_40_13_0 ) || (
_rtB -> B_40_104_0 [ 1 ] > rtb_B_40_13_0 ) ) { rtb_B_40_13_0 = _rtB ->
B_40_104_0 [ 1 ] ; _rtDW -> MinMax2_MODE = 2 ; } } else { tmp [ 0 ] = _rtB ->
B_40_104_0 [ 0 ] ; tmp [ 1 ] = _rtB -> B_40_105_0 ; tmp [ 2 ] = _rtB ->
B_40_104_0 [ 1 ] ; rtb_B_40_84_0 = tmp [ _rtDW -> MinMax_MODE ] ; tmp [ 0 ] =
_rtB -> B_40_104_0 [ 0 ] ; tmp [ 1 ] = _rtB -> B_40_105_0 ; tmp [ 2 ] = _rtB
-> B_40_104_0 [ 1 ] ; rtb_B_40_13_0 = tmp [ _rtDW -> MinMax2_MODE ] ; }
rtb_B_40_84_0 = ( rtb_B_40_84_0 + rtb_B_40_13_0 ) * _rtP -> P_58 ; _rtB ->
B_40_110_0 [ 0 ] = _rtB -> B_40_104_0 [ 0 ] + rtb_B_40_84_0 ; _rtB ->
B_40_110_0 [ 1 ] = _rtB -> B_40_105_0 + rtb_B_40_84_0 ; _rtB -> B_40_110_0 [
2 ] = _rtB -> B_40_104_0 [ 1 ] + rtb_B_40_84_0 ; _rtB -> B_40_111_0 [ 0 ] =
_rtB -> B_40_110_0 [ 0 ] + _rtB -> B_40_10_0 ; _rtB -> B_40_111_0 [ 1 ] =
_rtB -> B_40_110_0 [ 1 ] + _rtB -> B_40_10_0 ; _rtB -> B_40_111_0 [ 2 ] =
_rtB -> B_40_110_0 [ 2 ] + _rtB -> B_40_10_0 ; _rtB -> B_40_116_0 =
look1_binlxpw ( muDoubleScalarRem ( ssGetT ( S ) - _rtB -> B_40_113_0 , _rtB
-> B_40_11_0_c ) , _rtP -> P_60 , _rtP -> P_59 , 2U ) ; isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { if ( ssIsModeUpdateTimeStep ( S ) ) {
_rtDW -> RelationalOperator7_Mode = ( _rtB -> B_40_111_0 [ 0 ] >= _rtB ->
B_40_116_0 ) ; } _rtB -> B_40_117_0 = _rtDW -> RelationalOperator7_Mode ; if
( ssIsModeUpdateTimeStep ( S ) ) { _rtDW -> RelationalOperator6_Mode = ( _rtB
-> B_40_111_0 [ 1 ] >= _rtB -> B_40_116_0 ) ; _rtDW ->
RelationalOperator2_Mode = ( _rtB -> B_40_111_0 [ 2 ] >= _rtB -> B_40_116_0 )
; } _rtB -> B_40_123_0 [ 0 ] = _rtB -> B_40_117_0 ; _rtB -> B_40_123_0 [ 1 ]
= ! _rtB -> B_40_117_0 ; _rtB -> B_40_123_0 [ 2 ] = _rtDW ->
RelationalOperator6_Mode ; _rtB -> B_40_123_0 [ 3 ] = ! _rtDW ->
RelationalOperator6_Mode ; _rtB -> B_40_123_0 [ 4 ] = _rtDW ->
RelationalOperator2_Mode ; _rtB -> B_40_123_0 [ 5 ] = ! _rtDW ->
RelationalOperator2_Mode ; } isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit
!= 0 ) { _rtB -> B_40_130_0 = ( 0.0 - _rtB -> B_40_71_0 [ 0 ] ) - _rtB ->
B_40_71_0 [ 1 ] ; } diansai2026A_22b_TrueRMS ( S , _rtB -> B_40_16_0 , _rtB
-> B_40_71_0 [ 0 ] , _rtB -> B_40_130_0 , _rtB -> B_40_71_0 [ 1 ] , & _rtB ->
TrueRMS , & _rtDW -> TrueRMS , & _rtP -> TrueRMS , & _rtX -> TrueRMS , & ( (
XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S ) ) -> TrueRMS ) ;
diansai2026A_22b_RMS ( S , _rtB -> B_40_17_0 , _rtB -> B_40_71_0 [ 0 ] , _rtB
-> B_40_130_0 , _rtB -> B_40_71_0 [ 1 ] , & _rtB -> RMS , & _rtDW -> RMS , &
_rtP -> RMS , & _rtX -> RMS , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> RMS ) ; if ( _rtB -> B_40_16_0 ) { _rtB ->
B_40_133_0 [ 0 ] = _rtB -> TrueRMS . B_12_10_0 [ 0 ] ; _rtB -> B_40_133_0 [ 1
] = _rtB -> TrueRMS . B_12_10_0 [ 1 ] ; _rtB -> B_40_133_0 [ 2 ] = _rtB ->
TrueRMS . B_12_10_0 [ 2 ] ; } else { _rtB -> B_40_133_0 [ 0 ] = _rtB -> RMS .
B_10_22_0 [ 0 ] ; _rtB -> B_40_133_0 [ 1 ] = _rtB -> RMS . B_10_22_0 [ 1 ] ;
_rtB -> B_40_133_0 [ 2 ] = _rtB -> RMS . B_10_22_0 [ 2 ] ; }
ssCallAccelRunBlock ( S , 40 , 134 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 135 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 136 , SS_CALL_MDL_OUTPUTS ) ; _rtB ->
B_40_137_0 = _rtX -> integrator_CSTATE ; { real_T * * uBuffer = ( real_T * *
) & _rtDW -> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T simTime =
ssGetT ( S ) ; real_T tMinusDelay = simTime - _rtP -> P_62 ; _rtB ->
B_40_138_0 = diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 ,
* uBuffer , _rtDW -> TransportDelay_IWORK . CircularBufSize , & _rtDW ->
TransportDelay_IWORK . Last , _rtDW -> TransportDelay_IWORK . Tail , _rtDW ->
TransportDelay_IWORK . Head , _rtP -> P_63 , 0 , ( boolean_T ) (
ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> TransportDelay_IWORK .
CircularBufSize ) [ _rtDW -> TransportDelay_IWORK . Head ] == ssGetT ( S ) )
) ) ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB ->
B_40_140_0 = _rtP -> P_64 ; } rtb_B_40_91_0 = ( ssGetT ( S ) >= _rtB ->
B_40_140_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB
-> B_40_142_0 = _rtDW -> Memory_PreviousInput ; } if ( rtb_B_40_91_0 ) { _rtB
-> B_5_0_0 = _rtB -> B_40_137_0 - _rtB -> B_40_138_0 ; _rtB -> B_5_1_0 = _rtP
-> P_0 * _rtB -> B_5_0_0 ; _rtB -> B_40_144_0 = _rtB -> B_5_1_0 ; } else {
_rtB -> B_40_144_0 = _rtB -> B_40_142_0 ; } diansai2026A_22b_TrueRMS_f ( S ,
_rtB -> B_40_28_0 , _rtB -> B_40_144_0 , & _rtB -> TrueRMS_fw , & _rtDW ->
TrueRMS_fw , & _rtP -> TrueRMS_fw , & _rtX -> TrueRMS_fw , & ( (
XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S ) ) -> TrueRMS_fw ) ;
diansai2026A_22b_RMS_l ( S , _rtB -> B_40_29_0 , _rtB -> B_40_144_0 , & _rtB
-> RMS_l , & _rtDW -> RMS_l , & _rtP -> RMS_l , & _rtX -> RMS_l , & ( (
XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S ) ) -> RMS_l ) ; if (
_rtB -> B_40_28_0 ) { rtb_B_40_84_0 = _rtB -> TrueRMS_fw . B_34_10_0 ; } else
{ rtb_B_40_84_0 = _rtB -> RMS_l . B_32_22_0 ; } _rtB -> B_40_149_0 = _rtX ->
integrator_CSTATE_n ; { real_T * * uBuffer = ( real_T * * ) & _rtDW ->
TransportDelay_PWORK_e . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ;
real_T tMinusDelay = simTime - _rtP -> P_67 ; B_40_150_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
_rtDW -> TransportDelay_IWORK_i . CircularBufSize , & _rtDW ->
TransportDelay_IWORK_i . Last , _rtDW -> TransportDelay_IWORK_i . Tail ,
_rtDW -> TransportDelay_IWORK_i . Head , _rtP -> P_68 , 0 , ( boolean_T ) (
ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> TransportDelay_IWORK_i .
CircularBufSize ) [ _rtDW -> TransportDelay_IWORK_i . Head ] == ssGetT ( S )
) ) ) ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB ->
B_40_152_0 = _rtP -> P_69 ; } rtb_B_40_91_0 = ( ssGetT ( S ) >= _rtB ->
B_40_152_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB
-> B_40_154_0 = _rtDW -> Memory_PreviousInput_i ; } if ( rtb_B_40_91_0 ) {
_rtB -> B_40_156_0 = ( _rtB -> B_40_149_0 - B_40_150_0 ) * _rtP -> P_3 ; }
else { _rtB -> B_40_156_0 = _rtB -> B_40_154_0 ; } rtb_B_40_13_0 = _rtB ->
B_40_156_0 * _rtB -> B_40_156_0 ; diansai2026A_22b_TrueRMS_f ( S , _rtB ->
B_40_31_0 , _rtB -> B_40_144_0 , & _rtB -> TrueRMS_a , & _rtDW -> TrueRMS_a ,
& _rtP -> TrueRMS_a , & _rtX -> TrueRMS_a , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> TrueRMS_a ) ; diansai2026A_22b_RMS_l ( S ,
_rtB -> B_40_32_0 , _rtB -> B_40_144_0 , & _rtB -> RMS_d , & _rtDW -> RMS_d ,
& _rtP -> RMS_d , & _rtX -> RMS_d , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> RMS_d ) ; if ( _rtB -> B_40_31_0 ) {
rtb_B_40_17_0 = _rtB -> TrueRMS_a . B_34_10_0 ; } else { rtb_B_40_17_0 = _rtB
-> RMS_d . B_32_22_0 ; } rtb_B_40_17_0 *= rtb_B_40_17_0 ; if ( rtb_B_40_17_0
> _rtP -> P_71 ) { rtb_B_40_19_0 = _rtP -> P_71 ; } else if ( rtb_B_40_17_0 <
_rtP -> P_72 ) { rtb_B_40_19_0 = _rtP -> P_72 ; } else { rtb_B_40_19_0 =
rtb_B_40_17_0 ; } rtb_B_40_84_0 = ( ( rtb_B_40_84_0 * rtb_B_40_84_0 -
rtb_B_40_13_0 ) - rtb_B_40_17_0 ) / rtb_B_40_19_0 ; if ( rtb_B_40_84_0 > _rtP
-> P_73 ) { rtb_B_40_84_0 = _rtP -> P_73 ; } else if ( rtb_B_40_84_0 < _rtP
-> P_74 ) { rtb_B_40_84_0 = _rtP -> P_74 ; } if ( ssIsMajorTimeStep ( S ) !=
0 ) { if ( _rtDW -> Sqrt_DWORK1_e != 0 ) {
ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; _rtDW ->
Sqrt_DWORK1_e = 0 ; } _rtB -> B_40_166_0 = muDoubleScalarSqrt ( rtb_B_40_84_0
) ; } else if ( rtb_B_40_84_0 < 0.0 ) { _rtB -> B_40_166_0 = -
muDoubleScalarSqrt ( muDoubleScalarAbs ( rtb_B_40_84_0 ) ) ; _rtDW ->
Sqrt_DWORK1_e = 1 ; } else { _rtB -> B_40_166_0 = muDoubleScalarSqrt (
rtb_B_40_84_0 ) ; } ssCallAccelRunBlock ( S , 6 , 0 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 168 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 169 , SS_CALL_MDL_OUTPUTS ) ;
diansai2026A_22b_TrueRMS ( S , _rtB -> B_40_22_0 , _rtB -> B_40_71_0 [ 0 ] ,
_rtB -> B_40_130_0 , _rtB -> B_40_71_0 [ 1 ] , & _rtB -> TrueRMS_f , & _rtDW
-> TrueRMS_f , & _rtP -> TrueRMS_f , & _rtX -> TrueRMS_f , & ( (
XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S ) ) -> TrueRMS_f ) ;
diansai2026A_22b_RMS ( S , _rtB -> B_40_23_0 , _rtB -> B_40_71_0 [ 0 ] , _rtB
-> B_40_130_0 , _rtB -> B_40_71_0 [ 1 ] , & _rtB -> RMS_e , & _rtDW -> RMS_e
, & _rtP -> RMS_e , & _rtX -> RMS_e , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> RMS_e ) ; if ( _rtB -> B_40_22_0 ) {
rtb_B_40_13_0 = _rtB -> TrueRMS_f . B_12_10_0 [ 0 ] ; rtb_B_40_17_0 = _rtB ->
TrueRMS_f . B_12_10_0 [ 1 ] ; rtb_B_40_84_0 = _rtB -> TrueRMS_f . B_12_10_0 [
2 ] ; } else { rtb_B_40_13_0 = _rtB -> RMS_e . B_10_22_0 [ 0 ] ;
rtb_B_40_17_0 = _rtB -> RMS_e . B_10_22_0 [ 1 ] ; rtb_B_40_84_0 = _rtB ->
RMS_e . B_10_22_0 [ 2 ] ; } B_40_173_0_idx_0 = rtb_B_40_13_0 * rtb_B_40_13_0
; _rtB -> B_40_174_0 [ 0 ] = _rtX -> integrator_CSTATE_c [ 0 ] ;
rtb_B_40_19_0 = rtb_B_40_17_0 * rtb_B_40_17_0 ; _rtB -> B_40_174_0 [ 1 ] =
_rtX -> integrator_CSTATE_c [ 1 ] ; rtb_B_40_20_0 = rtb_B_40_84_0 *
rtb_B_40_84_0 ; _rtB -> B_40_174_0 [ 2 ] = _rtX -> integrator_CSTATE_c [ 2 ]
; { real_T * * uBuffer = ( real_T * * ) & _rtDW -> TransportDelay_PWORK_b .
TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ; real_T tMinusDelay ; {
int_T i1 ; real_T * y0 = & B_40_175_0 [ 0 ] ; int_T * iw_Tail = & _rtDW ->
TransportDelay_IWORK_k . Tail [ 0 ] ; int_T * iw_Head = & _rtDW ->
TransportDelay_IWORK_k . Head [ 0 ] ; int_T * iw_Last = & _rtDW ->
TransportDelay_IWORK_k . Last [ 0 ] ; int_T * iw_CircularBufSize = & _rtDW ->
TransportDelay_IWORK_k . CircularBufSize [ 0 ] ; for ( i1 = 0 ; i1 < 3 ; i1
++ ) { tMinusDelay = ( ( _rtP -> P_76 > 0.0 ) ? _rtP -> P_76 : 0.0 ) ;
tMinusDelay = simTime - tMinusDelay ; y0 [ i1 ] =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
iw_CircularBufSize [ i1 ] , & iw_Last [ i1 ] , iw_Tail [ i1 ] , iw_Head [ i1
] , _rtP -> P_77 , 0 , ( boolean_T ) ( ssIsMinorTimeStep ( S ) && ( ( *
uBuffer + iw_CircularBufSize [ i1 ] ) [ iw_Head [ i1 ] ] == ssGetT ( S ) ) )
) ; uBuffer ++ ; } } } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { _rtB -> B_40_177_0 = _rtP -> P_78 ; } rtb_B_40_91_0 = ( ssGetT ( S ) >=
_rtB -> B_40_177_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0
) { _rtB -> B_40_179_0 [ 0 ] = _rtDW -> Memory_PreviousInput_a [ 0 ] ; _rtB
-> B_40_179_0 [ 1 ] = _rtDW -> Memory_PreviousInput_a [ 1 ] ; _rtB ->
B_40_179_0 [ 2 ] = _rtDW -> Memory_PreviousInput_a [ 2 ] ; } if (
rtb_B_40_91_0 ) { _rtB -> B_40_181_0 [ 0 ] = ( _rtB -> B_40_174_0 [ 0 ] -
B_40_175_0 [ 0 ] ) * _rtP -> P_2 ; _rtB -> B_40_181_0 [ 1 ] = ( _rtB ->
B_40_174_0 [ 1 ] - B_40_175_0 [ 1 ] ) * _rtP -> P_2 ; _rtB -> B_40_181_0 [ 2
] = ( _rtB -> B_40_174_0 [ 2 ] - B_40_175_0 [ 2 ] ) * _rtP -> P_2 ; } else {
_rtB -> B_40_181_0 [ 0 ] = _rtB -> B_40_179_0 [ 0 ] ; _rtB -> B_40_181_0 [ 1
] = _rtB -> B_40_179_0 [ 1 ] ; _rtB -> B_40_181_0 [ 2 ] = _rtB -> B_40_179_0
[ 2 ] ; } B_40_182_0_idx_0 = _rtB -> B_40_181_0 [ 0 ] * _rtB -> B_40_181_0 [
0 ] ; rtb_B_40_27_0 = _rtB -> B_40_181_0 [ 1 ] * _rtB -> B_40_181_0 [ 1 ] ;
rtb_B_40_29_0 = _rtB -> B_40_181_0 [ 2 ] ; diansai2026A_22b_TrueRMS ( S ,
_rtB -> B_40_25_0_p , _rtB -> B_40_71_0 [ 0 ] , _rtB -> B_40_130_0 , _rtB ->
B_40_71_0 [ 1 ] , & _rtB -> TrueRMS_e , & _rtDW -> TrueRMS_e , & _rtP ->
TrueRMS_e , & _rtX -> TrueRMS_e , & ( ( XDis_diansai2026A_22b_T * )
ssGetContStateDisabled ( S ) ) -> TrueRMS_e ) ; diansai2026A_22b_RMS ( S ,
_rtB -> B_40_26_0 , _rtB -> B_40_71_0 [ 0 ] , _rtB -> B_40_130_0 , _rtB ->
B_40_71_0 [ 1 ] , & _rtB -> RMS_f , & _rtDW -> RMS_f , & _rtP -> RMS_f , &
_rtX -> RMS_f , & ( ( XDis_diansai2026A_22b_T * ) ssGetContStateDisabled ( S
) ) -> RMS_f ) ; if ( _rtB -> B_40_25_0_p ) { rtb_B_40_13_0 = _rtB ->
TrueRMS_e . B_12_10_0 [ 0 ] ; rtb_B_40_17_0 = _rtB -> TrueRMS_e . B_12_10_0 [
1 ] ; rtb_B_40_84_0 = _rtB -> TrueRMS_e . B_12_10_0 [ 2 ] ; } else {
rtb_B_40_13_0 = _rtB -> RMS_f . B_10_22_0 [ 0 ] ; rtb_B_40_17_0 = _rtB ->
RMS_f . B_10_22_0 [ 1 ] ; rtb_B_40_84_0 = _rtB -> RMS_f . B_10_22_0 [ 2 ] ; }
B_40_172_0_0 = rtb_B_40_13_0 * rtb_B_40_13_0 ; B_40_173_0_idx_0 = (
B_40_173_0_idx_0 - B_40_182_0_idx_0 ) - B_40_172_0_0 ; if ( B_40_172_0_0 >
_rtP -> P_80 ) { B_40_172_0_0 = _rtP -> P_80 ; } else if ( B_40_172_0_0 <
_rtP -> P_81 ) { B_40_172_0_0 = _rtP -> P_81 ; } B_40_172_0_0 =
B_40_173_0_idx_0 / B_40_172_0_0 ; if ( B_40_172_0_0 > _rtP -> P_82 ) {
rtb_B_40_13_0 = _rtP -> P_82 ; } else if ( B_40_172_0_0 < _rtP -> P_83 ) {
rtb_B_40_13_0 = _rtP -> P_83 ; } else { rtb_B_40_13_0 = B_40_172_0_0 ; }
B_40_172_0_0 = rtb_B_40_17_0 * rtb_B_40_17_0 ; B_40_173_0_idx_0 = (
rtb_B_40_19_0 - rtb_B_40_27_0 ) - B_40_172_0_0 ; if ( B_40_172_0_0 > _rtP ->
P_80 ) { B_40_172_0_0 = _rtP -> P_80 ; } else if ( B_40_172_0_0 < _rtP ->
P_81 ) { B_40_172_0_0 = _rtP -> P_81 ; } B_40_172_0_0 = B_40_173_0_idx_0 /
B_40_172_0_0 ; if ( B_40_172_0_0 > _rtP -> P_82 ) { rtb_B_40_17_0 = _rtP ->
P_82 ; } else if ( B_40_172_0_0 < _rtP -> P_83 ) { rtb_B_40_17_0 = _rtP ->
P_83 ; } else { rtb_B_40_17_0 = B_40_172_0_0 ; } B_40_172_0_0 = rtb_B_40_84_0
* rtb_B_40_84_0 ; B_40_173_0_idx_0 = ( rtb_B_40_20_0 - rtb_B_40_29_0 *
rtb_B_40_29_0 ) - B_40_172_0_0 ; if ( B_40_172_0_0 > _rtP -> P_80 ) {
B_40_172_0_0 = _rtP -> P_80 ; } else if ( B_40_172_0_0 < _rtP -> P_81 ) {
B_40_172_0_0 = _rtP -> P_81 ; } B_40_172_0_0 = B_40_173_0_idx_0 /
B_40_172_0_0 ; if ( B_40_172_0_0 > _rtP -> P_82 ) { B_40_172_0_0 = _rtP ->
P_82 ; } else if ( B_40_172_0_0 < _rtP -> P_83 ) { B_40_172_0_0 = _rtP ->
P_83 ; } if ( ssIsMajorTimeStep ( S ) != 0 ) { if ( _rtDW -> Sqrt_DWORK1_n !=
0 ) { ssSetBlockStateForSolverChangedAtMajorStep ( S ) ;
ssSetContTimeOutputInconsistentWithStateAtMajorStep ( S ) ; _rtDW ->
Sqrt_DWORK1_n = 0 ; } _rtB -> B_40_191_0 [ 0 ] = muDoubleScalarSqrt (
rtb_B_40_13_0 ) ; _rtB -> B_40_191_0 [ 1 ] = muDoubleScalarSqrt (
rtb_B_40_17_0 ) ; _rtB -> B_40_191_0 [ 2 ] = muDoubleScalarSqrt (
B_40_172_0_0 ) ; } else { if ( rtb_B_40_13_0 < 0.0 ) { _rtB -> B_40_191_0 [ 0
] = - muDoubleScalarSqrt ( muDoubleScalarAbs ( rtb_B_40_13_0 ) ) ; _rtDW ->
Sqrt_DWORK1_n = 1 ; } else { _rtB -> B_40_191_0 [ 0 ] = muDoubleScalarSqrt (
rtb_B_40_13_0 ) ; } if ( rtb_B_40_17_0 < 0.0 ) { _rtB -> B_40_191_0 [ 1 ] = -
muDoubleScalarSqrt ( muDoubleScalarAbs ( rtb_B_40_17_0 ) ) ; _rtDW ->
Sqrt_DWORK1_n = 1 ; } else { _rtB -> B_40_191_0 [ 1 ] = muDoubleScalarSqrt (
rtb_B_40_17_0 ) ; } if ( B_40_172_0_0 < 0.0 ) { _rtB -> B_40_191_0 [ 2 ] = -
muDoubleScalarSqrt ( muDoubleScalarAbs ( B_40_172_0_0 ) ) ; _rtDW ->
Sqrt_DWORK1_n = 1 ; } else { _rtB -> B_40_191_0 [ 2 ] = muDoubleScalarSqrt (
B_40_172_0_0 ) ; } } ssCallAccelRunBlock ( S , 40 , 192 , SS_CALL_MDL_OUTPUTS
) ; ssCallAccelRunBlock ( S , 40 , 193 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 194 , SS_CALL_MDL_OUTPUTS ) ; _rtB ->
B_40_195_0 = _rtX -> integrator_CSTATE_i ; { real_T * * uBuffer = ( real_T *
* ) & _rtDW -> TransportDelay_PWORK_o . TUbufferPtrs [ 0 ] ; real_T simTime =
ssGetT ( S ) ; real_T tMinusDelay = simTime - _rtP -> P_85 ; B_40_196_0 =
diansai2026A_22b_acc_rt_TDelayInterpolate ( tMinusDelay , 0.0 , * uBuffer ,
_rtDW -> TransportDelay_IWORK_ib . CircularBufSize , & _rtDW ->
TransportDelay_IWORK_ib . Last , _rtDW -> TransportDelay_IWORK_ib . Tail ,
_rtDW -> TransportDelay_IWORK_ib . Head , _rtP -> P_86 , 0 , ( boolean_T ) (
ssIsMinorTimeStep ( S ) && ( ( * uBuffer + _rtDW -> TransportDelay_IWORK_ib .
CircularBufSize ) [ _rtDW -> TransportDelay_IWORK_ib . Head ] == ssGetT ( S )
) ) ) ; } isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB ->
B_40_198_0 = _rtP -> P_87 ; } rtb_B_40_91_0 = ( ssGetT ( S ) >= _rtB ->
B_40_198_0 ) ; isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtB
-> B_40_200_0 = _rtDW -> Memory_PreviousInput_as ; } if ( rtb_B_40_91_0 ) {
_rtB -> B_40_202_0 = ( _rtB -> B_40_195_0 - B_40_196_0 ) * _rtP -> P_1 ; }
else { _rtB -> B_40_202_0 = _rtB -> B_40_200_0 ; } _rtB -> B_40_204_0 = (
_rtB -> B_40_133_0 [ 0 ] * _rtB -> B_40_7_0_b [ 0 ] + _rtB -> B_40_133_0 [ 1
] * _rtB -> B_40_7_0_b [ 1 ] ) + _rtB -> B_40_133_0 [ 2 ] * _rtB ->
B_40_7_0_b [ 2 ] ; _rtB -> B_40_205_0 = _rtB -> B_40_202_0 / _rtB ->
B_40_204_0 ; ssCallAccelRunBlock ( S , 40 , 206 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 207 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 208 , SS_CALL_MDL_OUTPUTS ) ; _rtB ->
B_40_212_0 = _rtP -> P_89 * _rtB -> B_40_47_0 ; isHit = ssIsSampleHit ( S , 2
, 0 ) ; if ( isHit != 0 ) { _rtB -> B_40_213_0 = _rtP -> P_90 * _rtB ->
B_40_54_0 ; } _rtB -> B_40_215_0 = ( muDoubleScalarSin ( _rtP -> P_93 *
ssGetTaskTime ( S , 0 ) + _rtP -> P_94 ) * _rtP -> P_91 + _rtP -> P_92 ) *
_rtB -> B_40_213_0 ; _rtB -> B_40_217_0 = ( muDoubleScalarSin ( _rtP -> P_97
* ssGetTaskTime ( S , 0 ) + _rtP -> P_98 ) * _rtP -> P_95 + _rtP -> P_96 ) *
_rtB -> B_40_213_0 ; isHit = ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 )
{ _rtB -> B_40_218_0 = _rtP -> P_99 * _rtB -> B_40_51_0 ; } _rtB ->
B_40_220_0 = ( muDoubleScalarSin ( _rtP -> P_102 * ssGetTaskTime ( S , 0 ) +
_rtP -> P_103 ) * _rtP -> P_100 + _rtP -> P_101 ) * _rtB -> B_40_218_0 ; _rtB
-> B_40_222_0 = ( muDoubleScalarSin ( _rtP -> P_106 * ssGetTaskTime ( S , 0 )
+ _rtP -> P_107 ) * _rtP -> P_104 + _rtP -> P_105 ) * _rtB -> B_40_218_0 ;
ssCallAccelRunBlock ( S , 40 , 223 , SS_CALL_MDL_OUTPUTS ) ;
ssCallAccelRunBlock ( S , 40 , 224 , SS_CALL_MDL_OUTPUTS ) ; isHit =
ssIsSampleHit ( S , 2 , 0 ) ; if ( isHit != 0 ) { _rtB -> B_40_226_0 = ( _rtB
-> B_40_71_0 [ 0 ] * _rtB -> B_40_2_0 + _rtB -> B_40_130_0 * _rtB -> B_40_4_0
) + _rtB -> B_40_71_0 [ 1 ] * _rtB -> B_40_3_0 ; } _rtB -> B_40_227_0 [ 0 ] =
_rtB -> B_40_71_0 [ 0 ] ; _rtB -> B_40_227_0 [ 1 ] = _rtB -> B_40_130_0 ;
_rtB -> B_40_227_0 [ 2 ] = _rtB -> B_40_71_0 [ 1 ] ; UNUSED_PARAMETER ( tid )
; } static void mdlOutputsTID4 ( SimStruct * S , int_T tid ) {
B_diansai2026A_22b_T * _rtB ; P_diansai2026A_22b_T * _rtP ; int32_T i ; _rtP
= ( ( P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) ; _rtB = ( (
B_diansai2026A_22b_T * ) _ssGetModelBlockIO ( S ) ) ; _rtB -> B_40_0_0_m [ 0
] = _rtP -> P_108 [ 0 ] ; _rtB -> B_40_0_0_m [ 1 ] = _rtP -> P_108 [ 1 ] ;
_rtB -> B_40_0_0_m [ 2 ] = _rtP -> P_108 [ 2 ] ; _rtB -> B_40_0_0_m [ 3 ] =
_rtP -> P_108 [ 3 ] ; for ( i = 0 ; i < 6 ; i ++ ) { _rtB -> B_40_1_0_c [ i ]
= _rtP -> P_109 [ i ] ; } _rtB -> B_40_4_0_k = _rtP -> P_112 ; _rtB ->
B_40_5_0 = _rtP -> P_113 ; _rtB -> B_40_6_0 = _rtP -> P_114 ; _rtB ->
B_40_7_0 = _rtP -> P_115 ; _rtB -> B_40_8_0 = _rtP -> P_116 ; _rtB ->
B_40_10_0 = _rtP -> P_117 ; _rtB -> B_40_11_0_c = _rtP -> P_118 ; _rtB ->
B_40_12_0 = _rtP -> P_119 ; _rtB -> B_40_13_0 = _rtP -> P_120 * _rtP -> P_110
; _rtB -> B_40_14_0 = _rtP -> P_121 * _rtP -> P_111 ; _rtB -> B_40_16_0 = (
_rtP -> P_122 != 0.0 ) ; _rtB -> B_40_17_0 = ! _rtB -> B_40_16_0 ; _rtB ->
B_40_19_0 = ( _rtP -> P_123 != 0.0 ) ; _rtB -> B_40_20_0 = ! _rtB ->
B_40_19_0 ; _rtB -> B_40_22_0 = ( _rtP -> P_124 != 0.0 ) ; _rtB -> B_40_23_0
= ! _rtB -> B_40_22_0 ; _rtB -> B_40_25_0_p = ( _rtP -> P_125 != 0.0 ) ; _rtB
-> B_40_26_0 = ! _rtB -> B_40_25_0_p ; _rtB -> B_40_28_0 = ( _rtP -> P_126 !=
0.0 ) ; _rtB -> B_40_29_0 = ! _rtB -> B_40_28_0 ; _rtB -> B_40_31_0 = ( _rtP
-> P_127 != 0.0 ) ; _rtB -> B_40_32_0 = ! _rtB -> B_40_31_0 ; _rtB ->
B_40_33_0 = _rtP -> P_128 ; _rtB -> B_40_34_0 = _rtP -> P_129 ;
UNUSED_PARAMETER ( tid ) ; }
#define MDL_UPDATE
static void mdlUpdate ( SimStruct * S , int_T tid ) { B_diansai2026A_22b_T *
_rtB ; DW_diansai2026A_22b_T * _rtDW ; P_diansai2026A_22b_T * _rtP ; int32_T
isHit ; _rtDW = ( ( DW_diansai2026A_22b_T * ) ssGetRootDWork ( S ) ) ; _rtP =
( ( P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) ; _rtB = ( (
B_diansai2026A_22b_T * ) _ssGetModelBlockIO ( S ) ) ; isHit = ssIsSampleHit (
S , 2 , 0 ) ; if ( isHit != 0 ) { ssCallAccelRunBlock ( S , 40 , 1 ,
SS_CALL_MDL_UPDATE ) ; } diansai2026A_22b_TrueRMS_Update ( S , & _rtB ->
TrueRMS_g , & _rtDW -> TrueRMS_g , & _rtP -> TrueRMS_g ) ;
diansai2026A_22b_RMS_Update ( S , & _rtB -> RMS_n , & _rtDW -> RMS_n , & _rtP
-> RMS_n ) ; { real_T * * uBuffer = ( real_T * * ) & _rtDW -> T_PWORK .
TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ; _rtDW -> T_IWORK . Head
= ( ( _rtDW -> T_IWORK . Head < ( _rtDW -> T_IWORK . CircularBufSize - 1 ) )
? ( _rtDW -> T_IWORK . Head + 1 ) : 0 ) ; if ( _rtDW -> T_IWORK . Head ==
_rtDW -> T_IWORK . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW -> T_IWORK .
CircularBufSize , & _rtDW -> T_IWORK . Tail , & _rtDW -> T_IWORK . Head , &
_rtDW -> T_IWORK . Last , simTime - _rtP -> P_21 , uBuffer , ( boolean_T ) 0
, false , & _rtDW -> T_IWORK . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + _rtDW ->
T_IWORK . CircularBufSize ) [ _rtDW -> T_IWORK . Head ] = simTime ; ( *
uBuffer ) [ _rtDW -> T_IWORK . Head ] = _rtB -> B_40_11_0 ; } { real_T * *
uBuffer = ( real_T * * ) & _rtDW -> T1_PWORK . TUbufferPtrs [ 0 ] ; real_T
simTime = ssGetT ( S ) ; _rtDW -> T1_IWORK . Head = ( ( _rtDW -> T1_IWORK .
Head < ( _rtDW -> T1_IWORK . CircularBufSize - 1 ) ) ? ( _rtDW -> T1_IWORK .
Head + 1 ) : 0 ) ; if ( _rtDW -> T1_IWORK . Head == _rtDW -> T1_IWORK . Tail
) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW ->
T1_IWORK . CircularBufSize , & _rtDW -> T1_IWORK . Tail , & _rtDW -> T1_IWORK
. Head , & _rtDW -> T1_IWORK . Last , simTime - _rtP -> P_24 , uBuffer , (
boolean_T ) 0 , false , & _rtDW -> T1_IWORK . MaxNewBufSize ) ) {
ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } ( *
uBuffer + _rtDW -> T1_IWORK . CircularBufSize ) [ _rtDW -> T1_IWORK . Head ]
= simTime ; ( * uBuffer ) [ _rtDW -> T1_IWORK . Head ] = _rtB -> B_40_15_0 ;
} { real_T * * uBuffer = ( real_T * * ) & _rtDW -> T_PWORK_e . TUbufferPtrs [
0 ] ; real_T simTime = ssGetT ( S ) ; _rtDW -> T_IWORK_d . Head = ( ( _rtDW
-> T_IWORK_d . Head < ( _rtDW -> T_IWORK_d . CircularBufSize - 1 ) ) ? (
_rtDW -> T_IWORK_d . Head + 1 ) : 0 ) ; if ( _rtDW -> T_IWORK_d . Head ==
_rtDW -> T_IWORK_d . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW -> T_IWORK_d .
CircularBufSize , & _rtDW -> T_IWORK_d . Tail , & _rtDW -> T_IWORK_d . Head ,
& _rtDW -> T_IWORK_d . Last , simTime - _rtP -> P_27 , uBuffer , ( boolean_T
) 0 , false , & _rtDW -> T_IWORK_d . MaxNewBufSize ) ) { ssSetErrorStatus ( S
, "tdelay memory allocation error" ) ; return ; } } ( * uBuffer + _rtDW ->
T_IWORK_d . CircularBufSize ) [ _rtDW -> T_IWORK_d . Head ] = simTime ; ( *
uBuffer ) [ _rtDW -> T_IWORK_d . Head ] = _rtB -> B_40_21_0 ; } { real_T * *
uBuffer = ( real_T * * ) & _rtDW -> T1_PWORK_h . TUbufferPtrs [ 0 ] ; real_T
simTime = ssGetT ( S ) ; _rtDW -> T1_IWORK_g . Head = ( ( _rtDW -> T1_IWORK_g
. Head < ( _rtDW -> T1_IWORK_g . CircularBufSize - 1 ) ) ? ( _rtDW ->
T1_IWORK_g . Head + 1 ) : 0 ) ; if ( _rtDW -> T1_IWORK_g . Head == _rtDW ->
T1_IWORK_g . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW -> T1_IWORK_g .
CircularBufSize , & _rtDW -> T1_IWORK_g . Tail , & _rtDW -> T1_IWORK_g . Head
, & _rtDW -> T1_IWORK_g . Last , simTime - _rtP -> P_30 , uBuffer , (
boolean_T ) 0 , false , & _rtDW -> T1_IWORK_g . MaxNewBufSize ) ) {
ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } ( *
uBuffer + _rtDW -> T1_IWORK_g . CircularBufSize ) [ _rtDW -> T1_IWORK_g .
Head ] = simTime ; ( * uBuffer ) [ _rtDW -> T1_IWORK_g . Head ] = _rtB ->
B_40_25_0 ; } diansai2026A_22b_TrueRMS_Update ( S , & _rtB -> TrueRMS , &
_rtDW -> TrueRMS , & _rtP -> TrueRMS ) ; diansai2026A_22b_RMS_Update ( S , &
_rtB -> RMS , & _rtDW -> RMS , & _rtP -> RMS ) ; { real_T * * uBuffer = (
real_T * * ) & _rtDW -> TransportDelay_PWORK . TUbufferPtrs [ 0 ] ; real_T
simTime = ssGetT ( S ) ; _rtDW -> TransportDelay_IWORK . Head = ( ( _rtDW ->
TransportDelay_IWORK . Head < ( _rtDW -> TransportDelay_IWORK .
CircularBufSize - 1 ) ) ? ( _rtDW -> TransportDelay_IWORK . Head + 1 ) : 0 )
; if ( _rtDW -> TransportDelay_IWORK . Head == _rtDW -> TransportDelay_IWORK
. Tail ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW
-> TransportDelay_IWORK . CircularBufSize , & _rtDW -> TransportDelay_IWORK .
Tail , & _rtDW -> TransportDelay_IWORK . Head , & _rtDW ->
TransportDelay_IWORK . Last , simTime - _rtP -> P_62 , uBuffer , ( boolean_T
) 0 , false , & _rtDW -> TransportDelay_IWORK . MaxNewBufSize ) ) {
ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } ( *
uBuffer + _rtDW -> TransportDelay_IWORK . CircularBufSize ) [ _rtDW ->
TransportDelay_IWORK . Head ] = simTime ; ( * uBuffer ) [ _rtDW ->
TransportDelay_IWORK . Head ] = _rtB -> B_40_137_0 ; } isHit = ssIsSampleHit
( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtDW -> Memory_PreviousInput = _rtB ->
B_40_144_0 ; } diansai2026A_22b_TrueRMS_g_Update ( S , & _rtB -> TrueRMS_fw ,
& _rtDW -> TrueRMS_fw , & _rtP -> TrueRMS_fw ) ;
diansai2026A_22b_RMS_k_Update ( S , & _rtB -> RMS_l , & _rtDW -> RMS_l , &
_rtP -> RMS_l ) ; { real_T * * uBuffer = ( real_T * * ) & _rtDW ->
TransportDelay_PWORK_e . TUbufferPtrs [ 0 ] ; real_T simTime = ssGetT ( S ) ;
_rtDW -> TransportDelay_IWORK_i . Head = ( ( _rtDW -> TransportDelay_IWORK_i
. Head < ( _rtDW -> TransportDelay_IWORK_i . CircularBufSize - 1 ) ) ? (
_rtDW -> TransportDelay_IWORK_i . Head + 1 ) : 0 ) ; if ( _rtDW ->
TransportDelay_IWORK_i . Head == _rtDW -> TransportDelay_IWORK_i . Tail ) {
if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW ->
TransportDelay_IWORK_i . CircularBufSize , & _rtDW -> TransportDelay_IWORK_i
. Tail , & _rtDW -> TransportDelay_IWORK_i . Head , & _rtDW ->
TransportDelay_IWORK_i . Last , simTime - _rtP -> P_67 , uBuffer , (
boolean_T ) 0 , false , & _rtDW -> TransportDelay_IWORK_i . MaxNewBufSize ) )
{ ssSetErrorStatus ( S , "tdelay memory allocation error" ) ; return ; } } (
* uBuffer + _rtDW -> TransportDelay_IWORK_i . CircularBufSize ) [ _rtDW ->
TransportDelay_IWORK_i . Head ] = simTime ; ( * uBuffer ) [ _rtDW ->
TransportDelay_IWORK_i . Head ] = _rtB -> B_40_149_0 ; } isHit =
ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtDW ->
Memory_PreviousInput_i = _rtB -> B_40_156_0 ; }
diansai2026A_22b_TrueRMS_g_Update ( S , & _rtB -> TrueRMS_a , & _rtDW ->
TrueRMS_a , & _rtP -> TrueRMS_a ) ; diansai2026A_22b_RMS_k_Update ( S , &
_rtB -> RMS_d , & _rtDW -> RMS_d , & _rtP -> RMS_d ) ;
diansai2026A_22b_TrueRMS_Update ( S , & _rtB -> TrueRMS_f , & _rtDW ->
TrueRMS_f , & _rtP -> TrueRMS_f ) ; diansai2026A_22b_RMS_Update ( S , & _rtB
-> RMS_e , & _rtDW -> RMS_e , & _rtP -> RMS_e ) ; { real_T * * uBuffer = (
real_T * * ) & _rtDW -> TransportDelay_PWORK_b . TUbufferPtrs [ 0 ] ; real_T
simTime = ssGetT ( S ) ; _rtDW -> TransportDelay_IWORK_k . Head [ 0 ] = ( (
_rtDW -> TransportDelay_IWORK_k . Head [ 0 ] < ( _rtDW ->
TransportDelay_IWORK_k . CircularBufSize [ 0 ] - 1 ) ) ? ( _rtDW ->
TransportDelay_IWORK_k . Head [ 0 ] + 1 ) : 0 ) ; if ( _rtDW ->
TransportDelay_IWORK_k . Head [ 0 ] == _rtDW -> TransportDelay_IWORK_k . Tail
[ 0 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW
-> TransportDelay_IWORK_k . CircularBufSize [ 0 ] , & _rtDW ->
TransportDelay_IWORK_k . Tail [ 0 ] , & _rtDW -> TransportDelay_IWORK_k .
Head [ 0 ] , & _rtDW -> TransportDelay_IWORK_k . Last [ 0 ] , simTime - _rtP
-> P_76 , uBuffer , ( boolean_T ) 0 , false , & _rtDW ->
TransportDelay_IWORK_k . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + _rtDW ->
TransportDelay_IWORK_k . CircularBufSize [ 0 ] ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 0 ] ] = simTime ; ( * uBuffer ++ ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 0 ] ] = _rtB -> B_40_174_0 [ 0 ] ; _rtDW ->
TransportDelay_IWORK_k . Head [ 1 ] = ( ( _rtDW -> TransportDelay_IWORK_k .
Head [ 1 ] < ( _rtDW -> TransportDelay_IWORK_k . CircularBufSize [ 1 ] - 1 )
) ? ( _rtDW -> TransportDelay_IWORK_k . Head [ 1 ] + 1 ) : 0 ) ; if ( _rtDW
-> TransportDelay_IWORK_k . Head [ 1 ] == _rtDW -> TransportDelay_IWORK_k .
Tail [ 1 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( &
_rtDW -> TransportDelay_IWORK_k . CircularBufSize [ 1 ] , & _rtDW ->
TransportDelay_IWORK_k . Tail [ 1 ] , & _rtDW -> TransportDelay_IWORK_k .
Head [ 1 ] , & _rtDW -> TransportDelay_IWORK_k . Last [ 1 ] , simTime - _rtP
-> P_76 , uBuffer , ( boolean_T ) 0 , false , & _rtDW ->
TransportDelay_IWORK_k . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + _rtDW ->
TransportDelay_IWORK_k . CircularBufSize [ 1 ] ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 1 ] ] = simTime ; ( * uBuffer ++ ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 1 ] ] = _rtB -> B_40_174_0 [ 1 ] ; _rtDW ->
TransportDelay_IWORK_k . Head [ 2 ] = ( ( _rtDW -> TransportDelay_IWORK_k .
Head [ 2 ] < ( _rtDW -> TransportDelay_IWORK_k . CircularBufSize [ 2 ] - 1 )
) ? ( _rtDW -> TransportDelay_IWORK_k . Head [ 2 ] + 1 ) : 0 ) ; if ( _rtDW
-> TransportDelay_IWORK_k . Head [ 2 ] == _rtDW -> TransportDelay_IWORK_k .
Tail [ 2 ] ) { if ( ! diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( &
_rtDW -> TransportDelay_IWORK_k . CircularBufSize [ 2 ] , & _rtDW ->
TransportDelay_IWORK_k . Tail [ 2 ] , & _rtDW -> TransportDelay_IWORK_k .
Head [ 2 ] , & _rtDW -> TransportDelay_IWORK_k . Last [ 2 ] , simTime - _rtP
-> P_76 , uBuffer , ( boolean_T ) 0 , false , & _rtDW ->
TransportDelay_IWORK_k . MaxNewBufSize ) ) { ssSetErrorStatus ( S ,
"tdelay memory allocation error" ) ; return ; } } ( * uBuffer + _rtDW ->
TransportDelay_IWORK_k . CircularBufSize [ 2 ] ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 2 ] ] = simTime ; ( * uBuffer ) [ _rtDW ->
TransportDelay_IWORK_k . Head [ 2 ] ] = _rtB -> B_40_174_0 [ 2 ] ; } isHit =
ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtDW ->
Memory_PreviousInput_a [ 0 ] = _rtB -> B_40_181_0 [ 0 ] ; _rtDW ->
Memory_PreviousInput_a [ 1 ] = _rtB -> B_40_181_0 [ 1 ] ; _rtDW ->
Memory_PreviousInput_a [ 2 ] = _rtB -> B_40_181_0 [ 2 ] ; }
diansai2026A_22b_TrueRMS_Update ( S , & _rtB -> TrueRMS_e , & _rtDW ->
TrueRMS_e , & _rtP -> TrueRMS_e ) ; diansai2026A_22b_RMS_Update ( S , & _rtB
-> RMS_f , & _rtDW -> RMS_f , & _rtP -> RMS_f ) ; { real_T * * uBuffer = (
real_T * * ) & _rtDW -> TransportDelay_PWORK_o . TUbufferPtrs [ 0 ] ; real_T
simTime = ssGetT ( S ) ; _rtDW -> TransportDelay_IWORK_ib . Head = ( ( _rtDW
-> TransportDelay_IWORK_ib . Head < ( _rtDW -> TransportDelay_IWORK_ib .
CircularBufSize - 1 ) ) ? ( _rtDW -> TransportDelay_IWORK_ib . Head + 1 ) : 0
) ; if ( _rtDW -> TransportDelay_IWORK_ib . Head == _rtDW ->
TransportDelay_IWORK_ib . Tail ) { if ( !
diansai2026A_22b_acc_rt_TDelayUpdateTailOrGrowBuf ( & _rtDW ->
TransportDelay_IWORK_ib . CircularBufSize , & _rtDW ->
TransportDelay_IWORK_ib . Tail , & _rtDW -> TransportDelay_IWORK_ib . Head ,
& _rtDW -> TransportDelay_IWORK_ib . Last , simTime - _rtP -> P_85 , uBuffer
, ( boolean_T ) 0 , false , & _rtDW -> TransportDelay_IWORK_ib .
MaxNewBufSize ) ) { ssSetErrorStatus ( S , "tdelay memory allocation error" )
; return ; } } ( * uBuffer + _rtDW -> TransportDelay_IWORK_ib .
CircularBufSize ) [ _rtDW -> TransportDelay_IWORK_ib . Head ] = simTime ; ( *
uBuffer ) [ _rtDW -> TransportDelay_IWORK_ib . Head ] = _rtB -> B_40_195_0 ;
} isHit = ssIsSampleHit ( S , 1 , 0 ) ; if ( isHit != 0 ) { _rtDW ->
Memory_PreviousInput_as = _rtB -> B_40_202_0 ; } UNUSED_PARAMETER ( tid ) ; }
#define MDL_UPDATE
static void mdlUpdateTID4 ( SimStruct * S , int_T tid ) { UNUSED_PARAMETER (
tid ) ; }
#define MDL_DERIVATIVES
static void mdlDerivatives ( SimStruct * S ) { B_diansai2026A_22b_T * _rtB ;
DW_diansai2026A_22b_T * _rtDW ; P_diansai2026A_22b_T * _rtP ;
XDot_diansai2026A_22b_T * _rtXdot ; X_diansai2026A_22b_T * _rtX ; _rtDW = ( (
DW_diansai2026A_22b_T * ) ssGetRootDWork ( S ) ) ; _rtXdot = ( (
XDot_diansai2026A_22b_T * ) ssGetdX ( S ) ) ; _rtX = ( ( X_diansai2026A_22b_T
* ) ssGetContStates ( S ) ) ; _rtP = ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) ; _rtB = ( ( B_diansai2026A_22b_T * )
_ssGetModelBlockIO ( S ) ) ; diansai2026A_22b_TrueRMS_Deriv ( S , & _rtB ->
TrueRMS_g , & _rtDW -> TrueRMS_g , & _rtXdot -> TrueRMS_g ) ;
diansai2026A_22b_RMS_Deriv ( S , & _rtB -> RMS_n , & _rtDW -> RMS_n , &
_rtXdot -> RMS_n ) ; _rtXdot -> integ1_CSTATE = _rtB -> B_40_220_0 ; _rtXdot
-> Integ2_CSTATE = _rtB -> B_40_222_0 ; _rtXdot -> integ1_CSTATE_h = _rtB ->
B_40_215_0 ; _rtXdot -> Integ2_CSTATE_m = _rtB -> B_40_217_0 ; _rtXdot ->
TransferFcn_CSTATE [ 0 ] = 0.0 ; _rtXdot -> TransferFcn_CSTATE [ 0 ] += _rtP
-> P_37 [ 0 ] * _rtX -> TransferFcn_CSTATE [ 0 ] ; _rtXdot ->
TransferFcn_CSTATE [ 1 ] = 0.0 ; _rtXdot -> TransferFcn_CSTATE [ 0 ] += _rtP
-> P_37 [ 1 ] * _rtX -> TransferFcn_CSTATE [ 1 ] ; _rtXdot ->
TransferFcn_CSTATE [ 1 ] += _rtX -> TransferFcn_CSTATE [ 0 ] ; _rtXdot ->
TransferFcn_CSTATE [ 0 ] += _rtB -> B_40_45_0 ; _rtXdot -> Integrator_CSTATE
= _rtB -> B_40_212_0 ; _rtXdot -> Integrator_CSTATE_p = _rtB -> B_40_13_0 ;
diansai2026A_22b_PR_i_Deriv ( S , & _rtB -> PR_j , & _rtP -> PR_j , & _rtX ->
PR_j , & _rtXdot -> PR_j ) ; diansai2026A_22b_PR_i_Deriv ( S , & _rtB ->
PR1_o , & _rtP -> PR1_o , & _rtX -> PR1_o , & _rtXdot -> PR1_o ) ;
diansai2026A_22b_PR_Deriv ( S , & _rtB -> PR , & _rtP -> PR , & _rtX -> PR ,
& _rtXdot -> PR ) ; diansai2026A_22b_PR_Deriv ( S , & _rtB -> PR1 , & _rtP ->
PR1 , & _rtX -> PR1 , & _rtXdot -> PR1 ) ; diansai2026A_22b_TrueRMS_Deriv ( S
, & _rtB -> TrueRMS , & _rtDW -> TrueRMS , & _rtXdot -> TrueRMS ) ;
diansai2026A_22b_RMS_Deriv ( S , & _rtB -> RMS , & _rtDW -> RMS , & _rtXdot
-> RMS ) ; _rtXdot -> integrator_CSTATE = _rtB -> B_40_54_0 ;
diansai2026A_22b_TrueRMS_d_Deriv ( S , & _rtB -> TrueRMS_fw , & _rtDW ->
TrueRMS_fw , & _rtXdot -> TrueRMS_fw ) ; diansai2026A_22b_RMS_b_Deriv ( S , &
_rtB -> RMS_l , & _rtDW -> RMS_l , & _rtXdot -> RMS_l ) ; _rtXdot ->
integrator_CSTATE_n = _rtB -> B_40_144_0 ; diansai2026A_22b_TrueRMS_d_Deriv (
S , & _rtB -> TrueRMS_a , & _rtDW -> TrueRMS_a , & _rtXdot -> TrueRMS_a ) ;
diansai2026A_22b_RMS_b_Deriv ( S , & _rtB -> RMS_d , & _rtDW -> RMS_d , &
_rtXdot -> RMS_d ) ; diansai2026A_22b_TrueRMS_Deriv ( S , & _rtB -> TrueRMS_f
, & _rtDW -> TrueRMS_f , & _rtXdot -> TrueRMS_f ) ;
diansai2026A_22b_RMS_Deriv ( S , & _rtB -> RMS_e , & _rtDW -> RMS_e , &
_rtXdot -> RMS_e ) ; _rtXdot -> integrator_CSTATE_c [ 0 ] = _rtB ->
B_40_227_0 [ 0 ] ; _rtXdot -> integrator_CSTATE_c [ 1 ] = _rtB -> B_40_227_0
[ 1 ] ; _rtXdot -> integrator_CSTATE_c [ 2 ] = _rtB -> B_40_227_0 [ 2 ] ;
diansai2026A_22b_TrueRMS_Deriv ( S , & _rtB -> TrueRMS_e , & _rtDW ->
TrueRMS_e , & _rtXdot -> TrueRMS_e ) ; diansai2026A_22b_RMS_Deriv ( S , &
_rtB -> RMS_f , & _rtDW -> RMS_f , & _rtXdot -> RMS_f ) ; _rtXdot ->
integrator_CSTATE_i = _rtB -> B_40_226_0 ; }
#define MDL_ZERO_CROSSINGS
static void mdlZeroCrossings ( SimStruct * S ) { B_diansai2026A_22b_T * _rtB
; DW_diansai2026A_22b_T * _rtDW ; P_diansai2026A_22b_T * _rtP ;
ZCV_diansai2026A_22b_T * _rtZCSV ; real_T tmp [ 3 ] ; real_T minV ; _rtDW = (
( DW_diansai2026A_22b_T * ) ssGetRootDWork ( S ) ) ; _rtZCSV = ( (
ZCV_diansai2026A_22b_T * ) ssGetSolverZcSignalVector ( S ) ) ; _rtP = ( (
P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) ; _rtB = ( (
B_diansai2026A_22b_T * ) _ssGetModelBlockIO ( S ) ) ;
diansai2026A_22b_TrueRMS_ZC ( S , & _rtB -> TrueRMS_g , & _rtDW -> TrueRMS_g
, & _rtP -> TrueRMS_g , & _rtZCSV -> TrueRMS_g ) ; diansai2026A_22b_PR_e_ZC (
S , & _rtB -> PR_j , & _rtP -> PR_j , & _rtZCSV -> PR_j ) ;
diansai2026A_22b_PR_e_ZC ( S , & _rtB -> PR1_o , & _rtP -> PR1_o , & _rtZCSV
-> PR1_o ) ; diansai2026A_22b_PR_ZC ( S , & _rtB -> PR , & _rtP -> PR , &
_rtZCSV -> PR ) ; diansai2026A_22b_PR_ZC ( S , & _rtB -> PR1 , & _rtP -> PR1
, & _rtZCSV -> PR1 ) ; _rtZCSV -> RelationalOperator_RelopInput_ZC = _rtB ->
B_40_85_0 - _rtB -> B_40_90_0 ; _rtZCSV -> RelationalOperator1_RelopInput_ZC
= _rtB -> B_40_93_0 - _rtB -> B_40_90_0 ; minV = _rtB -> B_40_104_0 [ 0 ] ;
if ( ( _rtB -> B_40_104_0 [ 0 ] != _rtB -> B_40_104_0 [ 0 ] ) || ( _rtB ->
B_40_105_0 < _rtB -> B_40_104_0 [ 0 ] ) ) { minV = _rtB -> B_40_105_0 ; } if
( ( minV != minV ) || ( _rtB -> B_40_104_0 [ 1 ] < minV ) ) { minV = _rtB ->
B_40_104_0 [ 1 ] ; } tmp [ 0 ] = _rtB -> B_40_104_0 [ 0 ] ; tmp [ 1 ] = _rtB
-> B_40_105_0 ; tmp [ 2 ] = _rtB -> B_40_104_0 [ 1 ] ; _rtZCSV ->
MinMax_MinmaxInput_ZC = minV - tmp [ _rtDW -> MinMax_MODE ] ; minV = _rtB ->
B_40_104_0 [ 0 ] ; if ( ( _rtB -> B_40_104_0 [ 0 ] != _rtB -> B_40_104_0 [ 0
] ) || ( _rtB -> B_40_105_0 > _rtB -> B_40_104_0 [ 0 ] ) ) { minV = _rtB ->
B_40_105_0 ; } if ( ( minV != minV ) || ( _rtB -> B_40_104_0 [ 1 ] > minV ) )
{ minV = _rtB -> B_40_104_0 [ 1 ] ; } tmp [ 0 ] = _rtB -> B_40_104_0 [ 0 ] ;
tmp [ 1 ] = _rtB -> B_40_105_0 ; tmp [ 2 ] = _rtB -> B_40_104_0 [ 1 ] ;
_rtZCSV -> MinMax2_MinmaxInput_ZC = minV - tmp [ _rtDW -> MinMax2_MODE ] ;
_rtZCSV -> RelationalOperator7_RelopInput_ZC = _rtB -> B_40_111_0 [ 0 ] -
_rtB -> B_40_116_0 ; _rtZCSV -> RelationalOperator6_RelopInput_ZC = _rtB ->
B_40_111_0 [ 1 ] - _rtB -> B_40_116_0 ; _rtZCSV ->
RelationalOperator2_RelopInput_ZC = _rtB -> B_40_111_0 [ 2 ] - _rtB ->
B_40_116_0 ; diansai2026A_22b_TrueRMS_ZC ( S , & _rtB -> TrueRMS , & _rtDW ->
TrueRMS , & _rtP -> TrueRMS , & _rtZCSV -> TrueRMS ) ;
diansai2026A_22b_TrueRMS_e_ZC ( S , & _rtB -> TrueRMS_fw , & _rtDW ->
TrueRMS_fw , & _rtP -> TrueRMS_fw , & _rtZCSV -> TrueRMS_fw ) ;
diansai2026A_22b_TrueRMS_e_ZC ( S , & _rtB -> TrueRMS_a , & _rtDW ->
TrueRMS_a , & _rtP -> TrueRMS_a , & _rtZCSV -> TrueRMS_a ) ;
diansai2026A_22b_TrueRMS_ZC ( S , & _rtB -> TrueRMS_f , & _rtDW -> TrueRMS_f
, & _rtP -> TrueRMS_f , & _rtZCSV -> TrueRMS_f ) ;
diansai2026A_22b_TrueRMS_ZC ( S , & _rtB -> TrueRMS_e , & _rtDW -> TrueRMS_e
, & _rtP -> TrueRMS_e , & _rtZCSV -> TrueRMS_e ) ; } static void
mdlInitializeSizes ( SimStruct * S ) { ssSetChecksumVal ( S , 0 , 3439958740U
) ; ssSetChecksumVal ( S , 1 , 3032759110U ) ; ssSetChecksumVal ( S , 2 ,
2997164690U ) ; ssSetChecksumVal ( S , 3 , 3624872943U ) ; { mxArray *
slVerStructMat = ( NULL ) ; mxArray * slStrMat = mxCreateString ( "simulink"
) ; char slVerChar [ 10 ] ; int status = mexCallMATLAB ( 1 , & slVerStructMat
, 1 , & slStrMat , "ver" ) ; if ( status == 0 ) { mxArray * slVerMat =
mxGetField ( slVerStructMat , 0 , "Version" ) ; if ( slVerMat == ( NULL ) ) {
status = 1 ; } else { status = mxGetString ( slVerMat , slVerChar , 10 ) ; }
} mxDestroyArray ( slStrMat ) ; mxDestroyArray ( slVerStructMat ) ; if ( (
status == 1 ) || ( strcmp ( slVerChar , "10.7" ) != 0 ) ) { return ; } }
ssSetOptions ( S , SS_OPTION_EXCEPTION_FREE_CODE ) ; if ( ssGetSizeofDWork (
S ) != ( SLSize ) sizeof ( DW_diansai2026A_22b_T ) ) { static char msg [ 256
] ; sprintf ( msg , "Unexpected error: Internal DWork sizes do "
"not match for accelerator mex file (%ld vs %lu)." , ( signed long )
ssGetSizeofDWork ( S ) , ( unsigned long ) sizeof ( DW_diansai2026A_22b_T ) )
; ssSetErrorStatus ( S , msg ) ; } if ( ssGetSizeofGlobalBlockIO ( S ) != (
SLSize ) sizeof ( B_diansai2026A_22b_T ) ) { static char msg [ 256 ] ;
sprintf ( msg , "Unexpected error: Internal BlockIO sizes do "
"not match for accelerator mex file (%ld vs %lu)." , ( signed long )
ssGetSizeofGlobalBlockIO ( S ) , ( unsigned long ) sizeof (
B_diansai2026A_22b_T ) ) ; ssSetErrorStatus ( S , msg ) ; } { int
ssSizeofParams ; ssGetSizeofParams ( S , & ssSizeofParams ) ; if (
ssSizeofParams != sizeof ( P_diansai2026A_22b_T ) ) { static char msg [ 256 ]
; sprintf ( msg , "Unexpected error: Internal Parameters sizes do "
"not match for accelerator mex file (%d vs %lu)." , ssSizeofParams , (
unsigned long ) sizeof ( P_diansai2026A_22b_T ) ) ; ssSetErrorStatus ( S ,
msg ) ; } } _ssSetModelRtp ( S , ( real_T * ) & diansai2026A_22b_rtDefaultP )
; rt_InitInfAndNaN ( sizeof ( real_T ) ) ; ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) -> P_71 = rtInf ; ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) -> P_73 = rtInf ; ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) -> P_80 = rtInf ; ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) -> P_82 = rtInf ; ( ( P_diansai2026A_22b_T * )
ssGetModelRtp ( S ) ) -> TrueRMS_a . P_7 = rtInf ; ( ( P_diansai2026A_22b_T *
) ssGetModelRtp ( S ) ) -> TrueRMS_fw . P_7 = rtInf ; ( (
P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) -> TrueRMS_e . P_7 = rtInf ; (
( P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) -> TrueRMS_f . P_7 = rtInf ;
( ( P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) -> TrueRMS_g . P_7 = rtInf
; ( ( P_diansai2026A_22b_T * ) ssGetModelRtp ( S ) ) -> TrueRMS . P_7 = rtInf
; } static void mdlInitializeSampleTimes ( SimStruct * S ) { { SimStruct *
childS ; SysOutputFcn * callSysFcns ; childS = ssGetSFunction ( S , 0 ) ;
callSysFcns = ssGetCallSystemOutputFcnList ( childS ) ; callSysFcns [ 3 + 0 ]
= ( SysOutputFcn ) ( NULL ) ; childS = ssGetSFunction ( S , 1 ) ; callSysFcns
= ssGetCallSystemOutputFcnList ( childS ) ; callSysFcns [ 3 + 0 ] = (
SysOutputFcn ) ( NULL ) ; } slAccRegPrmChangeFcn ( S , mdlOutputsTID4 ) ; }
static void mdlTerminate ( SimStruct * S ) { }
#include "simulink.c"
