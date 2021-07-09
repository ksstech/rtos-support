/*
1 *	FreeRTOS_Support.c
 *	Copyright (c) KSS Technologies (Pty) Ltd , All rights reserved.
 *	Author		Andre M. Maree
 *	Date		Ver		Comments/changes
 *	20150708	1.00	Separated from the main application module
 */

#include	"hal_config.h"
#include	"FreeRTOS_Support.h"						// Must be before hal_nvic.h"

#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"syslog.h"
#include	"systiming.h"
#include	"x_errors_events.h"
#include	"x_stdio.h"

#include	"hal_nvic.h"
#include	"hal_mcu.h"									// halMCU_ReportMemory

#include	<string.h>

#define	debugFLAG					0xD000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #######################################  Build macros ###########################################

#define rtosPROFILING_ENABLE			0

// #################################### FreeRTOS global variables ##################################

EventGroupHandle_t	xEventStatus = 0,
					TaskRunState = 0,
					TaskDeleteState = 0 ;
int	xTaskIndex = 0 ;
uint32_t g_HeapBegin ;

#if		defined(cc3200)
	volatile uint64_t	PreSleepSCC ;
	#if	(rtosPROFILING_ENABLE == 1)
	x32mma_t	SleepIntvl ;
	#endif
#endif

#if		defined(ESP_PLATFORM)
	static	portMUX_TYPE	HeapFreeSafelyMutex ;
#endif

// ################################# FreeRTOS heap & stack  ########################################

/*
 * Required to handle FreeRTOS heap_5.c implementation
 * The array is terminated using a NULL zero sized region definition, and the
 * memory regions defined in the array ***must*** appear in address order from
 * low address to high address.
 */
#if		defined(cc3200) && defined( __TI_ARM__ )

	extern	uint32_t	__TI_static_base__, __HEAP_SIZE ;
	HeapRegion_t xHeapRegions[] = {
		{ ( uint8_t * ) SRAM_BASE,				SRAM1_SIZE 				},	// portion of memory used by bootloader
		{ ( uint8_t * )	&__TI_static_base__, 	(size_t) &__HEAP_SIZE	},
		{ ( uint8_t * ) NULL, 					0						},
	} ;

#elif	defined(HW_P_PHOTON) && defined( __CC_ARM )

	extern	uint8_t		Image$$RW_IRAM1$$ZI$$Limit[] ;
	extern	uint8_t		Image$$ARM_LIB_STACK$$ZI$$Base[] ;
	HeapRegion_t xHeapRegions[] = {
		{ Image$$RW_IRAM1$$ZI$$Limit,	(size_t) Image$$ARM_LIB_STACK$$ZI$$Base } ,
		{ NULL,							0 }
	} ;

#elif	defined(HW_P_PHOTON) && defined( __GNUC__ )

	extern	uint8_t		__HEAP_BASE[], __HEAP_SIZE[] ;
	HeapRegion_t xHeapRegions[] = {
		{ __HEAP_BASE,	(size_t) __HEAP_SIZE } ,
		{ NULL,					0 }
	} ;

#elif	defined(ESP_PLATFORM)
//	#warning "Anything specific for ESP32 here..."
#endif

/**
 * vRtosHeapSetup()
 */
void	vRtosHeapSetup(void ) {
#if		defined( HW_P_PHOTON ) && defined( __CC_ARM )
	xHeapRegions[0].xSizeInBytes	-= (size_t) Image$$RW_IRAM1$$ZI$$Limit ;
	vPortDefineHeapRegions(xHeapRegions) ;

#elif	defined( cc3200 ) && defined( __TI_ARM__ )
	vPortDefineHeapRegions(xHeapRegions) ;

#elif	defined(ESP_PLATFORM)
	vPortCPUInitializeMutex(&HeapFreeSafelyMutex) ;

#endif
	g_HeapBegin = xPortGetFreeHeapSize() ;
}

/**
 * vRtosHeapFreeSafely()
 */
void	vRtosHeapFreeSafely(void ** MemBuf) {
#if		defined(ESP_PLATFORM)
	portENTER_CRITICAL(&HeapFreeSafelyMutex) ;
#else
	taskENTER_CRITICAL() ;
#endif
	free(*MemBuf) ;
	*MemBuf = 0 ;
#if		defined(ESP_PLATFORM)
	portEXIT_CRITICAL(&HeapFreeSafelyMutex) ;
#else
	taskEXIT_CRITICAL() ;
#endif
}

