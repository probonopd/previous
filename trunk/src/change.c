/*
  Hatari - change.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This code handles run-time configuration changes. We keep all our
  configuration details in a structure called 'ConfigureParams'.  Before
  doing he changes, a backup copy is done of this structure. When
  the changes are done, these are compared to see whether emulator
  needs to be rebooted
*/
const char Change_fileid[] = "Hatari change.c : " __DATE__ " " __TIME__;

#include <ctype.h>
#include "main.h"
#include "configuration.h"
#include "change.h"
#include "dialog.h"
#include "ioMem.h"
#include "m68000.h"
#include "options.h"
#include "reset.h"
#include "screen.h"
#include "statusbar.h"
#include "video.h"
#include "hatari-glue.h"
#include "scsi.h"
#include "mo.h"

#define DEBUG 1
#if DEBUG
#define Dprintf(a) printf(a)
#else
#define Dprintf(a)
#endif

/*-----------------------------------------------------------------------*/
/**
 * Check if user needs to be warned that changes will take place after reset.
 * Return true if wants to reset.
 */
bool Change_DoNeedReset(CNF_PARAMS *current, CNF_PARAMS *changed)
{
    /* Did we change ROM file? */
    if (current->System.nMachineType == NEXT_CUBE030 && strcmp(current->Rom.szRom030FileName, changed->Rom.szRom030FileName)) {
        printf("rom030 reset\n");
        return true;
    }
    if (current->System.nMachineType == NEXT_CUBE040 || current->System.nMachineType == NEXT_STATION) {
        if (!current->System.bTurbo && strcmp(current->Rom.szRom040FileName, changed->Rom.szRom040FileName)) {
            printf("rom040 reset\n");
            return true;
        }
        if (current->System.bTurbo && strcmp(current->Rom.szRomTurboFileName, changed->Rom.szRomTurboFileName)) {
            printf("romturbo reset\n");
            return true;
        }
    }
    
    /* Did we change machine type? */
    if (current->System.nMachineType != changed->System.nMachineType) {
        printf("machine type reset\n");
        return true;
    }
    if (current->System.bColor != changed->System.bColor) {
        printf("machine type reset (color)\n");
        return true;
    }
    if (current->System.bTurbo != changed->System.bTurbo) {
        printf("machine type reset (turbo)\n");
        return true;
    }
    
    /* Did we change CPU type? */
    if (current->System.nCpuLevel != changed->System.nCpuLevel) {
        printf("cpu type reset\n");
        return true;
    }
    
    /* Did we change FPU type? */
    if (current->System.n_FPUType != changed->System.n_FPUType) {
        printf("fpu type reset\n");
        return true;
    }

    /* Did we change SCSI controller? */
    if (current->System.nSCSI != changed->System.nSCSI) {
        printf("scsi controller reset\n");
        return true;
    }
    
    /* Did we change RTC chip? */
    if (current->System.nRTC != changed->System.nRTC) {
        printf("rtc chip reset\n");
        return true;
    }
    
    /* Did we change ADB emulation? */
    if (current->System.bADB != changed->System.bADB) {
        printf("adb reset\n");
        return true;
    }
    
    /* Did we change memory size? */
    int i;
    for (i = 0; i < 4; i++) {
        if (current->Memory.nMemoryBankSize[i] != changed->Memory.nMemoryBankSize[i]) {
            printf("memory size reset\n");
            return true;
        }
    }
    
    /* Did we change boot options? */
    if (current->Boot.nBootDevice != changed->Boot.nBootDevice) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bEnableDRAMTest != changed->Boot.bEnableDRAMTest) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bEnablePot != changed->Boot.bEnablePot) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bEnableSoundTest != changed->Boot.bEnableSoundTest) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bEnableSCSITest != changed->Boot.bEnableSCSITest) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bLoopPot != changed->Boot.bLoopPot) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bVerbose != changed->Boot.bVerbose) {
        printf("boot options reset\n");
        return true;
    }
    if (current->Boot.bExtendedPot != changed->Boot.bExtendedPot) {
        printf("boot options reset\n");
        return true;
    }
    
    /* Did we change SCSI disk? */
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (!current->SCSI.target[target].bCDROM || !(current->SCSI.target[target].bCDROM == changed->SCSI.target[target].bCDROM)) {
            if ((current->SCSI.target[target].bAttached || current->SCSI.target[target].bAttached != changed->SCSI.target[target].bAttached)) {
                if (strcmp(current->SCSI.target[target].szImageName, changed->SCSI.target[target].szImageName)) {
                    printf("scsi disk reset\n");
                    return true;
                }
            }
        }
    }
    
    /* Did we change MO drive? */
    int drive;
    for (drive = 0; drive < MO_MAX_DRIVES; drive++) {
        if (current->MO.drive[drive].bDriveConnected != changed->MO.drive[drive].bDriveConnected) {
            printf("mo drive reset\n");
            return true;
        }
    }
    
    /* Else no reset is required */
    printf("No Reset needed!\n");
    return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Copy details back to configuration and perform reset.
 */
