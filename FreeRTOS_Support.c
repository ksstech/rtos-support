/*
1 *	FreeRTOS_Support.c
 *	Copyright (c) KSS Technologies (Pty) Ltd , All rights reserved.
 *	Author		Andre M. Maree
 *	Date		Ver		Comments/changes
 *	20150708	1.00	Separated from the main application module
 */

#include	<string.h>

#include	"FreeRTOS_Support.h"						// Must be before hal_nvic.h"
#include	"hal_variables.h"
#include	"hal_nvic.h"
#include	"hal_mcu.h"									// halMCU_ReportMemory

#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"syslog.h"
#include	"systiming.h"
#include	"x_errors_events.h"

#define	debugFLAG					0xF000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #######################################  Build macros ###########################################


// #################################### FreeRTOS global variables ##################################

EventGroupHandle_t	xEventStatus = 0,TaskRunState = 0, TaskDeleteState, HttpRequests = 0;
static uint32_t g_HeapBegin;

// ################################# FreeRTOS heap & stack  ########################################

/*
 * Required to handle FreeRTOS heap_5.c implementation
 * The array is terminated using a NULL zero sized region definition, and the
 * memory regions defined in the array ***must*** appear in address order from
 * low address to high address.
 */
#if	 defined(cc3200) && defined( __TI_ARM__ )
	extern	uint32_t	__TI_static_base__, __HEAP_SIZE ;
	HeapRegion_t xHeapRegions[] = {
		{ ( uint8_t * ) SRAM_BASE,				SRAM1_SIZE 				},	// portion of memory used by bootloader
		{ ( uint8_t * )	&__TI_static_base__, 	(size_t) &__HEAP_SIZE	},
		{ ( uint8_t * ) NULL, 					0						},
	} ;
#elif defined(HW_P_PHOTON) && defined( __CC_ARM )
	extern	uint8_t		Image$$RW_IRAM1$$ZI$$Limit[] ;
	extern	uint8_t		Image$$ARM_LIB_STACK$$ZI$$Base[] ;
	HeapRegion_t xHeapRegions[] = {
		{ Image$$RW_IRAM1$$ZI$$Limit,	(size_t) Image$$ARM_LIB_STACK$$ZI$$Base } ,
		{ NULL,							0 }
	} ;
#elif defined(HW_P_PHOTON) && defined( __GNUC__ )
	extern	uint8_t		__HEAP_BASE[], __HEAP_SIZE[] ;
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

// #################################### General support routines ###################################

SemaphoreHandle_t xRtosSemaphoreInit(void) {
	SemaphoreHandle_t xHandle = xSemaphoreCreateMutex();
	IF_myASSERT(debugRESULT, xHandle != 0);
	return xHandle;
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSema, TickType_t xTicks) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING || halNVIC_CalledFromISR())
		return pdTRUE;
	if (*pSema == NULL)
		*pSema = xRtosSemaphoreInit();
	xTicks = (xTicks < 100) ? 100 : (xTicks == portMAX_DELAY) ? portMAX_DELAY : (xTicks + 5) % 10;
	int X = 0;
	do {
		if ((++X % 100) == 0)
			RP("T=%s P=%d S=%p C=%d\n", pcTaskGetName(NULL), uxTaskPriorityGet(NULL), pSema, X);
		myASSERT(X < 500);
		if (xSemaphoreTake(*pSema, 10) == pdTRUE) {
			return pdTRUE;
		}
		if (xTicks != portMAX_DELAY)
			xTicks -= 10;
		vTaskDelay(10);
	} while (xTicks);
	return pdFALSE;
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSema) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING ||
		halNVIC_CalledFromISR() ||
		*pSema == 0) {
		return pdTRUE;
	}
	BaseType_t btRV = xSemaphoreGive(*pSema);
	IF_myASSERT(debugRESULT, btRV == pdTRUE);
	return btRV;
}

void vRtosSemaphoreDelete(SemaphoreHandle_t * pSema) {
	if (*pSema == NULL)
		return;
	vSemaphoreDelete(*pSema);
	*pSema = 0;
}

