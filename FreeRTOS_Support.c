//	FreeRTOS_Support.c - Copyright (c) 2015-24 Andre M. MAree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"
#include "FreeRTOS_Support.h"							// Must be before hal_nvic.h"
#include "hal_options.h"
#include "hal_memory.h"
#include "hal_nvic.h"
#include "hal_stdio.h"
#include "printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include "syslog.h"
#include "systiming.h"
#include "errors_events.h"
#include "utilitiesX.h"

#include "esp_debug_helpers.h"
#include <string.h>

#if (buildGUI > 0)
	#include "gui_main.hpp"
#endif

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

#define P							PX
#define IF_P						IF_PX

// ##################################### Semaphore support #########################################

#if	(rtosDEBUG_SEMA > -1)
SemaphoreHandle_t * pSHmatch = NULL;
SemaphoreHandle_t * IgnoreList[] = { &shUARTmux, /* &shTaskInfo &SL_VarMux &SL_NetMux */ };

/**
 * @brief	match semaphore address provided against list entries
 * @param	pSH	pointer to semaphore handle
 * @return	1 if found in list else 0
*/
bool xRtosSemaphoreCheck(SemaphoreHandle_t * pSH) {
	for(int i = 0; i < NO_MEM(IgnoreList); ++i) if (IgnoreList[i] == pSH) return 1;
	return 0;
}

/**
 * @brief	report details regarding semaphore handler, receiver, elapsed time etc..
 * @param	pSH	pointer to semaphore handle
 * @param	pcMess string indicating type of event
 * @param	tElap elapsed time (ticks)
*/
void vRtosSemaphoreReport(SemaphoreHandle_t * pSH, const char * pcMess, TickType_t tElap) {
	if ((pSHmatch && (pSH == pSHmatch)) ||		// Specific match address; or
		(anySYSFLAGS(sfTRACKER) == 1) ||		// general tracking flag enabled; or
		(xRtosSemaphoreCheck(pSH) == 1)) {		// address found in the list; then report
		char *pcHldr = pcTaskGetName(xSemaphoreGetMutexHolder(*pSH));
		char *pcRcvr = pcTaskGetName(xTaskGetCurrentTaskHandle());
		P("SH %s %d %p H=%s R=%s (%lu)" strNL, pcMess, esp_cpu_get_core_id(), pSH, pcHldr, pcRcvr, tElap);
	}
}
#endif

SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t * pSH) {
	*pSH = xSemaphoreCreateMutex();
#if (rtosDEBUG_SEMA > 0)
	if ((pSHmatch && pSH == pSHmatch) ||		// Specific match address; or
		(anySYSFLAGS(sfTRACKER) == 1) || 		// general tracking flag enabled; or
		(xRtosSemaphoreCheck(pSH) == 1)) {		// address found in the list; then
		P("SH Init %p=%p" strNL, pSH, *pSH);		// report the event
	}
#endif
	IF_myASSERT(debugRESULT, *pSH != 0);
	return *pSH;
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSH, TickType_t tWait) {
/*	if (halNVIC_CalledFromISR()) {
		esp_backtrace_print(3); 
		*pSH = NULL;
		return pdTRUE;
	} */
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return pdTRUE;
	if (*pSH == NULL) xRtosSemaphoreInit(pSH);
	bool FromISR = halNVIC_CalledFromISR();
	BaseType_t btRV, btHPTwoken = pdFALSE;
#if	(rtosDEBUG_SEMA > -1)
	tWait = u32RoundUP(tWait,10);
	TickType_t tStep = (tWait == portMAX_DELAY) ? pdMS_TO_TICKS(10000) : tWait / 10;
	TickType_t tElap = 0;
	do {
		if (FromISR) btRV = xSemaphoreTakeFromISR(*pSH, &btHPTwoken);
		else btRV = xSemaphoreTake(*pSH, tStep);
		IF_EXEC_3(rtosDEBUG_SEMA > 0, vRtosSemaphoreReport, pSH, "TAKE", tElap);
		IF_EXEC_1(rtosDEBUG_SEMA > 0, esp_backtrace_print, rtosDEBUG_SEMA); // Decode return addresses [optional]
		if (btRV == pdTRUE) break;
		IF_EXEC_3(rtosDEBUG_SEMA == 0, vRtosSemaphoreReport, pSH, "TAKE", tElap);
		if (tWait != portMAX_DELAY) tWait -= tStep;
		tElap += tStep;
	} while (tWait > tStep);
#else
	if (FromISR) btRV = xSemaphoreTakeFromISR(*pSH, &btHPTwoken);
	else btRV = xSemaphoreTake(*pSH, tWait);
#endif
	if (btHPTwoken == pdTRUE) portYIELD_FROM_ISR();
	return btRV;
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSH) {
//	if (halNVIC_CalledFromISR()) { esp_backtrace_print(3); return pdTRUE; }
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || *pSH == 0)
		return pdTRUE;
	bool FromISR = halNVIC_CalledFromISR();
	BaseType_t btRV, btHPTwoken = pdFALSE;
	if (FromISR) btRV = xSemaphoreGiveFromISR(*pSH, &btHPTwoken);
	else btRV = xSemaphoreGive(*pSH);
