/*
 * FreeRTOS_Support.h
 */

#pragma	once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "struct_union.h"		// x_time definitions stdint time
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// #######################################  Build macros ###########################################

#define configFR_MAX_TASKS	24

// ##################################### MACRO definitions #########################################

#if (tskKERNEL_VERSION_MAJOR > 9 && tskKERNEL_VERSION_MINOR > 3 && tskKERNEL_VERSION_BUILD > 4)
	#define configRUNTIME_SIZE	8
#else
	#define configRUNTIME_SIZE	4
#endif

#define	MALLOC_MARK()	u32_t y,x=xPortGetFreeHeapSize();
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)\r\n",x,y,y-x);

// ###################################### BUILD : CONFIG definitions ##############################

#define rtosDEBUG_SEMA	1			// 0=disable, 1=no return Address, >1=add return addresses

// ################################### Event status manipulation ###################################

#define	xRtosSetStateRUN(X)			xEventGroupSetBits(TaskRunState, (X))
#define	xRtosClearStateRUN(X)		xEventGroupClearBits(TaskRunState, (X))
#define	xRtosWaitStateRUN(X,Y)		xEventGroupWaitBits(TaskRunState, (X), pdFALSE, pdTRUE, (Y))

#define	xRtosSetStateDELETE(X)		xEventGroupSetBits(TaskDeleteState, (X))
#define	xRtosClearStateDELETE(X)	xEventGroupClearBits(TaskDeleteState, (X))
#define	xRtosWaitStateDELETE(X,Y)	xEventGroupWaitBits(TaskDeleteState, (X), pdFALSE, pdTRUE, (Y))

#define	xRtosSetStatus(X)			xEventGroupSetBits(xEventStatus, (X))
#define	xRtosClearStatus(X)			xEventGroupClearBits(xEventStatus, (X))

/**
 * @brief	Wait until ALL of bit[s] (X) are set
 */
#define	vRtosWaitStatus(X)			xEventGroupWaitBits(xEventStatus, (X), pdFALSE, pdTRUE, portMAX_DELAY)
/**
 * @brief	Wait (Y) ticks for ANY of bit[s] (X) to be set
 * @return	bits of X that are set
 */
#define	xRtosWaitStatusANY(X,Y)		(xEventGroupWaitBits(xEventStatus, (X), pdFALSE, pdFALSE, (Y)) & (X))
/**
 * @brief	read mask X of event status bits
 * @return	bits of X that are set
 */
#define	xRtosGetStatus(X)			(xEventGroupGetBits(xEventStatus) & (X))
/***
 * @brief	check for an EXACT match of ALL event status bits specified
 */
#define bRtosCheckStatus(X)			((xRtosGetStatus(X) == (X)) ? 1 : 0)

// ############################################ Enumerations #######################################


// #################################### FreeRTOS global variables ##################################

extern	EventGroupHandle_t	xEventStatus, TaskRunState,	TaskDeleteState ;
#if (configPRODUCTION == 0) && (rtosDEBUG_SEMA > 0)
	extern SemaphoreHandle_t * pSHmatch;
#endif

// ##################################### global function prototypes ################################

void myApplicationTickHook(void);
void vApplicationStackOverflowHook(TaskHandle_t, char *);
void vApplicationMallocFailedHook(void);

// ##################################### Semaphore support #########################################

SemaphoreHandle_t xRtosSemaphoreInit(void);
void vRtosSemaphoreInit(SemaphoreHandle_t *);
BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t *, TickType_t);
BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t *);
void vRtosSemaphoreDelete(SemaphoreHandle_t *);

// ##################################### Malloc/free support #######################################

void * pvRtosMalloc(size_t S);
void vRtosFree(void * pV);

void vRtosHeapSetup(void);

// ################################### Event status manipulation ###################################

bool bRtosToggleStatus(const EventBits_t uxBitsToToggle);
bool bRtosVerifyState(const EventBits_t uxTaskToVerify);

// ################################### Task status reporting #######################################

bool bRtosStatsUpdateHook(void);
int	xRtosReportTasks(char *, size_t, fm_t);
int xRtosReportMemory(char *, size_t, fm_t);

// ################################## Task creation/deletion #######################################

int	xRtosTaskCreate(TaskFunction_t pxTaskCode, const char * const pcName,
					const u32_t usStackDepth, void * const pvParameters,
					UBaseType_t uxPriority, TaskHandle_t * pxCreatedTask,
					const BaseType_t xCoreID);
TaskHandle_t xRtosTaskCreateStatic(TaskFunction_t pxTaskCode, const char * const pcName,
					const u32_t usStackDepth, void * const pvParameters,
					UBaseType_t uxPriority, StackType_t * const pxStackBuffer,
					StaticTask_t * const pxTaskBuffer, const BaseType_t xCoreID);
void vRtosTaskTerminate(const EventBits_t uxTaskMask);
void vRtosTaskDelete(TaskHandle_t TH);


// ####################################### Debug support ###########################################

void vTaskDumpStack(void *);
void vRtosReportCallers(int Base, int Depth);

#ifdef __cplusplus
}
#endif
