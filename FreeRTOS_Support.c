/*
 *	FreeRTOS_Support.c
 *	Copyright (c) 2015-22 Andre M. MAree / KSS Technologies (Pty) Ltd.
 */

#include "main.h"
#include "FreeRTOS_Support.h"						// Must be before hal_nvic.h"
#include "hal_nvic.h"
#include "hal_mcu.h"									// halMCU_ReportMemory

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

EventGroupHandle_t	xEventStatus = 0,
					TaskRunState = 0,
					TaskDeleteState = 0,
					HttpRequests = 0;
static u32_t g_HeapBegin;

// ################################# FreeRTOS heap & stack  ########################################

/*
 * Required to handle FreeRTOS heap_5.c implementation
 * The array is terminated using a NULL zero sized region definition, and the
 * memory regions defined in the array ***must*** appear in address order from
 * low address to high address.
 */
#if	 defined(cc3200) && defined( __TI_ARM__ )
	extern	u32_t	__TI_static_base__, __HEAP_SIZE ;
	HeapRegion_t xHeapRegions[] = {
		{ ( u8_t * ) SRAM_BASE,				SRAM1_SIZE 				},	// portion of memory used by bootloader
		{ ( u8_t * )	&__TI_static_base__, 	(size_t) &__HEAP_SIZE	},
		{ ( u8_t * ) NULL, 					0						},
	} ;

#elif defined(HW_P_PHOTON) && defined( __CC_ARM )
	extern	u8_t		Image$$RW_IRAM1$$ZI$$Limit[] ;
	extern	u8_t		Image$$ARM_LIB_STACK$$ZI$$Base[] ;
	HeapRegion_t xHeapRegions[] = {
		{ Image$$RW_IRAM1$$ZI$$Limit,	(size_t) Image$$ARM_LIB_STACK$$ZI$$Base } ,
		{ NULL,							0 }
	} ;

#elif defined(HW_P_PHOTON) && defined( __GNUC__ )
	extern	u8_t		__HEAP_BASE[], __HEAP_SIZE[] ;
	HeapRegion_t xHeapRegions[] = {
		{ __HEAP_BASE,	(size_t) __HEAP_SIZE } ,
		{ NULL,					0 }
	} ;
#endif

void vRtosHeapSetup(void ) {
	#if defined(HW_P_PHOTON ) && defined( __CC_ARM )
	xHeapRegions[0].xSizeInBytes	-= (size_t) Image$$RW_IRAM1$$ZI$$Limit ;
	vPortDefineHeapRegions(xHeapRegions) ;
	#elif defined( cc3200 ) && defined( __TI_ARM__ )
	vPortDefineHeapRegions(xHeapRegions) ;
	#endif
	g_HeapBegin = xPortGetFreeHeapSize() ;
}

// ##################################### Semaphore support #########################################

#if	(configPRODUCTION == 0) && (rtosDEBUG_SEMA > 0)
SemaphoreHandle_t * pSHmatch = NULL;
#endif

SemaphoreHandle_t xRtosSemaphoreInit(void) {
	SemaphoreHandle_t xHandle = xSemaphoreCreateMutex();
	IF_myASSERT(debugRESULT, xHandle != 0);
	return xHandle;
}

