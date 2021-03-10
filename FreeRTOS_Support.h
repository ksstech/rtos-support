/*
 * FreeRTOS_Support.h
 */

#pragma	once

#include	"freertos/FreeRTOS.h"
#include	"freertos/task.h"
#include	"freertos/timers.h"
#include	"freertos/queue.h"
#include	"freertos/semphr.h"
#include	"freertos/event_groups.h"

#include	"x_struct_union.h"							// x_time x_definitions stdint.h time.h

#ifdef __cplusplus
extern "C" {
#endif

// ##################################### MACRO definitions #########################################

#define	MALLOC_MARK				int32_t m0, m1 ; m0 = xPortGetFreeHeapSize() ;
#define	MALLOC_CHECK			m1 = xPortGetFreeHeapSize() ; if (m0 != m1) PRINT("m0=%d m1=%d d=%d\n", m0, m1, m0-m1) ;

//#define pdTICKS_TO_MS(xTicks)	(((TickType_t) (xTicks) * (TickType_t) 1000 ) / (TickType_t) configTICK_RATE_HZ )

// ############################################ Enumerations #######################################


// ###################################### BUILD : CONFIG definitions ##############################


// #################################### FreeRTOS global variables ##################################

extern	EventGroupHandle_t	xEventStatus, TaskRunState,	TaskDeleteState ;

// ################################### Event status manipulation ###################################

#define	xRtosSetStateRUN(X)			xEventGroupSetBits(TaskRunState, X)
#define	xRtosClearStateRUN(X)		xEventGroupClearBits(TaskRunState, X)
#define	xRtosWaitStateRUN(X,Y)		xEventGroupWaitBits(TaskRunState, X, pdFALSE, pdTRUE, Y)

#define	xRtosSetStateDELETE(X)		xEventGroupSetBits(TaskDeleteState, X)
#define	xRtosClearStateDELETE(X)	xEventGroupClearBits(TaskDeleteState, X)
#define	xRtosWaitStateDELETE(X,Y)	xEventGroupWaitBits(TaskDeleteState, X, pdFALSE, pdTRUE, Y)

#define	xRtosSetStatus(X)			xEventGroupSetBits(xEventStatus, X)
#define	xRtosClearStatus(X)			xEventGroupClearBits(xEventStatus, X)
#define	xRtosWaitStatus(X,Y)		xEventGroupWaitBits(xEventStatus, X, pdFALSE, pdTRUE, Y)

#define	bRtosWaitStatusALL(X,Y)		(((xEventGroupWaitBits(xEventStatus, X, pdFALSE, pdTRUE, Y) & X) == X) ? 1 : 0)
#define	xRtosWaitStatusANY(X,Y)		xEventGroupWaitBits(xEventStatus, X, pdFALSE, pdFALSE, Y)

#define	bRtosCheckStatus(X)			(((xEventGroupGetBits(xEventStatus) & (X)) == (X)) ? 1 : 0)
#define	xRtosGetStatus(X)			(xEventGroupGetBits(xEventStatus) & (X))

bool	bRtosToggleStatus(const EventBits_t uxBitsToToggle) ;
bool	bRtosVerifyState(const EventBits_t uxTaskToVerify) ;

// ##################################### global function prototypes ################################

void	myApplicationTickHook( void ) ;
bool	myApplicationIdleHook( void ) ;
void	vApplicationStackOverflowHook( TaskHandle_t * , char * ) ;
void	vApplicationMallocFailedHook( void ) ;

int32_t	xRtosTaskCreate(TaskFunction_t pxTaskCode,
							const char * const pcName,
							const uint32_t usStackDepth,
				            void * pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * pxCreatedTask,
							const BaseType_t xCoreID) ;

BaseType_t	xRtosSemaphoreTake(SemaphoreHandle_t * pSema, uint32_t mSec) ;
BaseType_t	xRtosSemaphoreGive(SemaphoreHandle_t * pSema) ;

void	vRtosHeapSetup( void ) ;
void	vRtosHeapFreeSafely( void * * ) ;

void	vRtosReportMemory(void) ;
int32_t	xRtosReportTasks(char *, size_t) ;
int32_t	xRtosReportTasksNew(const flagmask_t, char *, size_t) ;
void	vTaskDumpStack(void *, uint32_t ) ;

#ifdef __cplusplus
}
#endif
