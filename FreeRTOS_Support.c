/*
1 *	FreeRTOS_Support.c
 *	Copyright (c) KSS Technologies (Pty) Ltd , All rights reserved.
 *	Author		Andre M. Maree
 *	Date		Ver		Comments/changes
 *	20150708	1.00	Separated from the main application module
 */

#include	"FreeRTOS_Support.h"						// Must be before hal_nvic.h"

#include	"printfx.h"									// +x_definitions +stdarg +stdint +stdio
#include	"syslog.h"
#include	"systiming.h"
#include	"x_errors_events.h"
#include	"x_stdio.h"

#include	"hal_config.h"
#include	"hal_debug.h"
#include	"hal_timer.h"
#include	"hal_nvic.h"
#include	"hal_mcu.h"

#include	<string.h>

#define	debugFLAG					0x8000

#define	debugTIMING					(debugFLAG_GLOBAL & debugFLAG & 0x1000)
#define	debugTRACK					(debugFLAG_GLOBAL & debugFLAG & 0x2000)
#define	debugPARAM					(debugFLAG_GLOBAL & debugFLAG & 0x4000)
#define	debugRESULT					(debugFLAG_GLOBAL & debugFLAG & 0x8000)

// #######################################  Build macros ###########################################

#define rtosPROFILING_ENABLE			0

// #################################### FreeRTOS global variables ##################################

EventGroupHandle_t	xEventStatus = 0,
					TaskRunState = 0,
					TaskDeleteState = 0;
int32_t				xTaskIndex = 0 ;
uint32_t			g_HeapBegin ;

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

/*
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

/*
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

/*
 * vApplicationTickHook()
 */

#if		defined(ESP_PLATFORM)

void	myApplicationTickHook(void) {}

#else

void	vApplicationTickHook(void) {
	#if		(halTIMER_RTOS_XTIMETICKHOOK == 1)
	halTIMER_TickHook() ;						// update CurSecs & RunSecs
	#endif
	#if (configHAL_GPIO_DIG_IN > 0)
	halGPIO_TickHook() ;						// button debounce functionality
	#endif
}

#endif

/*
 * vApplicationIdleHook()
 */
bool	myApplicationIdleHook(void) {
#if 0
	UBaseType_t		uxTasks	= uxTaskGetNumberOfTasks();		// get number of TCBs to reserve space for
	TaskSnapshot_t *pTSA	= malloc(uxTasks * sizeof(TaskStatus_t)) ;
	TaskSnapshot_t *psTS	= pTSA ;
	UBaseType_t		uxTCBsz ;
	uxTasks = uxTaskGetSnapshotAll(pTSA, uxTasks * sizeof(TaskStatus_t), &uxTCBsz ) ;
	for (uint32_t Idx = 0; Idx < uxTasks; ++Idx, ++psTS) {
		if (psTS->pxEndOfStack) {
		}
	}
#endif
	return true ;
}

