/*
 * FreeRTOSTrace.c
 */

#include	"FreeRTOS_Support.h"

#if		(configBUILD_WITH_TRACE == 1)

#include	"FreeRTOS_Trace.h"

#include	"x_terminal.h"										// Colour definitions & functions
#include	"x_stdio.h"
#include	"x_values_to_string.h"

#include	<string.h>

// ################################### Macros & definitions ########################################

#define		flagTRACE_ENABLED		0x40000000

// #################################### Local and Global variables #################################
void	vTaskTrace(void * pVoid) ;

void *		TraceTask ;										// tracking CurrentTCB
void *		TaskTrace ;										// Task doing tracing output
uint32_t	TraceFlag ;
uint32_t	TraceWidth ;
uint32_t	TraceTime = 0 ;
#if		(configTRACE_USE_GPIO == 1)
	uint64_t	trcMASK_GPIO	= trcMASK_NONE ; /* (trcMASK_TaSO | trcMASK_TaSI) */
#endif

trcEvent_t 	TrcBuf[configTRACE_BUFSIZE] ;
trcEvent_t *	pTrcIn, * pTrcOut ;

const	char *	traceCODE[] = {
	"TaC ",		"TaIT", 	"TaSI", 	"TaSO", 	"TaDy",		"TaDU",		"TaPS",		"TaR ",
	"TRfI",		"TMRy",		"TaS ",		"TaD ",
	"QuC ",		"QuTx",		"QuTF",		"QuRx",		"QuRF",		"QTfI",		"QTIF",		"QRfI",
	"QRIF",		"QuPk",		"QuPF",		"QPfI",		"QPIF",		"QuD ",		"QBoS",		"QBoR",
	"MuC ",		"MuCF",		"MuRG",		"MRGF",		"MuRT",		"MRTF",
	"CSC ",		"CSCF",
	"TiC ",		"TiCR",		"TiCS",		"TiEx",
	"Aloc",		"Free",
} ;

#if 	(configTRACE_USE_MASK == 0)						// Default trace masks
TraceTask_t trcMASK_Tasks[taskNUM_MAX] = {
#if		(configBUILD_WITH_TRACE == 1)
	/*IDLE*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourFG_RED },
	/*TmrSvc*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourFG_GREEN },
#endif
	/*SlSpawn*/	{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_60*/,		0,		0,		0,		colourFG_BRRED },
	/*Console*/	{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_61*/,		0,		0,		0,		colourFG_BRGREEN },
	/*IPStack*/	{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_62*/,		0,		0,		0,		colourFG_BRYELLOW },
	/*Rx*/		{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_63*/,		0,		0,		0,		colourFG_BRBLUE },
	/*Tx*/		{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_03*/,		0,		0,		0,		colourFG_BRMAGENTA },
	/*Events*/	{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_05*/,		0,		0,		0,		colourFG_BRCYAN },
	/*Sensors*/	{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_06*/,		0,		0,		0,		colourFG_BRWHITE },
	/*Tftp*/	{ (trcMASK_MeALL),
					0UL,	255,				0,		0,		0,		colourFG_YELLOW }
} ;
#elif	(configTRACE_USE_MASK == 1)						// memory TRACE all tasks
#if		(configBUILD_WITH_TRACE == 1)
	/*IDLE*/	{ (trcMASK_MeALL),
					0UL,	255,				0,		0,		0,		colourRED+colourFOREGND },
	/*TmrSvc*/	{ (trcMASK_MeALL),
					0UL,	255,				0,		0,		0,		colourGREEN+colourFOREGND },
