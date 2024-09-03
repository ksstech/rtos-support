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
#define	MALLOC_CHECK()	y=xPortGetFreeHeapSize();IF_TRACK(y<x,"%u->%u (%d)\r\n",x,y,y-x);

#define MESSAGE(mess,...)	IF_PX(debugTRACK && ioB1GET(ioUpDown), mess, ##__VA_ARGS__)
#define TASK_START(name) 	MESSAGE("[%s] starting\r\n", name)
#define TASK_STOP(name) 	MESSAGE("[%s] stopping\r\n", name)

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

// ##################################### Malloc/free support #######################################
// ################################### Task status manipulation ####################################
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