// ############################## Power save / sleep mode support ##################################

void	vRtosInitSleep(TickType_t xExpectedIdleTime) {
#if	(rtosPROFILING_ENABLE == 1)
	x32MMAinit(&SleepIntvl) ;
#endif
}

void	vRtosPreSleep(TickType_t xExpectedIdleTime) {
#if		defined( cc3200 ) && defined( __TI_ARM__ )
	PreSleepSCC = PRCMSlowClkCtrFastGet() ;		// using PRCMSlowClkCtrGet() causes errors UtilsDelay() under debug
#else
	myASSERT(0) ;
#endif
}

void	vRtosPostSleep(void) {
#if		defined( cc3200 )
/* The slow clock counter (SCC) is a 48 bit register and is clocked at 32,768Hz or 32.768KHz
 * This means that the SCC will count from 0 -> 2^32 - 1 in 99,420.54 days or 272.3 years.
 * If only the bottom 32 bits are considered a range of 0->131,072sec or 36.41hr is possible
 * Since Systick is a 24bit timer clocked @ 80MHz maximum sleep time is 209mSec
 */
uint64_t	ElapsedSCC, Correction ;
// first read the 48bit SCC and store bottom 32 bits
	ElapsedSCC	= PRCMSlowClkCtrFastGet() ;			// use PRCMSlowClkCtrGet() cause UtilsDelay() errors debugging
	ElapsedSCC -= PreSleepSCC ;						// calc actual elapsed SCC counts

/* 32768 SCC ticks represent 1 second in time. Now convert the SCC ticks to fractions of a second */
	Correction		= ElapsedSCC * (0x0000000100000000 / 32768ULL) ;
	Correction		/= FRACTIONS_PER_MICROSEC ;			// now a number of uSecs
	CurSecs.usec	+= Correction ;
	if (CurSecs.usec > MICROS_IN_SECOND) {
		CurSecs.usec %= MICROS_IN_SECOND ;
		CurSecs.unit++ ;
	}
	RunSecs.usec	+= Correction ;
	if (RunSecs.usec > MICROS_IN_SECOND) {
		RunSecs.usec %= MICROS_IN_SECOND ;
		RunSecs.unit++ ;
	}
#else
	myASSERT(0) ;
#endif

#if		(rtosPROFILING_ENABLE == 1)
	i32MMAupdate(&SleepIntvl, (x32_t) xTimeFractionToMillis((uint32_t) (Correction & 0x00000000FFFFFFFF))) ;
#endif
}

// ####################################### FREERTOS HOOKS ##########################################

/**
 * vApplicationTickHook()
 */

#ifndef	ESP_PLATFORM
void	vApplicationTickHook(void) { halGPIO_TickHook() ; }	// button debounce functionality
#endif

/**
 * vApplicationStackOverflowHook()
 */
void	vApplicationStackOverflowHook(TaskHandle_t *pxTask, char * pcTaskName) {
	SL_EMER("Stack overflow task %s %p", pcTaskName, pxTask) ;
#if		defined(ESP_PLATFORM)
	esp_restart() ;
#else
	__error_report__(100, __func__, pcTaskName, (int32_t) pxTask) ;
#endif
}

/**
 * vApplicationMallocFailedHook()
 */
void	vApplicationMallocFailedHook(void) {
	SL_EMER("Malloc() failure") ;
#if		defined(ESP_PLATFORM)
	esp_restart() ;
#else
	__error_report__(100, __func__, "FAILED" , 0) ;
#endif
}

// #################################### General support routines ###################################

int	xRtosTaskCreate(TaskFunction_t pxTaskCode,
						const char * const pcName,
						const uint32_t usStackDepth,
			            void * pvParameters,
						UBaseType_t uxPriority,
						TaskHandle_t * pxCreatedTask,
						const BaseType_t xCoreID) {
	int iRV = pdFAIL ;
#if		defined(ESP_PLATFORM)
	#if	defined(CONFIG_FREERTOS_UNICORE)
	iRV = xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask) ;
	#else
	iRV = xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID ) ;
	#endif
#else
	#error "No/invalid platform defined"
#endif
	IF_myASSERT(debugRESULT, iRV == pdPASS) ;
	return (iRV == pdPASS) ? erSUCCESS : erFAILURE ;
}