#endif
TraceTask_t trcMASK_Tasks[taskNUM_MAX] = {
#if		(configBUILD_WITH_TRACE == 1)
	/*IDLE*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourRED },
	/*TmrSvc*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourGREEN },
#endif
	/*SlSpawn*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_60*/,		0,		0,		0,		colourRED+colourBRIGHT+colourFOREGND },
	/*Console*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_61*/,		0,		0,		0,		colourGREEN+colourBRIGHT+colourFOREGND },
	/*IPStack*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_62*/,		0,		0,		0,		colourYELLOW+colourBRIGHT+colourFOREGND },
	/*Rx*/		{ (trcMASK_MeALL),
					0UL,	255/*PIN_63*/,		0,		0,		0,		colourBLUE+colourBRIGHT+colourFOREGND },
	/*Tx*/		{ (trcMASK_MeALL),
					0UL,	255/*PIN_03*/,		0,		0,		0,		colourMAGENTA+colourBRIGHT+colourFOREGND },
	/*Events*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_05*/,		0,		0,		0,		colourCYAN+colourBRIGHT+colourFOREGND },
	/*Sensors*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_06*/,		0,		0,		0,		colourWHITE+colourBRIGHT+colourFOREGND },
	/*Tftp*/	{ (trcMASK_MeALL),
					0UL,	255,				0,		0,		0,		colourYELLOW+colourFOREGND }
} ;
#elif	(configTRACE_USE_MASK == 2)
TraceTask_t trcMASK_Tasks[taskNUM_MAX] = {
#if		(configBUILD_WITH_TRACE == 1)
	/*IDLE*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourRED },
	/*TmrSvc*/	{ (trcMASK_MeALL),
					0UL,	255,		0,		0,		0,		colourGREEN },
#endif
	/*SlSpawn*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_60*/,		0,		0,		0,		colourFG_BRRED },
	/*Console*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_61*/,		0,		0,		0,		colourFG_BRGREEN },
	/*IPStack*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_62*/,		0,		0,		0,		colourFG_BRYELLOW },
	/*Rx*/		{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_63*/,		0,		0,		0,		colourFG_BRBLUE },
	/*Tx*/		{ (trcMASK_TaALL | trcMASK_TiALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_QuALL | trcMASK_MeALL) & (~trcMASK_TaIT),
					0UL,	255/*PIN_03*/,		0,		0,		0,		colourFG_BRMAGENTA },
	/*Events*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_05*/,		0,		0,		0,		colourFG_BRCYAN },
	/*Sensors*/	{ (trcMASK_MeALL),
					0UL,	255/*PIN_06*/,		0,		0,		0,		colourFG_BRWHITE },
	/*Tftp*/	{ (trcMASK_MeALL),
					0UL,	255,				0,		0,		0,		colourFG_YELLOW }
} ;
#endif

void	TraceString(const char * pStr, int32_t Len) {
	while (Len--) {
		if (*pStr != CHR_NUL) {
			putchar(*pStr++) ;
		} else {
			putchar(CHR_SPACE) ;
		}
	}
}

/* ################################# Common Trace handler ####################################### */

/*
 * vTrace() - translate trace event info and store into buffer
 */
void	vTrace(uint32_t trcCode, void * P1, uint32_t P2) {
	if ((TraceTask == NULL) ||							// not yet initialised ?
		(TraceFlag & flagTRACE_ENABLED) == 0 ||			// tracing not enabled ?
		(TraceTask == TaskTrace)) {						// tracing the trace task ?
		return ;										// ...skip out
	}
	pTrcIn->ts		= xRtosStatsTimerValue ;			// freeze the time stamp ASAP
	if (trcCode <= trcCODE_TaD) {
		pTrcIn->task	= P1 ;							// Task context is the specified task
	} else {
		pTrcIn->task	= TraceTask ;					// Task context is current task handle...
	}
	pTrcIn->code	= trcCode ;
	pTrcIn->P1		= P1 ;
	pTrcIn->P2		= P2 ;
	pTrcIn++ ;
	if (pTrcIn == &TrcBuf[configTRACE_BUFSIZE]) {
		pTrcIn = &TrcBuf[0] ;
	}
	if (pTrcIn == pTrcOut) {
		pTrcOut++ ;
		if (pTrcOut == &TrcBuf[configTRACE_BUFSIZE]) {
			pTrcOut = &TrcBuf[0] ;
		}
	}
}

#if		(configTRACE_USE_GPIO == 1)
void	vTraceGPIO(uint32_t trcCode, uint32_t TraceIdx) {
	if (trcMASK_Tasks[TraceIdx].PinNumber != 255) {		// trace pin configured?
	// set applicable pin status
		switch (trcCode) {
		case trcCODE_TaSI:								// Set pin HIGH
			trcMASK_Tasks[TraceIdx].PinState = trcMASK_Tasks[TraceIdx].PinMask ;
			break ;
		case trcCODE_TaSO:								// Set pin LOW
			trcMASK_Tasks[TraceIdx].PinState = 0 ;
			break ;
		default:										// TOGGLE pin
			trcMASK_Tasks[TraceIdx].PinState ^= trcMASK_Tasks[TraceIdx].PinMask ;
			break ;
		}
	// Write the new pin value
		MAP_GPIOPinWrite(trcMASK_Tasks[TraceIdx].BaseAddr, trcMASK_Tasks[TraceIdx].PinMask, trcMASK_Tasks[TraceIdx].PinState) ;
	}
}
#endif

