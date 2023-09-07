/*
 *	FreeRTOS_Support.c
 *	Copyright (c) 2015-22 Andre M. MAree / KSS Technologies (Pty) Ltd.
 */

#include "hal_variables.h"			// required by options.h

#include "FreeRTOS_Support.h"							// Must be before hal_nvic.h"
#include "hal_nvic.h"
#include "hal_mcu.h"									// halMCU_ReportMemory
#include "options.h"
#include "printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include "syslog.h"
#include "systiming.h"
#include "x_errors_events.h"

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #######################################  Build macros ###########################################


// #################################### FreeRTOS global variables ##################################

static u32_t g_HeapBegin;

// ################################# FreeRTOS heap & stack  ########################################

/*
 * Required to handle FreeRTOS heap_5.c implementation
 * The array is terminated using a NULL zero sized region definition, and the
 * memory regions defined in the array ***must*** appear in address order from
 * low address to high address.
 */
#if	 defined(cc3200) && defined( __TI_ARM__ )
	extern	u32_t	__TI_static_base__, __HEAP_SIZE;
	HeapRegion_t xHeapRegions[] = {
		{ ( u8_t * ) SRAM_BASE,				SRAM1_SIZE 				},	// portion of memory used by bootloader
		{ ( u8_t * )	&__TI_static_base__, 	(size_t) &__HEAP_SIZE	},
		{ ( u8_t * ) NULL, 					0						},
	};

#elif defined(HW_P_PHOTON) && defined( __CC_ARM )
	extern	u8_t		Image$$RW_IRAM1$$ZI$$Limit[];
	extern	u8_t		Image$$ARM_LIB_STACK$$ZI$$Base[];
	HeapRegion_t xHeapRegions[] = {
		{ Image$$RW_IRAM1$$ZI$$Limit,	(size_t) Image$$ARM_LIB_STACK$$ZI$$Base } ,
		{ NULL,							0 }
	};

#elif defined(HW_P_PHOTON) && defined( __GNUC__ )
	extern	u8_t		__HEAP_BASE[], __HEAP_SIZE[];
	HeapRegion_t xHeapRegions[] = {
		{ __HEAP_BASE,	(size_t) __HEAP_SIZE } ,
		{ NULL,					0 }
	};
#endif

void vRtosHeapSetup(void ) {
	#if defined(HW_P_PHOTON ) && defined( __CC_ARM )
	xHeapRegions[0].xSizeInBytes	-= (size_t) Image$$RW_IRAM1$$ZI$$Limit;
	vPortDefineHeapRegions(xHeapRegions);
	#elif defined( cc3200 ) && defined( __TI_ARM__ )
	vPortDefineHeapRegions(xHeapRegions);
	#endif
	g_HeapBegin = xPortGetFreeHeapSize();
}

// ################################### Forward declarations ########################################

TaskStatus_t * psRtosStatsFindWithHandle(TaskHandle_t);

// ##################################### Malloc/free support #######################################

void * pvRtosMalloc(size_t S) {
	void * pV = malloc(S);
	IF_myASSERT(debugRESULT, pV);
	IF_CP(debugTRACK && ioB1GET(ioMemory), "malloc %p:%u\r\n", pV, S);
	return pV;
}

void vRtosFree(void * pV) {
	IF_CP(debugTRACK && ioB1GET(ioMemory), " free  %p\r\n", pV);
	free(pV);
}

// ################################### Event status manipulation ###################################

inline EventBits_t xRtosSetStatus(const EventBits_t ebX) {
	return xEventGroupSetBits(xEventStatus, ebX);
}

inline EventBits_t xRtosGetStatus(const EventBits_t ebX) {
	return xEventGroupGetBits(xEventStatus) & ebX;
}

inline EventBits_t xRtosWaitStatusANY(EventBits_t ebX, TickType_t ttW) {
	return xEventGroupWaitBits(xEventStatus, ebX, pdFALSE, pdFALSE, ttW) & ebX;
}

inline bool bRtosWaitStatusALL(EventBits_t ebX, TickType_t ttW) {
	return (xEventGroupWaitBits(xEventStatus, ebX, pdFALSE, pdTRUE, ttW) & ebX) == ebX ? 1 : 0;
}