/*
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

/*
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

int32_t	xRtosTaskCreate(TaskFunction_t pxTaskCode,
						const char * const pcName,
						const uint32_t usStackDepth,
			            void * pvParameters,
						UBaseType_t uxPriority,
						TaskHandle_t * pxCreatedTask,
						const BaseType_t xCoreID) {
	IF_SL_INFO(debugTRACK, "'%s' S=%d P=%d", pcName, usStackDepth, uxPriority) ;
#if		defined(ESP_PLATFORM) && defined(CONFIG_FREERTOS_UNICORE)
	UNUSED(xCoreID) ;
	return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask) ;
#elif	defined(ESP_PLATFORM)
	return xTaskCreatePinnedToCore(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask, xCoreID ) ;
#else
	#error "No/invalid platform deefined"
#endif
}

#define	rtosCHECK_CURTASK			0
#if		(rtosCHECK_CURTASK == 1)
	static	TaskHandle_t CurTask = 0 ;
#endif

BaseType_t	xRtosSemaphoreTake(SemaphoreHandle_t * pSema, uint32_t mSec) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING ||
		halNVIC_CalledFromISR()) {
		return pdTRUE ;
	}
	if (*pSema == NULL) {
		*pSema = xSemaphoreCreateMutex() ;				// no, create now...
		IF_myASSERT(debugRESULT, *pSema != 0) ;
	}
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

BaseType_t	xRtosSemaphoreGive(SemaphoreHandle_t * pSema) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING ||
		halNVIC_CalledFromISR() ||
		*pSema == 0) {
		return pdTRUE ;
	}
#if		(rtosCHECK_CURTASK == 1)
	if (xSemaphoreGetMutexHolder(*pSema) == xTaskGetCurrentTaskHandle() && CurTask != 0) {
		CurTask = 0 ;
		return pdTRUE ;
	}
#endif
	return xSemaphoreGive(*pSema) ;
}

// ################################### Event status manipulation ###################################

bool	bRtosCheckStatus(const EventBits_t uxBitsToCheckFor) {
	if (xEventStatus == 0)
		return false ;
	return (xEventGroupGetBits(xEventStatus) & uxBitsToCheckFor) == uxBitsToCheckFor ? true : false ;
}

bool	bRtosToggleStatus(const EventBits_t uxBitsToToggle) {
	int8_t	bRV ;
	if (bRtosCheckStatus(uxBitsToToggle) == 1) {
		xRtosClearStatus(uxBitsToToggle) ;
		bRV = false ;
	} else {
		xRtosSetStatus(uxBitsToToggle) ;
		bRV = true ;
	}
	return bRV ;
}

/**
 * bRtosVerifyState() - check a) if task should self delete else b) if task should run
 * @param uxTaskToVerify
 * @return	false if task should delete, true if it should run...
 */
bool	bRtosVerifyState(const EventBits_t uxBitsTasks) {
	// step 1: if task is meant to delete/terminate, inform it as such
	if ((xEventGroupGetBits(TaskDeleteState) & uxBitsTasks) == uxBitsTasks)
		return false ;

	// step 2: if not meant to terminate, check if/wait until enabled to run again
	xEventGroupWaitBits(TaskRunState, uxBitsTasks, pdFALSE, pdTRUE, portMAX_DELAY) ;

	// step 3: since now definitely enabled to run, check for delete state again
	return ((xEventGroupGetBits(TaskDeleteState) & uxBitsTasks) == uxBitsTasks) ? false : true ;
}

// ################################# FreeRTOS Task statistics reporting ############################

#if		(configMAX_TASK_NAME_LEN == 16)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"----TaskName---- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-16s "
#elif	(configMAX_TASK_NAME_LEN == 15)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"---Task Name--- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-15s "
#elif	(configMAX_TASK_NAME_LEN == 14)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"---TaskName--- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-14s "
#elif	(configMAX_TASK_NAME_LEN == 13)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"--Task Name-- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-13s "
#elif	(configMAX_TASK_NAME_LEN == 12)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"--TaskName-- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-12s "
#elif	(configMAX_TASK_NAME_LEN == 11)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"-Task Name- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-11s "
#elif	(configMAX_TASK_NAME_LEN == 10)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"-TaskName- "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-10s "
#elif	(configMAX_TASK_NAME_LEN == 9)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"Task Name "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-9s "
#elif	(configMAX_TASK_NAME_LEN == 8)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"TaskName"
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-8s "
#elif	(configMAX_TASK_NAME_LEN == 7)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"TskName "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-7s "
#elif	(configMAX_TASK_NAME_LEN == 6)
	#define	configFREERTOS_TASKLIST_HDR_DETAIL		"TskNam "
	#define	configFREERTOS_TASKLIST_FMT_DETAIL		"%-6s "
#else
	#error "configMAX_TASK_NAME_LEN is out of range !!!"
#endif

static const char TaskState[] = "RPBSD" ;

uint8_t * pxTaskGetStackStart( TaskHandle_t xTask) ;

typedef struct	rtosinfo_s {
	TaskStatus_t *	pTSA ;								// pointer to malloc'd status info
	UBaseType_t 	NumTask ;							// Currently "active" tasks
	UBaseType_t 	MaxNum ;							// Highest logical task number
	union	{
		struct {
			uint32_t		u32SystemRunTimeLSW ;
			uint32_t		u32SystemRunTimeMSW ;
		};
		uint64_t	u64SystemRunTime ;
	};
	uint64_t		u64TotalRunTime ;					// Sum of ALL tasks runtime
	uint64_t		u64TasksRunTime ;					// Sum of non-IDLE tasks runtime
#if		(portNUM_PROCESSORS > 1)
	uint64_t		u64CoresRunTime[portNUM_PROCESSORS+1] ;	// Sum of non-IDLE task runtime/core
#endif
} rtosinfo_t ;