void	vTraceShow(void) {
char	Buffer[20] ;									// 16 should be fine...
char *	TraceName ;
char * 	pBuf ;
uint64_t	TraceMask ;
uint32_t	TraceIdx ;
// create 64 bit trace event request mask
	TraceMask	= BIT00MASK << pTrcOut->code ;
// Handle Task name based on category of traceXXXXX
	if (TraceMask & trcMASK_TaALL) {					// TASK related P1 is TaskHandle
		TraceName	= pcTaskGetName(pTrcOut->task) ;		// get the specified task name
	} else if (TraceMask & trcMASK_TiALL) {				// TIMER related, P1 is TimerHandle
		TraceName	= (char *) pcTimerGetName(pTrcOut->P1) ;	// Get timer name based on handle
	} else {											// Queue, semaphore or mutex related
		TraceName	= pcTaskGetName(NULL) ;				// Get current task name
	}

// Now calculate the task number/index
	for (TraceIdx = 0; TraceIdx < taskNUM_MAX; TraceIdx++) {
		if (pTrcOut->task == sTaskInfo[TraceIdx].TaskHandle) {
			break ;
		}
	}

	if (TraceIdx >= taskNUM_MAX) {
		ASSERT(0) ;
		return ;
	}

// Filter #2 Check if tracing for THIS task with THIS flag enabled
	if (trcMASK_Tasks[TraceIdx].EventMask & TraceMask) {
	// Filter #3 Check if GPIO action selected
#if		(configTRACE_USE_GPIO == 1)
		if (trcMASK_GPIO & TraceMask) {
			vTraceGPIO(pTrcOut->code, TraceIdx) ;
			return ;
		}
#endif
		if (((pTrcOut->ts / configTRACE_TIMESTAMP_INTERVAL) > TraceTime) ||
			((TraceTime == 0) && (TraceWidth == 0))) {
			TraceTime	= pTrcOut->ts / configTRACE_TIMESTAMP_INTERVAL ;
			puts("\r\n TS=") ;
			pBuf	= Buffer ;
			pBuf	+= xU32ToDecStr(TraceTime, pBuf) ;
			puts(Buffer) ;
			puts("\r\n") ;
			TraceWidth = 0 ;
		} else if ((pTrcOut->code == trcCODE_TaSI) || (TraceWidth > configTRACE_MAX_WIDTH)) {	// new task running ?
			puts("\r\n") ;
			TraceWidth = 0 ;
		}
	// print the fractional time stamp
		pTrcOut->ts %= configTRACE_TIMESTAMP_INTERVAL ;
#if		(configTRACE_TIMESTAMP_INTERVAL == 10)
		pTrcOut->ts *= 100 ;
#elif		(configTRACE_TIMESTAMP_INTERVAL == 100)
		pTrcOut->ts *= 10 ;
#endif
		pBuf	= Buffer ;
		*pBuf++	= CHR_0 ;
		*pBuf++	= CHR_FULLSTOP ;		// "0."
		if (pTrcOut->ts < 100) {
			*pBuf++	= CHR_0 ;			// "0.0"
			if (pTrcOut->ts < 10) {
				*pBuf++	= CHR_0 ;		// "0.00"
			}
		}
		pBuf	+= xU32ToDecStr(pTrcOut->ts, pBuf) ;
		*pBuf++	= CHR_SPACE ;
		*pBuf++	= CHR_NUL ;
		puts(Buffer) ;
		TraceWidth	+= pBuf - Buffer ;
	// setup colors for TRACEd task name
		vTermSetForeBackground(trcMASK_Tasks[TraceIdx].ColorFG, colourBLACK) ;
	// print the TRACEd task name
		TraceString(TraceName, 5) ;
		TraceWidth += 5 ;

	// Setup colors for TRACE function
		vTermSetForeBackground(colourWHITE_BRIGHT,  (pTrcOut->code < trcCODE_QuC)	? colourRED		:
													(pTrcOut->code < trcCODE_MuC)	? colourGREEN	:
													(pTrcOut->code < trcCODE_CSC)	? colourYELLOW	:
													(pTrcOut->code < trcCODE_TiC)	? colourBLUE	:
													(pTrcOut->code < trcCODE_MAll)	? colourMAGENTA	:
													(pTrcOut->code < trcCODE_NUM)	? colourCYAN	: colourWHITE) ;
	// print the TRACE function
		TraceString(traceCODE[pTrcOut->code], 5) ;
		TraceWidth += 5 ;
	// Setup colors for TRACE parameters
		vTermSetForeBackground(colourWHITE, colourBLACK) ;
	// Handle the parameters based on category
		if (TraceMask & trcMASK_TaALL) {
			if ((pTrcOut->code == trcCODE_TaIT) ||
				(pTrcOut->code == trcCODE_TaDU) ||
				(pTrcOut->code == trcCODE_TaPS)) {
				TraceWidth += printfx("%-14d  ", pTrcOut->P2) ;
			} else {
				TraceString("", 16) ;
				TraceWidth += 16 ;
			}
		} else if (TraceMask & trcMASK_MeALL) {
			TraceWidth += printfx("x%08x/%04d  ", pTrcOut->P1, pTrcOut->P2) ;
		} else {
			TraceWidth += printfx("x%08x       ", pTrcOut->P1) ;
		}
	}
}