#if (rtosDEBUG_SEMA > 0)
	vRtosSemaphoreReport(pSH, "GIVE", 0);
#endif
	if (btHPTwoken == pdTRUE) portYIELD_FROM_ISR(); 
	return btRV;
}

void vRtosSemaphoreDelete(SemaphoreHandle_t * pSH) {
	if (*pSH) vSemaphoreDelete(*pSH);
#if (rtosDEBUG_SEMA > 0)
	if ((pSHmatch && (pSH == pSHmatch)) ||		// Specific match address; or
		(anySYSFLAGS(sfTRACKER) == 1) ||		// general tracking flag enabled; then
		(xRtosSemaphoreCheck(pSH) == 1)) {		// address found in the list; or
		P("SH Delete %p" strNL, pSH);			// report the event
	}
#endif
	*pSH = 0;
}

// ############################### Task Run/Delete status support ##################################

bool bRtosTaskWaitOK(const EventBits_t xEB, TickType_t ttW) {
	// step 1: check if task is meant to delete/terminate, if true return 0
	if (xRtosCheckTaskDELETE(xEB)) return 0;
	// step 2: check if enabled to run again, or wait for period...
	if (xRtosWaitTaskRUN(xEB, ttW) == 0) return 0;
	// step 3: since now definitely enabled to run, check for delete state again
	return xRtosCheckTaskDELETE(xEB) ? 0 : 1;
}

// ################################### Task status reporting #######################################

#if		(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 16)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"---Task Name--- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-16.15s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 15)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"---TaskName--- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-15.14s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 14)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"--Task Name-- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-14.13s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 13)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"--TaskName-- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-13.12s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 12)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"-Task Name- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-12.11s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 11)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"-TaskName- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-11.10s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 10)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"Task Name "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-10.9s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 9)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"TaskName "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-9.8s"
#elif	(CONFIG_FREERTOS_MAX_TASK_NAME_LEN == 8)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"TskName "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-8.7s"
#else
	#error "CONFIG_FREERTOS_MAX_TASK_NAME_LEN is out of range !!!"
#endif

static u64rt_t Active = { 0 };							// Sum non-IDLE tasks
static u8_t NumTasks = 0;								// Currently "active" tasks
static u8_t MaxNum = 0;									// Highest logical task number

static const char TaskState[6] = { 'A', 'R', 'B', 'S', 'D', 'I' };
static TaskHandle_t IdleHandle[portNUM_PROCESSORS] = { 0 };
// table where task status is stored when xRtosReportTasks() is called, avoid alloc/free
static TaskStatus_t	sTS[configFR_MAX_TASKS] = { 0 };
#if	(portNUM_PROCESSORS > 1)
	static const char caMCU[3] = { '0', '1', 'X' };
	static u64rt_t Cores[portNUM_PROCESSORS+1];			// Sum of non-IDLE task runtime/core
	static SemaphoreHandle_t shTaskInfo;
#endif

static TaskStatus_t * psRtosStatsFindWithNumber(UBaseType_t xTaskNumber) {
	IF_myASSERT(debugPARAM, xTaskNumber != 0);
	for (int i = 0; i <= NumTasks; ++i) {
		if (sTS[i].xTaskNumber == xTaskNumber) return &sTS[i];
	}
	return NULL;
}

