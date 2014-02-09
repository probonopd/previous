/*
  Hatari - dlgMissingFile.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMissingFile_fileid[] = "Hatari dlgMissingFile.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


/* Missing ROM dialog */
#define DLGMISSINGROM_MACHINE   3

#define DLGMISSINGROM_BROWSE    6
#define DLGMISSINGROM_DEFAULT   7
#define DLGMISSINGROM_NAME      8

#define DLGMISSINGROM_SELECT    9
#define DLGMISSINGROM_QUIT      10


static SGOBJ missingromdlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 16,1, 9,1, "ROM file not found!" },
    { SGTEXT, 0, 0, 2,4, 9,1, "Please select a compatible ROM for machine type" },
    { SGTEXT, 0, 0, 2,5, 9,1, NULL },
    
    { SGBOX, 0, 0, 1,7, 50,4, NULL },
    { SGTEXT, 0, 0, 2,8, 46,1, "Filename:" },
    { SGBUTTON, 0, 0, 13,8, 8,1, "Browse" },
    { SGBUTTON, 0, 0, 22,8, 9,1, "Default" },
    { SGTEXT, 0, 0, 2,9, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 9,13, 14,1, "Select" },
    { SGBUTTON, 0, 0, 29,13, 14,1, "Quit" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};


/* Missing SCSI dialog */
#define DLGMISSINGSCSI_ALERT     1
#define DLGMISSINGSCSI_TARGET    3

#define DLGMISSINGSCSI_BROWSE    5
#define DLGMISSINGSCSI_CDROM     6
#define DLGMISSINGSCSI_NAME      7

#define DLGMISSINGSCSI_SELECT    8
#define DLGMISSINGSCSI_EJECT     9
#define DLGMISSINGSCSI_QUIT      10


static SGOBJ missingscsidlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 9,1, 9,1, NULL },
    { SGTEXT, 0, 0, 2,4, 9,1, "Please eject or select a valid disk image for" },
    { SGTEXT, 0, 0, 2,5, 9,1, NULL },
    
    { SGBOX, 0, 0, 1,7, 50,4, NULL },
    { SGBUTTON, 0, 0, 2,8, 10,1, "Browse" },
    { SGCHECKBOX, 0, 0, 15,8, 8,1, "CD-ROM" },
    { SGTEXT, 0, 0, 2,9, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 4,13, 10,1, "Select" },
    { SGBUTTON, 0, 0, 18,13, 9,1, "Eject" },
    { SGBUTTON, 0, 0, 38,13, 10,1, "Quit" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};


/* Missing MO dialog */
#define DLGMISSINGMO_ALERT      1
#define DLGMISSINGMO_DRIVE      3

#define DLGMISSINGMO_BROWSE     5
#define DLGMISSINGMO_PROTECT    6
#define DLGMISSINGMO_NAME       7

#define DLGMISSINGMO_SELECT     8
#define DLGMISSINGMO_EJECT      9
#define DLGMISSINGMO_DISCONNECT 10
#define DLGMISSINGMO_QUIT       11


static SGOBJ missingmodlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 9,1, 9,1, NULL },
    { SGTEXT, 0, 0, 2,4, 9,1, "Please eject or select a valid disk image for" },
    { SGTEXT, 0, 0, 2,5, 9,1, NULL },
    
    { SGBOX, 0, 0, 1,7, 50,4, NULL },
    { SGBUTTON, 0, 0, 2,8, 10,1, "Browse" },
    { SGCHECKBOX, 0, 0, 15,8, 17,1, "Write protected" },
    { SGTEXT, 0, 0, 2,9, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 4,13, 10,1, "Select" },
    { SGBUTTON, 0, 0, 16,13, 9,1, "Eject" },
    { SGBUTTON, 0, 0, 27,13, 9,1, "Discon." },
    { SGBUTTON, 0, 0, 38,13, 10,1, "Quit" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};




/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing ROM dialog.
 */
