/*
  Hatari - cycInt.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This code handles our table with callbacks for cycle accurate program
  interruption. We add any pending callback handler into a table so that we do
  not need to test for every possible interrupt event. We then scan
  the list if used entries in the table and copy the one with the least cycle
  count into the global 'PendingInterrupt' variable. This is then
  decremented by the execution loop - rather than decrement each and every
  entry (as the others cannot occur before this one).
  We support three time units: CPU cycles, ticks, and microseconds.
  Ticks are bound to CPU cycles and run at TICK_RATE MHz. Microseconds are either
  bound to the host CPU's performance counter in real-time mode or to the emulated
  CPU cycles if non-realtime mode.
*/

const char CycInt_fileid[] = "Previous cycInt.c : " __DATE__ " " __TIME__;

#include <stdint.h>
#include <assert.h>
#include "main.h"
#include "cycInt.h"
#include "m68000.h"
#include "screen.h"
#include "video.h"
#include "sysReg.h"
#include "esp.h"
#include "mo.h"
#include "ethernet.h"
#include "dma.h"
#include "floppy.h"
#include "snd.h"
#include "printer.h"
#include "kms.h"
#include "configuration.h"
#include "main.h"
#include "nd_sdl.h"

void (*PendingInterruptFunction)(void);
Sint64 PendingInterruptCounter;
int    usCheckCycles;

static Sint64 nCyclesOver;
Sint64 nCyclesMainCounter;         /* Main cycles counter, counts emulated CPU cycles sind reset */

static const Sint64 TICK_RATE = 8; /* Tick rate is 8MHz */

/* List of possible interrupt handlers to be store in 'PendingInterruptTable',
 * used for 'MemorySnapShot' */
static void (* const pIntHandlerFunctions[MAX_INTERRUPTS])(void) =
{
	NULL,
	Video_InterruptHandler_VBL,
	Hardclock_InterruptHandler,
    Mouse_Handler,
    ESP_InterruptHandler,
    ESP_IO_Handler,
    M2RDMA_InterruptHandler,
    R2MDMA_InterruptHandler,
    MO_InterruptHandler,
    MO_IO_Handler,
    ECC_IO_Handler,
    ENET_IO_Handler,
    FLP_IO_Handler,
    SND_Out_Handler,
    SND_In_Handler,
    Printer_IO_Handler,
    Main_EventHandlerInterrupt,
    nd_vbl_handler,
    nd_video_vbl_handler,
};

static INTERRUPTHANDLER InterruptHandlers[MAX_INTERRUPTS];
INTERRUPTHANDLER        PendingInterrupt;
static int              ActiveInterrupt=0;

static void CycInt_SetNewInterrupt(void);

extern Uint8 NEXTRom[0x20000];

/*-----------------------------------------------------------------------*/
/**
 * Reset interrupts, handlers
 */