typedef struct	taskinfo_s {
	TaskHandle_t	xHandle ;
	union {
		struct {
			uint32_t	u32RunTimeLSW ;					// LSW then MSW sequence critical
			uint32_t	u32RunTimeMSW ;
		};
		uint64_t	u64RunTime ;
	};
} taskinfo_t ;

static	rtosinfo_t	sRI = { 0 } ;
static	taskinfo_t	sTI[CONFIG_ESP_COREDUMP_MAX_TASKS_NUM] = { 0 } ;

int32_t	vRtosStatsUpdate(bool fFree) {
	// Step 1: Update number of active running tasks
	sRI.NumTask	= uxTaskGetNumberOfTasks();	// get number of TCBs to reserve space for

	// Step 2: allocate buffer for stats of actively running tasks
	sRI.pTSA	= malloc((sRI.NumTask + 1) * sizeof(TaskStatus_t)) ;
	if (sRI.pTSA == NULL) {
		SL_ERR("Error allocating Task Status memory") ;
		return erFAILURE ;
	}
	memset(sRI.pTSA, 0, (sRI.NumTask+1) * sizeof(TaskStatus_t)) ;

	// Step 3:  Fetch stats of actively running tasks
	uint32_t	ulTotalRunTime ;
	sRI.NumTask	= uxTaskGetSystemState(sRI.pTSA, sRI.NumTask, &ulTotalRunTime ) ;

	// Step 4: Update SystemRunTime
	if (sRI.u32SystemRunTimeLSW > ulTotalRunTime) {		// wrapped?
		++sRI.u32SystemRunTimeMSW ;						// yes, increment wrapped counter...
	}
	sRI.u32SystemRunTimeLSW = ulTotalRunTime ;			// Save current [wrapped] SystemRunTime

	// Step 5: Clear running task totals.
	sRI.u64TotalRunTime 	= 0 ;						// reset TasksRunTime (incl IDLE tasks)

	for(int i = 0; i < sRI.NumTask; ++i) {
		// Step 6: Update highest task# found
		if (sRI.pTSA[i].xTaskNumber > sRI.MaxNum)		// Find the highest task number..
			sRI.MaxNum = sRI.pTSA[i].xTaskNumber ;

		// Step 7: Save[(new) or Update(known) task run time
		for(int j = 0; j < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++j) {
			if (sTI[j].xHandle == NULL) {				// empty entry so just add the task info
				sTI[j].xHandle			= sRI.pTSA[i].xHandle ;
				sTI[j].u32RunTimeLSW	= sRI.pTSA[i].ulRunTimeCounter ;
//				PRINT("New T#=%u  RT=%llu\n", sRI.pTSA[i].xTaskNumber, sTI[j].u64RunTime) ;
				break ;
			}
			if (sTI[j].xHandle == sRI.pTSA[i].xHandle) {	// existing task?
				if (sTI[j].u32RunTimeLSW > sRI.pTSA[i].ulRunTimeCounter) {	// yes, wrapped
					++sTI[j].u32RunTimeMSW ;			// yes, update MSW counter ....
				}
				sTI[j].u32RunTimeLSW	= sRI.pTSA[i].ulRunTimeCounter ;
				break ;
			}
		}
		sRI.u64TotalRunTime += sTI[i].u64RunTime ;
	}
	if (fFree == true)
		free(sRI.pTSA) ;
	return erSUCCESS ;
}

TaskStatus_t *	psRtosStatsFindEntry(UBaseType_t xNum) {
	for (int i = 0; i < sRI.NumTask; ++i) {
		if (sRI.pTSA[i].xTaskNumber == xNum)	return &sRI.pTSA[i] ;
	}
	return NULL ;
}

uint64_t xRtosStatsGetRunTime(TaskHandle_t xHandle) {
	for(int i = 0; sTI[i].xHandle != NULL && i < CONFIG_ESP_COREDUMP_MAX_TASKS_NUM; ++i) {
		if (sTI[i].xHandle == xHandle)			return sTI[i].u64RunTime ;
	}
	return 0ULL ;
}