void * pvRtosMalloc(size_t S) {
	void * pV = malloc(S);
	IF_myASSERT(debugRESULT, pV);
	IF_RP(debugTRACK && ioB1GET(ioMemory), "malloc %p:%u\n", pV, S);
	return pV;
}

void vRtosFree(void * pV) {
	IF_RP(debugTRACK && ioB1GET(ioMemory), " free  %p\n", pV) ;
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
	if ((xEventGroupGetBits(TaskDeleteState) & uxTaskMask) == uxTaskMask) {
		return 0;
	}
	// step 2: if not meant to terminate, check if/wait until enabled to run again
	xEventGroupWaitBits(TaskRunState, uxTaskMask, pdFALSE, pdTRUE, portMAX_DELAY) ;
	// step 3: since now definitely enabled to run, check for delete state again
	return ((xEventGroupGetBits(TaskDeleteState) & uxTaskMask) == uxTaskMask) ? 0 : 1 ;
}

// ################################# FreeRTOS Task statistics reporting ############################

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
	struct { uint32_t LSW, MSW ; } ;
	uint64_t U64 ;
} u64rt_t ;

static const char TaskState[] = "RPBSD" ;
#if	(portNUM_PROCESSORS > 1)
	static const char caMCU[3] = { '0', '1', 'X' } ;
#endif

static u64rt_t Total;									// Sum all tasks (incl IDLE)
static u64rt_t Active;									// Sum non-IDLE tasks
static uint8_t NumTasks;								// Currently "active" tasks
static uint8_t MaxNum;									// Highest logical task number

static TaskHandle_t IdleHandle[portNUM_PROCESSORS] = { 0 };
static TaskStatus_t	sTS[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM] = { 0 };
#if	(portNUM_PROCESSORS > 1)
	static u64rt_t Cores[portNUM_PROCESSORS+1];			// Sum of non-IDLE task runtime/core
#endif

#if (configRUN_TIME_COUNTER_SIZE == 4)

static SemaphoreHandle_t RtosStatsMux;
static uint16_t	Counter;
static u64rt_t Tasks[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM];
static TaskHandle_t Handle[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM];