#define	rtosCHECK_CURTASK			0
#if		(rtosCHECK_CURTASK == 1)
	static	TaskHandle_t CurTask = 0 ;
#endif

SemaphoreHandle_t xRtosSemaphoreInit(void) {
	SemaphoreHandle_t xHandle = xSemaphoreCreateMutex();
	IF_myASSERT(debugRESULT, xHandle != 0);
	return xHandle;
}

BaseType_t xRtosSemaphoreTake(SemaphoreHandle_t * pSema, uint32_t mSec) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING
	|| halNVIC_CalledFromISR()) return pdTRUE;

	if (*pSema == NULL) *pSema = xRtosSemaphoreInit();

#if		(rtosCHECK_CURTASK == 1)
	/* If the mutex is being held by the current task AND we are trying to take it AGAIN
	 * then we just fake the xSemaphoreTake() and return success.
	 * In order to ensure we do not unlock it prematurely when the matching UnLock call
	 * occurs, we need to mark it in some way....
	 */
	if (xSemaphoreGetMutexHolder(*pSema) == xTaskGetCurrentTaskHandle() && CurTask == 0) {
		CurTask = xTaskGetCurrentTaskHandle() ;
		return pdTRUE ;
	}
#endif

	return xSemaphoreTake(*pSema, pdMS_TO_TICKS(mSec)) ;
}

BaseType_t xRtosSemaphoreGive(SemaphoreHandle_t * pSema) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING
	|| halNVIC_CalledFromISR() || *pSema == 0) return pdTRUE;

#if		(rtosCHECK_CURTASK == 1)
	if (xSemaphoreGetMutexHolder(*pSema) == xTaskGetCurrentTaskHandle() && CurTask != 0) {
		CurTask = 0 ;
		return pdTRUE ;
	}
#endif
	return xSemaphoreGive(*pSema) ;
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
 * bRtosVerifyState() - check a) if task should self delete else b) if task should run
 * @param uxTaskToVerify
 * @return	0 if task should delete, 1 if it should run...
 */