int32_t	xRtosReportTasksNew(const flagmask_t FlagMask, char * pcBuf, size_t Size) {
	int32_t	i, iRV = 0 ;
	if (vRtosStatsUpdate(false) != erSUCCESS)
		return erFAILURE ;

	/* u64SystemRunTime is the running total number of ticks. If we have >1 MCU the "effective"
	 * number of ticks available for task executions is a multiple of the number of MCU's */
	uint64_t SystemRT = sRI.u64SystemRunTime / (100ULL /portNUM_PROCESSORS) ;	// adjust for %'s

	TaskHandle_t	IdleHandle[portNUM_PROCESSORS] ;
	for(i = 0; i < portNUM_PROCESSORS; ++i)	{			// Identify the IDLE task(s) for later use
		IdleHandle[i] = xTaskGetIdleTaskHandleForCPU(i) ;
#if		(portNUM_PROCESSORS > 1)
		sRI.u64CoresRunTime[i] = 0ULL ;					// reset count MCU < portNUM_PROCESSORS
#endif
	}

#if		(portNUM_PROCESSORS > 1)
	sRI.u64CoresRunTime[portNUM_PROCESSORS] = 0ULL ;	// reset count MCU = portNUM_PROCESSORS
#endif
	sRI.u64TasksRunTime = 0ULL ;
	TaskHandle_t	pCurTCB		= xTaskGetCurrentTaskHandle() ;
	// If no buffer is specified then we need to manually lock the stdout printfx semaphore
	if (pcBuf == NULL)
		printfx_lock() ;
	// build column organised header
	if (FlagMask.bColor)	iRV += wsnprintfx(&pcBuf, &Size, "%C", xpfSGR(attrRESET, colourFG_CYAN, 0, 0)) ;
	if (FlagMask.bCount)	iRV += wsnprintfx(&pcBuf, &Size, "T# ") ;
	if (FlagMask.bPrioX)	iRV += wsnprintfx(&pcBuf, &Size, "Pc/Pb ") ;
	iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_HDR_DETAIL) ;
	if (FlagMask.bState)	iRV += wsnprintfx(&pcBuf, &Size, "S ") ;
	if (FlagMask.bStack)	iRV += wsnprintfx(&pcBuf, &Size, " LowS ") ;
#if		(portNUM_PROCESSORS > 1)
	if (FlagMask.bCore)		iRV += wsnprintfx(&pcBuf, &Size, "X ") ;
#endif
	iRV += wsnprintfx(&pcBuf, &Size, "%%Util ") ;
#if		(!defined(NDEBUG) || defined(DEBUG))
	if (FlagMask.bXtras)	iRV += wsnprintfx(&pcBuf, &Size, " Ticks Stack Base -Task TCB-") ;