bool bRtosTaskIsIdleTask(TaskHandle_t xHandle) {
	for (int i = 0; i < portNUM_PROCESSORS; ++i) {
		 if (xHandle == IdleHandle[i]) return 1;
	}
	return 0;
}

int	xRtosReportTasks(report_t * psR) {
	int	iRV = 0;										// reset the character output counter
	if (psR == NULL || psR->sFM.u32Val == 0) return erINV_PARA;
	if (NumTasks == 0) {								// first time once only
		for (int i = 0; i < portNUM_PROCESSORS; ++i) IdleHandle[i] = xTaskGetIdleTaskHandleForCore(i);
	}
	memset(sTS, 0, sizeof(sTS));
	u64_t TotalAdj;
	// Get up-to-date task status
#if (portNUM_PROCESSORS > 1)
	xRtosSemaphoreTake(&shTaskInfo, portMAX_DELAY);
#endif

	NumTasks = uxTaskGetSystemState(sTS, configFR_MAX_TASKS, &TotalAdj);
	IF_myASSERT(debugPARAM, INRANGE(1, NumTasks, configFR_MAX_TASKS));

#if (portNUM_PROCESSORS > 1)
	xRtosSemaphoreGive(&shTaskInfo);
#endif

	TotalAdj /= (100ULL / portNUM_PROCESSORS);			// will be used to calc % for each task...
	if (TotalAdj == 0ULL)			return 0;
	Active.U64val = 0;									// reset overall active running total
#if (portNUM_PROCESSORS > 1)
	memset(&Cores[0], 0, sizeof(Cores));			// reset time/core running totals
#endif
	for (int a = 0; a < NumTasks; ++a) {				// determine value of highest numbered task
		TaskStatus_t * psTS = &sTS[a];
		if (psTS->xTaskNumber > MaxNum) MaxNum = psTS->xTaskNumber;
	}
	xPrintFxSaveLock(psR);
	iRV += wprintfx(psR, "%C", xpfCOL(colourFG_CYAN,0));
	if (psR->sFM.bTskNum)			iRV += wprintfx(psR, "T# ");
	if (psR->sFM.bPrioX)			iRV += wprintfx(psR, "Pc/Pb ");
	iRV += wprintfx(psR, configFREERTOS_TASKLIST_HDR_DETAIL);
	if (psR->sFM.bState)			iRV += wprintfx(psR, "S ");
	if (psR->sFM.bStack)			iRV += wprintfx(psR, "LowS ");
#if (portNUM_PROCESSORS > 1)
	if (psR->sFM.bCore)				iRV += wprintfx(psR, "X ");
#endif
	iRV += wprintfx(psR, " Util Ticks");
	if (debugTRACK && (SL_LEV_DEF > SL_SEV_NOTICE) && psR->sFM.bXtras) iRV += wprintfx(psR, " Stack Base -Task TCB-");
	iRV += wprintfx(psR, "%C" strNL, xpfCOL(attrRESET,0));

	u32_t Units, Fracts, TaskMask = 0x1;				// display individual task info
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if ((psTS == NULL) ||
			(psTS->eCurrentState >= eInvalid) ||
			(psR->sFM.uCount & TaskMask) == 0 ||
			(psTS->uxCurrentPriority >= (UBaseType_t) configMAX_PRIORITIES) ||
			(psTS->uxBasePriority >= configMAX_PRIORITIES))
			goto next;
		// Check for invalid Core ID, often happens in process of shutting down tasks.
		if ((psTS->xCoreID != tskNO_AFFINITY) && !INRANGE(0, psTS->xCoreID, portNUM_PROCESSORS-1)) {
			iRV += wprintfx(psR, "%d CoreID=%d skipped !!!" strNL, a, psTS->xCoreID);
			goto next;
		}
		if (psR->sFM.bTskNum)		iRV += wprintfx(psR, "%2u ", psTS->xTaskNumber);
		if (psR->sFM.bPrioX)		iRV += wprintfx(psR, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
		iRV += wprintfx(psR, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
		if (psR->sFM.bState)		iRV += wprintfx(psR, "%c ", TaskState[psTS->eCurrentState]);
		if (psR->sFM.bStack)		iRV += wprintfx(psR, "%4u ", psTS->usStackHighWaterMark);
	#if (portNUM_PROCESSORS > 1)
		int c = (psTS->xCoreID == tskNO_AFFINITY) ? 2 : psTS->xCoreID;
		if (psR->sFM.bCore)			iRV += wprintfx(psR, "%c ", caMCU[c]);
	#endif
		// Calculate & display individual task utilisation.
		Units = psTS->ulRunTimeCounter / TotalAdj;
		Fracts = ((psTS->ulRunTimeCounter * 100) / TotalAdj) % 100;
		iRV += wprintfx(psR, "%2lu.%02lu %#'5llu", Units, Fracts, psTS->ulRunTimeCounter);

		if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && psR->sFM.bXtras) iRV += wprintfx(psR, " %p %p", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
		if (psR->sFM.bNL)			iRV += wprintfx(psR, strNL);
		// For idle task(s) we do not want to add RunTime % to the task or Core RunTime
		if (bRtosTaskIsIdleTask(psTS->xHandle) == 0) {	// NOT an IDLE task
			Active.U64val += psTS->ulRunTimeCounter;	// Update total active time
		#if (portNUM_PROCESSORS > 1)
			Cores[c].U64val += psTS->ulRunTimeCounter;	// Update core active time
		#endif
		}
next:
		TaskMask <<= 1;
	}

	Units = Active.U64val / TotalAdj;	// Calculate & display total for "real" tasks utilization.
	Fracts = ((Active.U64val * 100) / TotalAdj) % 100;
#if	(portNUM_PROCESSORS > 1)
	iRV += wprintfx(psR, "%u Tasks %lu.%02lu%% [", NumTasks, Units, Fracts);
	for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
		Units = Cores[i].U64val / TotalAdj;
		Fracts = ((Cores[i].U64val * 100) / TotalAdj) % 100;
		iRV += wprintfx(psR, "%c=%lu.%02lu%c", caMCU[i], Units, Fracts, i < 2 ? ' ' : ']');
	}
#else
	iRV += wprintfx(psR, "%u Tasks %lu.%02lu%%", NumTasks, Units, Fracts);
#endif
	iRV += wprintfx(psR, psR->sFM.bNL ? strNLx2 : strNL);
	xPrintFxRestoreUnLock(psR);
	return iRV;
}

