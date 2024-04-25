//	FreeRTOS_Support.c - Copyright (c) 2015-24 Andre M. MAree / KSS Technologies (Pty) Ltd.

#include "hal_platform.h"
#include "FreeRTOS_Support.h"							// Must be before hal_nvic.h"
#include "hal_options.h"
#include "hal_memory.h"
#include "hal_nvic.h"
#include "printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include "syslog.h"
#include "systiming.h"
#include "x_errors_events.h"
#include <string.h>

// ########################################### Macros ##############################################

#define	debugFLAG					0xF000
#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

#define XP							RP
#define IF_XP						IF_RP

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
	extern	u32_t __TI_static_base__, __HEAP_SIZE;
	HeapRegion_t xHeapRegions[] = {
		{ ( u8_t * ) SRAM_BASE,				SRAM1_SIZE 				},	// portion of memory used by bootloader
		{ ( u8_t * ) &__TI_static_base__, 	(size_t) &__HEAP_SIZE	},
		{ ( u8_t * ) NULL, 					0						},
	};

#elif defined(HW_P_PHOTON) && defined( __CC_ARM )
	extern	u8_t Image$$RW_IRAM1$$ZI$$Limit[];
	extern	u8_t Image$$ARM_LIB_STACK$$ZI$$Base[];
	HeapRegion_t xHeapRegions[] = {
		{ Image$$RW_IRAM1$$ZI$$Limit,	(size_t) Image$$ARM_LIB_STACK$$ZI$$Base } ,
		{ NULL,							0 }
	};

#elif defined(HW_P_PHOTON) && defined( __GNUC__ )
	extern	u8_t __HEAP_BASE[], __HEAP_SIZE[];
	HeapRegion_t xHeapRegions[] = {
		{ __HEAP_BASE,	(size_t) __HEAP_SIZE } ,
		{ NULL,					0 }
	};
#endif

