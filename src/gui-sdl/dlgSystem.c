/*
  Hatari - dlgSystem.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

 This file contains 2 system panels :
    - 1 for the old uae CPU
    - 1 for the new WinUae cpu

 The selection is made during compilation with the ENABLE_WINUAE_CPU define

*/
const char DlgSystem_fileid[] = "Hatari dlgSystem.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"


#define DLGSYS_68030      4
#define DLGSYS_68040      5
#define DLGSYS_68060      6

#define DLGSYS_CUBE030     9
#define DLGSYS_MONOSLAB       10
#define DLGSYS_TURBOSLAB         11


#define DLGSYS_8MHZ       16
#define DLGSYS_16MHZ      17
#define DLGSYS_32MHZ      18

#define DLGSYS_DSPOFF     21
#define DLGSYS_DSPDUMMY   22
#define DLGSYS_DSPON      23


static SGOBJ systemdlg[] =
{
 	{ SGBOX, 0, 0, 0,0, 60,25, NULL }, // 0
 	{ SGTEXT, 0, 0, 23,1, 14,1, "System options" }, // 1
 
 	{ SGBOX, 0, 0, 19,3, 11,9, NULL }, // 2
 	{ SGTEXT, 0, 0, 20,3, 8,1, "CPU type" }, // 3
 	{ SGRADIOBUT, 0, 0, 20,8, 13,1, "68030" }, // 4
 	{ SGRADIOBUT, 0, 0, 20,9, 13,1, "68040" }, // 5
 	{ SGRADIOBUT, 0, 0, 20,10, 7,1, "68060" }, // 6
 
 	{ SGBOX, 0, 0, 2,3, 15,9, NULL }, // 7
 	{ SGTEXT, 0, 0, 3,3, 13,1, "Machine type" }, // 8
 	{ SGRADIOBUT, 0, 0, 3,5, 8,1, "Next computer (cube)" }, // 9
 	{ SGRADIOBUT, 0, 0, 3,6, 8,1, "Mono slab" }, // 10
 	{ SGRADIOBUT, 0, 0, 3,7, 8,1, "Turbo mono slab" }, // 11
 	{ SGRADIOBUT, 0, 0, 3,8, 8,1, "not yet" }, // 12
 	{ SGRADIOBUT, 0, 0, 3,8, 8,1, "not yet" }, // 13
 
 	{ SGBOX, 0, 0, 32,3, 12,9, NULL }, // 14
 	{ SGTEXT, 0, 0, 33,3, 15,1, "CPU clock" }, // 15
 	{ SGRADIOBUT, 0, 0, 33,5, 3,1, " 8 Mhz" }, // 16
 	{ SGRADIOBUT, 0, 0, 33,6, 4,1, "25 Mhz" }, // 17
 	{ SGRADIOBUT, 0, 0, 33,7, 4,1, "33 Mhz" }, // 18
 
 	{ SGBOX, 0, 0, 46,3, 12,9, NULL }, // 19
 	{ SGTEXT, 0, 0, 47,3, 11,1, "DSP" }, // 20
 	{ SGRADIOBUT, 0, 0, 47,5, 5,1, "None" }, // 21
 	{ SGRADIOBUT, 0, 0, 47,6, 7,1, "Dummy" }, // 22
 	{ SGRADIOBUT, 0, 0, 47,7, 4,1, "Full" }, // 23
   
 	{ SGBUTTON, SG_DEFAULT, 0, 21,23, 20,1, "Back to main menu" }, // 24
 	{ -1, 0, 0, 0,0, 0,0, NULL }
 };


/*-----------------------------------------------------------------------*/
/**
  * Show and process the "System" dialog (specific to winUAE cpu).
  */
void Dialog_SystemDlg(void)
{
 	int i;
 	MACHINETYPE	mti;
 
 	SDLGui_CenterDlg(systemdlg);
 
 	/* Set up dialog from actual values: */
 
 	for (i = DLGSYS_68030; i <= DLGSYS_68060; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
 	systemdlg[DLGSYS_68030+ConfigureParams.System.nCpuLevel-3].state |= SG_SELECTED;
 
 	for (i = DLGSYS_CUBE030; i <= DLGSYS_TURBOSLAB; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
 	systemdlg[DLGSYS_CUBE030 + ConfigureParams.System.nMachineType].state |= SG_SELECTED;
 
 	/* CPU frequency: */
 	for (i = DLGSYS_8MHZ; i <= DLGSYS_16MHZ; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
 	if (ConfigureParams.System.nCpuFreq == 32)
 		systemdlg[DLGSYS_32MHZ].state |= SG_SELECTED;
 	else if (ConfigureParams.System.nCpuFreq == 16)
 		systemdlg[DLGSYS_16MHZ].state |= SG_SELECTED;
	else
 		systemdlg[DLGSYS_8MHZ].state |= SG_SELECTED;
 
 	/* DSP mode: */
 	for (i = DLGSYS_DSPOFF; i <= DLGSYS_DSPON; i++)
 	{
 		systemdlg[i].state &= ~SG_SELECTED;
 	}
 	if (ConfigureParams.System.nDSPType == DSP_TYPE_NONE)
 		systemdlg[DLGSYS_DSPOFF].state |= SG_SELECTED;
 	else if (ConfigureParams.System.nDSPType == DSP_TYPE_DUMMY)
 		systemdlg[DLGSYS_DSPDUMMY].state |= SG_SELECTED;
 	else
 		systemdlg[DLGSYS_DSPON].state |= SG_SELECTED;
 


 
 		
 	/* Show the dialog: */
 	SDLGui_DoDialog(systemdlg, NULL);
 
 	/* Read values from dialog: */
 
 	for (i = DLGSYS_68030; i <= DLGSYS_68060; i++)
 	{
 		if (systemdlg[i].state&SG_SELECTED)
 		{
 			ConfigureParams.System.nCpuLevel = i-DLGSYS_68030+3;
 			break;
 		}
 	}
 
 	for (mti = DLGSYS_CUBE030; mti <= DLGSYS_TURBOSLAB; mti++)
 	{
 		if (systemdlg[mti + DLGSYS_CUBE030].state&SG_SELECTED)
 		{
 			ConfigureParams.System.nMachineType = mti;
 			break;
 		}
	}
 
 	if (systemdlg[DLGSYS_32MHZ].state & SG_SELECTED)
 		ConfigureParams.System.nCpuFreq = 33;
 	else if (systemdlg[DLGSYS_16MHZ].state & SG_SELECTED)
 		ConfigureParams.System.nCpuFreq = 25;
 	else
 		ConfigureParams.System.nCpuFreq = 8;
 
 	if (systemdlg[DLGSYS_DSPOFF].state & SG_SELECTED)
 		ConfigureParams.System.nDSPType = DSP_TYPE_NONE;
 	else if (systemdlg[DLGSYS_DSPDUMMY].state & SG_SELECTED)
 		ConfigureParams.System.nDSPType = DSP_TYPE_DUMMY;
 	else
 		ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
 
 	ConfigureParams.System.bCompatibleCpu = 1;
 	ConfigureParams.System.bBlitter = 0;
 	ConfigureParams.System.bRealTimeClock = 0;
 	ConfigureParams.System.bPatchTimerD = 0;
 	ConfigureParams.System.bAddressSpace24 = 0;
 	ConfigureParams.System.bCycleExactCpu = 0;
 
 	/* FPU emulation */
 	ConfigureParams.System.n_FPUType = FPU_CPU;
 
 	ConfigureParams.System.bCompatibleFPU = 1;
 	ConfigureParams.System.bMMU = 1;
}

