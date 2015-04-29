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
    bool bOldMouseVisibility;
    bOldMouseVisibility = SDL_ShowCursor(SDL_QUERY);
    SDL_ShowCursor(SDL_ENABLE);
    
    /* Check if ROM file exists. If it is missing present a dialog to select a new ROM file. */
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            while (!File_Exists(ConfigureParams.Rom.szRom030FileName)) {
                DlgMissing_Rom();
                if (bQuitProgram) {
                    Main_RequestQuit();
                    if (bQuitProgram)
                        return;
                }
            }
            break;
        case NEXT_CUBE040:
        case NEXT_STATION:
            if (ConfigureParams.System.bTurbo) {
                while (!File_Exists(ConfigureParams.Rom.szRomTurboFileName)) {
                    DlgMissing_Rom();
                    if (bQuitProgram) {
                        Main_RequestQuit();
                        if (bQuitProgram)
                            return;
                    }
                }
            } else {
                while (!File_Exists(ConfigureParams.Rom.szRom040FileName)) {
                    DlgMissing_Rom();
                    if (bQuitProgram) {
                        Main_RequestQuit();
                        if (bQuitProgram)
                            return;
                    }
                }
            }
            break;
            
        default:
            break;
    }
    
    /* Check if SCSI disk images exist. Present a dialog to select missing files. */
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        while ((ConfigureParams.SCSI.target[target].nDeviceType!=DEVTYPE_NONE) &&
               ConfigureParams.SCSI.target[target].bDiskInserted
               && !File_Exists(ConfigureParams.SCSI.target[target].szImageName)) {
            DlgMissing_SCSIdisk(target);
            if (bQuitProgram) {
                Main_RequestQuit();
                if (bQuitProgram)
                    return;
            }
        }
    }
    
    /* Check if MO disk images exist. Present a dialog to select missing files. */
    int drive;
    for (drive = 0; drive < MO_MAX_DRIVES; drive++) {
        while (ConfigureParams.MO.drive[drive].bDiskInserted &&
               ConfigureParams.MO.drive[drive].bDriveConnected &&
               !File_Exists(ConfigureParams.MO.drive[drive].szImageName)) {
            DlgMissing_MOdisk(drive);
            if (bQuitProgram) {
                Main_RequestQuit();
                if (bQuitProgram)
                    return;
            }
        }
    }
    
    SDL_ShowCursor(bOldMouseVisibility);
}