inline bool bRtosCheckStatus(const EventBits_t ebX) {
	return (xEventGroupGetBits(xEventStatus) & ebX) == ebX ? 1 : 0;
}

// ################################ Device EVENT status support ####################################

inline EventBits_t xRtosSetEVTdevice(const EventBits_t ebX) {
	return xEventGroupSetBits(EventDevices, ebX);
}

inline EventBits_t xRtosGetEVTdevice(const EventBits_t ebX) {
	return xEventGroupGetBits(EventDevices) & ebX;
}

inline bool bRtosWaitEVTdevices(EventBits_t ebX, TickType_t ttW) {
	return (xEventGroupWaitBits(EventDevices, ebX, pdFALSE, pdTRUE, ttW) & ebX) == ebX ? 1 : 0;
}

inline bool bRtosCheckEVTdevices(const EventBits_t ebX) {
	return (xEventGroupGetBits(EventDevices) & ebX) == ebX ? 1 : 0;
}

// ################################### Task status manipulation ####################################

inline EventBits_t xRtosTaskSetRUN(EventBits_t ebX) {
	return xEventGroupSetBits(TaskRunState, ebX);
}

inline EventBits_t xRtosTaskClearRUN(EventBits_t ebX) {
	return xEventGroupClearBits(TaskRunState, ebX);
}

inline EventBits_t xRtosTaskWaitRUN(EventBits_t ebX, TickType_t ttW) {
	return xEventGroupWaitBits(TaskRunState, ebX, pdFALSE, pdTRUE, ttW);
}

inline EventBits_t xRtosTaskSetDELETE(EventBits_t ebX) {
	return xEventGroupSetBits(TaskDeleteState, ebX);
}

inline EventBits_t xRtosTaskClearDELETE(EventBits_t ebX) {
	return xEventGroupClearBits(TaskDeleteState, ebX);
}

inline EventBits_t xRtosTaskWaitDELETE(EventBits_t ebX, TickType_t ttW) {
	return xEventGroupWaitBits(TaskDeleteState, ebX, pdFALSE, pdTRUE, ttW);
}

bool bRtosTaskCheckOK(const EventBits_t ebX) {
	// step 1: if task is meant to delete/terminate, PROBLEM !!!
	if ((xEventGroupGetBits(TaskDeleteState) & ebX) == ebX ||
		(xEventGroupGetBits(TaskRunState) & ebX) != ebX)
		return 0;
	return 1;
}

/**
 * @brief	check if a task should a) terminate or b) run
 *			if, at entry, set to terminate immediately return result
 * 			if not, wait (possibly 0 ticks) for run status
 *			Before returning, again check if set to terminate.
 * @param	uxTaskMask - specific task bitmap
 * @return	0 if task should delete, 1 if it should run...
 */
