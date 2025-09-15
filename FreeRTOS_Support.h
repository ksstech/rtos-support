// FreeRTOS_Support.h

#pragma	once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "definitions.h"
#include "options.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Macros ###############################################

#define configFR_MAX_TASKS	24

#define	MALLOC_MARK()	u32_t y,x=xPortGetFreeHeapSize();
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)" strNL,x,y,y-x);

#define MESSAGE(mess,...)	IF_PX(debugTRACK && OPT_GET(ioUpDown), mess, ##__VA_ARGS__)
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

#if (appPRODUCTION > 0)
	#define rtosSEMA_DEBUG			0	
#else
	#define rtosSEMA_DEBUG			1
	extern SemaphoreHandle_t * pSHmatch;
#endif

#if	(rtosSEMA_DEBUG > 0)

/**
 * @brief
 * @param[in]	pSH pointer to semaphore handle
 */
void xRtosSemaphoreSetMatch(SemaphoreHandle_t * Match);

#endif

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

/**
 * @brief	check whether the current task holds the semaphore
 * @param	pSH	pointer to SemaphoreHandle_t to be checked
 * @return	pdTRUE if current task holds the semaphore, else pdFALSE
 */
BaseType_t xRtosSemaphoreCheckCurrent(SemaphoreHandle_t * pSH);

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

/**
 * @brief		Creates a FreeRTOS task with additional tracking and validation based on a bitmask
 * @param[in]	psTP pointer to task parameter structure
 * @param[in]	pvPara pointer to task parameter
 * @return		TaskHandle_t of the created task
 * @note		Ensures bitmask has a single bit set, verifies no duplicate bit is already tracked.
 * 				Ppdates task tracker and associates bitmask with task using thread-local storage.
 * 				Supporting multi-core systems and optional task wrapping.
 */
TaskHandle_t xTaskCreateWithMask(const task_param_t * psTP, void * const pvPara);

/**
 * @brief	Set/clear all flags to force task[s] to initiate an organised shutdown
 * @param[in]	uxTaskMask indicating the task[s] to terminate
 * @note		Sets termination flags for tasks specified by uxTaskMask.
 * 				Optionally handles GUI task de-initialisation when using LVGL with BSP.
 * 				Updates task states to mark them for deletion and enable their termination process.
 */
void vTaskSetTerminateFlags(EventBits_t uxTaskMask);

#if (cmakeWRAP_TASKS == 1)
BaseType_t __real_xTaskCreate(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, TaskHandle_t *);
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, TaskHandle_t *, const BaseType_t);
TaskHandle_t __real_xTaskCreateStatic(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, StackType_t * const, StaticTask_t * const);
TaskHandle_t __real_xTaskCreateStaticPinnedToCore(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, StackType_t * const, StaticTask_t * const, const BaseType_t);
void __real_vTaskDelete(TaskHandle_t xHandle);
#endif

// ####################################### Debug support ###########################################

void vTaskDumpStack(void *);

#ifdef __cplusplus
}
#endif
