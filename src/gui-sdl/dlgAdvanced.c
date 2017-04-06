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


#define DLGADV_REALTIME   4
#define DLGADV_16MHZ      5
#define DLGADV_20MHZ      6
#define DLGADV_25MHZ      7
#define DLGADV_33MHZ      8
#define DLGADV_40MHZ      9

#define DLGADV_CUSTOM     12
#define DLGADV_8MB        13
#define DLGADV_16MB       14
#define DLGADV_32MB       15
#define DLGADV_64MB       16
#define DLGADV_128MB      17

#define DLGADV_120NS      20
#define DLGADV_100NS      21
#define DLGADV_80NS       22
#define DLGADV_60NS       23

#define DLGADV_DSPNONE    26
#define DLGADV_DSP56001   27
#define DLGADV_DSPMEM24   29
#define DLGADV_DSPMEM96   30

#define DLGADV_NBIC       33

#define DLGADV_SCSI_OLD   36
#define DLGADV_SCSI_NEW   37

#define DLGADV_RTC_OLD    40
#define DLGADV_RTC_NEW    41

#define DLGADV_EXIT       42

char custom_memory[16] = "Customize";

#define DLG_VAR_Y         16
#define DLG_MEM_Y         16

static SGOBJ advanceddlg[] =
{
    { SGBOX, 0, 0, 0,0, 63,31, NULL },
    { SGTEXT, 0, 0, 20,1, 23,1, "Advanced system options" },
    
    { SGBOX, 0, 0, 2,3, 14,15, NULL },
    { SGTEXT, 0, 0, 3,4, 12,1, "CPU clock" },
	{ SGRADIOBUT, 0, 0, 4,DLG_VAR_Y, 10,1, "Variable" },
    { SGRADIOBUT, 0, 0, 4,6, 8,1, "16 MHz" },
    { SGRADIOBUT, 0, 0, 4,8, 8,1, "20 MHz" },
    { SGRADIOBUT, 0, 0, 4,10, 8,1, "25 MHz" },
    { SGRADIOBUT, 0, 0, 4,12, 8,1, "33 MHz" },
    { SGRADIOBUT, 0, 0, 4,14, 8,1, "40 MHz" },

    { SGBOX, 0, 0, 17,3, 14,15, NULL },
    { SGTEXT, 0, 0, 18,4, 12,1, "Memory size" },
	{ SGRADIOBUT, 0, 0, 19,DLG_MEM_Y, 8,1, custom_memory },
    { SGRADIOBUT, 0, 0, 19,6, 6,1, "8 MB" },
    { SGRADIOBUT, 0, 0, 19,8, 7,1, "16 MB" },
    { SGRADIOBUT, 0, 0, 19,10, 7,1, "32 MB" },
    { SGRADIOBUT, 0, 0, 19,12, 7,1, "64 MB" },
    { SGRADIOBUT, 0, 0, 19,14, 8,1, "128 MB" },

    { SGBOX, 0, 0, 32,3, 14,15, NULL },
    { SGTEXT, 0, 0, 33,4, 12,1, "Memory speed" },
    { SGRADIOBUT, 0, 0, 34,6, 8,1, "120 ns" },
    { SGRADIOBUT, 0, 0, 34,8, 8,1, "100 ns" },
    { SGRADIOBUT, 0, 0, 34,10, 7,1, "80 ns" },
    { SGRADIOBUT, 0, 0, 34,12, 7,1, "60 ns" },

    { SGBOX, 0, 0, 47,3, 14,15, NULL },
    { SGTEXT, 0, 0, 48,4, 12,1, "DSP type" },
    { SGRADIOBUT, 0, 0, 49,6, 6,1, "none" },
    { SGRADIOBUT, 0, 0, 49,8, 7,1, "56001" },
    { SGTEXT, 0, 0, 48,10, 8,1, "DSP memory" },
    { SGRADIOBUT, 0, 0, 49,12, 7,1, "24 kB" },
    { SGRADIOBUT, 0, 0, 49,14, 7,1, "96 kB" },
    
    { SGBOX, 0, 0, 2,19, 19,7, NULL },
    { SGTEXT, 0, 0, 3,20, 14,1, "NBIC" },
    { SGCHECKBOX, 0, 0, 4,22, 10,1, "present" },

    { SGBOX, 0, 0, 22,19, 19,7, NULL },
    { SGTEXT, 0, 0, 23,20, 14,1, "SCSI chip" },
    { SGRADIOBUT, 0, 0, 24,22, 10,1, "NCR53C90" },
    { SGRADIOBUT, 0, 0, 24,24, 11,1, "NCR53C90A" },
    
    { SGBOX, 0, 0, 42,19, 19,7, NULL },
    { SGTEXT, 0, 0, 43,20, 14,1, "RTC chip" },
    { SGRADIOBUT, 0, 0, 44,22, 12,1, "MC68HC68T1" },
    { SGRADIOBUT, 0, 0, 44,24, 10,1, "MCCS1850" },

    { SGBUTTON, SG_DEFAULT, 0, 20,28, 23,1, "Back to system menu" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};


/* Variable objects */
SGOBJ disable_128mb_opt = { SGTEXT, 0, 0, 19,14, 0,1, "" };
SGOBJ enable_128mb_opt = { SGRADIOBUT, 0, 0, 19,14, 8,1, "128 MB" };
SGOBJ disable_64mb_opt = { SGTEXT, 0, 0, 19,12, 0,1, "" };
SGOBJ enable_64mb_opt = { SGRADIOBUT, 0, 0, 19,12, 7,1, "64 MB" };
SGOBJ disable_40mhz_opt = { SGTEXT, 0, 0, 4,14, 0,1, "" };
SGOBJ enable_40mhz_opt = { SGRADIOBUT, 0, 0, 4,14, 8,1, "40 MHz" };

/* Default configuration constants */
int defmemsize[9][4] = {
    { 4, 4, 0, 0},  /*  8 MB for monochrome non-turbo */
    { 8, 0, 0, 0},  /*  8 MB for turbo and non-turbo color */
    {16, 0, 0, 0},  /* 16 MB for monochrome non-turbo */
    { 8, 8, 0, 0},  /* 16 MB for turbo and non-turbo color */
    {16,16, 0, 0},  /* 32 MB for monochrome non-turbo */
    { 8, 8, 8, 8},  /* 32 MB for turbo and non-turbo color */
    {16,16,16,16},  /* 64 MB for monochrome non-turbo */
    {32,32, 0, 0},  /* 64 MB for turbo */
    {32,32,32,32}   /* 128 MB for turbo */
};
int defmemsizecount=0; /* Compare defsizes up to this value */

/*-----------------------------------------------------------------------*/
/**
 * Draw the memory options.
 */
static void Dialog_AdvancedDlg_MemDraw(void) {
    int i;
    int memsum, memsize;
    
    sprintf(custom_memory, "Customize");
    
    for (i = DLGADV_CUSTOM; i <= DLGADV_128MB; i++)
    {
        advanceddlg[i].state &= ~SG_SELECTED;
    }
    
    for (i = DLGADV_120NS; i <= DLGADV_60NS; i++)
    {
        advanceddlg[i].state &= ~SG_SELECTED;
    }
    
    /* Check memory configuration and find out if it is one of our default configurations */
    memsum = Configuration_CheckMemory(ConfigureParams.Memory.nMemoryBankSize);
    
    for (i=0; i<defmemsizecount; i++) {
        if (memcmp(ConfigureParams.Memory.nMemoryBankSize, defmemsize[i],
                   sizeof(ConfigureParams.Memory.nMemoryBankSize))) {
            memsize = 0;
        } else {
            memsize = memsum;
            break;
        }
    }
    
    switch (memsize)
    {
        case 8:
            advanceddlg[DLGADV_8MB].state |= SG_SELECTED;
            break;
        case 16:
            advanceddlg[DLGADV_16MB].state |= SG_SELECTED;
            break;
        case 32:
            advanceddlg[DLGADV_32MB].state |= SG_SELECTED;
            break;
        case 64:
            advanceddlg[DLGADV_64MB].state |= SG_SELECTED;
            break;
        case 128:
            advanceddlg[DLGADV_128MB].state |= SG_SELECTED;
            break;
        default:
            advanceddlg[DLGADV_CUSTOM].state |= SG_SELECTED;
            sprintf(custom_memory, "Custom");
            break;
    }
    
    switch (ConfigureParams.Memory.nMemorySpeed) {
        case MEMORY_120NS:
            advanceddlg[DLGADV_120NS].state |= SG_SELECTED;
            break;
        case MEMORY_100NS:
            advanceddlg[DLGADV_100NS].state |= SG_SELECTED;
            break;
        case MEMORY_80NS:
            advanceddlg[DLGADV_80NS].state |= SG_SELECTED;
            break;
        case MEMORY_60NS:
            advanceddlg[DLGADV_60NS].state |= SG_SELECTED;
            break;
            
        default:
            ConfigureParams.Memory.nMemorySpeed = MEMORY_100NS;
            advanceddlg[DLGADV_100NS].state |= SG_SELECTED;
            break;
    }
}


/*-----------------------------------------------------------------------*/
/**
  * Show and process the "Advanced" dialog.
  */
void Dialog_AdvancedDlg(void) {

 	int i;
    int but;
 
 	SDLGui_CenterDlg(advanceddlg);
 
 	/* Set up dialog from actual values: */
    
    for (i = DLGADV_16MHZ; i <= DLGADV_40MHZ; i++)
	{
		advanceddlg[i].state &= ~SG_SELECTED;
	}
	
	/* Remove 40 MHz option if system is non-Turbo */
	if (ConfigureParams.System.bTurbo) {
		advanceddlg[DLGADV_40MHZ] = enable_40mhz_opt;
		advanceddlg[DLGADV_REALTIME].y = DLG_VAR_Y;
	} else {
		advanceddlg[DLGADV_40MHZ] = disable_40mhz_opt;
		advanceddlg[DLGADV_REALTIME].y = DLG_VAR_Y-2;
	}

	if (ConfigureParams.System.bRealtime) {
        advanceddlg[DLGADV_REALTIME].state |= SG_SELECTED;
	} else {
        advanceddlg[DLGADV_REALTIME].state &= ~SG_SELECTED;
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
	}
	
    /* Remove 64 and 128MB option if system is non-Turbo Slab,
     * remove 128MB option if system is not Turbo */
    if (ConfigureParams.System.bTurbo) {
        advanceddlg[DLGADV_64MB] = enable_64mb_opt;
        advanceddlg[DLGADV_128MB] = enable_128mb_opt;
		advanceddlg[DLGADV_CUSTOM].y = DLG_MEM_Y;
        defmemsizecount = 9;
    } else if (ConfigureParams.System.bColor ||
               ConfigureParams.System.nMachineType == NEXT_STATION) {
        advanceddlg[DLGADV_64MB] = disable_64mb_opt;
        advanceddlg[DLGADV_128MB] = disable_128mb_opt;
		advanceddlg[DLGADV_CUSTOM].y = DLG_MEM_Y-4;
        defmemsizecount = 6;
    } else {
        advanceddlg[DLGADV_64MB] = enable_64mb_opt;
        advanceddlg[DLGADV_128MB] = disable_128mb_opt;
		advanceddlg[DLGADV_CUSTOM].y = DLG_MEM_Y-2;
        defmemsizecount = 8;
    }
    
    /* Display memory speed options depending on system type */
    if (ConfigureParams.System.bTurbo) {
        advanceddlg[DLGADV_120NS].txt = "60 ns";
        advanceddlg[DLGADV_100NS].txt = "70 ns";
        advanceddlg[DLGADV_80NS].txt = "80 ns";
        advanceddlg[DLGADV_60NS].txt = "100 ns";
    } else {
        advanceddlg[DLGADV_120NS].txt = "120 ns";
        advanceddlg[DLGADV_100NS].txt = "100 ns";
        advanceddlg[DLGADV_80NS].txt = "80 ns";
        advanceddlg[DLGADV_60NS].txt = "60 ns";
    }
    Dialog_AdvancedDlg_MemDraw();
    
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
    
    advanceddlg[DLGADV_NBIC].state &= ~SG_SELECTED;
    if (ConfigureParams.System.bNBIC) {
        advanceddlg[DLGADV_NBIC].state |= SG_SELECTED;
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

        switch (but) {
            case DLGADV_8MB:
                if (ConfigureParams.System.bColor || ConfigureParams.System.bTurbo) {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[1],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                } else {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[0],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                }
                sprintf(custom_memory, "Customize");
                break;
            case DLGADV_16MB:
                if (ConfigureParams.System.bColor || ConfigureParams.System.bTurbo) {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[3],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                } else {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[2],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                }
                sprintf(custom_memory, "Customize");
                break;
            case DLGADV_32MB:
                if (ConfigureParams.System.bColor || ConfigureParams.System.bTurbo) {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[5],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                } else {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[4],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                }
                sprintf(custom_memory, "Customize");
                break;
            case DLGADV_64MB:
                if (ConfigureParams.System.bTurbo) {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[7],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                } else {
                    memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[6],
                           sizeof(ConfigureParams.Memory.nMemoryBankSize));
                }
                sprintf(custom_memory, "Customize");
                break;
            case DLGADV_128MB:
                memcpy(ConfigureParams.Memory.nMemoryBankSize, defmemsize[8],
                       sizeof(ConfigureParams.Memory.nMemoryBankSize));
                sprintf(custom_memory, "Customize");
                break;
            case DLGADV_CUSTOM:
                Dialog_MemAdvancedDlg(ConfigureParams.Memory.nMemoryBankSize);
                Dialog_AdvancedDlg_MemDraw();
                break;

            default:
                break;
        }
    }
    while (but != DLGADV_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
 
 	/* Read values from dialog: */
	
	if (advanceddlg[DLGADV_REALTIME].state & SG_SELECTED) {
		ConfigureParams.System.bRealtime = true;
		ConfigureParams.System.nCpuFreq = ConfigureParams.System.bTurbo ? 33 : 25;
	} else {
		ConfigureParams.System.bRealtime = false;
		
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
	}

    if (advanceddlg[DLGADV_120NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_120NS;
    else if (advanceddlg[DLGADV_100NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_100NS;
    else if (advanceddlg[DLGADV_80NS].state & SG_SELECTED)
        ConfigureParams.Memory.nMemorySpeed = MEMORY_80NS;
    else
        ConfigureParams.Memory.nMemorySpeed = MEMORY_60NS;

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
    
    if (advanceddlg[DLGADV_NBIC].state & SG_SELECTED)
        ConfigureParams.System.bNBIC = true;
    else
        ConfigureParams.System.bNBIC = false;
        
    if (advanceddlg[DLGADV_SCSI_OLD].state & SG_SELECTED)
        ConfigureParams.System.nSCSI = NCR53C90;
    else
        ConfigureParams.System.nSCSI = NCR53C90A;

    if (advanceddlg[DLGADV_RTC_OLD].state & SG_SELECTED)
        ConfigureParams.System.nRTC = MC68HC68T1;
    else
        ConfigureParams.System.nRTC = MCCS1850;
}