bool bRtosTaskWaitOK(const EventBits_t xEB, TickType_t ttW) {
	// step 1: if task is meant to delete/terminate, inform it as such
	if ((xEventGroupGetBits(TaskDeleteState) & xEB) == xEB) return 0;

	// step 2: if not meant to terminate, check if/wait until enabled to run again
	xEventGroupWaitBits(TaskRunState, xEB, pdFALSE, pdTRUE, ttW);

	// step 3: since now definitely enabled to run, check for delete state again
	return ((xEventGroupGetBits(TaskDeleteState) & xEB) == xEB) ? 0 : 1;
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

static u64rt_t Total;									// Sum all tasks (incl IDLE)
static u64rt_t Active;									// Sum non-IDLE tasks
static u8_t NumTasks;									// Currently "active" tasks
static u8_t MaxNum;										// Highest logical task number

static const char TaskState[6] = { 'A', 'R', 'B', 'S', 'D', 'I' };
static TaskHandle_t IdleHandle[portNUM_PROCESSORS] = { 0 };
static TaskStatus_t	sTS[configFR_MAX_TASKS] = { 0 };
#if	(portNUM_PROCESSORS > 1)
	static const char caMCU[3] = { '0', '1', 'X' };
	static u64rt_t Cores[portNUM_PROCESSORS+1];			// Sum of non-IDLE task runtime/core
#endif

#if (configRUNTIME_SIZE == 4)
	static SemaphoreHandle_t RtosStatsMux;
	static u16_t Counter;
	static u64rt_t Tasks[configFR_MAX_TASKS];
	static TaskHandle_t Handle[configFR_MAX_TASKS];
#endif

#if (configRUNTIME_SIZE == 4)
u64_t xRtosStatsFindRuntime(TaskHandle_t xHandle) {
	for (int i = 0; i < configFR_MAX_TASKS; ++i) { if (Handle[i] == xHandle) return Tasks[i].U64; } return 0ULL;
}

bool bRtosStatsUpdateHook(void) {
	if (++Counter % CONFIG_FREERTOS_HZ) return 1;
	if (NumTasks == 0) {							// Initial, once-off processing
		for (int i = 0; i < portNUM_PROCESSORS; ++i) IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i);
		IF_SYSTIMER_INIT(debugTIMING, stRTOS, stMICROS, "FreeRTOS", 1200, 5000);
	}
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	IF_SYSTIMER_START(debugTIMING, stRTOS);
	u32_t NowTotal;
	memset(sTS, 0, sizeof(sTS));

	NumTasks = uxTaskGetSystemState(sTS, configFR_MAX_TASKS, &NowTotal);
	IF_myASSERT(debugPARAM, NumTasks < configFR_MAX_TASKS);

	if (Total.U64 && Total.LSW > NowTotal) ++Total.MSW;		// Handle wrapped System counter
	Total.LSW = NowTotal;

	Active.U64 = 0;
	#if	(portNUM_PROCESSORS > 1)
	memset(&Cores[0], 0, sizeof(Cores));
	#endif
	for (int a = 0; a < NumTasks; ++a) {
		TaskStatus_t * psTS = &sTS[a];
		if (MaxNum < psTS->xTaskNumber) MaxNum = psTS->xTaskNumber;
		for (int b = 0; b <= configFR_MAX_TASKS; ++b) {
			if (Handle[b] == psTS->xHandle) {		// known task, update RT
				if (Tasks[b].LSW > psTS->ulRunTimeCounter) ++Tasks[b].MSW;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else if (Handle[b] == NULL) {			// empty entry so add ...
				Handle[b] = psTS->xHandle;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else {
				continue;								// not empty or match entry, try next
			}

			// For idle task(s) we do not want to add RunTime %'s to the task's RunTime or Cores' RunTime
			int c;
			for (c = 0; c < portNUM_PROCESSORS; ++c) {
				if (Handle[b] == IdleHandle[c]) break;	// IDLE task, skip and try the next...
			}
			if (c == portNUM_PROCESSORS) {				// NOT an IDLE task
				Active.U64 += Tasks[b].U64;				// Update total active time
				#if	(portNUM_PROCESSORS > 1)
				c = (psTS->xCoreID != tskNO_AFFINITY) ? psTS->xCoreID : 2;
				Cores[c].U64 += Tasks[b].U64;			// Update specific core's active time
				#endif
			}
			break;
		}
	}
	IF_SYSTIMER_STOP(debugTIMING, stRTOS);
	xRtosSemaphoreGive(&RtosStatsMux);
	return 1;
}
#endif

TaskStatus_t * psRtosStatsFindWithHandle(TaskHandle_t xHandle) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (sTS[i].xHandle == xHandle) return &sTS[i];
	}
	return NULL;
}

TaskStatus_t * psRtosStatsFindWithNumber(UBaseType_t xTaskNumber) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (sTS[i].xTaskNumber == xTaskNumber) return &sTS[i];
	}
	return NULL;
}

