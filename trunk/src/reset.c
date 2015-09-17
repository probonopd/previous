/*
  Hatari

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Reset emulation state.
*/
const char Reset_fileid[] = "Hatari reset.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "cycInt.h"
#include "m68000.h"
#include "reset.h"
#include "screen.h"
#include "nextMemory.h"
#include "tmc.h"
#include "video.h"
#include "debugcpu.h"
#include "scsi.h"
#include "mo.h"
#include "sysReg.h"
#include "rtcnvram.h"
#include "scc.h"
#include "ethernet.h"
#include "floppy.h"
#include "snd.h"
#include "dsp.h"


/*-----------------------------------------------------------------------*/
/**
 * Reset ST emulator states, chips, interrupts and registers.
 * Return zero or negative TOS image load error code.
 */
static const char* Reset_ST(bool bCold)
{
	if (bCold)
	{
		const char* error_str;
		error_str=memory_init(ConfigureParams.Memory.nMemoryBankSize);
		if (error_str!=NULL) {
			return error_str;
		}
	}
	CycInt_Reset();               /* Reset interrupts */
	Video_Reset();                /* Reset video */
	TMC_Reset();				  /* Reset TMC Registers */
	SCR_Reset();                  /* Reset System Control Registers */
	nvram_init();                 /* Reset NVRAM */
	SCSI_Reset();                 /* Reset SCSI disks */
	MO_Reset();                   /* Reset MO disks */
	Floppy_Reset();               /* Reset Floppy disks */
	SCC_Reset(2);                 /* Reset SCC */
	Ethernet_Reset(true);         /* Reset Ethernet */
	Sound_Reset();                /* Reset Sound */
	Screen_Reset();               /* Reset screen */
	DSP_Reset();                  /* Reset DSP */
	M68000_Reset(bCold);          /* Reset CPU */
	DebugCpu_SetDebugging();      /* Re-set debugging flag if needed */

	return NULL;
}


/*-----------------------------------------------------------------------*/
/**
 * Cold reset ST (reset memory, all registers and reboot)
 */
const char* Reset_Cold(void)
{
	Main_WarpMouse(sdlscrn->w/2, sdlscrn->h/2);  /* Set mouse pointer to the middle of the screen */

	return Reset_ST(true);
}


/*-----------------------------------------------------------------------*/
/**
 * Warm reset ST (reset registers, leave in same state and reboot)
 */
const char* Reset_Warm(void)
{
	return Reset_ST(false);
}