#endif
	if (FlagMask.bColor)	iRV += wsnprintfx(&pcBuf, &Size, "%C", attrRESET) ;
	iRV += wsnprintfx(&pcBuf, &Size, "\n") ;

	// loop through the whole task list
	uint32_t TaskMask = 0x00000001, Units, Fract ;
	for (UBaseType_t xNum = 1; xNum <= sRI.MaxNum; ++xNum) {
		TaskStatus_t *	psTS = psRtosStatsFindEntry(xNum) ;
		if (psTS == NULL)								// task# missing
			continue ;
		uint64_t u64RunTime = xRtosStatsGetRunTime(psTS->xHandle) ;

		// For IDLe task(s) we do not want to add RunTime %'s to the TasksRunTime or CoresRunTime
	    for(i = 0; i < portNUM_PROCESSORS; ++i) { if (psTS->xHandle == IdleHandle[i]) break ; }
	    if (i == portNUM_PROCESSORS) {				// fell through so NOT an IDLE task
	    	sRI.u64TasksRunTime += u64RunTime ;
	#if		(portNUM_PROCESSORS > 1)
	    	i = psTS->xCoreID == tskNO_AFFINITY ? 2 : psTS->xCoreID ;
			sRI.u64CoresRunTime[i] += u64RunTime ;
	#endif
	    }

	    // if task info display not enabled, skip....
		if (!(FlagMask.uCount & TaskMask))
			goto NextTask ;
		if (FlagMask.bCount)	iRV += wsnprintfx(&pcBuf, &Size, "%2u ",psTS->xTaskNumber) ;
		if (FlagMask.bPrioX)	iRV += wsnprintfx(&pcBuf, &Size, "%2u/%2u ", psTS->uxCurrentPriority, psTS->uxBasePriority) ;
		iRV += wsnprintfx(&pcBuf, &Size, configFREERTOS_TASKLIST_FMT_DETAIL, psTS->pcTaskName) ;
		/* Since the array was filled with scheduler suspended, current ie calling task
		 * is also in a Ready/Waiting state, so change flag if processing current task
		 * This fix still leaves the task running on the other MCU unidentified */
		if (FlagMask.bState)	iRV += wsnprintfx(&pcBuf, &Size, "%c ", psTS->xHandle == pCurTCB ? CHR_R : TaskState[psTS->eCurrentState]) ;
		if (FlagMask.bStack)	iRV += wsnprintfx(&pcBuf, &Size, "%'5u ", psTS->usStackHighWaterMark) ;
#if		(portNUM_PROCESSORS > 1)
		if (FlagMask.bCore)		iRV += wsnprintfx(&pcBuf, &Size, "%c ", (psTS->xCoreID > 1) ? CHR_X : CHR_0 + psTS->xCoreID) ;
#endif

		// Calculate & display individual task utilization.
    	Units = u64RunTime / SystemRT ;
    	Fract = (u64RunTime * 100 / SystemRT) % 100 ;
		iRV += wsnprintfx(&pcBuf, &Size, "%2u.%02u ", Units, Fract) ;

#if		(!defined(NDEBUG) || defined(DEBUG))
		if (FlagMask.bXtras)	iRV += wsnprintfx(&pcBuf, &Size, "%#'6llu %p %p", u64RunTime, pxTaskGetStackStart(psTS->xHandle), psTS->xHandle) ;
#endif
		iRV += wsnprintfx(&pcBuf, &Size, "\n") ;
NextTask:
		TaskMask <<= 1 ;
	}

	// Calculate & display total for "real" tasks utilization.
	Units = sRI.u64TasksRunTime / SystemRT ;
	Fract = (sRI.u64TasksRunTime * 100 / SystemRT) % 100 ;
	iRV += wsnprintfx(&pcBuf, &Size, "T=%u U=%u.%02u", sRI.NumTask, Units, Fract) ;

#if		(rtosSHOW_RTOSTIME)
	// Calculate & display total for ''other" (OS) utilization.
	uint64_t u64IntRunTime = (sRI.u64SystemRunTime * portNUM_PROCESSORS) - sRI.u64TotalRunTime ;
	Units = u64IntRunTime / SystemRT ;
	Fract = (u64IntRunTime * 100 / SystemRT) % 100 ;
	iRV += wsnprintfx(&pcBuf, &Size, " S=%u.%02u", Units, Fract) ;
#endif

#if		(portNUM_PROCESSORS > 1)
	// calculate & display individual core's utilization
    for(i = 0; i < (portNUM_PROCESSORS + 1); ++i) {
    	Units = sRI.u64CoresRunTime[i] / SystemRT ;
    	Fract = (sRI.u64CoresRunTime[i] * 100 / SystemRT) % 100 ;
    	iRV += wsnprintfx(&pcBuf, &Size, " %c=%u.%02u", i==portNUM_PROCESSORS ? 'X' : '0'+i, Units, Fract) ;
    }
#endif
    iRV += wsnprintfx(&pcBuf, &Size, FlagMask.bNL ? "\n\n" : "\n") ;
	if (pcBuf == NULL)
		printfx_unlock() ;

	free(sRI.pTSA) ;	    						// Free from vRtosStatsUpdate()
	return iRV ;
}