int	xRtosReportTasks(report_t * psR) {
	#if (configRUNTIME_SIZE == 8)
	if (IdleHandle[0] == NULL || IdleHandle[1] == NULL) {		// first time once only
		for (int i = 0; i < portNUM_PROCESSORS; ++i)
			IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i);
	}
	// Get up-to-date task status
	memset(sTS, 0, sizeof(sTS));
	u32_t NowTasks = uxTaskGetSystemState(sTS, configFR_MAX_TASKS, &Total.U64);
	IF_myASSERT(debugPARAM, NowTasks <= configFR_MAX_TASKS);
	Active.U64 = 0;
	for (int a = 0; a < NowTasks; ++a) {
		TaskStatus_t * psTS = &sTS[a];
		if (MaxNum < psTS->xTaskNumber)
			MaxNum = psTS->xTaskNumber;
		// If not an IDLE task
		if (IdleHandle[0] != psTS->xHandle && IdleHandle[1] != psTS->xHandle)
			Active.U64 += psTS->ulRunTimeCounter;		// update active tasks RT
		#if	(portNUM_PROCESSORS > 1)
		int c = (psTS->xCoreID != tskNO_AFFINITY) ? psTS->xCoreID : 2;
		Cores[c].U64 += psTS->ulRunTimeCounter;
		#endif
	}
	#endif

	// With 2 MCU's "effective" ticks is a multiple of the number of MCU's
	u64_t TotalAdj = Total.U64 / (100ULL / portNUM_PROCESSORS);
	if (TotalAdj == 0ULL)
		return 0;
	int	iRV = 0;
	printfx_lock(psR);
	if (psR->sFM.bColor) iRV += wprintfx(psR, "%C", colourFG_CYAN);
	if (psR->sFM.bTskNum) iRV += wprintfx(psR, "T# ");
	if (psR->sFM.bPrioX) iRV += wprintfx(psR, "Pc/Pb ");
	iRV += wprintfx(psR, configFREERTOS_TASKLIST_HDR_DETAIL);
	if (psR->sFM.bState) iRV += wprintfx(psR, "S ");
	#if (portNUM_PROCESSORS > 1)
	if (psR->sFM.bCore) iRV += wprintfx(psR, "X ");
	#endif
	if (psR->sFM.bStack) iRV += wprintfx(psR, "LowS ");
	iRV += wprintfx(psR, " Util Ticks");
	#if (debugTRACK && (SL_LEV_DEF > SL_SEV_NOTICE))
	if (psR->sFM.bXtras) iRV += wprintfx(psR, " Stack Base -Task TCB-");
	#endif
	if (psR->sFM.bColor) iRV += wprintfx(psR, "%C", attrRESET);
	iRV += wprintfx(psR, strCRLF);

	u32_t TaskMask = 0x1, Units, Fract;
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if ((psTS == NULL) ||
			(psTS->eCurrentState >= eInvalid) ||
			(psR->sFM.uCount & TaskMask) == 0 ||
			(psTS->uxCurrentPriority >= (UBaseType_t) configMAX_PRIORITIES) ||
			(psTS->uxBasePriority >= configMAX_PRIORITIES))
			goto next;
		if ((psTS->xCoreID >= portNUM_PROCESSORS) && (psTS->xCoreID != tskNO_AFFINITY))
			goto next;
		if (psR->sFM.bTskNum) iRV += wprintfx(psR, "%2u ", psTS->xTaskNumber);
		if (psR->sFM.bPrioX) iRV += wprintfx(psR, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
		iRV += wprintfx(psR, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
		if (psR->sFM.bState) iRV += wprintfx(psR, "%c ", TaskState[psTS->eCurrentState]);
		#if (portNUM_PROCESSORS > 1)
		if (psR->sFM.bCore) iRV += wprintfx(psR, "%c ", caMCU[psTS->xCoreID == 0 ? 0 : psTS->xCoreID == 1 ? 1 : 2]);
		#endif
		if (psR->sFM.bStack) iRV += wprintfx(psR, "%4u ", psTS->usStackHighWaterMark);
		// Calculate & display individual task utilisation.
		#if (configRUNTIME_SIZE == 8)
		u64_t u64RunTime = psTS->ulRunTimeCounter;
		#else
		u64_t u64RunTime = xRtosStatsFindRuntime(psTS->xHandle);
		#endif
    	Units = u64RunTime / TotalAdj;
    	Fract = ((u64RunTime * 100) / TotalAdj) % 100;
		iRV += wprintfx(psR, "%2lu.%02lu %#'5llu", Units, Fract, u64RunTime);

		if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && psR->sFM.bXtras)
			iRV += wprintfx(psR, " %p %p\r\n", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
		else
			iRV += wprintfx(psR, strCRLF);
next:
		TaskMask <<= 1;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = Active.U64 / TotalAdj;
	Fract = ((Active.U64 * 100) / TotalAdj) % 100;
	iRV += wprintfx(psR, "T=%u U=%lu.%02lu", NumTasks, Units, Fract);

	#if	(portNUM_PROCESSORS > 1)
	// calculate & display individual core's utilization
    for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
    	Units = Cores[i].U64 / TotalAdj;
    	Fract = ((Cores[i].U64 * 100) / TotalAdj) % 100;
    	iRV += wprintfx(psR, "  %c=%lu.%02lu", caMCU[i], Units, Fract);
    }
	#endif
    iRV += wprintfx(psR, psR->sFM.bNL ? "\r\n\n" : strCRLF);
	printfx_unlock(psR);
	return iRV;
}

