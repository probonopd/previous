/*
  Hatari - dialog.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Code to handle our options dialog.
*/
const char Dialog_fileid[] = "Hatari dialog.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "change.h"
#include "dialog.h"
#include "log.h"
#include "sdlgui.h"
#include "screen.h"
#include "paths.h"
#include "file.h"


/*-----------------------------------------------------------------------*/
/**
 * Open Property sheet Options dialog.
 * 
 * We keep all our configuration details in a structure called
 * 'ConfigureParams'. When we open our dialog we make a backup
 * of this structure. When the user finally clicks on 'OK',
 * we can compare and makes the necessary changes.
 * 
 * Return true if user chooses OK, or false if cancel!
 */
bool Dialog_DoProperty(void)
{
	bool bOKDialog;  /* Did user 'OK' dialog? */
	bool bForceReset;
	bool bLoadedSnapshot;
	CNF_PARAMS current;

	Main_PauseEmulation(true);
	bForceReset = false;

	/* Copy details (this is so can restore if 'Cancel' dialog) */
	current = ConfigureParams;
	ConfigureParams.Screen.bFullScreen = bInFullScreen;
	bOKDialog = Dialog_MainDlg(&bForceReset, &bLoadedSnapshot);

	/* If a memory snapshot has been loaded, no further changes are required */
	if (bLoadedSnapshot)
	{
		Main_UnPauseEmulation();
		return true;
	}

	/* Check if reset is required and ask user if he really wants to continue then */
	if (bOKDialog && !bForceReset
	    && Change_DoNeedReset(&current, &ConfigureParams)
	    && ConfigureParams.Log.nAlertDlgLogLevel > LOG_FATAL) {
		bOKDialog = DlgAlert_Query("The emulated system must be "
		                           "reset to apply these changes. "
		                           "Apply changes now and reset "
		                           "the emulator?");
	}

	/* Copy details to configuration */
	if (bOKDialog) {
		Change_CopyChangedParamsToConfiguration(&current, &ConfigureParams, bForceReset);
	} else {
		ConfigureParams = current;
	}

	Main_UnPauseEmulation();
    
	if (bQuitProgram)
		Main_RequestQuit();
 
	return bOKDialog;
}


/* Function to check if all necessary files exist. If loading of a file fails, we bring up a dialog to let the
 * user choose another file. This is repeated for each missing file.
 */

void Dialog_CheckFiles(void) {
    int i;
    
    char *szMissingFile = strdup("");
    char szDefault[FILENAME_MAX];
    char szMachine[64];
    bool bEnable = false;
    
    /* Check if ROM file exists. If it is missing present a dialog to select a new ROM file. */
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            szMissingFile = ConfigureParams.Rom.szRom030FileName;
            sprintf(szMachine, "NeXT Computer");
            sprintf(szDefault, "%s%cRev_1.0_v41.BIN", Paths_GetWorkingDir(), PATHSEP);
            break;
        case NEXT_CUBE040:
        case NEXT_STATION:
            if (ConfigureParams.System.bTurbo) {
                szMissingFile = ConfigureParams.Rom.szRomTurboFileName;
                sprintf(szMachine, "%s Turbo %s",
                        (ConfigureParams.System.nMachineType==NEXT_CUBE040)?"NeXTcube":"NeXTstation",
                        (ConfigureParams.System.bColor)?"Color":"");
                sprintf(szDefault, "%s%cRev_3.3_v74.BIN", Paths_GetWorkingDir(), PATHSEP);
            } else {
                szMissingFile = ConfigureParams.Rom.szRom040FileName;
                sprintf(szMachine, "%s %s",
                        (ConfigureParams.System.nMachineType==NEXT_CUBE040)?"NeXTcube":"NeXTstation",
                        (ConfigureParams.System.bColor)?"Color":"");
                sprintf(szDefault, "%s%cRev_2.5_v66.BIN", Paths_GetWorkingDir(), PATHSEP);
            }
            break;
            
        default:
            break;
    }
    while (!File_Exists(szMissingFile)) {
        DlgMissing_Rom(szMachine, szMissingFile, szDefault, &bEnable);
        if (bQuitProgram) {
            Main_RequestQuit();
            if (bQuitProgram)
                return;
        }
    }
    while (ConfigureParams.Dimension.bEnabled && !File_Exists(ConfigureParams.Dimension.szRomFileName)) {
        sprintf(szDefault, "%s%cdimension_eeprom.bin", Paths_GetWorkingDir(), PATHSEP);
        DlgMissing_Rom("NeXTdimension", ConfigureParams.Dimension.szRomFileName,
                       szDefault, &ConfigureParams.Dimension.bEnabled);
        if (bQuitProgram) {
            Main_RequestQuit();
            if (bQuitProgram)
                return;
        }
    }
    
    /* Check if SCSI disk images exist. Present a dialog to select missing files. */
    for (i = 0; i < ESP_MAX_DEVS; i++) {
        while ((ConfigureParams.SCSI.target[i].nDeviceType!=DEVTYPE_NONE) &&
               ConfigureParams.SCSI.target[i].bDiskInserted &&
               !File_Exists(ConfigureParams.SCSI.target[i].szImageName)) {
            DlgMissing_Disk("SCSI disk", i,
                            ConfigureParams.SCSI.target[i].szImageName,
                            &ConfigureParams.SCSI.target[i].bDiskInserted,
                            &ConfigureParams.SCSI.target[i].bWriteProtected);
            if (ConfigureParams.SCSI.target[i].nDeviceType==DEVTYPE_HARDDISK &&
                !ConfigureParams.SCSI.target[i].bDiskInserted) {
                ConfigureParams.SCSI.target[i].nDeviceType=DEVTYPE_NONE;
            }
            if (bQuitProgram) {
                Main_RequestQuit();
                if (bQuitProgram)
                    return;
            }
        }
    }
    
    /* Check if MO disk images exist. Present a dialog to select missing files. */
    for (i = 0; i < MO_MAX_DRIVES; i++) {
        while (ConfigureParams.MO.drive[i].bDriveConnected &&
               ConfigureParams.MO.drive[i].bDiskInserted &&
               !File_Exists(ConfigureParams.MO.drive[i].szImageName)) {
            DlgMissing_Disk("MO disk", i,
                            ConfigureParams.MO.drive[i].szImageName,
                            &ConfigureParams.MO.drive[i].bDiskInserted,
                            &ConfigureParams.MO.drive[i].bWriteProtected);
            if (bQuitProgram) {
                Main_RequestQuit();
                if (bQuitProgram)
                    return;
            }
        }
    }

    /* Check if Floppy disk images exist. Present a dialog to select missing files. */
    for (i = 0; i < FLP_MAX_DRIVES; i++) {
        while (ConfigureParams.Floppy.drive[i].bDriveConnected &&
               ConfigureParams.Floppy.drive[i].bDiskInserted &&
               !File_Exists(ConfigureParams.Floppy.drive[i].szImageName)) {
            DlgMissing_Disk("Floppy", i,
                            ConfigureParams.Floppy.drive[i].szImageName,
                            &ConfigureParams.Floppy.drive[i].bDiskInserted,
                            &ConfigureParams.Floppy.drive[i].bWriteProtected);
            if (bQuitProgram) {
                Main_RequestQuit();
                if (bQuitProgram)
                    return;
            }
        }
    }
}