static u32_t g_HeapBegin;

void vRtosHeapSetup(void) { g_HeapBegin = xPortGetFreeHeapSize(); }

int xRtosReportMemory(report_t * psR) {
	return wprintfx(psR, "%CFreeRTOS:%C %#'u -> %#'u <- %#'u%s", xpfCOL(colourFG_CYAN,0), xpfCOL(attrRESET,0),
		xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin, repFORM_TST(psR,aNL) ? strNLx2 : strNL);
}

/**
 * @brief	report config & status of timer specified
 * @param	psR pointer to report control structure
 * @param	thTmr timer handle to be reported on
 * @return	size of character output generated
 * @note	DOES specifically lock/unlock console UART
*/
int xRtosReportTimer(report_t * psR, TimerHandle_t thTmr) {
	int iRV;
	if (halMemorySRAM(thTmr)) {
		TickType_t tPer = xTimerGetPeriod(thTmr);
		TickType_t tExp = xTimerGetExpiryTime(thTmr);
		i32_t tRem = tExp - xTaskGetTickCount();
		BaseType_t bActive = xTimerIsTimerActive(thTmr); 
		iRV = wprintfx(psR, "%C%s%C\t#%lu  Auto=%c  Run=%c", xpfCOL(colourFG_CYAN,0), pcTimerGetName(thTmr), xpfCOL(attrRESET,0),
			uxTimerGetTimerNumber(thTmr), uxTimerGetReloadMode(thTmr) ? CHR_Y : CHR_N, bActive ? CHR_Y : CHR_N);
		if (bActive) iRV += wprintfx(psR, "  tPeriod=%#'lu  tExpiry=%#'lu  tRemain=%#'ld", tPer, tExp, tRem);
	} else {
		iRV = wprintfx(psR, "\t%p Invalid Timer handle", thTmr);
	}
	if (repFORM_TST(psR,aNL)) iRV += wprintfx(psR, strNL);
	return iRV;
}