// ##################################### trace support #######################################

/*
 * vTraceInitialise()
 * \brief		Initialize the pins, setup config array and set inital status of pins
 * \param[in]	uxSize - size of buffer to allocate
 * \param[out]	none
 * \return		none
 */
void 	vTraceInitialise(void) {
int32_t	iRV ;
#if		(configTRACE_USE_GPIO == 1)
halGPIO_Init_t	tracePIN ;
int32_t	iIdx ;
// clear structure & set common info...
	memset(&tracePIN, 0xFF, sizeof(tracePIN)) ;			// Preinit all fields to 0xFF
	tracePIN.mode		= PIN_MODE_0 ;
	tracePIN.type		= PIN_TYPE_STD ;
	tracePIN.drive		= PIN_STRENGTH_2MA ;
// now do individual pin setup
	for (iIdx = 0; iIdx < taskNUM_MAX; iIdx ++) {
		if ((trcMASK_Tasks[iIdx].PinNumber) != 255) {
			tracePIN.pin_num				= trcMASK_Tasks[iIdx].PinNumber ;
			halGPIO_Config(&tracePIN) ;
		// set pin initial status
			MAP_GPIOPinWrite(tracePIN.base, tracePIN.mask, 0) ;		// write '0 as off
		// save pin info
			trcMASK_Tasks[iIdx].GpioNumber	= tracePIN.gpio_num ;
			trcMASK_Tasks[iIdx].PinMask		= tracePIN.mask ;
			trcMASK_Tasks[iIdx].BaseAddr	= tracePIN.base ;
			trcMASK_Tasks[iIdx].PinState	= 0 ;
		}
	}
#endif
// save the Timer Task handle
	iRV = xRtosTaskFindIndex(NULL) ;			// find empty entry
	if (iRV == erFAILURE) {
		ASSERT(0)
	}
	sTaskInfo[iRV].TaskHandle	= xTimerGetTimerDaemonTaskHandle() ;
// save the Idle task handle
	iRV = xRtosTaskFindIndex(NULL) ;			// find empty entry
	if (iRV == erFAILURE) {
		ASSERT(0)
	}
	sTaskInfo[iRV].TaskHandle	= xTaskGetIdleTaskHandle() ;
// create the actual tracing task
	iRV = xRtosTaskCreate(vTaskTrace, "TRACE", 512, 0, 5, NULL, 1) ;
	ASSERT(iRV == pdPASS)
	pTrcIn	= pTrcOut	= &TrcBuf[0] ;
}

/*
 * vTraceStart()
 * \brief		start running trace and optionally couple UART (& uncouple StdOut)
 * \param[in]	flag = true to enable auto display switching
 * \return		none
 */
void	vTraceStart(void) {
	TraceFlag |= flagTRACE_ENABLED ;
}

/*
 * vTraceStop()
 * \brief		stop running trace and optionally uncouple UART (& couple StdOut)
 * \param[in]	flag = true to enable auto display switching
 * \return		none
 */
void	vTraceStop(void) {
	TraceFlag &= ~flagTRACE_ENABLED ;
}

/*
 * vTraceMaskSet()
 * \brief		Change the trace mask for a specific task by adding specified bit mask
 * \param[in]	uxNumber - task number for which to change the mask
 * \param[in]	uxMask - bit mask to set/add
 * \return		none
 */
void	vTraceMaskSet(uint32_t uxNumber, uint64_t uxMask) {
	trcMASK_Tasks[uxNumber].EventMask |= uxMask ;
}

/*
 * vTraceMaskClear()
 * \brief		Change the trace mask for a specific task, by removing specified bit mask
 * \param[in]	uxNumber - task number for which to change the mask
 * \param[in]	uxMask - bit mask to clear/remove
 * \return		none
 */
void	vTraceMaskClear(uint32_t uxNumber, uint64_t uxMask) {
	trcMASK_Tasks[uxNumber].EventMask &= ~uxMask ;
}

void	vTaskTrace(void * pVoid) {
	while(1) {
		while (pTrcOut != pTrcIn) {
			vTraceShow() ;
			pTrcOut++ ;
			if (pTrcOut == &TrcBuf[configTRACE_BUFSIZE]) {
				pTrcOut	= &TrcBuf[0] ;
			}
		}
		vTaskDelay(pdMS_TO_TICKS(5)) ;
	}
}
#endif