int xRtosReportMemory(report_t * psR) {
	int iRV = 0;
	printfx_lock(psR);
#if defined(ESP_PLATFORM)
	if (psR->sFM.rmCAPS & MALLOC_CAP_32BIT) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_32BIT);
	if (psR->sFM.rmCAPS & MALLOC_CAP_8BIT) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_8BIT);
	if (psR->sFM.rmCAPS & MALLOC_CAP_DMA) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_DMA);
	if (psR->sFM.rmCAPS & MALLOC_CAP_EXEC) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_EXEC);
	if (psR->sFM.rmCAPS & MALLOC_CAP_IRAM_8BIT) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_IRAM_8BIT);
	#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
	if (psR->sFM.rmCAPS & MALLOC_CAP_SPIRAM) iRV += halMCU_ReportMemory(psR, MALLOC_CAP_SPIRAM);
	#endif
#endif
    if (psR->sFM.bColor) iRV += wprintfx(psR, "%C", colourFG_CYAN);
    iRV += wprintfx(psR, "FreeRTOS");
    if (psR->sFM.bColor) iRV += wprintfx(psR, "%C", attrRESET);
	iRV += wprintfx(psR, "    Min=%#'u  Free=%#'u  Orig=%#'u\r\n", xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin);
	if (psR->sFM.rmCompact) iRV += wprintfx(psR, strCRLF);
	printfx_unlock(psR);
	return iRV;
}

int xRtosReportTimer(report_t * psR, TimerHandle_t thTmr) {
	TickType_t tPer = xTimerGetPeriod(thTmr);
	TickType_t tExp = xTimerGetExpiryTime(thTmr);
	i32_t tRem = tExp - xTaskGetTickCount();
	BaseType_t bActive = xTimerIsTimerActive(thTmr);
	int iRV = wprintfx(psR, "\t%s: #=%lu Auto=%c Run=%s", pcTimerGetName(thTmr), uxTimerGetTimerNumber(thTmr),
		uxTimerGetReloadMode(thTmr) ? CHR_Y : CHR_N, bActive ? "Y" : "N\r\n");
	if (bActive) iRV += wprintfx(psR, " tPer=%lu tExp=%lu tRem=%ld\r\n", tPer, tExp, tRem);
	return iRV;
}

// ################################## Task creation/deletion #######################################

int	xRtosTaskCreate(TaskFunction_t pxTaskCode,
	const char * const pcName, const u32_t usStackDepth,
	void * pvParameters,
	UBaseType_t uxPriority,
	TaskHandle_t * pxCreatedTask,
	const BaseType_t xCoreID)
{
	IF_CP(debugTRACK && ioB1GET(ioUpDown), "[%s] creating\r\n", pcName);
	int iRV = pdFAIL;
	#ifdef CONFIG_FREERTOS_UNICORE
	iRV = xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
	#else
	iRV = xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
	#endif
	IF_myASSERT(debugRESULT, iRV == pdPASS);
	return iRV;
}