// ################################## Task creation/deletion #######################################

BaseType_t __real_xTaskCreate(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, TaskHandle_t *);
BaseType_t __real_xTaskCreatePinnedToCore(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, TaskHandle_t *, const BaseType_t);
TaskHandle_t __real_xTaskCreateStatic(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, StackType_t * const, StaticTask_t * const);
TaskHandle_t __real_xTaskCreateStaticPinnedToCore(TaskFunction_t, const char * const, const u32_t, void *, UBaseType_t, StackType_t * const, StaticTask_t * const, const BaseType_t);
void __real_vTaskDelete(TaskHandle_t xHandle);

#if 0
#define buildMAX_TASKS		(sizeof(u32_t) * BITS_IN_BYTE)
static u32_t TaskTracker = 0;

BaseType_t __wrap_xTaskCreate(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * pvParameters, UBaseType_t uxPriority, TaskHandle_t * pxCreatedTask) {
	TASK_START(pcName);
#if	(portNUM_PROCESSORS > 1)
	BaseType_t btSR = xRtosSemaphoreTake(&shTaskInfo, portMAX_DELAY);
#endif
	BaseType_t btRV = __real_xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
	IF_myASSERT(debugRESULT, btRV == pdPASS);
	int i; for (i = 0; i < buildMAX_TASKS && TaskTracker & (1UL << i); ++i);
	IF_myASSERT(debugTRACK, i < buildMAX_TASKS);
	TaskTracker |= 1UL << i;
	vTaskSetThreadLocalStoragePointer(*pxCreatedTask, buildFRTLSP_EVT_MASK, (void *)(1UL << i));
#if	(portNUM_PROCESSORS > 1)
	if (btSR == pdTRUE) xRtosSemaphoreGive(&shTaskInfo);
#endif
	return btRV;
}

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * pvParameters, UBaseType_t uxPriority, TaskHandle_t * pxCreatedTask, const BaseType_t xCoreID) {
	TASK_START(pcName);
#if	(portNUM_PROCESSORS > 1)
	BaseType_t btSR = xRtosSemaphoreTake(&shTaskInfo, portMAX_DELAY);
#endif
	BaseType_t btRV = __real_xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
	IF_myASSERT(debugRESULT, btRV == pdPASS);
	int i; for (i = 0; i < buildMAX_TASKS && TaskTracker & (1UL << i); ++i);
	IF_myASSERT(debugTRACK, i < buildMAX_TASKS);
	TaskTracker |= 1UL << i;
	vTaskSetThreadLocalStoragePointer(*pxCreatedTask, buildFRTLSP_EVT_MASK, (void *)(1UL << i));
#if	(portNUM_PROCESSORS > 1)
	if (btSR == pdTRUE) xRtosSemaphoreGive(&shTaskInfo);
#endif
	return btRV;
}

TaskHandle_t __wrap_xTaskCreateStatic(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, StackType_t * const pxStackBuffer, StaticTask_t * const pxTaskBuffer) {
	TASK_START(pcName);
#if	(portNUM_PROCESSORS > 1)
	BaseType_t btSR = xRtosSemaphoreTake(&shTaskInfo, portMAX_DELAY);
#endif
	TaskHandle_t thRV = __real_xTaskCreateStatic(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer);
	IF_myASSERT(debugRESULT, thRV != 0);
	int i; for (i = 0; i < buildMAX_TASKS && TaskTracker & (1UL << i); ++i);
	IF_myASSERT(debugTRACK, i < buildMAX_TASKS);
	TaskTracker |= 1UL << i;
	vTaskSetThreadLocalStoragePointer(thRV, buildFRTLSP_EVT_MASK, (void *)(1UL << i));
#if	(portNUM_PROCESSORS > 1)
	if (btSR == pdTRUE) xRtosSemaphoreGive(&shTaskInfo);
#endif
	return thRV;
}

