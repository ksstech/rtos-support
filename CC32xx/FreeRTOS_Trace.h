/*
 * FreeRTOS_Trace.h
 */

#pragma once

#include "../rtos-support/FreeRTOS_TraceMacros.h"
#include	"x_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// ########################################## Macros  ##############################################

#define		configTRACE_USE_GPIO			( 0 )
#define		configTRACE_USE_MASK			( 0 )
#define		configTRACE_BUFSIZE				( 100 )

#define		configTRACE_ONE_WIDTH			(6 + 5 + 5 + 16)
#define		configTRACE_MAX_WIDTH			(configTRACE_ONE_WIDTH * 4)
#define		configTRACE_TIMESTAMP_INTERVAL	( 1000 )			// RTOS stats timer interval for full Timestamps

#if		(configTRACE_TIMESTAMP_INTERVAL == 10)
	#define		configTRACE_TIMESTAMP_WIDTH		1
#elif	(configTRACE_TIMESTAMP_INTERVAL == 100)
	#define		configTRACE_TIMESTAMP_WIDTH		2
#elif	(configTRACE_TIMESTAMP_INTERVAL == 1000)
	#define		configTRACE_TIMESTAMP_WIDTH		3
#else
	#error	"configTRACE_TIMESTAMP_INTERVAL invalid !!!"
#endif

// ########################### Enumerations, macros and structures #################################

/* Bitmapped sequential definition of 64 different trace masks, full _u64 variable size.
 * Includes logical group definitions of type of events that can be referred to together for simplicity */

enum {
// Tasks
	trcMASK_TaC			=	(1ULL << trcCODE_TaC),
	trcMASK_TaIT		=	(1ULL << trcCODE_TaIT),
	trcMASK_TaSI		=	(1ULL << trcCODE_TaSI),
	trcMASK_TaSO		=	(1ULL << trcCODE_TaSO),
	trcMASK_TaDy		=	(1ULL << trcCODE_TaDy),
	trcMASK_TaDU		=	(1ULL << trcCODE_TaDU),
	trcMASK_TaPS		=	(1ULL << trcCODE_TaPS),
	trcMASK_TaR			=	(1ULL << trcCODE_TaR),
	trcMASK_TRfI		=	(1ULL << trcCODE_TRfI),
	trcMASK_TMRy		=	(1ULL << trcCODE_TMRy),
	trcMASK_TaS			=	(1ULL << trcCODE_TaS),
	trcMASK_TaD			=	(1ULL << trcCODE_TaD),
// Queues
	trcMASK_QuC			=	(1ULL << trcCODE_QuC),
	trcMASK_QuTx		=	(1ULL << trcCODE_QuTx),
	trcMASK_QuTF		=	(1ULL << trcCODE_QuTF),
	trcMASK_QuRx		=	(1ULL << trcCODE_QuRx),
	trcMASK_QuRF		=	(1ULL << trcCODE_QuRF),
	trcMASK_QTfI		=	(1ULL << trcCODE_QTfI),
	trcMASK_QTIF		=	(1ULL << trcCODE_QTIF),
	trcMASK_QRfI		=	(1ULL << trcCODE_QRfI),
	trcMASK_QRIF		=	(1ULL << trcCODE_QRIF),
	trcMASK_QuPk		=	(1ULL << trcCODE_QuPk),
	trcMASK_QuPF		=	(1ULL << trcCODE_QuPF),
	trcMASK_QPfI		=	(1ULL << trcCODE_QPfI),
	trcMASK_QPIF		=	(1ULL << trcCODE_QPIF),
	trcMASK_QuD			=	(1ULL << trcCODE_QuD),
	trcMASK_QBoS		=	(1ULL << trcCODE_QBoS),
	trcMASK_QBoR		=	(1ULL << trcCODE_QBoR),
// Mutexes
	trcMASK_MuC			=	(1ULL << trcCODE_MuC),
	trcMASK_MuCF		=	(1ULL << trcCODE_MuCF),
	trcMASK_MuRG		=	(1ULL << trcCODE_MuRG),
	trcMASK_MRGF		=	(1ULL << trcCODE_MRGF),
	trcMASK_MuRT		=	(1ULL << trcCODE_MuRT),
	trcMASK_MRTF		=	(1ULL << trcCODE_MRTF),
// Counting Semaphores
	trcMASK_CSC			=	(1ULL << trcCODE_CSC),
	trcMASK_CSCF		=	(1ULL << trcCODE_CSCF),
// Timers
	trcMASK_TiC			=	(1ULL << trcCODE_TiC),
	trcMASK_TiCR		=	(1ULL << trcCODE_TiCR),
	trcMASK_TiCS		=	(1ULL << trcCODE_TiCS),
	trcMASK_TiEx		=	(1ULL << trcCODE_TiEx),
// Malloc/Free
	trcMASK_MAll		=	(1ULL << trcCODE_MAll),
	trcMASK_MFre		=	(1ULL << trcCODE_MFre),
// ALL categories combined
	trcMASK_NONE		=	( 0ULL ),