bool bRtosVerifyState(const EventBits_t uxBitsTasks) {
	// step 1: if task is meant to delete/terminate, inform it as such
	if ((xEventGroupGetBits(TaskDeleteState) & uxBitsTasks) == uxBitsTasks) return 0 ;
	// step 2: if not meant to terminate, check if/wait until enabled to run again
	xEventGroupWaitBits(TaskRunState, uxBitsTasks, pdFALSE, pdTRUE, portMAX_DELAY) ;
	// step 3: since now definitely enabled to run, check for delete state again
	return ((xEventGroupGetBits(TaskDeleteState) & uxBitsTasks) == uxBitsTasks) ? 0 : 1 ;
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

static const char TaskState[] = "RPBSD" ;
#if		(portNUM_PROCESSORS > 1)
	static const char caMCU[3] = { '0', '1', 'X' } ;
#endif

typedef union {				// LSW then MSW sequence critical
	struct { uint32_t LSW, MSW ; } ;
	uint64_t U64 ;
} u64rt_t ;

typedef struct {
	u64rt_t			Total ;								// Sum all tasks (incl IDLE)
	u64rt_t			Active ;							// Sum non-IDLE tasks
	u64rt_t			Tasks[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM] ;
	TaskHandle_t	Handle[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM] ;
	TaskHandle_t	IdleHandle[portNUM_PROCESSORS] ;
#if		(portNUM_PROCESSORS > 1)
	u64rt_t			Cores[portNUM_PROCESSORS+1];		// Sum of non-IDLE task runtime/core
#endif
	uint16_t		Counter;
	uint8_t 		NumTask;							// Currently "active" tasks
	uint8_t 		MaxNum;								// Highest logical task number
} RtosStatus_t ;

static	RtosStatus_t	sRS = { 0 } ;
static	TaskStatus_t	sTS[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM] = { 0 } ;

bool	bRtosStatsUpdateHook(void) {
	if (++sRS.Counter % CONFIG_FREERTOS_HZ)	return true ;
uint64_t xRtosStatsFindRuntime(TaskHandle_t xHandle) {
	for (int i = 0; i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i)
		if (sRS.Handle[i] == xHandle) return sRS.Tasks[i].U64;
	return 0ULL;
}

	if (sRS.NumTask == 0) {
		for (int i = 0; i < portNUM_PROCESSORS; ++i) {
			sRS.IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i) ;
		}
		IF_SYSTIMER_INIT(debugTIMING, stRTOS, stMICROS, "STAT", 1300, 2200) ;
	}
	IF_SYSTIMER_START(debugTIMING, stRTOS) ;
	uint32_t NowTotal ;
	memset(sTS, 0, sizeof(sTS)) ;
	uint32_t NowTasks = uxTaskGetSystemState(sTS, CONFIG_ESP_COREDUMP_MAX_TASKS_NUM, &NowTotal ) ;
	if (sRS.Total.LSW > NowTotal)					 	// Handle wrapped System counter
		++sRS.Total.MSW ;
	sRS.Total.LSW	= NowTotal ;

	if (sRS.NumTask < NowTasks) {
		IF_myASSERT(debugPARAM, NowTasks < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM) ;
		sRS.NumTask = NowTasks ;
	}
	sRS.Active.U64 = 0 ;
	memset(&sRS.Cores, 0, SO_MEM(RtosStatus_t,Cores)) ;
	for (int a = 0; a < sRS.NumTask; ++a) {
		TaskStatus_t * psTS = &sTS[a] ;
		if (sRS.MaxNum < psTS->xTaskNumber)				// keep track of highest task#
			sRS.MaxNum = psTS->xTaskNumber ;

		for (int b = 0; b < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++b) {
			if (sRS.Handle[b] == psTS->xHandle) {		// already there, update
				if (sRS.Tasks[b].LSW > psTS->ulRunTimeCounter)
					++sRS.Tasks[b].MSW ;
				sRS.Tasks[b].LSW	= psTS->ulRunTimeCounter ;

			} else if (sRS.Handle[b] == NULL) {			// Not in table, add ...
				sRS.Handle[b] = psTS->xHandle ;
				sRS.Tasks[b].LSW = psTS->ulRunTimeCounter ;

			} else {
				continue;								// not empty or match entry, try next
			}

			// For idle task(s) we do not want to add RunTime %'s to the task's RunTime or CoresRunTime
			int c ;
			for (c = 0; c < portNUM_PROCESSORS; ++c) {	// current handle = IDLE task ?
				if (sRS.Handle[b] == sRS.IdleHandle[c])
					break ;
			}
			if (c == portNUM_PROCESSORS) {				// NOT an IDLE task
				sRS.Active.U64	+= sRS.Tasks[b].U64 ;
				#if	(portNUM_PROCESSORS > 1)
				c = (psTS->xCoreID != tskNO_AFFINITY) ? psTS->xCoreID : 2;
				sRS.Cores[c].U64 += sRS.Tasks[b].U64 ;
				#endif
			}
			break ;
		}
	}
	IF_SYSTIMER_STOP(debugTIMING, stRTOS) ;
	return true ;
}

TaskStatus_t *	psRtosStatsFindEntry(TaskHandle_t xHandle) {
	for (int i = 0; i < sRS.NumTask; ++i) {
		if (sTS[i].xHandle == xHandle) return &sTS[i] ;
	}
	return NULL ;
}

