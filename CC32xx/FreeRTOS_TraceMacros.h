/*
 * FreeRTOS_TraceMacros.h
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// ########################### Enumerations, macros and structures #################################

/* Numerically sequential definitions for 64 different trace code, full _u64 variable
 * Organised in sequence to task, queue, timer and custom trace events together in groups */

enum trcCODE {
// Tasks
	trcCODE_TaC,
	trcCODE_TaIT,
	trcCODE_TaSI,
	trcCODE_TaSO,
	trcCODE_TaDy,
	trcCODE_TaDU,
	trcCODE_TaPS,
	trcCODE_TaR,
	trcCODE_TRfI,
	trcCODE_TMRy,
	trcCODE_TaS,
	trcCODE_TaD,
// Queues
	trcCODE_QuC,
	trcCODE_QuTx,
	trcCODE_QuTF,
	trcCODE_QuRx,
	trcCODE_QuRF,
	trcCODE_QTfI,
	trcCODE_QTIF,
	trcCODE_QRfI,
	trcCODE_QRIF,
	trcCODE_QuPk,
	trcCODE_QuPF,
	trcCODE_QPfI,
	trcCODE_QPIF,
	trcCODE_QuD,
	trcCODE_QBoS,
	trcCODE_QBoR,
// Mutexes
	trcCODE_MuC,
	trcCODE_MuCF,
	trcCODE_MuRG,
	trcCODE_MRGF,
	trcCODE_MuRT,
	trcCODE_MRTF,
// Counting Semaphores
	trcCODE_CSC,
	trcCODE_CSCF,
// Timers
	trcCODE_TiC,
	trcCODE_TiCR,
	trcCODE_TiCS,
	trcCODE_TiEx,
// Malloc/Free
	trcCODE_MAll,
	trcCODE_MFre,
// end marker, insert before here...
	trcCODE_NUM,
} ;

extern	void *		TraceTask ;

// ############################### Default trace handler function ##################################

void		vTrace(uint32_t trcCode, void * P1, uint32_t P2) ;

// ##################################### Task trace support ########################################

#define		traceTASK_CREATE(pxNewTCB)												vTrace(trcCODE_TaC,		pxNewTCB,		0)
#define		traceTASK_INCREMENT_TICK(xTickCount)									vTrace(trcCODE_TaIT, 	pxCurrentTCB,	xTickCount)
#define		traceTASK_SWITCHED_IN()													TraceTask = pxCurrentTCB ;							\
																					vTrace(trcCODE_TaSI,	pxCurrentTCB,	0)
#define		traceTASK_SWITCHED_OUT()												vTrace(trcCODE_TaSO,	pxCurrentTCB,	0)
#define		traceTASK_DELAY()														vTrace(trcCODE_TaDy,	pxCurrentTCB,	0)
#define		traceTASK_DELAY_UNTIL(xTimeToWake)										vTrace(trcCODE_TaDU,	pxCurrentTCB,	xTimeToWake)
#define		traceTASK_PRIORITY_SET(xTask,uxNewPriority)								vTrace(trcCODE_TaPS,	xTask,			uxNewPriority)
#define		traceTASK_RESUME(xTask)													vTrace(trcCODE_TaR,		xTask,			0)
#define		traceTASK_RESUME_FROM_ISR(xTask)										vTrace(trcCODE_TRfI,	xTask,			0)
#define		traceMOVED_TASK_TO_READY_STATE(xTask)									vTrace(trcCODE_TMRy,	xTask,			0)
#define		traceTASK_SUSPEND(xTask)												vTrace(trcCODE_TaS,		xTask,			0)
#define		traceTASK_DELETE(xTask)													vTrace(trcCODE_TaD,		xTask,			0)

//#define traceTASK_PRIORITY_INHERIT( pxTCBOfMutexHolder, uxInheritedPriority )
//#define traceTASK_PRIORITY_DISINHERIT( pxTCBOfMutexHolder, uxOriginalPriority )
//#define traceTASK_NOTIFY_TAKE_BLOCK()
//#define traceTASK_NOTIFY_TAKE()
//#define traceTASK_NOTIFY_WAIT_BLOCK()
//#define traceTASK_NOTIFY_WAIT()
//#define traceTASK_NOTIFY()
//#define traceTASK_NOTIFY_FROM_ISR()
//#define traceTASK_NOTIFY_GIVE_FROM_ISR()
//#define tracePOST_MOVED_TASK_TO_READY_STATE( pxTCB )
//#define traceINCREASE_TICK_COUNT( x )
//#define traceLOW_POWER_IDLE_BEGIN()
//#define traceLOW_POWER_IDLE_END()

// ##################################### Queue trace support #######################################