TaskHandle_t __wrap_xTaskCreateStaticPinnedToCore(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, StackType_t * const pxStackBuffer, StaticTask_t * const pxTaskBuffer, const BaseType_t xCoreID) {
	TASK_START(pcName);
#if	(portNUM_PROCESSORS > 1)
	BaseType_t btSR = xRtosSemaphoreTake(&shTaskInfo, portMAX_DELAY);
#endif
	TaskHandle_t thRV = __real_xTaskCreateStaticPinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer, xCoreID);
	IF_myASSERT(debugRESULT, thRV != 0);
	int i; for (i = 0; i < buildMAX_TASKS && TaskTracker & (1UL << i); ++i);
	IF_myASSERT(debugTRACK, i < buildMAX_TASKS);
	TaskTracker |= 1UL << i;
	vTaskSetThreadLocalStoragePointer(thRV, buildFRTLSP_EVT_MASK, (void *)(1UL << i));
#if	(portNUM_PROCESSORS > 1)
	if (btSR == pdTRUE) xRtosSemaphoreGive(&shTaskInfo);
#endif
	return thRV;
}

/**
 * @brief	Clear task runtime and static statistics data then delete the task
 * @param	Handle of task to be terminated (NULL = calling task)
 */
void __wrap_vTaskDelete(TaskHandle_t xHandle) {
#if (debugTRACK)
	char caName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN+1];
	strncpy(caName, pcTaskGetName(xHandle), CONFIG_FREERTOS_MAX_TASK_NAME_LEN);
#endif
	if (xHandle == NULL) xHandle = xTaskGetCurrentTaskHandle();
	EventBits_t ebX = (EventBits_t) pvTaskGetThreadLocalStoragePointer(xHandle, 1);
	if (ebX) {
		TaskTracker &= ~(ebX);							// clear task mask
		xRtosClearTaskRUN(ebX);							// clear RUN and
		xRtosClearTaskDELETE(ebX);						// DELete flags
		MESSAGE("[%s] RUN/DELETE flags cleared" strNL, caName);
	}
	__real_vTaskDelete(xHandle);
	TASK_STOP(caName);
}

#else

BaseType_t __wrap_xTaskCreate(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * pvParameters, UBaseType_t uxPriority, TaskHandle_t * pxCreatedTask) {
	TASK_START(pcName);
	BaseType_t btRV = __real_xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
	IF_myASSERT(debugRESULT, btRV == pdPASS);
	return btRV;
}

BaseType_t __wrap_xTaskCreatePinnedToCore(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * pvParameters, UBaseType_t uxPriority, TaskHandle_t * pxCreatedTask, const BaseType_t xCoreID) {
	TASK_START(pcName);
	BaseType_t btRV = __real_xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
	IF_myASSERT(debugRESULT, btRV == pdPASS);
	return btRV;
}

TaskHandle_t __wrap_xTaskCreateStatic(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, StackType_t * const pxStackBuffer, StaticTask_t * const pxTaskBuffer) {
	TASK_START(pcName);
	TaskHandle_t thRV = __real_xTaskCreateStatic(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer);
	IF_myASSERT(debugRESULT, thRV != 0);
	return thRV;
}

TaskHandle_t __wrap_xTaskCreateStaticPinnedToCore(TaskFunction_t pxTaskCode, const char * const pcName, const u32_t usStackDepth, void * const pvParameters, UBaseType_t uxPriority, StackType_t * const pxStackBuffer, StaticTask_t * const pxTaskBuffer, const BaseType_t xCoreID) {
	TASK_START(pcName);
	TaskHandle_t thRV = __real_xTaskCreateStaticPinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer, xCoreID);
	IF_myASSERT(debugRESULT, thRV != 0);
	return thRV;
}

/**
 * @brief	Clear task runtime and static statistics data then delete the task
 * @param	Handle of task to be terminated (NULL = calling task)
 */
void __wrap_vTaskDelete(TaskHandle_t xHandle) {
#if (debugTRACK)
	char caName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN+1];
	strncpy(caName, pcTaskGetName(xHandle), CONFIG_FREERTOS_MAX_TASK_NAME_LEN);
#endif
	if (xHandle == NULL) xHandle = xTaskGetCurrentTaskHandle();
	EventBits_t ebX = (EventBits_t) pvTaskGetThreadLocalStoragePointer(xHandle, 1);
	if (ebX) {
		xRtosClearTaskRUN(ebX);							// clear RUN and
		xRtosClearTaskDELETE(ebX);						// DELete flags
		MESSAGE("[%s] RUN/DELETE flags cleared" strNL, caName);
	}
	__real_vTaskDelete(xHandle);
	TASK_STOP(caName);
}
#endif