int		xRtosReportTasksNew(const flagmask_t FlagMask, char * pcBuf, size_t Size) {
int	xRtosReportTasks(const flagmask_t FlagMask, char * pcBuf, size_t Size) {
	int	iRV = 0 ;
	if (FlagMask.bColor) iRV += wsnprintfx(&pcBuf, &Size, "%C", xpfSGR(colourFG_CYAN, 0, 0, 0)) ;
	if (FlagMask.bCount) iRV += wsnprintfx(&pcBuf, &Size, "T# ") ;
	if (FlagMask.bPrioX) iRV += wsnprintfx(&pcBuf, &Size, "Pc/Pb ") ;
	iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_HDR_DETAIL) ;
	if (FlagMask.bState) iRV += wsnprintfx(&pcBuf, &Size, "S ") ;
	if (FlagMask.bStack) iRV += wsnprintfx(&pcBuf, &Size, "LowS ") ;
	#if	(portNUM_PROCESSORS > 1)
	if (FlagMask.bCore) iRV += wsnprintfx(&pcBuf, &Size, "X ") ;
	#endif
	iRV += wsnprintfx(&pcBuf, &Size, "%%Util Ticks") ;
	#if	(!defined(NDEBUG) || defined(DEBUG)) && (SL_LEVEL > SL_SEV_NOTICE)
	if (FlagMask.bXtras) iRV += wsnprintfx(&pcBuf, &Size, " Stack Base -Task TCB-") ;
	#endif
	if (FlagMask.bColor) iRV += wsnprintfx(&pcBuf, &Size, "%C", attrRESET) ;
	iRV += wsnprintfx(&pcBuf, &Size, "\n") ;

	// With 2 MCU's "effective" ticks is a multiple of the number of MCU's
	uint64_t TotalAdj = sRS.Total.U64 / (100ULL / portNUM_PROCESSORS) ;
	uint32_t TaskMask = 0x00000001, Units, Fract ;
	for (int a = 0; a < sRS.MaxNum; ++a) {
		TaskStatus_t *	psTS = psRtosStatsFindEntry(sRS.Handle[a]) ;
		if (psTS == NULL) 								// check if possible ?
			continue ;
		uint64_t u64RunTime = sRS.Tasks[a].U64 ;

	    // if task info display not enabled, skip....
		if (FlagMask.uCount & TaskMask) {
			if (FlagMask.bCount) iRV += wsnprintfx(&pcBuf, &Size, "%2u ",psTS->xTaskNumber);
			if (FlagMask.bPrioX) iRV += wsnprintfx(&pcBuf, &Size, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority);
			iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName);
			if (FlagMask.bState) iRV += wsnprintfx(&pcBuf, &Size, "%c ", TaskState[psTS->eCurrentState]);
			if (FlagMask.bStack) iRV += wsnprintfx(&pcBuf, &Size, "%4u ", psTS->usStackHighWaterMark);
			#if	(portNUM_PROCESSORS > 1)
			if (FlagMask.bCore) iRV += wsnprintfx(&pcBuf, &Size, "%c ", caMCU[(psTS->xCoreID > 1) ? 2 : psTS->xCoreID]);
			#endif

			// Calculate & display individual task utilisation.
	    	Units = u64RunTime / TotalAdj;
	    	Fract = ((u64RunTime * 100) / TotalAdj) % 100;
			iRV += wsnprintfx(&pcBuf, &Size, "%2u.%02u %#5llu", Units, Fract, u64RunTime);

			#if	(!defined(NDEBUG) || defined(DEBUG)) && (SL_LEVEL > SL_SEV_NOTICE)
			if (FlagMask.bXtras) iRV += wsnprintfx(&pcBuf, &Size, " %p %p\n", pxTaskGetStackStart(psTS->xHandle), psTS->xHandle);
			#else
			iRV += wsnprintfx(&pcBuf, &Size, "\n");
			#endif
		}
		TaskMask <<= 1 ;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = sRS.Active.U64 / TotalAdj;
	Fract = ((sRS.Active.U64 * 100) / TotalAdj) % 100 ;
	iRV += wsnprintfx(&pcBuf, &Size, "T=%u U=%u.%02u", sRS.NumTask, Units, Fract);

#if		(portNUM_PROCESSORS > 1)
	// calculate & display individual core's utilization
    for(int i = 0; i <= portNUM_PROCESSORS; ++i) {
    	Units = sRS.Cores[i].U64 / TotalAdj;
    	Fract = ((sRS.Cores[i].U64 * 100) / TotalAdj) % 100;
    	iRV += wsnprintfx(&pcBuf, &Size, "  %c=%u.%02u", caMCU[i], Units, Fract);
    }
#endif
    iRV += wsnprintfx(&pcBuf, &Size, FlagMask.bNL ? "\n\n" : "\n");
	return iRV;
}

void vRtosReportMemory(void) {
#if		defined(ESP_PLATFORM)
	halMCU_ReportMemory(MALLOC_CAP_32BIT) ;
	halMCU_ReportMemory(MALLOC_CAP_8BIT) ;
	halMCU_ReportMemory(MALLOC_CAP_DMA) ;
	halMCU_ReportMemory(MALLOC_CAP_EXEC) ;
	halMCU_ReportMemory(MALLOC_CAP_IRAM_8BIT) ;
	#if	(CONFIG_ESP32_SPIRAM_SUPPORT == 1)
	halMCU_ReportMemory(MALLOC_CAP_SPIRAM) ;
	#endif
#endif
    printfx("%CFreeRTOS%C\tMin=%'#u  Free=%'#u  Orig=%'#u\n\n", xpfSGR(colourFG_CYAN, 0, 0, 0), xpfSGR(attrRESET, 0, 0, 0), xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin) ;
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
