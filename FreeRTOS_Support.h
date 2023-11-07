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

#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// #######################################  Build macros ###########################################

#define configFR_MAX_TASKS	24

// ##################################### MACRO definitions #########################################

#if (tskKERNEL_VERSION_MAJOR >= 10 && tskKERNEL_VERSION_MINOR >= 5 && tskKERNEL_VERSION_BUILD >= 0)
	#define configRUNTIME_SIZE	8
#else
	#define configRUNTIME_SIZE	4
#endif

#define	MALLOC_MARK()	u32_t y,x=xPortGetFreeHeapSize();
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)\r\n",x,y,y-x);

// ###################################### BUILD : CONFIG definitions ##############################

#define rtosDEBUG_SEMA			-1			// -1=disable, 0=no return Address, >0=add return addresses
#define rtosDEBUG_SEMA_HLDR		0
#define rtosDEBUG_SEMA_RCVR		0

// ############################################ Enumerations #######################################


// #################################### FreeRTOS global variables ##################################

#if (configPRODUCTION == 0) && (rtosDEBUG_SEMA > -1)
	extern SemaphoreHandle_t * pSHmatch;
#endif

// ##################################### global function prototypes ################################

void myApplicationTickHook(void);
void vApplicationStackOverflowHook(TaskHandle_t, char *);
void vApplicationMallocFailedHook(void);

// ##################################### Semaphore support #########################################

SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t *);
BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t *, TickType_t);
BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t *);
void vRtosSemaphoreDelete(SemaphoreHandle_t *);

// ##################################### Malloc/free support #######################################

void * pvRtosMalloc(size_t S);
void vRtosFree(void * pV);
void vRtosHeapSetup(void);

// ################################### Task status manipulation ####################################


// ################################### Task status reporting #######################################

bool bRtosStatsUpdateHook(void);
struct report_t;
int	xRtosReportTasks(struct report_t * psRprt);
int xRtosReportMemory(struct report_t * psRprt);
int xRtosReportTimer(struct report_t * psRprt, TimerHandle_t thTimer);

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