void CycInt_Reset(void) {
	int i;

	/* Reset counts */
    PendingInterrupt.time = 0;
	ActiveInterrupt       = 0;
	nCyclesOver           = 0;
    nCyclesMainCounter    = 0;
    usCheckCycles         = 0;
        
	/* Reset interrupt table */
	for (i=0; i<MAX_INTERRUPTS; i++) {
		InterruptHandlers[i].type      = CYC_INT_NONE;
		InterruptHandlers[i].time      = INT64_MAX;
		InterruptHandlers[i].pFunction = pIntHandlerFunctions[i];
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Find next interrupt to occur, and store to global variables for decrement
 * in instruction decode loop.
 * (SC) Microseconf interrupts are skipped here and handled in the decode loop.
 */
static void CycInt_SetNewInterrupt(void) {
	Sint64       LowestCycleCount = INT64_MAX;
	interrupt_id LowestInterrupt  = INTERRUPT_NULL;
    
	/* Find next interrupt to go off */
	for(int i = INTERRUPT_NULL+1; i < MAX_INTERRUPTS; i++) {
        /* Is interrupt pending? */
        if(InterruptHandlers[i].type == CYC_INT_CPU && InterruptHandlers[i].time < LowestCycleCount) {
            LowestCycleCount = InterruptHandlers[i].time;
            LowestInterrupt  = i;
        }
	}

	/* Set new counts, active interrupt */
    PendingInterrupt = InterruptHandlers[LowestInterrupt];
	ActiveInterrupt  = LowestInterrupt;
}

/*-----------------------------------------------------------------------*/
/**
 * Adjust all interrupt timings, MUST call CycInt_SetNewInterrupt after this.
 */
static void CycInt_UpdateInterrupt(void) {
	Sint64 CycleSubtract;
	int i;

	/* Find out how many cycles we went over (<=0) */
	nCyclesOver = PendingInterrupt.time;
	/* Calculate how many cycles have passed, included time we went over */
	CycleSubtract = InterruptHandlers[ActiveInterrupt].time - nCyclesOver;

	/* Adjust table */
	for (i = 0; i < MAX_INTERRUPTS; i++) {
		if (InterruptHandlers[i].type == CYC_INT_CPU)
			InterruptHandlers[i].time -= CycleSubtract;
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Check all microsecond interrupt timings
 */
bool CycInt_SetNewInterruptUs(void) {
    Sint64 now = host_time_us();
    for(int i = 0; i < MAX_INTERRUPTS; i++) {
        if (InterruptHandlers[i].type == CYC_INT_US && now > InterruptHandlers[i].time) {
            PendingInterrupt = InterruptHandlers[i];
            PendingInterrupt.time = -1;
            ActiveInterrupt       = i;
            return true;
        }
    }
    return false;
}

/*-----------------------------------------------------------------------*/
/**
 * Adjust all interrupt timings as 'ActiveInterrupt' has occured, and
 * remove from active list.
 */
void CycInt_AcknowledgeInterrupt(void) {
	/* Update list cycle counts */
	CycInt_UpdateInterrupt();

	/* Disable interrupt entry which has just occured */
	InterruptHandlers[ActiveInterrupt].type = CYC_INT_NONE;

	/* Set new */
	CycInt_SetNewInterrupt();
}

/*-----------------------------------------------------------------------*/
/**
 * Add interrupt to occur from now.
 */
void CycInt_AddRelativeInterruptCycles(Sint64 CycleTime, interrupt_id Handler) {
	assert(CycleTime >= 0);

	/* Update list cycle counts with current PendingInterruptCount before adding a new int, */
	/* because CycInt_SetNewInterrupt can change the active int / PendingInterruptCount */
	if ( ActiveInterrupt > 0 )
		CycInt_UpdateInterrupt();

	InterruptHandlers[Handler].type = CYC_INT_CPU;
	InterruptHandlers[Handler].time = CycleTime;

	/* Set new active int and compute a new value for PendingInterruptCount*/
	CycInt_SetNewInterrupt();
}

/*-----------------------------------------------------------------------*/
/**
 * Add interrupt to occur from now.
 */
void CycInt_AddRelativeInterruptTicks(Sint64 TickTime, interrupt_id Handler) {
    TickTime *= ConfigureParams.System.nCpuFreq;
    CycInt_AddRelativeInterruptCycles(TickTime / TICK_RATE, Handler);
}

/*-----------------------------------------------------------------------*/
/**
 * Add interrupt to occur us microsencods from now
 */
void CycInt_AddRelativeInterruptUs(Sint64 us, interrupt_id Handler) {
    assert(us >= 0);
    
    if(ConfigureParams.System.bRealtime) {
        /* Update list cycle counts with current PendingInterruptCount before adding a new int, */
        /* because CycInt_SetNewInterrupt can change the active int / PendingInterruptCount */
        if ( ActiveInterrupt > 0 ) CycInt_UpdateInterrupt();
        
        InterruptHandlers[Handler].type = CYC_INT_US;
        InterruptHandlers[Handler].time = host_time_us() + us;
        
        /* Set new active int and compute a new value for PendingInterruptCount*/
        CycInt_SetNewInterrupt();
    } else {
        CycInt_AddRelativeInterruptCycles(us * ConfigureParams.System.nCpuFreq, Handler);
    }
}

/*-----------------------------------------------------------------------*/
/**
 * Remove a pending interrupt from our table
 */
void CycInt_RemovePendingInterrupt(interrupt_id Handler) {
	/* Update list cycle counts, including the handler we want to remove */
	/* to be able to resume it later */
	CycInt_UpdateInterrupt();

	/* Stop interrupt after CycInt_UpdateInterrupt */
	InterruptHandlers[Handler].type = CYC_INT_NONE;

	/* Set new */
	CycInt_SetNewInterrupt();
}


/*-----------------------------------------------------------------------*/
/**
 * Return true if interrupt is active in list
 */
bool CycInt_InterruptActive(interrupt_id Handler)
{
    return InterruptHandlers[Handler].type != CYC_INT_NONE;
}