TaskHandle_t xRtosTaskCreateStatic(TaskFunction_t pxTaskCode, const char * const pcName,
	const u32_t usStackDepth, void * const pvParameters,
	UBaseType_t uxPriority, StackType_t * const pxStackBuffer,
    StaticTask_t * const pxTaskBuffer, const BaseType_t xCoreID)
{
	IF_CP(debugTRACK && ioB1GET(ioUpDown), "[%s] creating\r\n", pcName);
	#ifdef CONFIG_FREERTOS_UNICORE
	TaskHandle_t thRV = xTaskCreateStatic(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer);
	#else
	TaskHandle_t thRV = xTaskCreateStaticPinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxStackBuffer, pxTaskBuffer, xCoreID);
	#endif
	IF_myASSERT(debugRESULT, thRV != 0);
	return thRV;
}

/**
 * @brief	Set/clear all flags to force task[s] to initiate an organised shutdown
 * @param	mask indicating the task[s] to terminate
 */
void vRtosTaskTerminate(const EventBits_t uxTaskMask) {
	xRtosTaskSetDELETE(uxTaskMask);
	xRtosTaskSetRUN(uxTaskMask);						// must enable run to trigger delete
}

/**
 * @brief	Clear task runtime and static statistics data then delete the task
 * @param	Handle of task to be terminated (NULL = calling task)
 */
void vRtosTaskDelete(TaskHandle_t xHandle) {
	if (xHandle == NULL) xHandle = xTaskGetCurrentTaskHandle();
	bool UpDown = ioB1GET(ioUpDown);
	#if (debugTRACK)
	char caName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN+1];
	strncpy(caName, pcTaskGetName(xHandle), CONFIG_FREERTOS_MAX_TASK_NAME_LEN);
	#endif
	EventBits_t ebX = (EventBits_t) pvTaskGetThreadLocalStoragePointer(xHandle, 1);
	if (ebX) {						// Clear the RUN & DELETE task flags
		xRtosTaskClearRUN(ebX);
		xRtosTaskClearDELETE(ebX);
		IF_CP(debugTRACK && UpDown, "[%s] RUN/DELETE flags cleared\r\n", caName);
	}

	#if (configRUNTIME_SIZE == 4)	// 32bit tick counters, clear runtime stats collected.
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (Handle[i] == xHandle) {	// Clear dynamic runtime info
			Tasks[i].U64 = 0ULL;
			Handle[i] = NULL;
			IF_CP(debugTRACK && UpDown, "[%s] dynamic stats removed\r\n", caName);
			break;
		}
	}
	TaskStatus_t * psTS = psRtosStatsFindWithHandle(xHandle);
	if (psTS) {						// Clear "static" task info
		memset(psTS, 0, sizeof(TaskStatus_t));
		IF_CP(debugTRACK && UpDown, "[%s] static task info cleared\r\n", caName);
	}
	xRtosSemaphoreGive(&RtosStatsMux);
	#endif

	IF_CP(debugTRACK && UpDown, "[%s] deleting\r\n", caName);
	vTaskDelete(xHandle);
}

// ##################################### Semaphore support #########################################

#if	(configPRODUCTION == 0) && (rtosDEBUG_SEMA > -1)
SemaphoreHandle_t * pSHmatch = NULL;

extern SemaphoreHandle_t i2cPortMux;

SemaphoreHandle_t * IgnoreList[] = { &RtosStatsMux, &printfxMux, &SL_VarMux, &SL_NetMux, &i2cPortMux, NULL };

bool xRtosSemaphoreCheck(SemaphoreHandle_t * pSH) {
	for(int i = 0; IgnoreList[i] != NULL; ++i) if (IgnoreList[i] == pSH) return 1;
	return 0;
}

#endif

SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t * pSH) {
	SemaphoreHandle_t shX = xSemaphoreCreateMutex();
	if (pSH) *pSH = shX;
	#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
	if (!xRtosSemaphoreCheck(pSH) && (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH==pSHmatch)))
		CP("SH Init %p=%p\r\n", pSH, *pSH);
	#endif
	IF_myASSERT(debugRESULT, shX != 0);
	return shX;
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSH, TickType_t tWait) {
	IF_myASSERT(debugTRACK, halNVIC_CalledFromISR() == 0);
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) return pdTRUE;
	if (*pSH == NULL) xRtosSemaphoreInit(pSH);

	#if	(configPRODUCTION == 0  && rtosDEBUG_SEMA > -1)
	TickType_t tStep = (tWait == portMAX_DELAY) ? pdMS_TO_TICKS(10000) : tWait / 10;
	TickType_t tElap = 0;
	BaseType_t btRV;
	do {
		btRV = xSemaphoreTake(*pSH, tStep);
		if (!xRtosSemaphoreCheck(pSH) && (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSHmatch == pSH))) {
			#if (rtosDEBUG_SEMA_HLDR > 0)
			TaskHandle_t thHolder = xSemaphoreGetMutexHolder(*pSH);
			#endif
			CP("SH Take %d %p"
				#if (rtosDEBUG_SEMA_HLDR > 0)
				" H=%s/%d"
				#endif
				#if (rtosDEBUG_SEMA_RCVR > 0)
				" R=%s/%d"
				#endif
				" t=%lu\r\n",
				esp_cpu_get_core_id(), pSH,
				#if (rtosDEBUG_SEMA_HLDR > 0)
				pcTaskGetName(thHolder), uxTaskPriorityGet(thHolder),
				#endif
				#if (rtosDEBUG_SEMA_RCVR > 0)
				pcTaskGetName(NULL), uxTaskPriorityGet(NULL),
				#endif
				tElap);
			// Decode return addresses [optional]
			#if (rtosDEBUG_SEMA == 1)
			CP(" %p\r\n", __builtin_return_address(0));

			#elif (rtosDEBUG_SEMA == 2)
			CP(" %p %p\r\n",__builtin_return_address(0), __builtin_return_address(1));

			#elif (rtosDEBUG_SEMA == 3)
			CP(" %p %p %p\r\n",
				__builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2));

			#elif (rtosDEBUG_SEMA == 4)
			CP(" %p %p %p %p\r\n", __builtin_return_address(0),
				__builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3));

			#elif (rtosDEBUG_SEMA == 5)
			CP(" %p %p %p %p %p\r\n", __builtin_return_address(0), __builtin_return_address(1),
				__builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4));

			#elif (rtosDEBUG_SEMA == 6)
			CP(" %p %p %p %p %p %p\r\n",
				__builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2),
				__builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5));

			#elif (rtosDEBUG_SEMA == 7)
			CP(" %p %p %p %p %p %p %p\r\n", __builtin_return_address(0),
				__builtin_return_address(1), __builtin_return_address(2), __builtin_return_address(3),
				__builtin_return_address(4), __builtin_return_address(5), __builtin_return_address(6));

			#elif (rtosDEBUG_SEMA == 8)
			CP(" %p %p %p %p %p %p %p %p\r\n", __builtin_return_address(0), __builtin_return_address(1),
				__builtin_return_address(2), __builtin_return_address(3), __builtin_return_address(4),
				__builtin_return_address(5), __builtin_return_address(6), __builtin_return_address(7));

			#elif (rtosDEBUG_SEMA == 9)
			CP(" %p %p %p %p %p %p %p %p %p\r\n",
				__builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2),
				__builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5),
				__builtin_return_address(6), __builtin_return_address(7), __builtin_return_address(8));

			#elif (rtosDEBUG_SEMA == 10)
			CP(" %p %p %p %p %p %p %p %p %p %p\r\n",
				__builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2),
				__builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5),
				__builtin_return_address(6), __builtin_return_address(7), __builtin_return_address(8),
				__builtin_return_address(9));

			#elif (rtosDEBUG_SEMA > 10)
			CP(" %p %p %p %p %p %p %p %p %p %p %p\r\n",
				__builtin_return_address(0), __builtin_return_address(1), __builtin_return_address(2),
				__builtin_return_address(3), __builtin_return_address(4), __builtin_return_address(5),
				__builtin_return_address(6), __builtin_return_address(7), __builtin_return_address(8),
				__builtin_return_address(9), __builtin_return_address(10));

			#else
			CP(strCRLF);
			#endif
		}
		if (btRV == pdTRUE)				break;
		if (tWait != portMAX_DELAY)		tWait -= tStep;
		tElap += tStep;
	} while (tWait > tStep);
	return btRV;

	#else
	return xSemaphoreTake(*pSH, tWait);
	#endif
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSH) {
	IF_myASSERT(debugTRACK, halNVIC_CalledFromISR() == 0);
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || *pSH == 0) return pdTRUE;
	#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
	if (!xRtosSemaphoreCheck(pSH) && (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH == pSHmatch))) {
		#if (rtosDEBUG_SEMA_HLDR > 0)
		TaskHandle_t thHolder = xSemaphoreGetMutexHolder(*pSH);
		#endif
		CP("SH Give %d %p"
			#if (rtosDEBUG_SEMA_HLDR > 0)
			" H=%s/%d"
			#endif
			#if (rtosDEBUG_SEMA_RCVR > 0)
			" R=%s/%d"
			#endif
			"\r\n", esp_cpu_get_core_id(), pSH
			#if (rtosDEBUG_SEMA_HLDR > 0)
			,pcTaskGetName(thHolder), uxTaskPriorityGet(thHolder)
			#endif
			#if (rtosDEBUG_SEMA_RCVR > 0)
			,pcTaskGetName(NULL), uxTaskPriorityGet(NULL)
			#endif
			);
	}
	#endif
	return xSemaphoreGive(*pSH);
}

