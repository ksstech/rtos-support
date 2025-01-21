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

// ################################### Task status reporting #######################################

bool bRtosStatsUpdateHook(void);
struct report_t;
int	xRtosReportTasks(struct report_t * psRprt);
int xRtosReportMemory(struct report_t * psRprt);
int xRtosReportTimer(struct report_t * psRprt, TimerHandle_t thTimer);

// ################################## Task creation/deletion #######################################

TaskHandle_t xTaskCreateWithMask(const task_param_t * psTP, void * const pvPara);

void vTaskSetTerminateFlags(const EventBits_t uxTaskMask);

#if (buildWRAP_TASKS == 1)
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