/**
 * @brief	Set/clear all flags to force task[s] to initiate an organised shutdown
 * @param	mask indicating the task[s] to terminate
 */
void vTaskSetTerminateFlags(const EventBits_t uxTaskMask) {
#if (halUSE_BSP == 1 && buildGUI == 4)
	if (uxTaskMask & taskGUI_MASK) 	vGuiDeInit();
#endif
	xRtosSetTaskDELETE(uxTaskMask);
	xRtosSetTaskRUN(uxTaskMask);						// must enable run to trigger delete
}

// ####################################### Debug support ###########################################

/*
 * 	FreeRTOS TCB structure as of 8.2.3
 * 	00 - 03			pxTopOfStack
 * 	?? - ??	04 - 08 MPU wrappers
 * 	04 - 23			xGenericListItem
 * 	24 - 43			xEventListItem
 * 	44 - 47			uxPriority
 * 	48 - 51			pxStack. If stack growing downwards, end of stack
 * 	?? - ??			pxEndOfStack
 * 	Example code:
	u32_t	OldStackMark, NewStackMark;
	OldStackMark = uxTaskGetStackHighWaterMark(NULL);
   	NewStackMark = uxTaskGetStackHighWaterMark(NULL);
   	if (NewStackMark != OldStackMark) {
   		vFreeRTOSDumpStack(NULL, STACK_SIZE);
   		OldStackMark = NewStackMark;
   	}
 */
void vTaskDumpStack(void * pTCB) {
	if (pTCB == NULL)
		pTCB = xTaskGetCurrentTaskHandle();
	void * pxTOS = (void *) * ((u32_t *) pTCB) ;
	void * pxStack = (void *) * ((u32_t *) pTCB + 12);		// 48 bytes / 4 = 12
	wprintfx(NULL, "Cur SP : %p - Stack HWM : %p" strNL, pxTOS,
		(u8_t *) pxStack + (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
}

//#include "freertos/task_snapshot.h"
// note for ESP32S3, you must alter esp_backtrace_print_from_frame to whitelist 0x400559DD
// as a valid memory address. see: https://github.com/espressif/esp-idf/issues/11512#issuecomment-1566943121
// Otherwise nearly all the backtraces will print as corrupt.
//
//	//Check if first frame is valid
//	bool corrupted = !(esp_stack_ptr_is_sane(stk_frame.sp) &&
//					   (esp_ptr_executable((void *)esp_cpu_process_stack_pc(stk_frame.pc)) ||
//		 /*whitelist*/  esp_cpu_process_stack_pc(stk_frame.pc) == 0x400559DD ||
//						/* Ignore the first corrupted PC in case of InstrFetchProhibited */
//					   (stk_frame.exc_frame && ((XtExcFrame *)stk_frame.exc_frame)->exccause == EXCCAUSE_INSTR_PROHIBITED)));
/*
esp_err_t IRAM_ATTR esp_backtrace_print_all_tasks(int depth, bool panic) {
	u32_t task_count = uxTaskGetNumberOfTasks();
	TaskSnapshot_t* snapshots = (TaskSnapshot_t*) calloc(task_count * sizeof(TaskSnapshot_t), 1);
	// get snapshots
	UBaseType_t tcb_size = 0;
	u32_t got = uxTaskGetSnapshotAll(snapshots, task_count, &tcb_size);
	u32_t len = got < task_count ? got : task_count;
	print_str("printing all tasks:\n\n", panic);
	esp_err_t err = ESP_OK;
	for (u32_t i = 0; i < len; i++) {
		TaskHandle_t handle = (TaskHandle_t) snapshots[i].pxTCB;
		char* name = pcTaskGetName(handle);
		print_str(name ? name : "No Name", panic);
		XtExcFrame* xtf = (XtExcFrame*)snapshots[i].pxTopOfStack;
		esp_backtrace_frame_t frame = { .pc = xtf->pc, .sp = xtf->a1, .next_pc = xtf->a0, .exc_frame = xtf };
		esp_err_t nerr = esp_backtrace_print_from_frame(depth, &frame, panic);
		if (nerr != ESP_OK) err = nerr;
	}
	free(snapshots);
	return err;
}
*/