void vRtosHeapSetup(void) {
	#if defined(HW_P_PHOTON ) && defined( __CC_ARM )
		xHeapRegions[0].xSizeInBytes	-= (size_t) Image$$RW_IRAM1$$ZI$$Limit;
		vPortDefineHeapRegions(xHeapRegions);
	#elif defined(cc3200) && defined( __TI_ARM__ )
		vPortDefineHeapRegions(xHeapRegions);
	#elif defined(ESP_PLATFORM) && defined( __GNUC__ )
//		#warning "right options!!"
	#endif
	g_HeapBegin = xPortGetFreeHeapSize();
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

static u64rt_t Active;									// Sum non-IDLE tasks
static u8_t NumTasks;									// Currently "active" tasks
static u8_t MaxNum;										// Highest logical task number

static const char TaskState[6] = { 'A', 'R', 'B', 'S', 'D', 'I' };
static TaskHandle_t IdleHandle[portNUM_PROCESSORS] = { 0 };
// table where task status is stored when xRtosReportTasks() is called, avoid alloc/free
static TaskStatus_t	sTS[configFR_MAX_TASKS] = { 0 };
#if	(portNUM_PROCESSORS > 1)
	static const char caMCU[3] = { '0', '1', 'X' };
	static u64rt_t Cores[portNUM_PROCESSORS+1];			// Sum of non-IDLE task runtime/core
#endif

TaskStatus_t * psRtosStatsFindWithNumber(UBaseType_t xTaskNumber) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
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

#if (configRUNTIME_SIZE == 8)
int	xRtosReportTasks(report_t * psR) {
	if (NumTasks == 0) {								// first time once only
		for (int i = 0; i < portNUM_PROCESSORS; ++i) {
			IdleHandle[i] = xTaskGetIdleTaskHandleForCore(i);
		}
	}

	memset(sTS, 0, sizeof(sTS));
	u64_t TotalAdj;
	// Get up-to-date task status
	NumTasks = uxTaskGetSystemState(sTS, configFR_MAX_TASKS, &TotalAdj);
	IF_myASSERT(debugPARAM, NumTasks <= configFR_MAX_TASKS);
	TotalAdj /= (100ULL / portNUM_PROCESSORS);			// will be used to calc % for each task...
	if (TotalAdj == 0ULL)
		return 0;

	Active.U64val = 0;									// reset overall active running total
	if	(portNUM_PROCESSORS > 1)
		memset(&Cores[0], 0, sizeof(Cores));			// reset time/core running totals
	for (int a = 0; a < NumTasks; ++a) {				// determine value of highest numbered task
		TaskStatus_t * psTS = &sTS[a];
		if (psTS->xTaskNumber > MaxNum) {
			MaxNum = psTS->xTaskNumber;
		}
	}

	int	iRV = 0;										// reset the character output counter
	WPFX_LOCK(psR);										// before the first wprintfx()
	iRV += wprintfx(psR, "%C", colourFG_CYAN);
	if (psR->sFM.bTskNum)
		iRV += wprintfx(psR, "T# ");
	if (psR->sFM.bPrioX) 
		iRV += wprintfx(psR, "Pc/Pb ");
	iRV += wprintfx(psR, configFREERTOS_TASKLIST_HDR_DETAIL);
	if (psR->sFM.bState)
		iRV += wprintfx(psR, "S ");
	if (psR->sFM.bStack)
		iRV += wprintfx(psR, "LowS ");
	if (portNUM_PROCESSORS > 1 && psR->sFM.bCore)
		iRV += wprintfx(psR, "X ");
	iRV += wprintfx(psR, " Util Ticks");
	if (debugTRACK && (SL_LEV_DEF > SL_SEV_NOTICE) && psR->sFM.bXtras)
		iRV += wprintfx(psR, " Stack Base -Task TCB-");
	iRV += wprintfx(psR, "%C\r\n", attrRESET);

	u32_t Units, Fracts, TaskMask = 0x1;				// display individual task info
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if ((psTS == NULL) ||
			(psTS->eCurrentState >= eInvalid) ||
			(psR->sFM.uCount & TaskMask) == 0 ||
			(psTS->uxCurrentPriority >= (UBaseType_t) configMAX_PRIORITIES) ||
			(psTS->uxBasePriority >= configMAX_PRIORITIES))
			goto next;
		if ((psTS->xCoreID != tskNO_AFFINITY) && !INRANGE(0, psTS->xCoreID, portNUM_PROCESSORS-1)) {
			iRV += wprintfx(psR, "Skipped #%d = %d !!!\r\n", a, psTS->xCoreID);
			goto next;
		}
		#if	(portNUM_PROCESSORS > 1)
			int c = (psTS->xCoreID == tskNO_AFFINITY) ? 2 : psTS->xCoreID;
		#endif
		if (psR->sFM.bTskNum)
			iRV += wprintfx(psR, "%2u ", psTS->xTaskNumber);
		if (psR->sFM.bPrioX)
			iRV += wprintfx(psR, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
		iRV += wprintfx(psR, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
		if (psR->sFM.bState)
			iRV += wprintfx(psR, "%c ", TaskState[psTS->eCurrentState]);
		if (psR->sFM.bStack)
			iRV += wprintfx(psR, "%4u ", psTS->usStackHighWaterMark);
		#if (portNUM_PROCESSORS > 1)
			if (psR->sFM.bCore) 
				iRV += wprintfx(psR, "%c ", caMCU[c]);
		#endif
		// Calculate & display individual task utilisation.
		Units = psTS->ulRunTimeCounter / TotalAdj;
    	Fracts = ((psTS->ulRunTimeCounter * 100) / TotalAdj) % 100;
		iRV += wprintfx(psR, "%2lu.%02lu %#'5llu", Units, Fracts, psTS->ulRunTimeCounter);

		if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && psR->sFM.bXtras)
			iRV += wprintfx(psR, " %p %p", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
		if (psR->sFM.bNL)
			iRV += wprintfx(psR, strCRLF);
		// For idle task(s) we do not want to add RunTime % to the task or Core RunTime
		if (!bRtosTaskIsIdleTask(psTS->xHandle)) {		// NOT an IDLE task
			Active.U64val += psTS->ulRunTimeCounter;	// Update total active time
			#if	(portNUM_PROCESSORS > 1)
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
	WPFX_UNLOCK(psR);								// before the last wprintfx()
	iRV += wprintfx(psR, "%s", psR->sFM.bNL ? strCR2xLF : strCRLF);
	return iRV;
}

#else

static SemaphoreHandle_t RtosStatsMux;
static u16_t Counter;
static u64rt_t Total;									// Sum all tasks (incl IDLE)
static u64rt_t Tasks[configFR_MAX_TASKS];				// Task info, hook updated with wrap handling
static TaskHandle_t Handle[configFR_MAX_TASKS];

u64_t xRtosStatsFindRuntime(TaskHandle_t xHandle) {
	for (int i = 0; i < configFR_MAX_TASKS; ++i) {
		if (Handle[i] == xHandle) return Tasks[i].U64val;
	}
	return 0ULL;
}

bool bRtosStatsUpdateHook(void) {
	if (++Counter % CONFIG_FREERTOS_HZ)
		return 1;
	if (NumTasks == 0) {							// Initial, once-off processing
		for (int i = 0; i < portNUM_PROCESSORS; ++i)
			IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i);
		IF_SYSTIMER_INIT(debugTIMING, stRTOS, stMICROS, "FreeRTOS", 1200, 5000);
	}
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	IF_SYSTIMER_START(debugTIMING, stRTOS);
	u32_t NowTotal;
	memset(sTS, 0, sizeof(sTS));

	NumTasks = uxTaskGetSystemState(sTS, configFR_MAX_TASKS, &NowTotal);
	IF_myASSERT(debugPARAM, NumTasks < configFR_MAX_TASKS);

	if (Total.U64val && Total.LSW > NowTotal)
		++Total.MSW;									// Handle wrapped System counter
	Total.LSW = NowTotal;
	Active.U64val = 0;
	#if	(portNUM_PROCESSORS > 1)
		memset(&Cores[0], 0, sizeof(Cores));
	#endif
	for (int a = 0; a < NumTasks; ++a) {
		TaskStatus_t * psTS = &sTS[a];
		if (MaxNum < psTS->xTaskNumber)
			MaxNum = psTS->xTaskNumber;
		for (int b = 0; b <= configFR_MAX_TASKS; ++b) {
			if (Handle[b] == psTS->xHandle) {			// known task, update RT
				if (Tasks[b].LSW > psTS->ulRunTimeCounter)
					++Tasks[b].MSW;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else if (Handle[b] == NULL) {				// empty entry so add ...
				Handle[b] = psTS->xHandle;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else {
				continue;								// not empty or match entry, try next
			}

			// For idle task(s) we do not want to add RunTime %'s to the task's RunTime or Cores' RunTime
			int c;
			for (c = 0; c < portNUM_PROCESSORS; ++c) {
				if (Handle[b] == IdleHandle[c])
					break;								// IDLE task, skip and try the next...
			}
			if (c == portNUM_PROCESSORS) {				// NOT an IDLE task
				Active.U64val += Tasks[b].U64val;		// Update total active time
				#if	(portNUM_PROCESSORS > 1)
				c = (psTS->xCoreID != tskNO_AFFINITY) ? psTS->xCoreID : 2;
				Cores[c].U64val += Tasks[b].U64val;		// Update specific core's active time
				#endif
			}
			break;
		}
	}
	IF_SYSTIMER_STOP(debugTIMING, stRTOS);
	xRtosSemaphoreGive(&RtosStatsMux);
	return 1;
}

int	xRtosReportTasks(report_t * psR) {
	// With 2 MCU's "effective" ticks is a multiple of the number of MCU's
	u64_t TotalAdj = Total.U64val / (100ULL / portNUM_PROCESSORS);
	if (TotalAdj == 0ULL) return 0;

	// Display the column headers
	int	iRV = 0;					// reset the character output counter
	iRV += wprintfx(psR, "%C", colourFG_CYAN);
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
	iRV += wprintfx(psR, "%C\r\n", attrRESET);

	// display individual task info
	u32_t TaskMask = 0x1, Units, Fract;
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if ((psTS == NULL) ||
			(psTS->eCurrentState >= eInvalid) ||
			(psR->sFM.uCount & TaskMask) == 0 ||
			(psTS->uxCurrentPriority >= (UBaseType_t) configMAX_PRIORITIES) ||
			(psTS->uxBasePriority >= configMAX_PRIORITIES))
			goto next;
		if ((psTS->xCoreID >= portNUM_PROCESSORS) && (psTS->xCoreID != tskNO_AFFINITY)) goto next;
		if (psR->sFM.bTskNum) iRV += wprintfx(psR, "%2u ", psTS->xTaskNumber);
		if (psR->sFM.bPrioX) iRV += wprintfx(psR, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
		iRV += wprintfx(psR, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
		if (psR->sFM.bState) iRV += wprintfx(psR, "%c ", TaskState[psTS->eCurrentState]);
	#if (portNUM_PROCESSORS > 1)
		if (psR->sFM.bCore) iRV += wprintfx(psR, "%c ", caMCU[psTS->xCoreID == 0 ? 0 : psTS->xCoreID == 1 ? 1 : 2]);
	#endif
		if (psR->sFM.bStack) iRV += wprintfx(psR, "%4u ", psTS->usStackHighWaterMark);
		// Calculate & display individual task utilisation.
		u64_t u64RunTime = xRtosStatsFindRuntime(psTS->xHandle);
    	Units = u64RunTime / TotalAdj;
    	Fract = ((u64RunTime * 100) / TotalAdj) % 100;
		iRV += wprintfx(psR, "%2lu.%02lu %#'5llu", Units, Fract, u64RunTime);

		if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && psR->sFM.bXtras)
			iRV += wprintfx(psR, " %p %p", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
		if (psR->sFM.bNL) iRV += wprintfx(psR, strCRLF);

		// For idle task(s) we do not want to add RunTime %'s to the task's RunTime or Cores' RunTime
		if (!bRtosTaskIsIdleTask(psTS->xHandle)) {		// NOT an IDLE task
			Active.U64val += u64RunTime;				// Update total active time
			#if	(portNUM_PROCESSORS > 1)
				int c = (psTS->xCoreID == tskNO_AFFINITY) ? 2 : psTS->xCoreID;
				Cores[c].U64val += u64RunTime;			// Update specific core's active time
			#endif
		}
next:
		TaskMask <<= 1;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = Active.U64val / TotalAdj;
	Fract = ((Active.U64val * 100) / TotalAdj) % 100;
	iRV += wprintfx(psR, "T=%u U=%lu.%02lu", NumTasks, Units, Fract);

	#if	(portNUM_PROCESSORS > 1)
		// calculate & display individual core's utilization
    	for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
    		Units = Cores[i].U64val / TotalAdj;
    		Fract = ((Cores[i].U64val * 100) / TotalAdj) % 100;
    		iRV += wprintfx(psR, "  %c=%lu.%02lu", caMCU[i], Units, Fract);
    	}
	#endif
    iRV += wprintfx(psR, psR->sFM.bNL ? "\r\n\n" : strCRLF);
	return iRV;
}

TaskStatus_t * psRtosStatsFindWithHandle(TaskHandle_t xHandle) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (sTS[i].xHandle == xHandle) return &sTS[i];
	}
	return NULL;
}
#endif

int xRtosReportMemory(report_t * psR) {
	int iRV = 0;
	WPFX_LOCK(psR);										// before the first wprintfx()
	iRV += wprintfx(psR, "%CFreeRTOS:%C %#'u -> %#'u <- %#'u", colourFG_CYAN, attrRESET, xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin);
	WPFX_UNLOCK(psR);									// before the last wprintfx()
	iRV += wprintfx(psR, psR->sFM.aNL ? strCR2xLF : strCRLF);
	return iRV;
}

/**
 * @brief	
 * @note	does NOT lock the console UART !!!
*/
int xRtosReportTimer(report_t * psR, TimerHandle_t thTmr) {
	int iRV;
	if (halCONFIG_inSRAM(thTmr)) {
		TickType_t tPer = xTimerGetPeriod(thTmr);
		TickType_t tExp = xTimerGetExpiryTime(thTmr);
		i32_t tRem = tExp - xTaskGetTickCount();
		BaseType_t bActive = xTimerIsTimerActive(thTmr);
		iRV = wprintfx(psR, "\t%s: #=%lu Auto=%c Run=%s", pcTimerGetName(thTmr), uxTimerGetTimerNumber(thTmr),
			uxTimerGetReloadMode(thTmr) ? CHR_Y : CHR_N, bActive ? "Y" : "N");
		if (bActive)
			iRV += wprintfx(psR, " tPer=%lu tExp=%lu tRem=%ld", tPer, tExp, tRem);
	} else {
		iRV = wprintfx(psR, "\t%p Invalid Timer handle", thTmr);
	}
	iRV += wprintfx(psR, psR->sFM.aNL ? strCR2xLF : strCRLF);
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
	TASK_START(pcName);
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
	TASK_START(pcName);
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
	xRtosSetTaskDELETE(uxTaskMask);
	xRtosSetTaskRUN(uxTaskMask);						// must enable run to trigger delete
}

/**
 * @brief	Clear task runtime and static statistics data then delete the task
 * @param	Handle of task to be terminated (NULL = calling task)
 */
void vRtosTaskDelete(TaskHandle_t xHandle) {
	if (xHandle == NULL) xHandle = xTaskGetCurrentTaskHandle();
	#if (debugTRACK)
	char caName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN+1];
	strncpy(caName, pcTaskGetName(xHandle), CONFIG_FREERTOS_MAX_TASK_NAME_LEN);
	#endif
	EventBits_t ebX = (EventBits_t) pvTaskGetThreadLocalStoragePointer(xHandle, 1);
	if (ebX) {						// Clear the RUN & DELETE task flags
		xRtosClearTaskRUN(ebX);
		xRtosClearTaskDELETE(ebX);
		MESSAGE("[%s] RUN/DELETE flags cleared\r\n", caName);
	}

	#if (configRUNTIME_SIZE == 4)	// 32bit tick counters, clear runtime stats collected.
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (Handle[i] == xHandle) {	// Clear dynamic runtime info
			Tasks[i].U64val = 0ULL;
			Handle[i] = NULL;
			MESSAGE("[%s] dynamic stats removed\r\n", caName);
			break;
		}
	}
	TaskStatus_t * psTS = psRtosStatsFindWithHandle(xHandle);
	if (psTS) {						// Clear "static" task info
		memset(psTS, 0, sizeof(TaskStatus_t));
		MESSAGE("[%s] static task info cleared\r\n", caName);
	}
	xRtosSemaphoreGive(&RtosStatsMux);
	#endif

	TASK_STOP(caName);
	vTaskDelete(xHandle);
}

// ##################################### Semaphore support #########################################

#include "esp_debug_helpers.h"

#if	(configPRODUCTION == 0) && (rtosDEBUG_SEMA > -1)
SemaphoreHandle_t * pSHmatch = NULL;
SemaphoreHandle_t * IgnoreList[] = { };	// &RtosStatsMux, &printfxMux, &SL_VarMux, &SL_NetMux

bool xRtosSemaphoreCheck(SemaphoreHandle_t * pSH) {
	for(int i = 0; i < NO_MEM(IgnoreList); ++i) {
		if (IgnoreList[i] == pSH) return 1;
	}
	return 0;
}
#endif

SemaphoreHandle_t xRtosSemaphoreInit(SemaphoreHandle_t * pSH) {
	SemaphoreHandle_t shX = xSemaphoreCreateMutex();
	if (pSH) *pSH = shX;
	#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
	if (!xRtosSemaphoreCheck(pSH) && (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH==pSHmatch)))
		XP("SH Init %p=%p\r\n", pSH, *pSH);
	#endif
	IF_myASSERT(debugRESULT, shX != 0);
	return shX;
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSH, TickType_t tWait) {
	if (halNVIC_CalledFromISR()) {
		esp_backtrace_print(3);
		*pSH = NULL;		// Why????
		return pdTRUE;
	}
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
		return pdTRUE;
	if (*pSH == NULL)
		xRtosSemaphoreInit(pSH);

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
			#if (rtosDEBUG_SEMA_HLDR > 0) && (rtosDEBUG_SEMA_RCVR > 0)
			XP("SH Take %d %p H=%s/%d R=%s/%d t=%lu\r\n", esp_cpu_get_core_id(), pSH, tElap);
			#elif (rtosDEBUG_SEMA_HLDR > 0)
			XP("SH Take %d %p H=%s/%d t=%lu\r\n", esp_cpu_get_core_id(), pSH, tElap);
			#elif(rtosDEBUG_SEMA_RCVR > 0)
			XP("SH Take %d %p R=%s/%d t=%lu\r\n", esp_cpu_get_core_id(), pSH, tElap);
			#else
			XP("SH Take %d %p t=%lu\r\n", esp_cpu_get_core_id(), pSH, tElap);
			#endif
			// Decode return addresses [optional]
			#if (rtosDEBUG_SEMA > 0)
			esp_backtrace_print(rtosDEBUG_SEMA)
			#else
			XP(strCRLF);
			#endif
		}
		if (btRV == pdTRUE) break;
		if (tWait != portMAX_DELAY) tWait -= tStep;
		tElap += tStep;
	} while (tWait > tStep);
	return btRV;

	#else
	return xSemaphoreTake(*pSH, tWait);
	#endif
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSH) {
	if (halNVIC_CalledFromISR()) {
		esp_backtrace_print(3);
		return pdTRUE;
	}
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || *pSH == 0) return pdTRUE;
	#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
	if (!xRtosSemaphoreCheck(pSH) && (anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH == pSHmatch))) {
		#if (rtosDEBUG_SEMA_HLDR > 0)
		TaskHandle_t thHolder = xSemaphoreGetMutexHolder(*pSH);
		#endif

		#if (rtosDEBUG_SEMA_HLDR > 0) && (rtosDEBUG_SEMA_RCVR > 0)
		XP("SH Give %d %p H=%s/%d R=%s/%d\r\n", esp_cpu_get_core_id(), pSH);
		#elif (rtosDEBUG_SEMA_HLDR > 0)
		XP("SH Give %d %p H=%s/%d\r\n", esp_cpu_get_core_id(), pSH);
		#elif(rtosDEBUG_SEMA_RCVR > 0)
		XP("SH Give %d %p R=%s/%d\r\n", esp_cpu_get_core_id(), pSH);
		#else
		XP("SH Give %d %p\r\n", esp_cpu_get_core_id(), pSH);
		#endif
	}
	#endif
	return xSemaphoreGive(*pSH);
}

void vRtosSemaphoreDelete(SemaphoreHandle_t * pSH) {
	if (*pSH) {
		vSemaphoreDelete(*pSH);
		#if (configPRODUCTION == 0 && rtosDEBUG_SEMA > -1)
		IF_XP(anySYSFLAGS(sfTRACKER) || (pSHmatch && pSH == pSHmatch), "SH Delete %p\r\n", pSH);
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

//#include "freertos/task_snapshot.h"
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