bool Change_CopyChangedParamsToConfiguration(CNF_PARAMS *current, CNF_PARAMS *changed, bool bForceReset)
{
	bool NeedReset;
	bool bReInitGemdosDrive = false;
	bool bReInitSCSIEmu = false;
    bool bReInitMOEmu = false;
	bool bReInitIDEEmu = false;
	bool bReInitIoMem = false;
	bool bScreenModeChange = false;
	bool bReInitMidi = false;
	bool bFloppyInsert[MAX_FLOPPYDRIVES];
	int i;

	Dprintf("Changes for:\n");
	/* Do we need to warn user that changes will only take effect after reset? */
	if (bForceReset)
		NeedReset = bForceReset;
	else
		NeedReset = Change_DoNeedReset(current, changed);
    
    /* Do we need to change SCSI disks? */
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (!NeedReset && (current->SCSI.target[target].bAttached != changed->SCSI.target[target].bAttached || strcmp(current->SCSI.target[target].szImageName, changed->SCSI.target[target].szImageName))) {
            bReInitSCSIEmu = true;
            break;
        }
    }
    
    /* Do we need to change MO disks? */
    int drive;
    for (drive = 0; drive < MO_MAX_DRIVES; drive++) {
        if (!NeedReset &&
            (current->MO.drive[drive].bDiskInserted != changed->MO.drive[drive].bDiskInserted ||
             current->MO.drive[drive].bWriteProtected != changed->MO.drive[drive].bWriteProtected ||
             strcmp(current->MO.drive[drive].szImageName, changed->MO.drive[drive].szImageName))) {
            bReInitMOEmu = true;
            break;
        }
    }

	/* Copy details to configuration,
	 * so it can be saved out or set on reset
	 */
	if (changed != &ConfigureParams)
	{
		ConfigureParams = *changed;
	}

	/* Copy details to global, if we reset copy them all */
	Configuration_Apply(NeedReset);
    
    /* Check if all necessary files exist */
    Dialog_CheckFiles();
    if (bQuitProgram)
    {
        SDL_Quit();
        exit(-2);
    }

	/* Re-init IO memory map? */
	if (bReInitIoMem)
	{
		Dprintf("- IO mem<\n");
		IoMem_Init();
	}
    
    /* Re-init SCSI disks? */
    if (bReInitSCSIEmu) {
        Dprintf("- SCSI disks<\n");
        SCSI_Reset();
    }
    
    /* Re-init SCSI disks? */
    if (bReInitMOEmu) {
        Dprintf("- MO drives<\n");
        MO_Reset();
    }

	/* Force things associated with screen change */
	if (bScreenModeChange)
	{
		Dprintf("- screenmode<\n");
		Screen_ModeChanged();
	}

	/* Do we need to perform reset? */
	if (NeedReset)
	{
		const char *err_msg;
		Dprintf("- reset\n");
		err_msg=Reset_Cold();
//		if (err_msg!=NULL) {
//			DlgAlert_Notice(err_msg);
//			return false;
//		}
	}

	/* Go into/return from full screen if flagged */
	if (!bInFullScreen && ConfigureParams.Screen.bFullScreen)
		Screen_EnterFullScreen();
	else if (bInFullScreen && !ConfigureParams.Screen.bFullScreen)
		Screen_ReturnFromFullScreen();

	/* update statusbar info (CPU, MHz, mem etc) */
	Statusbar_UpdateInfo();
	Dprintf("done.\n");
	return true;
}


/*-----------------------------------------------------------------------*/
/**
 * Change given Hatari options
 * Return false if parsing failed, true otherwise
 */
static bool Change_Options(int argc, const char *argv[])
{
	bool bOK;
	CNF_PARAMS current;

	Main_PauseEmulation(false);

	/* get configuration changes */
	current = ConfigureParams;
	ConfigureParams.Screen.bFullScreen = bInFullScreen;
	bOK = Opt_ParseParameters(argc, argv);

	/* Check if reset is required and ask user if he really wants to continue */
	if (bOK && Change_DoNeedReset(&current, &ConfigureParams)
	    && current.Log.nAlertDlgLogLevel > LOG_FATAL) {
		bOK = DlgAlert_Query("The emulated system must be "
				     "reset to apply these changes. "
				     "Apply changes now and reset "
				     "the emulator?");
	}
	/* Copy details to configuration */
	if (bOK) {
		if (!Change_CopyChangedParamsToConfiguration(&current, &ConfigureParams, false)) {
			ConfigureParams = current;
			DlgAlert_Notice("Return to old parameters...");
			Reset_Cold();
		}
	} else {
		ConfigureParams = current;
	}

	Main_UnPauseEmulation();
	return bOK;
}


/*-----------------------------------------------------------------------*/
/**
 * Parse given command line and change Hatari options accordingly
 * Return false if parsing failed or there were no args, true otherwise
 */
bool Change_ApplyCommandline(char *cmdline)
{
	int i, argc, inarg;
	const char **argv;
	bool ret;

	/* count args */
	inarg = argc = 0;
	for (i = 0; cmdline[i]; i++)
	{
		if (isspace(cmdline[i]))
		{
			inarg = 0;
			continue;
		}
		if (!inarg)
		{
			inarg++;
			argc++;
		}
	}
	if (!argc)
	{
		return false;
	}
	/* 2 = "hatari" + NULL */
	argv = malloc((argc+2) * sizeof(char*));
	if (!argv)
	{
		perror("command line alloc");
		return false;
	}

	/* parse them to array */
	fprintf(stderr, "Command line with '%d' arguments:\n", argc);
	inarg = argc = 0;
	argv[argc++] = "hatari";
	for (i = 0; cmdline[i]; i++)
	{
		if (isspace(cmdline[i]))
		{
			cmdline[i] = '\0';
			if (inarg)
			{
				fprintf(stderr, "- '%s'\n", argv[argc-1]);
			}
			inarg = 0;
			continue;
		}
		if (!inarg)
		{
			argv[argc++] = &(cmdline[i]);
			inarg++;
		}
	}
	if (inarg)
	{
		fprintf(stderr, "- '%s'\n", argv[argc-1]);
	}
	argv[argc] = NULL;
	
	/* do args */
	ret = Change_Options(argc, argv);
	free((void *)argv);
	return ret;
}