void	vRtosReportMemory(void) {
#if		defined(ESP_PLATFORM)
	halMCU_ReportMemory(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_32BIT) ;
	halMCU_ReportMemory(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) ;
	halMCU_ReportMemory(MALLOC_CAP_DMA) ;
	halMCU_ReportMemory(MALLOC_CAP_EXEC) ;
	halMCU_ReportMemory(MALLOC_CAP_IRAM_8BIT) ;
	halMCU_ReportMemory(MALLOC_CAP_SPIRAM) ;
#endif
    printfx("%CFreeRTOS%C\tMin=%'#u  Free=%'#u  Orig=%'#u\n\n", xpfSGR(attrRESET, colourFG_CYAN, 0, 0), xpfSGR(attrRESET, 0, 0, 0), xPortGetMinimumEverFreeHeapSize(), xPortGetFreeHeapSize(), g_HeapBegin) ;
}

#if 0

/**
 * halMCU_ReportMemory()
 * Based on heap_caps_print_heap_info() from the ESP-IDF
 */
int32_t printfx(const char *, ...) ;

#define	attrRESET			0
#define	attrULINE_ON		4
#define	attrREV_ON			7
#define	attrULINE_OFF		24
#define	attrREV_OFF			27

#define	attrFG_CYAN			36
#define	xpfSGR(a,b,c,d)		(((uint8_t) a << 24) + ((uint8_t) b << 16) + ((uint8_t) c << 8) + (uint8_t) d)

void	halMCU_ReportMemory(uint32_t caps) {
    printfx("%C TotBlk|UsedBlk|FreeBlk| LF_Blk| FromAddr | Size  | Used  | MinFr | Free  |%C0x%04x", xpfSGR(attrRESET, attrFG_CYAN, 0, 0), xpfSGR(attrRESET, 0, 0, 0), caps) ;
    if (caps & MALLOC_CAP_IRAM_8BIT)	printfx(" IRAM-8B") ;
    if (caps & MALLOC_CAP_DEFAULT)		printfx(" Default") ;
    if (caps & MALLOC_CAP_INTERNAL)		printfx(" Internal") ;
    if (caps & MALLOC_CAP_SPIRAM)		printfx(" SPIRAM") ;
    if (caps & MALLOC_CAP_DMA)			printfx(" DMA") ;
    if (caps & MALLOC_CAP_8BIT)			printfx(" 8B") ;
    if (caps & MALLOC_CAP_32BIT)		printfx(" 32B") ;
    if (caps & MALLOC_CAP_EXEC)			printfx(" EXEC") ;
    printfx("\n") ;
    multi_heap_info_t info ;
    heap_t * heap ;
    uint32_t size = 0 ;
    SLIST_FOREACH(heap, &registered_heaps, next) {
        if (heap_caps_match(heap, caps)) {
            multi_heap_get_info(heap->heap, &info) ;
            printfx("%'#7u %'#7u %'#7u %'#7u ",
        		info.total_blocks, info.allocated_blocks, info.free_blocks, info.largest_free_block) ;
            printfx("%p %'#7u %'#7u %'#7u %'#7u\n", heap->start, heap->end - heap->start,
        		info.total_allocated_bytes, info.minimum_free_bytes, info.total_free_bytes) ;
            size += heap->end - heap->start ;
        }
    }
    heap_caps_get_info(&info, caps) ;
    printfx("%C%'#7u %'#7u %'#7u %'#7u ", xpfSGR(attrFG_CYAN, 0, 0, 0), info.total_blocks,
    		info.allocated_blocks, info.free_blocks, info.largest_free_block) ;
    printfx("%C==Totals==%C", xpfSGR(attrREV_ON, 0, 0, 0), xpfSGR(attrREV_OFF, 0, 0, 0)) ;
    printfx(" %'#7u %'#7u %'#7u %'#7u%C\n\n", size, info.total_allocated_bytes,
    		info.minimum_free_bytes, info.total_free_bytes, xpfSGR(attrRESET, 0, 0, 0)) ;
}
#endif

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
void	vTaskDumpStack(void * pTCB, uint32_t StackSize) {
	if (pTCB == NULL)	pTCB = xTaskGetCurrentTaskHandle() ;

	void * pxTopOfStack	= (void *) * ((uint32_t *) pTCB)  ;
	void * pxStack		= (void *) * ((uint32_t *) pTCB + 12) ;		// 48 bytes / 4 = 12
	PRINT("Cur SP : %08x - Stack HWM : %08x\r\n", pxTopOfStack,
			(uint8_t *) pxStack + (uxTaskGetStackHighWaterMark(NULL) * sizeof(StackType_t))) ;
}