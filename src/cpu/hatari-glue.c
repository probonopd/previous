/*
  Hatari - hatari-glue.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains some code to glue the UAE CPU core to the rest of the
  emulator and Hatari's "illegal" opcodes.
*/
const char HatariGlue_fileid[] = "Hatari hatari-glue.c : " __DATE__ " " __TIME__;


#include <stdio.h>

#include "main.h"
#include "configuration.h"
#include "cycInt.h"
#include "cart.h"
#include "nextMemory.h"
#include "screen.h"
#include "video.h"

#include "sysdeps.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "hatari-glue.h"


struct uae_prefs currprefs, changed_prefs;

int pendingInterrupts = 0;


/**
 * Reset custom chips
 */
void customreset(void)
{
	pendingInterrupts = 0;

	/* In case the 6301 was executing a custom program from its RAM */
	/* we must turn it back to the 'normal' mode. */
//	IKBD_Reset_ExeMode ();

	/* Reseting the GLUE video chip should also set freq/res register to 0 */
	Video_Reset_Glue ();
}


/**
 * Return interrupt number (1 - 7), -1 means no interrupt.
 * Note that the interrupt stays pending if it can't be executed yet
 * due to the interrupt level field in the SR.
 */
int intlev(void)
{
	/* There are only VBL and HBL autovector interrupts in the ST... */
//	assert((pendingInterrupts & ~((1<<4)|(1<<2))) == 0);

	if (pendingInterrupts & (1 << 7))
	{
		if (regs.intmask < 7)
			pendingInterrupts &= ~(1 << 7);
		return 7;
	}
    else if (pendingInterrupts & (1 << 6))
	{
		if (regs.intmask < 6)
			pendingInterrupts &= ~(1 << 6);
		return 6;
	}
	else if (pendingInterrupts & (1 << 5))
	{
		if (regs.intmask < 5)
			pendingInterrupts &= ~(1 << 5);
		return 5;
	}
	else if (pendingInterrupts & (1 << 4))
	{
		if (regs.intmask < 4)
			pendingInterrupts &= ~(1 << 4);
		return 4;
	}
	else if (pendingInterrupts & (1 << 3))
	{
		if (regs.intmask < 3)
			pendingInterrupts &= ~(1 << 3);
		return 3;
	}
	else if (pendingInterrupts & (1 << 2))
	{
		if (regs.intmask < 2)
			pendingInterrupts &= ~(1 << 2);
		return 2;
	}
    else if (pendingInterrupts & (1 << 1))
	{
		if (regs.intmask < 1)
			pendingInterrupts &= ~(1 << 1);
		return 1;
	}

	return -1;
}

/**
 * Initialize 680x0 emulation
 */
int Init680x0(void)
{
	currprefs.cpu_level = changed_prefs.cpu_level = ConfigureParams.System.nCpuLevel;

	switch (currprefs.cpu_level) {
		case 0 : currprefs.cpu_model = 68000; break;
		case 1 : currprefs.cpu_model = 68010; break;
		case 2 : currprefs.cpu_model = 68020; break;
		case 3 : currprefs.cpu_model = 68030; break;
		case 4 : currprefs.cpu_model = 68040; break;
		case 5 : currprefs.cpu_model = 68060; break;
		default: fprintf (stderr, "Init680x0() : Error, cpu_level unknown\n");
	}
	
	currprefs.cpu_compatible = changed_prefs.cpu_compatible = ConfigureParams.System.bCompatibleCpu;
	currprefs.address_space_24 = changed_prefs.address_space_24 = ConfigureParams.System.bAddressSpace24;
	currprefs.cpu_cycle_exact = changed_prefs.cpu_cycle_exact = ConfigureParams.System.bCycleExactCpu;
	currprefs.fpu_model = changed_prefs.fpu_model = ConfigureParams.System.n_FPUType;
	currprefs.fpu_strict = changed_prefs.fpu_strict = ConfigureParams.System.bCompatibleFPU;
	currprefs.mmu_model = changed_prefs.mmu_model = ConfigureParams.System.bMMU;
   	write_log("Init680x0() called\n");

	init_m68k();

	return true;
}


/**
 * Deinitialize 680x0 emulation
 */
void Exit680x0(void)
{
	memory_uninit();

	free(table68k);
	table68k = NULL;
}
