// FreeRTOS_Support.h

#pragma	once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "definitions.h"
#include "esp_debug_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Macros ###############################################

#define configFR_MAX_TASKS	24

#define	MALLOC_MARK()	u32_t y,x=xPortGetFreeHeapSize();
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)" strNL,x,y,y-x);

#define MESSAGE(mess,...)	IF_PX(debugTRACK && ioB1GET(ioUpDown), mess, ##__VA_ARGS__)
#define TASK_START(name) 	MESSAGE("[%s] starting" strNL, name)
#define TASK_STOP(name) 	MESSAGE("[%s] stopping" strNL, name)

#ifndef CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U64
	#error "Must be built with 64bit Runtime Counter, support for 32bit removed !!!"
#endif

// ######################################## Enumerations ###########################################

// ######################################### Structures ############################################

typedef const struct {
	TaskFunction_t pxTaskCode;
	const char * const pcName;
	const u32_t usStackDepth;
	UBaseType_t uxPriority;
	StackType_t * const pxStackBuffer;
	StaticTask_t * const pxTaskBuffer;
	const BaseType_t xCoreID;
	u32_t const xMask;
} task_param_t;

// ###################################### Global variables #########################################

// ##################################### global function prototypes ################################

void vRtosHeapSetup(void);

void myApplicationTickHook(void);
void vApplicationStackOverflowHook(TaskHandle_t, char *);
void vApplicationMallocFailedHook(void);

// ##################################### Semaphore support #########################################

#if (appPRODUCTION > 0)			/* -1=disable, 0=block only, >0=all incl return addresses */
	#define rtosDEBUG_SEMA			-1	
#else
	#define rtosDEBUG_SEMA			-1
	#if (rtosDEBUG_SEMA > -1)
		extern SemaphoreHandle_t * pSHmatch;
	#endif
#endif

/**
 * @brief
 * @param[in]	State 0/1 to dis/enable tracking
 */
void xRtosSemaphoreSetTrack(bool State);

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 */
void xRtosSemaphoreSetMatch(SemaphoreHandle_t * Match);

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 * @return		newly initialized semaphore handle
 */
SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t * pSH);

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 * @param[in]	tW number of ticks to wait
 * @return		pdTRUE is taken else pdFALSE
 */
BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSH, TickType_t tW);

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 * @return		pdTRUE is released else pdFALSE
 */
BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSH);

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 */
void vRtosSemaphoreDelete(SemaphoreHandle_t * pSH);

// ################################### Task status manipulation ####################################

#define _EGset(EG,ebX)					xEventGroupSetBits(EG,ebX)
#define _EGclear(EG,ebX)				xEventGroupClearBits(EG,ebX)
#define _EGget(EG,ebX)					(xEventGroupGetBits(EG) & (ebX))
#define _EGcheckAny(EG,ebX)				(xEventGroupGetBits(EG) & (ebX) ? 1 : 0)			// ONE or more match
#define _EGcheck(EG,ebX)				((xEventGroupGetBits(EG) & (ebX)) == (ebX) ? 1 : 0)	// ALL must match
#define _EGwait(EG,ebX, ttW)			((xEventGroupWaitBits(EG,(ebX),pdFALSE,pdTRUE,ttW) & (ebX)) == (ebX))

// ################################### Task status reporting #######################################

bool bRtosStatsUpdateHook(void);
struct report_t;
int	xRtosReportTasks(struct report_t * psRprt);
int xRtosReportMemory(struct report_t * psRprt);
int xRtosReportTimer(struct report_t * psRprt, TimerHandle_t thTimer);

// ################################## Task creation/deletion #######################################

TaskHandle_t xTaskCreateWithMask(const task_param_t * psTP, void * const pvPara);

void vTaskSetTerminateFlags(const EventBits_t uxTaskMask);

// ####################################### Debug support ###########################################

void vTaskDumpStack(void *);

#ifdef __cplusplus
}
#endif