#define		traceQUEUE_CREATE(pxNewQueue)											vTrace(trcCODE_QuC ,	pxNewQueue,		0)
#define		traceQUEUE_SEND(xQueue)													vTrace(trcCODE_QuTx,	xQueue,			0)
#define		traceQUEUE_SEND_FAILED(xQueue)											vTrace(trcCODE_QuTF,	xQueue,			0)
#define		traceQUEUE_RECEIVE(xQueue)												vTrace(trcCODE_QuRx,	xQueue,			0)
#define		traceQUEUE_RECEIVE_FAILED(xQueue)										vTrace(trcCODE_QuRF,	xQueue,			0)
#define		traceQUEUE_SEND_FROM_ISR(xQueue)										vTrace(trcCODE_QTfI,	xQueue,			0)
#define		traceQUEUE_SEND_FROM_ISR_FAILED(xQueue)									vTrace(trcCODE_QTIF,	xQueue,			0)
#define		traceQUEUE_RECEIVE_FROM_ISR(xQueue)										vTrace(trcCODE_QRfI,	xQueue,			0)
#define		traceQUEUE_RECEIVE_FROM_ISR_FAILED(xQueue)								vTrace(trcCODE_QRIF,	xQueue,			0)
#define		traceQUEUE_PEEK(xQueue)													vTrace(trcCODE_QuPk,	xQueue,			0)
#define		traceQUEUE_PEEK_FAILED(xQueue)											vTrace(trcCODE_QuPF,	xQueue,			0)
#define		traceQUEUE_PEEK_FROM_ISR(xQueue)										vTrace(trcCODE_QPfI,	xQueue,			0)
#define		traceQUEUE_PEEK_FROM_ISR_FAILED(xQueue)									vTrace(trcCODE_QPIF,	xQueue,			0)
#define		traceQUEUE_DELETE(pxQueue)												vTrace(trcCODE_QuD ,	pxQueue,		0)
#define		traceBLOCKING_ON_QUEUE_SEND(xQueue)										vTrace(trcCODE_QBoS,	xQueue,			0)
#define		traceBLOCKING_ON_QUEUE_RECEIVE(xQueue)									vTrace(trcCODE_QBoR,	xQueue,			0)
//#define traceQUEUE_REGISTRY_ADD(xQueue, pcQueueName)

// ############################################ Mutexes ############################################

#define		traceCREATE_MUTEX(pxNewMutex)											vTrace(trcCODE_MuC, 	pxNewMutex,		0)
#define		traceCREATE_MUTEX_FAILED()												vTrace(trcCODE_MuCF,	NULL,			0)
#define		traceGIVE_MUTEX_RECURSIVE(xMutex)										vTrace(trcCODE_MuRG,	xMutex,			0)
#define		traceGIVE_MUTEX_RECURSIVE_FAILED(xMutex)								vTrace(trcCODE_MRGF,	xMutex,			0)
#define		traceTAKE_MUTEX_RECURSIVE(xMutex)										vTrace(trcCODE_MuRT,	xMutex,			0)
#define		traceTAKE_MUTEX_RECURSIVE_FAILED(xMutex)								vTrace(trcCODE_MRTF,	xMutex,			0)

// ######################################## Counting Semaphores ####################################

#define		traceCREATE_COUNTING_SEMAPHORE()										vTrace(trcCODE_CSC,		xHandle,		0)
#define		traceCREATE_COUNTING_SEMAPHORE_FAILED()									vTrace(trcCODE_CSCF,	NULL,			0)

// ##################################### Timer trace support #######################################

#define		traceTIMER_CREATE(pxNewTimer)											vTrace(trcCODE_TiC,		pxNewTimer,		0)
#define		traceTIMER_COMMAND_RECEIVED(pxTimer, xCmdID, xCmdVal)					vTrace(trcCODE_TiCR,	pxTimer,		0)
#define		traceTIMER_COMMAND_SEND(xTimer, xCommandID, xOptionalValue, xReturn)	vTrace(trcCODE_TiCS,	xTimer,			0)
#define		traceTIMER_EXPIRED(pxTimer)												vTrace(trcCODE_TiEx,	pxTimer,		0)
//#define tracePEND_FUNC_CALL_FROM_ISR(xFunctionToPend, pvParameter1, ulParameter2, ret)
//#define tracePEND_FUNC_CALL(xFunctionToPend, pvParameter1, ulParameter2, ret)

// ################################### Malloc / Free support ######################################

#define traceMALLOC( pvAddress, uiSize )											vTrace(trcCODE_MAll,	pvAddress,		uiSize)
#define traceFREE( pvAddress, uiSize )												vTrace(trcCODE_MFre,	pvAddress,		uiSize)

// ##################################### new from v9.0.0 ###########################################
/*
#define traceSTART()
#define traceEND()
#define traceEVENT_GROUP_CREATE( xEventGroup )
#define traceEVENT_GROUP_CREATE_FAILED()
#define traceEVENT_GROUP_SYNC_BLOCK( xEventGroup, uxBitsToSet, uxBitsToWaitFor )
#define traceEVENT_GROUP_SYNC_END( xEventGroup, uxBitsToSet, uxBitsToWaitFor, xTimeoutOccurred ) ( void ) xTimeoutOccurred
#define traceEVENT_GROUP_WAIT_BITS_BLOCK( xEventGroup, uxBitsToWaitFor )
#define traceEVENT_GROUP_WAIT_BITS_END( xEventGroup, uxBitsToWaitFor, xTimeoutOccurred ) ( void ) xTimeoutOccurred
#define traceEVENT_GROUP_CLEAR_BITS( xEventGroup, uxBitsToClear )
#define traceEVENT_GROUP_CLEAR_BITS_FROM_ISR( xEventGroup, uxBitsToClear )
#define traceEVENT_GROUP_SET_BITS( xEventGroup, uxBitsToSet )
#define traceEVENT_GROUP_SET_BITS_FROM_ISR( xEventGroup, uxBitsToSet )
#define traceEVENT_GROUP_DELETE( xEventGroup )
*/

#ifdef __cplusplus
}
#endif