void vRtosSemaphoreInit(SemaphoreHandle_t * pSH) {
	*pSH = xSemaphoreCreateMutex();
	#if	(configPRODUCTION == 0)  && (rtosDEBUG_SEMA > 0)
	IF_P((pSHmatch != NULL) && (pSH == pSHmatch), "SH Init %p=%p\r\n", pSH, *pSH);
	#endif
	IF_myASSERT(debugRESULT, *pSH != 0);
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSH, TickType_t tWait) {
	IF_myASSERT(debugTRACK, halNVIC_CalledFromISR() == 0);
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING)
		return pdTRUE;
	if (*pSH == NULL)				// ensure initialized
		vRtosSemaphoreInit(pSH);

	#if	(configPRODUCTION == 0)  && (rtosDEBUG_SEMA > 0)
	TickType_t tStep = (tWait == portMAX_DELAY) ? pdMS_TO_TICKS(10000) : tWait / 10;
	TickType_t tElap = 0;
	BaseType_t btRV;
	do {
		btRV = xSemaphoreTake(*pSH, tStep);
		if ((pSHmatch != NULL) && (pSHmatch == pSH)) {
			TaskHandle_t xHandle = xSemaphoreGetMutexHolder(*pSH);
			P("SH Take %p  %lu: #%u  H=%s/%d  R=%s/%d", pSH, tElap, esp_cpu_get_core_id(),
				pcTaskGetName(xHandle), uxTaskPriorityGet(xHandle),
				pcTaskGetName(NULL), uxTaskPriorityGet(NULL));
			#if (rtosDEBUG_SEMA > 1)
			#define rtosBASE 1
			P(" A=%p B=%p C=%p D=%p E=%p\r\n", __builtin_return_address(rtosBASE),
				__builtin_return_address(rtosBASE+1), __builtin_return_address(rtosBASE+2),
				__builtin_return_address(rtosBASE+3), __builtin_return_address(rtosBASE+4));
			#else
			P(strCRLF);
			#endif
		}
		if (btRV == pdTRUE)
			break;
		if (tWait != portMAX_DELAY)
			tWait -= tStep;
		tElap += tStep;
	} while (tWait > tStep);
	return btRV;

	#else

	return xSemaphoreTake(*pSH, tWait);

	#endif
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSH) {
	IF_myASSERT(debugTRACK, halNVIC_CalledFromISR() == 0);
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || *pSH == 0)
		return pdTRUE;
	#if	(configPRODUCTION == 0)  && (rtosDEBUG_SEMA > 0)
	IF_P((pSHmatch != NULL) && (pSH == pSHmatch), "SH Give %p\r\n", pSH);
	#endif
	return xSemaphoreGive(*pSH);
}

void vRtosSemaphoreDelete(SemaphoreHandle_t * pSH) {
	if (*pSH) {
		vSemaphoreDelete(*pSH);
		#if	(configPRODUCTION == 0)  && (rtosDEBUG_SEMA > 0)
		IF_P((pSHmatch != NULL) && (pSH == pSHmatch), "SH Delete %p\r\n", pSH);
		#endif
		*pSH = 0;
	}
}

// ##################################### Malloc/free support #######################################

void * pvRtosMalloc(size_t S) {
	void * pV = malloc(S);
	IF_myASSERT(debugRESULT, pV);
	IF_RP(debugTRACK && ioB1GET(ioMemory), "malloc %p:%u\r\n", pV, S);
	return pV;
}

void vRtosFree(void * pV) {
	IF_RP(debugTRACK && ioB1GET(ioMemory), " free  %p\r\n", pV) ;
	free(pV);
}

// ################################### Event status manipulation ###################################

bool bRtosToggleStatus(const EventBits_t uxBitsToToggle) {
	if (bRtosCheckStatus(uxBitsToToggle) == 1) {
		xRtosClearStatus(uxBitsToToggle) ;
		return 0 ;
	}
	xRtosSetStatus(uxBitsToToggle) ;
	return 1 ;
}

/**
 * check if a task should a) terminate or b) run
 * @brief	if, at entry, set to terminate immediately return result
 * 			if not, wait (possibly 0 ticks) for run status
 *			Before returning, again check if set to terminate.
 * @param	uxTaskMask - specific task bitmap
 * @return	0 if task should delete, 1 if it should run...
 */