void vRtosSemaphoreDelete(SemaphoreHandle_t * pSH) {
	if (*pSH) {
		vSemaphoreDelete(*pSH);
		#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
		IF_RP (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH == pSHmatch), "SH Delete %p\r\n", pSH);
		#endif
		*pSH = 0;
	}
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
	if (pTCB == NULL) pTCB = xTaskGetCurrentTaskHandle();
	void * pxTOS	= (void *) * ((u32_t *) pTCB) ;
	void * pxStack	= (void *) * ((u32_t *) pTCB + 12);		// 48 bytes / 4 = 12
	printfx("Cur SP : %p - Stack HWM : %p\r\r\n", pxTOS,
			(u8_t *) pxStack + (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t)));
}

#include "freertos/task_snapshot.h"
#include "esp_debug_helpers.h"

// note for ESP32S3, you must alter esp_backtrace_print_from_frame to whitelist 0x400559DD
// as a valid memory address. see: https://github.com/espressif/esp-idf/issues/11512#issuecomment-1566943121
// Otherwise nearly all the backtraces will print as corrupt.
//
//    //Check if first frame is valid
//    bool corrupted = !(esp_stack_ptr_is_sane(stk_frame.sp) &&
//                       (esp_ptr_executable((void *)esp_cpu_process_stack_pc(stk_frame.pc)) ||
//         /*whitelist*/  esp_cpu_process_stack_pc(stk_frame.pc) == 0x400559DD ||
//                        /* Ignore the first corrupted PC in case of InstrFetchProhibited */
//                       (stk_frame.exc_frame && ((XtExcFrame *)stk_frame.exc_frame)->exccause == EXCCAUSE_INSTR_PROHIBITED)));

esp_err_t IRAM_ATTR esp_backtrace_print_all_tasks(int depth, bool panic) {
    u32_t task_count = uxTaskGetNumberOfTasks();
    TaskSnapshot_t* snapshots = (TaskSnapshot_t*) calloc(task_count * sizeof(TaskSnapshot_t), 1);
    // get snapshots
    UBaseType_t tcb_size = 0;
    u32_t got = uxTaskGetSnapshotAll(snapshots, task_count, &tcb_size);
    u32_t len = got < task_count ? got : task_count;
//    print_str("printing all tasks:\n\n", panic);
    esp_err_t err = ESP_OK;
    for (u32_t i = 0; i < len; i++) {
/*
        TaskHandle_t handle = (TaskHandle_t) snapshots[i].pxTCB;
        char* name = pcTaskGetName(handle);
        print_str(name ? name : "No Name" , panic);
*/
        XtExcFrame* xtf = (XtExcFrame*)snapshots[i].pxTopOfStack;
        esp_backtrace_frame_t frame = { .pc = xtf->pc, .sp = xtf->a1, .next_pc = xtf->a0, .exc_frame = xtf };
        esp_err_t nerr = esp_backtrace_print_from_frame(depth, &frame, panic);
        if (nerr != ESP_OK) {
            err = nerr;
        }
    }
    free(snapshots);
    return err;
}
