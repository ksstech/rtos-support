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

#if defined(CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U64) && (CONFIG_FREERTOS_RUN_TIME_COUNTER_TYPE_U64 == 1)
	#define configRUNTIME_SIZE	8
#else
	#error "Must be built with 64bit Runtime Counter, support for 32bit removed !!!"
#endif

// ######################################## Enumerations ###########################################

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
#define _EGcheck(EG,ebX)				((xEventGroupGetBits(EG) & (ebX)) == (ebX))
#define _EGwait(EG,ebX, ttW)			((xEventGroupWaitBits(EG,(ebX),pdFALSE,pdTRUE,ttW) & (ebX)) == (ebX))

/**
 * @brief
 * @return
 */
#define xRtosSetStatus(ebX)				_EGset(xEventStatus,ebX)
/**
 * @brief
 * @return	selected bitmask from EventStatus as at time of call
 */
#define xRtosGetStatus(ebX)				_EGget(xEventStatus,ebX)
/**
 * @brief
 * @return	selected bitmask from EventStatus BEFORE bits were cleared
 */
#define xRtosClearStatus(ebX)			_EGclear(xEventStatus,ebX)
/**
 * @brief
 * @return	1/true if ALL the bits set in ebX are all also set in EventStatus at calling time
 */
#define xRtosCheckStatus(ebX)			_EGcheck(xEventStatus,ebX)
/**
 * @brief	wait for specified period of time for ALL bits set in ebX to become set in EventStatus
 * @return	1/true if ALL the bits set in ebX are all also set in EventStatus prior to timeout
 */
#define xRtosWaitStatus(ebX, ttW)		_EGwait(xEventStatus,ebX,ttW)

#define xRtosSetDevice(ebX)				_EGset(EventDevices,ebX)
#define xRtosClearDevice(ebX)			_EGclear(EventDevices,ebX)
#define xRtosCheckDevice(ebX)			_EGcheck(EventDevices,ebX)
#define xRtosWaitDevice(ebX, ttW)		_EGwait(EventDevices,ebX,ttW)

#define xRtosSetTaskRUN(ebX)			_EGset(TaskRunState,ebX)
#define xRtosClearTaskRUN(ebX)			_EGclear(TaskRunState,ebX)
#define xRtosCheckTaskRUN(ebX)			_EGcheck(TaskRunState,ebX)
#define xRtosWaitTaskRUN(ebX, ttW)		_EGwait(TaskRunState,ebX,ttW)

#define xRtosSetTaskDELETE(ebX)			_EGset(TaskDeleteState,ebX)
#define xRtosClearTaskDELETE(ebX)		_EGclear(TaskDeleteState,ebX)
/**
 * @brief	Check if task set to DELETE, return 1 if so else 0
 */
#define xRtosCheckTaskDELETE(ebX)		_EGcheck(TaskDeleteState,ebX)
#define xRtosWaitTaskDELETE(ebX, ttW)	_EGwait(TaskDeleteState,ebX,ttW)

// Combined RUN & DELETE checks
#define bRtosTaskCheckOK(ebX)			((xRtosCheckTaskDELETE(ebX) || !xRtosCheckTaskRUN(ebX)) ? 0 : 1)

/**
 * @brief	check if a task should a) terminate or b) run
 *			if, at entry, set to terminate immediately return result
 * 			if not, wait (possibly 0 ticks) for run status
 *			Before returning, again check if set to terminate.
 * @param	uxTaskMask - specific task bitmap
 * @return	0 if task should delete, 1 if it should run...
 */
bool bRtosTaskWaitOK(const u32_t ebX, u32_t ttW);

// ################################### Task status reporting #######################################

bool bRtosStatsUpdateHook(void);
struct report_t;
int	xRtosReportTasks(struct report_t * psRprt);
int xRtosReportMemory(struct report_t * psRprt);
int xRtosReportTimer(struct report_t * psRprt, TimerHandle_t thTimer);

// ################################## Task creation/deletion #######################################

void vTaskSetTerminateFlags(const EventBits_t uxTaskMask);

// ####################################### Debug support ###########################################

void vTaskDumpStack(void *);

#ifdef __cplusplus
}
#endif
