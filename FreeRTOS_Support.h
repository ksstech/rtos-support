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

#if (configPRODUCTION == 1)			// -1 = disable
	#define rtosDEBUG_SEMA			-1	
#else								// -1=disable, 0=block only, >0=all incl return addresses
	#define rtosDEBUG_SEMA			-1
#endif

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

#if (configPRODUCTION == 0) && (rtosDEBUG_SEMA > -1)
	extern SemaphoreHandle_t * pSHmatch;
#endif

// ##################################### global function prototypes ################################

void vRtosHeapSetup(void);

void myApplicationTickHook(void);
void vApplicationStackOverflowHook(TaskHandle_t, char *);
void vApplicationMallocFailedHook(void);

// ##################################### Semaphore support #########################################

SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t *);
BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t *, TickType_t);
BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t *);
void vRtosSemaphoreDelete(SemaphoreHandle_t *);

// ################################### Task status manipulation ####################################

#define _EGset(EG,ebX)					xEventGroupSetBits(EG,ebX)
#define _EGclear(EG,ebX)				xEventGroupClearBits(EG,ebX)
#define _EGget(EG,ebX)					(xEventGroupGetBits(EG) & (ebX))
#define _EGcheckAny(EG,ebX)				(xEventGroupGetBits(EG) & (ebX) ? 1 : 0)			// ONE or more match
#define _EGcheck(EG,ebX)				((xEventGroupGetBits(EG) & (ebX)) == (ebX) ? 1 : 0)	// ALL must match
#define _EGwait(EG,ebX, ttW)			((xEventGroupWaitBits(EG,(ebX),pdFALSE,pdTRUE,ttW) & (ebX)) == (ebX))

/**
 * @brief
 * @return
 */
#define xRtosSetStat0(ebX)				_EGset(xEventStat0,ebX)
#define xRtosSetStat1(ebX)				_EGset(xEventStat1,ebX)
#define xRtosSetDevice(ebX)				_EGset(xEventDevices,ebX)
#define xRtosSetTaskRUN(ebX)			_EGset(TaskRunState,ebX)
#define xRtosSetTaskDELETE(ebX)			_EGset(TaskDeleteState,ebX)

/**
 * @brief
 * @return	selected bitmask from EventStatus BEFORE bits were cleared
 */
#define xRtosClearStat0(ebX)			_EGclear(xEventStat0,ebX)
#define xRtosClearStat1(ebX)			_EGclear(xEventStat1,ebX)
#define xRtosClearDevice(ebX)			_EGclear(xEventDevices,ebX)
#define xRtosClearTaskRUN(ebX)			_EGclear(TaskRunState,ebX)
#define xRtosClearTaskDELETE(ebX)		_EGclear(TaskDeleteState,ebX)

/**
 * @brief
 * @return	selected bitmask from EventStatus as at time of call
 */
#define xRtosGetStat0(ebX)				_EGget(xEventStat0,ebX)
#define xRtosGetStat1(ebX)				_EGget(xEventStat1,ebX)
#define xRtosGetDevice(ebX)				_EGget(xEventDevices,ebX)
#define xRtosGetTaskRUN(ebX)			_EGget(TaskRunState,ebX)
#define xRtosGetTaskDELETE(ebX)			_EGget(TaskDeleteState,ebX)

/**
 * @brief
 * @return	1/true if ALL the bits set in ebX are all also set in EventStatus at calling time
 */
#define xRtosCheckStat0(ebX)			_EGcheck(xEventStat0,ebX)
#define xRtosCheckStat1(ebX)			_EGcheck(xEventStat1,ebX)
#define xRtosCheckAnyStat1(ebX)			_EGcheckAny(xEventStat1,ebX)
#define xRtosCheckDevice(ebX)			_EGcheck(xEventDevices,ebX)

/**
 * @brief	Check if task set to RUN/DELETE
 * @return	1 if so else 0
 */
#define xRtosCheckTaskRUN(ebX)			_EGcheck(TaskRunState,ebX)
#define xRtosCheckTaskDELETE(ebX)		_EGcheck(TaskDeleteState,ebX)

/**
 * @brief	wait for specified period of time for ALL bits set in ebX to become set in EventStatus
 * @return	1/true if ALL the bits set in ebX are all also set in EventStatus prior to timeout
 */
#define xRtosWaitStat0(ebX, ttW)		_EGwait(xEventStat0,ebX,ttW)
#define xRtosWaitStat1(ebX, ttW)		_EGwait(xEventStat1,ebX,ttW)
#define xRtosWaitDevice(ebX, ttW)		_EGwait(xEventDevices,ebX,ttW)

/**
 * @brief	Wait (for period) till task set to DELETE, return 1 (DELETE bit set before timeout) else 0
 */
#define xRtosWaitTaskRUN(ebX, ttW)		_EGwait(TaskRunState,ebX,ttW)
#define xRtosWaitTaskDELETE(ebX, ttW)	_EGwait(TaskDeleteState,ebX,ttW)

/**
 * @brief	Check if task set to RUN AND NOT set to DELETE, return 1 (task set to run but not delete) else 0
 */
#define bRtosTaskCheckOK(ebX)			((xRtosCheckTaskDELETE(ebX) && xRtosCheckTaskRUN(ebX)) ? 0 : 1)

/**
 * @brief	check if a task should a) terminate or b) run
 *			if, at entry, set to terminate immediately return result
 * 			if not, wait (possibly 0 ticks) for run status
 *			Before returning, again check if set to terminate.
 * @param	uxTaskMask - specific task bitmap, if NULL will get current mask from task LSP0
 * @return	0 if task should delete, 1 if it should run...
 */
bool bRtosTaskWaitOK(u32_t TaskMask, TickType_t ttW);

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
