/*
  Previous - dlgSystem.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

*/
const char DlgAdvanced_fileid[] = "Hatari dlgAdvanced.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"


#define DLGADV_68030      4
#define DLGADV_68040      5

#define DLGADV_FPU68881   7
#define DLGADV_FPU68882   8
#define DLGADV_FPU68040   9

#define DLGADV_DSPNONE    12
#define DLGADV_DSP56001   13
#define DLGADV_DSPMEM24   15
#define DLGADV_DSPMEM96   16

#define DLGADV_16MHZ      19
#define DLGADV_20MHZ      20
#define DLGADV_25MHZ      21
#define DLGADV_33MHZ      22
#define DLGADV_40MHZ      23

#define DLGADV_SCSI_OLD   26
#define DLGADV_SCSI_NEW   27

#define DLGADV_RTC_OLD    30
#define DLGADV_RTC_NEW    31

#define DLGADV_EXIT       32


static SGOBJ advanceddlg[] =
{
    { SGBOX, 0, 0, 0,0, 58,23, NULL },
    { SGTEXT, 0, 0, 17,1, 23,1, "Advanced system options" },
    
    { SGBOX, 0, 0, 2,3, 12,15, NULL },
    { SGTEXT, 0, 0, 3,4, 12,1, "CPU type" },
    { SGRADIOBUT, 0, 0, 4,6, 7,1, "68030" },
    { SGRADIOBUT, 0, 0, 4,8, 7,1, "68040" },
    { SGTEXT, 0, 0, 3,10, 14,1, "FPU type" },
    { SGRADIOBUT, 0, 0, 4,12, 7,1, "68881" },
    { SGRADIOBUT, 0, 0, 4,14, 7,1, "68882" },
    { SGRADIOBUT, 0, 0, 4,16, 7,1, "68040" },
    
    { SGBOX, 0, 0, 15,3, 12,15, NULL },
    { SGTEXT, 0, 0, 16,4, 12,1, "DSP type" },
    { SGRADIOBUT, 0, 0, 17,6, 6,1, "none" },
    { SGRADIOBUT, 0, 0, 17,8, 7,1, "56001" },
    { SGTEXT, 0, 0, 16,10, 8,1, "DSP memory" },
    { SGRADIOBUT, 0, 0, 17,12, 7,1, "24 kB" },
    { SGRADIOBUT, 0, 0, 17,14, 7,1, "96 kB" },

    { SGBOX, 0, 0, 28,3, 12,15, NULL },
    { SGTEXT, 0, 0, 29,4, 12,1, "CPU clock" },
    { SGRADIOBUT, 0, 0, 30,6, 8,1, "16 MHz" },
    { SGRADIOBUT, 0, 0, 30,8, 8,1, "20 MHz" },
    { SGRADIOBUT, 0, 0, 30,10, 8,1, "25 MHz" },
    { SGRADIOBUT, 0, 0, 30,12, 8,1, "33 MHz" },
    { SGRADIOBUT, 0, 0, 30,14, 8,1, "40 MHz" },
    
    { SGBOX, 0, 0, 41,3, 15,7, NULL },
    { SGTEXT, 0, 0, 42,4, 14,1, "SCSI chip" },
    { SGRADIOBUT, 0, 0, 43,6, 10,1, "NCR53C90" },
    { SGRADIOBUT, 0, 0, 43,8, 11,1, "NCR53C90A" },
    
    { SGBOX, 0, 0, 41,11, 15,7, NULL },
    { SGTEXT, 0, 0, 42,12, 14,1, "RTC chip" },
    { SGRADIOBUT, 0, 0, 43,14, 12,1, "MC68HC68T1" },
    { SGRADIOBUT, 0, 0, 43,16, 10,1, "MCCS1850" },
    
    { SGBUTTON, SG_DEFAULT, 0, 17,20, 23,1, "Back to system menu" },
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
    
    for (i = DLGADV_68030; i <= DLGADV_68040; i++)
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
    
    for (i = DLGADV_FPU68881; i <= DLGADV_FPU68040; i++)
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
            advanceddlg[DLGADV_FPU68040].state |= SG_SELECTED;
            break;
        default:
            break;
	}
    
    advanceddlg[DLGADV_DSPNONE].state &= ~SG_SELECTED;
    advanceddlg[DLGADV_DSP56001].state &= ~SG_SELECTED;
    switch (ConfigureParams.System.nDSPType) {
        case DSP_TYPE_NONE:
        case DSP_TYPE_DUMMY:
            advanceddlg[DLGADV_DSPNONE].state |= SG_SELECTED;
            break;
        case DSP_TYPE_EMU:
            advanceddlg[DLGADV_DSP56001].state |= SG_SELECTED;
            break;
        default:
            break;
    }
    
    advanceddlg[DLGADV_DSPMEM24].state &= ~SG_SELECTED;
    advanceddlg[DLGADV_DSPMEM96].state &= ~SG_SELECTED;
    if (ConfigureParams.System.bDSPMemoryExpansion) {
        advanceddlg[DLGADV_DSPMEM96].state |= SG_SELECTED;
    } else {
        advanceddlg[DLGADV_DSPMEM24].state |= SG_SELECTED;
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
        case MCCS1850:
            advanceddlg[DLGADV_RTC_NEW].state |= SG_SELECTED;
            break;
        default:
            break;
	}
    
 		
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
    
    if (advanceddlg[DLGADV_DSP56001].state & SG_SELECTED) {
        ConfigureParams.System.nDSPType = DSP_TYPE_EMU;
    } else {
        ConfigureParams.System.nDSPType = DSP_TYPE_NONE;
    }
    
    if (advanceddlg[DLGADV_DSPMEM96].state & SG_SELECTED) {
        ConfigureParams.System.bDSPMemoryExpansion = true;
    } else {
        ConfigureParams.System.bDSPMemoryExpansion = false;
    }
    
    if (advanceddlg[DLGADV_SCSI_OLD].state & SG_SELECTED)
        ConfigureParams.System.nSCSI = NCR53C90;
    else
        ConfigureParams.System.nSCSI = NCR53C90A;

    if (advanceddlg[DLGADV_RTC_OLD].state & SG_SELECTED)
        ConfigureParams.System.nRTC = MC68HC68T1;
    else
        ConfigureParams.System.nRTC = MCCS1850;
}