bool bRtosVerifyState(const EventBits_t uxTaskMask) {
	// step 1: if task is meant to delete/terminate, inform it as such
	if ((xEventGroupGetBits(TaskDeleteState) & uxTaskMask) == uxTaskMask)
		return 0;

	// step 2: if not meant to terminate, check if/wait until enabled to run again
	xEventGroupWaitBits(TaskRunState, uxTaskMask, pdFALSE, pdTRUE, portMAX_DELAY);

	// step 3: since now definitely enabled to run, check for delete state again
	return ((xEventGroupGetBits(TaskDeleteState) & uxTaskMask) == uxTaskMask) ? 0 : 1;
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

typedef union {				// LSW then MSW sequence critical
	struct { u32_t LSW, MSW; };
	u64_t U64;
} u64rt_t;

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

u64_t xRtosStatsFindRuntime(TaskHandle_t xHandle) {
	for (int i = 0; i < configFR_MAX_TASKS; ++i) {
		if (Handle[i] == xHandle)
			return Tasks[i].U64;
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

	if (Total.U64 && Total.LSW > NowTotal)
		++Total.MSW;		// Handle wrapped System counter
	Total.LSW = NowTotal;

	Active.U64 = 0;
	#if	(portNUM_PROCESSORS > 1)
	memset(&Cores[0], 0, sizeof(Cores));
	#endif
	for (int a = 0; a < NumTasks; ++a) {
		TaskStatus_t * psTS = &sTS[a];
		if (MaxNum < psTS->xTaskNumber)
			MaxNum = psTS->xTaskNumber;
		for (int b = 0; b <= configFR_MAX_TASKS; ++b) {
			if (Handle[b] == psTS->xHandle) {		// known task, update RT
				if (Tasks[b].LSW > psTS->ulRunTimeCounter)
					++Tasks[b].MSW;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else if (Handle[b] == NULL) {			// empty entry so add ...
				Handle[b] = psTS->xHandle;
				Tasks[b].LSW = psTS->ulRunTimeCounter;
			} else {
				continue;								// not empty or match entry, try next
			}

			// For idle task(s) we do not want to add RunTime %'s to the task's RunTime or Cores' RunTime
			int c ;
			for (c = 0; c < portNUM_PROCESSORS; ++c) {
				if (Handle[b] == IdleHandle[c])
					break;
			}
			if (c == portNUM_PROCESSORS) {				// NOT an IDLE task?
				Active.U64 += Tasks[b].U64 ;
				#if	(portNUM_PROCESSORS > 1)
				c = (psTS->xCoreID != tskNO_AFFINITY) ? psTS->xCoreID : 2;
				Cores[c].U64 += Tasks[b].U64 ;
				#endif
			}
			break ;
		}
	}
	IF_SYSTIMER_STOP(debugTIMING, stRTOS);
	xRtosSemaphoreGive(&RtosStatsMux);
	return 1 ;
}
#endif

TaskStatus_t * psRtosStatsFindWithHandle(TaskHandle_t xHandle) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (sTS[i].xHandle == xHandle)
			return &sTS[i];
	}
	return NULL;
}

TaskStatus_t * psRtosStatsFindWithNumber(UBaseType_t xTaskNumber) {
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (sTS[i].xTaskNumber == xTaskNumber)
			return &sTS[i];
	}
	return NULL;
}