void DlgMissing_Rom(void) {
    int but;
    char szDlgMissingRom[47];
    
    SDLGui_CenterDlg(missingromdlg);
    
    switch (ConfigureParams.System.nMachineType) {
        case NEXT_CUBE030:
            missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXT Computer (68030):";
            break;
        case NEXT_CUBE040:
            if (ConfigureParams.System.bTurbo)
                missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTcube Turbo:";
            else
                missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTcube:";
            break;
        case NEXT_STATION:
            if (ConfigureParams.System.bColor) {
                if (ConfigureParams.System.bTurbo)
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Turbo Color:";
                else
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Color:";
            } else {
                if (ConfigureParams.System.bTurbo)
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation Turbo:";
                else
                    missingromdlg[DLGMISSINGROM_MACHINE].txt = "NeXTstation:";
            }
            break;
        default:
            break;
    }
    
    szDlgMissingRom[0] = '\0';
    missingromdlg[DLGMISSINGROM_NAME].txt = szDlgMissingRom;
    
    do
	{
		but = SDLGui_DoDialog(missingromdlg, NULL);
		switch (but)
		{
            case DLGMISSINGROM_DEFAULT:
                switch (ConfigureParams.System.nMachineType) {
                    case NEXT_CUBE030:
                        sprintf(ConfigureParams.Rom.szRom030FileName, "%s%cRev_1.0_v41.BIN",
                                Paths_GetWorkingDir(), PATHSEP);
                        File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgMissingRom)-1);
                        break;
                        
                    case NEXT_CUBE040:
                    case NEXT_STATION:
                        if (ConfigureParams.System.bTurbo) {
                            sprintf(ConfigureParams.Rom.szRomTurboFileName, "%s%cRev_3.3_v74.BIN",
                                    Paths_GetWorkingDir(), PATHSEP);
                            File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRomTurboFileName, sizeof(szDlgMissingRom)-1);
                        } else {
                            sprintf(ConfigureParams.Rom.szRom040FileName, "%s%cRev_2.5_v66.BIN",
                                    Paths_GetWorkingDir(), PATHSEP);
                            File_ShrinkName(szDlgMissingRom, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgMissingRom)-1);
                        }
                        break;
                        
                    default:
                        break;
                }
                break;
                
            case DLGMISSINGROM_BROWSE:
                /* Show and process the file selection dlg */
                switch (ConfigureParams.System.nMachineType) {
                    case NEXT_CUBE030:
                        SDLGui_FileConfSelect(szDlgMissingRom,
                                              ConfigureParams.Rom.szRom030FileName,
                                              sizeof(szDlgMissingRom)-1,
                                              false);
                        break;
                        
                    case NEXT_CUBE040:
                    case NEXT_STATION:
                        if (ConfigureParams.System.bTurbo) {
                            SDLGui_FileConfSelect(szDlgMissingRom,
                                                  ConfigureParams.Rom.szRomTurboFileName,
                                                  sizeof(szDlgMissingRom)-1,
                                                  false);
                        } else {
                            SDLGui_FileConfSelect(szDlgMissingRom,
                                                  ConfigureParams.Rom.szRom040FileName,
                                                  sizeof(szDlgMissingRom)-1,
                                                  false);
                        }
                        break;
                        
                    default:
                        break;
                }
                break;
            case DLGMISSINGROM_QUIT:
                bQuitProgram = true;
                break;
		}
	}
	while (but != DLGMISSINGROM_SELECT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
}


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing SCSI disk dialog.
 */
