/*
  Hatari - dlgSystem.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

 This file contains 2 system panels :
    - 1 for the old uae CPU
    - 1 for the new WinUae cpu

 The selection is made during compilation with the ENABLE_WINUAE_CPU define

*/
const char DlgAdvanced_fileid[] = "Hatari dlgAdvanced.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"


#define DLGADV_68030      4
#define DLGADV_68040      5
#define DLGADV_68060      6

#define DLGADV_16MHZ      9
#define DLGADV_20MHZ      10
#define DLGADV_25MHZ      11
#define DLGADV_33MHZ      12
#define DLGADV_40MHZ      13

#define DLGADV_FPU68881   16
#define DLGADV_FPU68882   17
#define DLGADV_FPU040     18

#define DLGADV_SCSI_OLD   21
#define DLGADV_SCSI_NEW   22

#define DLGADV_RTC_OLD    25
#define DLGADV_RTC_NEW    26

#define DLGADV_ADB        29

#define DLGADV_EXIT       30


static SGOBJ advanceddlg[] =
{
    { SGBOX, 0, 0, 0,0, 57,29, NULL },
    { SGTEXT, 0, 0, 18,1, 14,1, "Advanced system options" },
    
    { SGBOX, 0, 0, 2,3, 14,13, NULL },
    { SGTEXT, 0, 0, 3,4, 14,1, "CPU type" },
    { SGRADIOBUT, 0, 0, 4,6, 7,1, "68030" },
    { SGRADIOBUT, 0, 0, 4,8, 7,1, "68040" },
    { SGRADIOBUT, 0, 0, 4,10, 7,1, "68060" },
    
    { SGBOX, 0, 0, 17,3, 14,13, NULL },
    { SGTEXT, 0, 0, 18,4, 14,1, "CPU clock" },
    { SGRADIOBUT, 0, 0, 19,6, 8,1, "16 MHz" },
    { SGRADIOBUT, 0, 0, 19,8, 8,1, "20 MHz" },
    { SGRADIOBUT, 0, 0, 19,10, 8,1, "25 MHz" },
    { SGRADIOBUT, 0, 0, 19,12, 8,1, "33 MHz" },
    { SGRADIOBUT, 0, 0, 19,14, 8,1, "40 MHz" },
    
    { SGBOX, 0, 0, 32,3, 23,13, NULL },
    { SGTEXT, 0, 0, 33,4, 14,1, "FPU type" },
    { SGRADIOBUT, 0, 0, 34,6, 7,1, "68881" },
    { SGRADIOBUT, 0, 0, 34,8, 7,1, "68882" },
    { SGRADIOBUT, 0, 0, 34,10, 16,1, "68040 internal" },
    
    { SGBOX, 0, 0, 2,17, 18,7, NULL },
    { SGTEXT, 0, 0, 3,18, 14,1, "SCSI controller" },
    { SGRADIOBUT, 0, 0, 4,20, 10,1, "NCR53C90" },
    { SGRADIOBUT, 0, 0, 4,22, 11,1, "NCR53C90A" },
    
    { SGBOX, 0, 0, 21,17, 16,7, NULL },
    { SGTEXT, 0, 0, 22,18, 14,1, "RTC chip" },
    { SGRADIOBUT, 0, 0, 23,20, 12,1, "MC68HC68T1" },
    { SGRADIOBUT, 0, 0, 23,22, 9,1, "MCS1850" },
    
    { SGBOX, 0, 0, 38,17, 17,7, NULL },
    { SGTEXT, 0, 0, 39,18, 14,1, "ADB" },
    { SGCHECKBOX, 0, 0, 40,20, 13,1, "Emulate ADB" },
    
    { SGBUTTON, SG_DEFAULT, 0, 18,26, 22,1, "Back to system menu" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
  * Show and process the "Advanced" dialog (specific to winUAE cpu).
  */
void Dialog_AdvancedDlg(void) {

 	int i;
    int but;
 
 	SDLGui_CenterDlg(advanceddlg);
 
 	/* Set up dialog from actual values: */
    
    for (i = DLGADV_68030; i <= DLGADV_68060; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
	switch (ConfigureParams.System.nCpuLevel)
	{
        case 3:
            advanceddlg[DLGADV_68030].state |= SG_SELECTED;
            break;
        case 4:
            advanceddlg[DLGADV_68040].state |= SG_SELECTED;
            break;
        case 5:
            advanceddlg[DLGADV_68060].state |= SG_SELECTED;
            break;
        default:
            break;
	}

    for (i = DLGADV_16MHZ; i <= DLGADV_40MHZ; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
	switch (ConfigureParams.System.nCpuFreq)
	{
        case 16:
            advanceddlg[DLGADV_16MHZ].state |= SG_SELECTED;
            break;
        case 20:
            advanceddlg[DLGADV_20MHZ].state |= SG_SELECTED;
            break;
        case 25:
            advanceddlg[DLGADV_25MHZ].state |= SG_SELECTED;
            break;
        case 33:
            advanceddlg[DLGADV_33MHZ].state |= SG_SELECTED;
            break;
        case 40:
            advanceddlg[DLGADV_40MHZ].state |= SG_SELECTED;
            break;
        default:
            break;
	}
    
    for (i = DLGADV_FPU68881; i <= DLGADV_FPU040; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
    switch (ConfigureParams.System.n_FPUType)
	{
        case FPU_68881:
            advanceddlg[DLGADV_FPU68881].state |= SG_SELECTED;
            break;
        case FPU_68882:
            advanceddlg[DLGADV_FPU68882].state |= SG_SELECTED;
            break;
        case FPU_CPU:
            advanceddlg[DLGADV_FPU040].state |= SG_SELECTED;
            break;
        default:
            break;
	}
    
    for (i = DLGADV_SCSI_OLD; i <= DLGADV_SCSI_NEW; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
    switch (ConfigureParams.System.nSCSI)
	{
        case NCR53C90:
            advanceddlg[DLGADV_SCSI_OLD].state |= SG_SELECTED;
            break;
        case NCR53C90A:
            advanceddlg[DLGADV_SCSI_NEW].state |= SG_SELECTED;
            break;
        default:
            break;
	}

    for (i = DLGADV_RTC_OLD; i <= DLGADV_RTC_NEW; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
    switch (ConfigureParams.System.nRTC)
	{
        case MC68HC68T1:
            advanceddlg[DLGADV_RTC_OLD].state |= SG_SELECTED;
            break;
        case MCS1850:
            advanceddlg[DLGADV_RTC_NEW].state |= SG_SELECTED;
            break;
        default:
            break;
	}
    
    if (ConfigureParams.System.bADB)
        advanceddlg[DLGADV_ADB].state |= SG_SELECTED;
    else
        advanceddlg[DLGADV_ADB].state &= ~SG_SELECTED;

 
 		
 	/* Draw and process the dialog: */

    do
	{
        but = SDLGui_DoDialog(advanceddlg, NULL);
        printf("Button: %i\n", but);
        switch (but) {
                 
            default:
                break;
        }
    }
    while (but != DLGADV_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
 
 	/* Read values from dialog: */
    
    if (advanceddlg[DLGADV_68030].state & SG_SELECTED)
        ConfigureParams.System.nCpuLevel = 3;
    else if (advanceddlg[DLGADV_68040].state & SG_SELECTED)
        ConfigureParams.System.nCpuLevel = 4;
    else
        ConfigureParams.System.nCpuLevel = 5;

    if (advanceddlg[DLGADV_16MHZ].state & SG_SELECTED)
        ConfigureParams.System.nCpuFreq = 16;
    else if (advanceddlg[DLGADV_20MHZ].state & SG_SELECTED)
        ConfigureParams.System.nCpuFreq = 20;
    else if (advanceddlg[DLGADV_25MHZ].state & SG_SELECTED)
        ConfigureParams.System.nCpuFreq = 25;
    else if (advanceddlg[DLGADV_33MHZ].state & SG_SELECTED)
        ConfigureParams.System.nCpuFreq = 33;
    else
        ConfigureParams.System.nCpuFreq = 40;
    
    if (advanceddlg[DLGADV_FPU68881].state & SG_SELECTED)
        ConfigureParams.System.n_FPUType = FPU_68881;
    else if (advanceddlg[DLGADV_FPU68882].state & SG_SELECTED)
        ConfigureParams.System.n_FPUType = FPU_68882;
    else
        ConfigureParams.System.n_FPUType = FPU_CPU;
    
    if (advanceddlg[DLGADV_SCSI_OLD].state & SG_SELECTED)
        ConfigureParams.System.nSCSI = NCR53C90;
    else
        ConfigureParams.System.nSCSI = NCR53C90A;

    if (advanceddlg[DLGADV_RTC_OLD].state & SG_SELECTED)
        ConfigureParams.System.nRTC = MC68HC68T1;
    else
        ConfigureParams.System.nRTC = MCS1850;
    
    if (advanceddlg[DLGADV_ADB].state & SG_SELECTED)
        ConfigureParams.System.bADB = true;
    else
        ConfigureParams.System.bADB = false;
}