int	xRtosReportTasks(char * pcBuf, size_t Size, const fm_t FlagMask) {
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
	int	iRV = 0 ;
	if (pcBuf == NULL || Size == 0) printfx_lock();		// unbuffered output, lock stdout
	if (FlagMask.bColor) iRV += wsnprintfx(&pcBuf, &Size, "%C", colourFG_CYAN);
	if (FlagMask.bCount) iRV += wsnprintfx(&pcBuf, &Size, "T# ");
	if (FlagMask.bPrioX) iRV += wsnprintfx(&pcBuf, &Size, "Pc/Pb ");
	iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_HDR_DETAIL);
	if (FlagMask.bState) iRV += wsnprintfx(&pcBuf, &Size, "S ");
	#if (portNUM_PROCESSORS > 1)
	if (FlagMask.bCore) iRV += wsnprintfx(&pcBuf, &Size, "X ");
	#endif
	if (FlagMask.bStack) iRV += wsnprintfx(&pcBuf, &Size, "LowS ");
	iRV += wsnprintfx(&pcBuf, &Size, " Util Ticks");
	#if (debugTRACK && (SL_LEV_DEF > SL_SEV_NOTICE))
	if (FlagMask.bXtras) iRV += wsnprintfx(&pcBuf, &Size, " Stack Base -Task TCB-");
	#endif
	if (FlagMask.bColor) iRV += wsnprintfx(&pcBuf, &Size, "%C", attrRESET);
	iRV += wsnprintfx(&pcBuf, &Size, strCRLF);

	u32_t TaskMask = 0x1, Units, Fract;
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if ((psTS == NULL) ||
			(psTS->eCurrentState >= eInvalid) ||
			(FlagMask.uCount & TaskMask) == 0 ||
			(psTS->uxCurrentPriority >= (UBaseType_t) configMAX_PRIORITIES) ||
			(psTS->uxBasePriority >= configMAX_PRIORITIES))
			goto next;
		if ((psTS->xCoreID >= portNUM_PROCESSORS) &&
			(psTS->xCoreID != tskNO_AFFINITY))
			goto next;
		if (FlagMask.bCount) iRV += wsnprintfx(&pcBuf, &Size, "%2u ", psTS->xTaskNumber);
		if (FlagMask.bPrioX) iRV += wsnprintfx(&pcBuf, &Size, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
		iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
		if (FlagMask.bState) iRV += wsnprintfx( &pcBuf, &Size, "%c ", TaskState[psTS->eCurrentState]);
		#if (portNUM_PROCESSORS > 1)
		if (FlagMask.bCore) iRV += wsnprintfx(&pcBuf, &Size, "%c ", caMCU[psTS->xCoreID==tskNO_AFFINITY ? 2 : psTS->xCoreID]);
		#endif
		if (FlagMask.bStack) iRV += wsnprintfx(&pcBuf, &Size, "%4u ", psTS->usStackHighWaterMark);
		// Calculate & display individual task utilisation.
		#if (configRUNTIME_SIZE == 8)
		u64_t u64RunTime = psTS->ulRunTimeCounter;
		#else
		u64_t u64RunTime = xRtosStatsFindRuntime(psTS->xHandle);
		#endif
    	Units = u64RunTime / TotalAdj;
    	Fract = ((u64RunTime * 100) / TotalAdj) % 100;
		iRV += wsnprintfx(&pcBuf, &Size, "%2lu.%02lu %#'5llu", Units, Fract, u64RunTime);

		if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && FlagMask.bXtras)
			iRV += wsnprintfx(&pcBuf, &Size, " %p %p\r\n", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
		else
			iRV += wsnprintfx(&pcBuf, &Size, strCRLF);
next:
		TaskMask <<= 1;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = Active.U64 / TotalAdj;
	Fract = ((Active.U64 * 100) / TotalAdj) % 100 ;
	iRV += wsnprintfx(&pcBuf, &Size, "T=%u U=%lu.%02lu", NumTasks, Units, Fract);

	#if	(portNUM_PROCESSORS > 1)
	// calculate & display individual core's utilization
    for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
    	Units = Cores[i].U64 / TotalAdj;
    	Fract = ((Cores[i].U64 * 100) / TotalAdj) % 100;
    	iRV += wsnprintfx(&pcBuf, &Size, "  %c=%lu.%02lu", caMCU[i], Units, Fract);
    }
	#endif
    iRV += wsnprintfx(&pcBuf, &Size, "\r\nEvt=0x%X  Run=0x%X  Del=0x%X", xEventGroupGetBits(xEventStatus),
			xEventGroupGetBits(TaskRunState), xEventGroupGetBits(TaskDeleteState));
    iRV += wsnprintfx(&pcBuf, &Size, FlagMask.bNL ? "\r\n\n" : strCRLF);
	if (pcBuf == NULL || Size == 0) printfx_unlock();	// unbuffered output, lock stdout
	return iRV;
}