void DlgMissing_SCSIdisk(int target)
{
    int but;

    char dlgname_missingscsi[47];
    char missingscsi_alert[64];
    char missingscsi_target[64];
    
	SDLGui_CenterDlg(missingscsidlg);
    
	/* Set up dialog to actual values: */
    sprintf(missingscsi_alert, "SCSI disk %i: disk image not found!", target);
    missingscsidlg[DLGMISSINGSCSI_ALERT].txt = missingscsi_alert;
    
    sprintf(missingscsi_target, "SCSI disk %i:", target);
    missingscsidlg[DLGMISSINGSCSI_TARGET].txt = missingscsi_target;
    
    File_ShrinkName(dlgname_missingscsi, ConfigureParams.SCSI.target[target].szImageName, missingscsidlg[DLGMISSINGSCSI_NAME].w);
    if (ConfigureParams.SCSI.target[target].bCDROM)
        missingscsidlg[DLGMISSINGSCSI_CDROM].state |= SG_SELECTED;
    else
        missingscsidlg[DLGMISSINGSCSI_CDROM].state &= ~SG_SELECTED;
    
	missingscsidlg[DLGMISSINGSCSI_NAME].txt = dlgname_missingscsi;
        
    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(missingscsidlg, NULL);
		switch (but)
		{
            case DLGMISSINGSCSI_EJECT:
                ConfigureParams.SCSI.target[target].bAttached = false;
                ConfigureParams.SCSI.target[target].szImageName[0] = '\0';
                dlgname_missingscsi[0] = '\0';
                break;
            case DLGMISSINGSCSI_BROWSE:
                if (SDLGui_FileConfSelect(dlgname_missingscsi,
                                          ConfigureParams.SCSI.target[target].szImageName,
                                          missingscsidlg[DLGMISSINGSCSI_NAME].w, false))
                    ConfigureParams.SCSI.target[target].bAttached = true;
                break;
            case DLGMISSINGSCSI_QUIT:
                bQuitProgram = true;
                break;
		}
	}
	while (but != DLGMISSINGSCSI_SELECT && but != DLGMISSINGSCSI_EJECT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog: */
    ConfigureParams.SCSI.target[target].bCDROM = (missingscsidlg[DLGMISSINGSCSI_CDROM].state & SG_SELECTED);
}



/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing MO disk dialog.
 */
void DlgMissing_MOdisk(int drive)
{
    int but;
    
    char dlgname_missingmo[47];
    char missingmo_alert[64];
    char missingmo_disk[64];
    
	SDLGui_CenterDlg(missingmodlg);
    
	/* Set up dialog to actual values: */
    sprintf(missingmo_alert, "MO drive %i: disk image not found!", drive);
    missingmodlg[DLGMISSINGMO_ALERT].txt = missingmo_alert;
    
    sprintf(missingmo_disk, "MO disk %i:", drive);
    missingmodlg[DLGMISSINGMO_DRIVE].txt = missingmo_disk;
    
    File_ShrinkName(dlgname_missingmo, ConfigureParams.MO.drive[drive].szImageName, missingmodlg[DLGMISSINGMO_NAME].w);
    if (ConfigureParams.MO.drive[drive].bWriteProtected)
        missingmodlg[DLGMISSINGMO_PROTECT].state |= SG_SELECTED;
    else
        missingmodlg[DLGMISSINGMO_PROTECT].state &= ~SG_SELECTED;
    
	missingmodlg[DLGMISSINGMO_NAME].txt = dlgname_missingmo;
    
    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(missingmodlg, NULL);
		switch (but)
		{
                
            case DLGMISSINGMO_BROWSE:
                if (SDLGui_FileConfSelect(dlgname_missingmo, ConfigureParams.MO.drive[drive].szImageName, missingmodlg[DLGMISSINGMO_NAME].w, false)) {
                        ConfigureParams.MO.drive[drive].bDiskInserted = true;
                    }
                break;
            case DLGMISSINGMO_DISCONNECT:
                ConfigureParams.MO.drive[drive].bDriveConnected = false;
                missingmodlg[DLGMISSINGMO_PROTECT].state &= ~SG_SELECTED;
            case DLGMISSINGMO_EJECT:
                ConfigureParams.MO.drive[drive].bDiskInserted = false;
                ConfigureParams.MO.drive[drive].szImageName[0] = '\0';
                dlgname_missingmo[0] = '\0';
                break;
            case DLGMISSINGMO_QUIT:
                bQuitProgram = true;
                break;
                
            default:
                break;
		}
	}
	while (but != DLGMISSINGMO_SELECT && but != DLGMISSINGMO_EJECT && but != DLGMISSINGMO_DISCONNECT
           && but != SDLGUI_QUIT && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog: */
    ConfigureParams.MO.drive[drive].bWriteProtected = (missingmodlg[DLGMISSINGMO_PROTECT].state & SG_SELECTED);
}
