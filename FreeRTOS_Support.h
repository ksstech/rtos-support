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

#include	"x_struct_union.h"							// x_time x_definitions stdint time

#ifdef __cplusplus
extern "C" {
#endif

// ##################################### MACRO definitions #########################################

#define	MALLOC_MARK()	uint32_t y,x=xPortGetFreeHeapSize();
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)\n",x,y,y-x);

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

// ##################################### global function prototypes ################################

bool bRtosToggleStatus(const EventBits_t uxBitsToToggle) ;
bool bRtosVerifyState(const EventBits_t uxTaskToVerify) ;

void myApplicationTickHook(void) ;
void vApplicationStackOverflowHook(TaskHandle_t *, char *) ;
void vApplicationMallocFailedHook(void) ;

int	xRtosTaskCreate(TaskFunction_t pxTaskCode,
							const char * const pcName,
							const uint32_t usStackDepth,
				            void * pvParameters,
							UBaseType_t uxPriority,
							TaskHandle_t * pxCreatedTask,
							const BaseType_t xCoreID) ;

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSema, uint32_t mSec) ;
BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSema) ;

void	vRtosHeapSetup(void) ;
void	vRtosHeapFreeSafely(void * *) ;

void	vRtosReportMemory(void) ;

bool	bRtosStatsUpdateHook(void) ;
int		xRtosReportTasks(const flagmask_t, char *, size_t) ;

void	vTaskDumpStack(void *);

#ifdef __cplusplus
}
#endif
