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
#include "video.h"
#include "sysReg.h"

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
 * Return interrupt number (1 - 7), 0 means no interrupt.
 * Note that the interrupt stays pending if it can't be executed yet
 * due to the interrupt level field in the SR.
 */
int intlev(void)
{
    /* Poll interrupt level from interrupt status and mask registers
     * --> see sysReg.c
     */
    return get_interrupt_level();
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
    
    currprefs.fpu_model = changed_prefs.fpu_model = ConfigureParams.System.n_FPUType;
    switch (currprefs.fpu_model) {
        case 68881: currprefs.fpu_revision = 0x1f; break;
        case 68882: currprefs.fpu_revision = 0x20; break;
        case 68040:
            if (ConfigureParams.System.bTurbo)
                currprefs.fpu_revision = 0x41;
            else
                currprefs.fpu_revision = 0x40;
            break;
		default: fprintf (stderr, "Init680x0() : Error, fpu_model unknown\n");
    }
	
	currprefs.cpu_compatible = changed_prefs.cpu_compatible = ConfigureParams.System.bCompatibleCpu;
	currprefs.address_space_24 = changed_prefs.address_space_24 = ConfigureParams.System.bAddressSpace24;
	currprefs.cpu_cycle_exact = changed_prefs.cpu_cycle_exact = ConfigureParams.System.bCycleExactCpu;
	currprefs.fpu_strict = changed_prefs.fpu_strict = ConfigureParams.System.bCompatibleFPU;
    currprefs.mmu_model = changed_prefs.mmu_model = ConfigureParams.System.bMMU?changed_prefs.cpu_model:0;

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