	trcMASK_TaALL		=	(trcMASK_TaC  | trcMASK_TaIT | trcMASK_TaSI | trcMASK_TaSO | trcMASK_TaDy | trcMASK_TaDU | \
							 trcMASK_TaPS | trcMASK_TaR  | trcMASK_TRfI | trcMASK_TMRy | trcMASK_TaS  | trcMASK_TaD),

	trcMASK_QuALL		=	(trcMASK_QuC  | trcMASK_QuTx | trcMASK_QuTF | trcMASK_QuRx | trcMASK_QuRF | trcMASK_QTfI | trcMASK_QTIF | trcMASK_QRfI | \
							 trcMASK_QRIF | trcMASK_QuPk | trcMASK_QuPF | trcMASK_QPfI | trcMASK_QPIF | trcMASK_QuD  | trcMASK_QBoS | trcMASK_QBoR),

	trcMASK_MuALL		=	(trcMASK_MuC  | trcMASK_MuCF | trcMASK_MuRG | trcMASK_MRGF | trcMASK_MuRT | trcMASK_MRTF),

	trcMASK_CsALL		=	(trcMASK_CSC | trcMASK_CSCF),

	trcMASK_TiALL		=	(trcMASK_TiC | trcMASK_TiCR | trcMASK_TiCS | trcMASK_TiEx),

	trcMASK_MeALL		= 	(trcMASK_MAll | trcMASK_MFre),

	trcMASK_ALL_FAIL	= 	(trcMASK_MuCF | trcMASK_CSCF),

	trcMASK_ALL			=	(trcMASK_TaALL | trcMASK_QuALL | trcMASK_MuALL | trcMASK_CsALL | trcMASK_TiALL),
} ;

typedef struct	{
	uint64_t	EventMask ;				// task trace flags mask
	uint32_t	BaseAddr ;				// GPIO port base address
	uint8_t		PinNumber ;				// PIN_01(0x00) to PIN_64 (0x3F)
	uint8_t		GpioNumber ;			// GPIO00 to GPIO32
	uint8_t		PinMask ;				// bit mask for specific pin
	uint8_t		PinState : 1 ;				// last state of pin
	uint8_t		ColorFG : 7 ;
} TraceTask_t ;

typedef struct	trc_event {
	uint32_t	ts ;
	uint32_t	code ;
	void *		task ;
	void *		P1 ;
	uint32_t	P2 ;
} trcEvent_t ;

extern	trcEvent_t		TrcBuf[] ;
extern	trcEvent_t *	pTrcIn, * pTrcOut ;

// ############################### Default trace handler function ##################################

void	vTraceInitialise(void) ;
void	vTraceStart(void) ;
void	vTraceStop(void) ;
void	vTraceMaskSet(uint32_t uxTaskNumber, uint64_t NewMask);
void	vTraceMaskClear(uint32_t uxNumber, uint64_t uxMask) ;
void	vTraceShow(void) ;

#ifdef __cplusplus
}
#endif