uint64_t xRtosStatsFindRuntime(TaskHandle_t xHandle) {
	for (int i = 0; i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i) {
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
	IF_SYSTIMER_START(debugTIMING, stRTOS);
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	uint32_t NowTotal;
	memset(sTS, 0, sizeof(sTS));

	NumTasks = uxTaskGetSystemState(sTS, CONFIG_ESP_COREDUMP_MAX_TASKS_NUM, &NowTotal);
	IF_myASSERT(debugPARAM, NumTasks < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM);

	if (Total.U64 && Total.LSW > NowTotal)
		++Total.MSW;		// Handle wrapped System counter
	Total.LSW = NowTotal;

	Active.U64 = 0;
	memset(&Cores[0], 0, sizeof(Cores));
	for (int a = 0; a < NumTasks; ++a) {
		TaskStatus_t * psTS = &sTS[a];
		if (MaxNum < psTS->xTaskNumber)
			MaxNum = psTS->xTaskNumber;
		for (int b = 0; b < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++b) {
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
	xRtosSemaphoreGive(&RtosStatsMux);
	IF_SYSTIMER_STOP(debugTIMING, stRTOS) ;
	return 1 ;
}

#endif

TaskStatus_t * psRtosStatsFindWithHandle(TaskHandle_t xHandle) {
	for (int i = 0; i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i) {
		if (sTS[i].xHandle == xHandle)
			return &sTS[i];
	}
	return NULL;
}

TaskStatus_t * psRtosStatsFindWithNumber(UBaseType_t xTaskNumber) {
	for (int i = 0; i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i) {
		if (sTS[i].xTaskNumber == xTaskNumber)
			return &sTS[i];
	}
	return NULL;
}

int	xRtosReportTasks(char * pcBuf, size_t Size, const flagmask_t FlagMask) {
	int	iRV = 0 ;
	if (pcBuf == NULL || Size == 0)
		printfx_lock();
	if (FlagMask.bColor)
		iRV += wsnprintfx(&pcBuf, &Size, "%C", colourFG_CYAN);
	if (FlagMask.bCount)
		iRV += wsnprintfx(&pcBuf, &Size, "T# ") ;
	if (FlagMask.bPrioX)
		iRV += wsnprintfx(&pcBuf, &Size, "Pc/Pb ") ;
	iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_HDR_DETAIL) ;
	if (FlagMask.bState)
		iRV += wsnprintfx(&pcBuf, &Size, "S ") ;
	if (FlagMask.bStack)
		iRV += wsnprintfx(&pcBuf, &Size, "LowS ") ;
	if (portNUM_PROCESSORS > 1 && FlagMask.bCore)
		iRV += wsnprintfx(&pcBuf, &Size, "X ") ;
	iRV += wsnprintfx(&pcBuf, &Size, "%%Util Ticks") ;
	if (debugTRACK && (SL_LEV_DEF > SL_SEV_NOTICE) && FlagMask.bXtras)
		iRV += wsnprintfx(&pcBuf, &Size, " Stack Base -Task TCB-") ;
	if (FlagMask.bColor)
		iRV += wsnprintfx(&pcBuf, &Size, "%C", attrRESET) ;
	iRV += wsnprintfx(&pcBuf, &Size, "\n") ;

	#if (configRUN_TIME_COUNTER_SIZE == 8)
	if (IdleHandle[0] == NULL || IdleHandle[1] == NULL) {		// first time once only
		for (int i = 0; i < portNUM_PROCESSORS; ++i)
			IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i);
	}
	// Get up-to-date task status
	memset(sTS, 0, sizeof(sTS));
	uint32_t NowTasks = uxTaskGetSystemState(sTS, CONFIG_ESP_COREDUMP_MAX_TASKS_NUM, &Total.U64);
	IF_myASSERT(debugPARAM, NowTasks < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM);
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
	uint64_t TotalAdj = Total.U64 / (100ULL / portNUM_PROCESSORS);
	if (TotalAdj == 0ULL)
		goto exit;
	uint32_t TaskMask = 0x00000001, Units, Fract ;
	for (int a = 1; a <= MaxNum; ++a) {
		TaskStatus_t * psTS = psRtosStatsFindWithNumber(a);
		if (psTS == NULL)
			continue;
	    // if task info display not enabled, skip....
		if (FlagMask.uCount & TaskMask) {
			if (FlagMask.bCount)
				iRV += wsnprintfx(&pcBuf, &Size, "%2u ",psTS->xTaskNumber);
			if (FlagMask.bPrioX)
				iRV += wsnprintfx(&pcBuf, &Size, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
			iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
			if (FlagMask.bState)
				iRV += wsnprintfx(&pcBuf, &Size, "%c ", TaskState[psTS->eCurrentState]);
			if (FlagMask.bStack)
				iRV += wsnprintfx(&pcBuf, &Size, "%4u ", psTS->usStackHighWaterMark);
			if (portNUM_PROCESSORS > 1 && FlagMask.bCore)
				iRV += wsnprintfx(&pcBuf, &Size, "%c ", caMCU[(psTS->xCoreID > 1) ? 2 : psTS->xCoreID]);

			// Calculate & display individual task utilisation.
			#if (configRUN_TIME_COUNTER_SIZE == 8)
			uint64_t u64RunTime = psTS->ulRunTimeCounter;
			#else
			uint64_t u64RunTime = xRtosStatsFindRuntime(psTS->xHandle);
			#endif
	    	Units = u64RunTime / TotalAdj;
	    	Fract = ((u64RunTime * 100) / TotalAdj) % 100;
			iRV += wsnprintfx(&pcBuf, &Size, "%2u.%02u %#5llu", Units, Fract, u64RunTime);

			if (debugTRACK && (SL_LEV_DEF >= SL_SEV_INFO) && FlagMask.bXtras)
				iRV += wsnprintfx(&pcBuf, &Size, " %p %p\n", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
			else
				iRV += wsnprintfx(&pcBuf, &Size, "\n");
		}
		TaskMask <<= 1 ;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = Active.U64 / TotalAdj;
	Fract = ((Active.U64 * 100) / TotalAdj) % 100 ;
	iRV += wsnprintfx(&pcBuf, &Size, "T=%u U=%u.%02u", NumTasks, Units, Fract);

	#if	(portNUM_PROCESSORS > 1)
	// calculate & display individual core's utilization
    for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
    	Units = Cores[i].U64 / TotalAdj;
    	Fract = ((Cores[i].U64 * 100) / TotalAdj) % 100;
    	iRV += wsnprintfx(&pcBuf, &Size, "  %c=%u.%02u", caMCU[i], Units, Fract);
    }
	#endif
    iRV += wsnprintfx(&pcBuf, &Size, FlagMask.bNL ? "\n\n" : "\n");
exit:
	if (pcBuf == NULL || Size == 0)
		printfx_unlock();
	return iRV;
}

int vRtosReportMemory(char * pcBuf, size_t Size, flagmask_t sFM) {
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
	#if	(CONFIG_ESP32_SPIRAM_SUPPORT == 1)
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
	iRV += wsnprintfx(&pcBuf, &Size, "    Min=%'#u  Free=%'#u  Orig=%'#u\n", xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin);
	if (sFM.rmSmall)
		iRV += wsnprintfx(&pcBuf, &Size, "\n");
	if (pcBuf == NULL || Size == 0)
		printfx_unlock();
	return iRV;
}

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
	uint32_t	OldStackMark, NewStackMark ;
	OldStackMark = uxTaskGetStackHighWaterMark(NULL) ;
   	NewStackMark = uxTaskGetStackHighWaterMark(NULL) ;
   	if (NewStackMark != OldStackMark) {
   		vFreeRTOSDumpStack(NULL, STACK_SIZE) ;
   		OldStackMark = NewStackMark ;
   	}
 */
void vTaskDumpStack(void * pTCB) {
	if (pTCB == NULL) pTCB = xTaskGetCurrentTaskHandle() ;
	void * pxTOS	= (void *) * ((uint32_t *) pTCB)  ;
	void * pxStack	= (void *) * ((uint32_t *) pTCB + 12) ;		// 48 bytes / 4 = 12
	printfx("Cur SP : %08x - Stack HWM : %08x\r\n", pxTOS,
			(uint8_t *) pxStack + (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t))) ;
}

int	xRtosTaskCreate(TaskFunction_t pxTaskCode,
	const char * const pcName, const uint32_t usStackDepth,
	void * pvParameters,
	UBaseType_t uxPriority,
	TaskHandle_t * pxCreatedTask,
	const BaseType_t xCoreID) {
	IF_RP(debugTRACK && ioB1GET(ioUpDown), "[%s] creating\n", pcName);
	int iRV = pdFAIL ;
#if defined(ESP_PLATFORM)
	#if	defined(CONFIG_FREERTOS_UNICORE)
	iRV = xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
	#else
	iRV = xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID);
	#endif
#else
	#error "No/invalid platform defined"
#endif
	return (iRV == pdPASS) ? erSUCCESS : erFAILURE ;
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
	if (xHandle == NULL) {
		xHandle = xTaskGetCurrentTaskHandle();
	}
	#if (configRUN_TIME_COUNTER_SIZE == 4)
	xRtosSemaphoreTake(&RtosStatsMux, portMAX_DELAY);
	// Clear dynamic runtime info
	for (int i = 0; i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i) {
		if (Handle[i] == xHandle) {
			Tasks[i].U64 = 0ULL;
			Handle[i] = NULL;
			break;
		}
	}
	// Clear "static" task info
	TaskStatus_t * psTS = psRtosStatsFindWithHandle(xHandle);
	if (psTS) {
		memset(psTS, 0, sizeof(TaskStatus_t));
	}
	xRtosSemaphoreGive(&RtosStatsMux);
	#endif
	IF_RP(debugTRACK && ioB1GET(ioUpDown), "[%s] deleting\n", pcTaskGetName(xHandle));
	vTaskDelete(xHandle);
}