int xRtosReportMemory(char * pcBuf, size_t Size, fm_t sFM) {
	int iRV = 0;
	if (pcBuf == NULL || Size == 0)
		printfx_lock();
	#if defined(ESP_PLATFORM)
	if (sFM.rm32b)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_32BIT);
	if (sFM.rm8b)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_8BIT);
	if (sFM.rmDma)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_DMA);
	if (sFM.rmExec)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_EXEC);
	if (sFM.rmIram)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_IRAM_8BIT);
	#ifdef CONFIG_SOC_SPIRAM_SUPPORTED
	if (sFM.rmPSram)
		iRV += halMCU_ReportMemory(&pcBuf, &Size, sFM, MALLOC_CAP_SPIRAM);
	#endif
	#endif
    if (sFM.rmColor) {
    	iRV += wsnprintfx(&pcBuf, &Size, "%C", colourFG_CYAN);
    }
    iRV += wsnprintfx(&pcBuf, &Size, "FreeRTOS");
    if (sFM.rmColor) {
    	iRV += wsnprintfx(&pcBuf, &Size, "%C", attrRESET);
    }
	iRV += wsnprintfx(&pcBuf, &Size, "    Min=%#'u  Free=%#'u  Orig=%#'u\r\n", xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin);
	if (sFM.rmSmall)
		iRV += wsnprintfx(&pcBuf, &Size, strCRLF);
	if (pcBuf == NULL || Size == 0)
		printfx_unlock();
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
	IF_P(debugTRACK && ioB1GET(ioUpDown), "[%s] creating\r\n", pcName);
	int iRV = pdFAIL ;
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
	IF_P(debugTRACK && ioB1GET(ioUpDown), "[%s] creating\r\n", pcName);
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
	xRtosSetStateDELETE(uxTaskMask);
	xRtosSetStateRUN(uxTaskMask);						// must enable run to trigger delete
}

/**
 * @brief	Clear task runtime and static statistics data then delete the task
 * @param	Handle of task to be terminated (NULL = calling task)
 */
void vRtosTaskDelete(TaskHandle_t xHandle) {
	if (xHandle == NULL)
		xHandle = xTaskGetCurrentTaskHandle();
	bool UpDown = ioB1GET(ioUpDown);
	#if (debugTRACK)
	char caName[CONFIG_FREERTOS_MAX_TASK_NAME_LEN];
	strncpy(caName, pcTaskGetName(xHandle), CONFIG_FREERTOS_MAX_TASK_NAME_LEN);
	#endif
	EventBits_t EB = (EventBits_t) pvTaskGetThreadLocalStoragePointer(xHandle, 1);
	if (EB) {						// Clear the RUN & DELETE task flags
		xRtosClearStateRUN(EB);
		xRtosClearStateDELETE(EB);
		IF_P(debugTRACK && UpDown, "[%s] RUN/DELETE flags cleared\r\n", caName);
	}

	#if (configRUNTIME_SIZE == 4)
	TaskStatus_t * psTS = psRtosStatsFindWithHandle(xHandle);
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	for (int i = 0; i <= configFR_MAX_TASKS; ++i) {
		if (Handle[i] == xHandle) {	// Clear dynamic runtime info
			Tasks[i].U64 = 0ULL;
			Handle[i] = NULL;
			IF_P(debugTRACK && UpDown, "[%s] dynamic stats removed\r\n", caName);
			break;
		}
	}
	if (psTS) {						// Clear "static" task info
		memset(psTS, 0, sizeof(TaskStatus_t));
		IF_P(debugTRACK && UpDown, "[%s] static task info cleared\r\n", caName);
	}
	xRtosSemaphoreGive(&RtosStatsMux);
	#endif

	IF_P(debugTRACK && UpDown, "[%s] deleting\r\n", caName);
	vTaskDelete(xHandle);
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
	u32_t	OldStackMark, NewStackMark ;
	OldStackMark = uxTaskGetStackHighWaterMark(NULL) ;
   	NewStackMark = uxTaskGetStackHighWaterMark(NULL) ;
   	if (NewStackMark != OldStackMark) {
   		vFreeRTOSDumpStack(NULL, STACK_SIZE) ;
   		OldStackMark = NewStackMark ;
   	}
 */
void vTaskDumpStack(void * pTCB) {
	if (pTCB == NULL)
		pTCB = xTaskGetCurrentTaskHandle() ;
	void * pxTOS	= (void *) * ((u32_t *) pTCB)  ;
	void * pxStack	= (void *) * ((u32_t *) pTCB + 12) ;		// 48 bytes / 4 = 12
	printfx("Cur SP : %p - Stack HWM : %p\r\r\n", pxTOS,
			(u8_t *) pxStack + (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t))) ;
}

/*void vRtosReportCallers(int Base, int Depth) {
	for (int i=Base; i < (Base+Depth); ++i) {
		void * pVoid = __builtin_return_address(i);
		P("%d=%p  ", i, pVoid);
	}
}*/
