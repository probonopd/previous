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


/* Missing Disk dialog */
#define DLGMISDSK_ALERT     1

#define DLGMISDSK_DRIVE     4
#define DLGMISDSK_BROWSE    5
#define DLGMISDSK_PROTECT   6
#define DLGMISDSK_NAME      7

#define DLGMISDSK_SELECT    8
#define DLGMISDSK_REMOVE    9
#define DLGMISDSK_QUIT      10


static SGOBJ missingdiskdlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 6,1, 38,1, NULL },
    { SGTEXT, 0, 0, 2,4, 43,1, "Please remove or select a valid disk image:" },
    
    { SGBOX, 0, 0, 1,6, 50,4, NULL },
    { SGTEXT, 0, 0, 2,7, 15,1, NULL },
    { SGBUTTON, 0, 0, 39,7, 10,1, "Browse" },
    { SGTEXT, 0, 0, 15,7, 10,1, NULL },
    { SGTEXT, 0, 0, 2,8, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 4,12, 10,1, "Select" },
    { SGBUTTON, 0, 0, 16,12, 10,1, "Remove" },
    { SGBUTTON, 0, 0, 38,12, 10,1, "Quit" },
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
 * Show and process the Missing Disk dialog.
 */
void DlgMissing_Disk(char type[], int num, char *imgname, bool *inserted, bool *wp)
{
    int but;
    
    char dlgname_missingdisk[64];
    char missingdisk_alert[64];
    char missingdisk_disk[64];

    SDLGui_CenterDlg(missingdiskdlg);
    
    /* Set up dialog to actual values: */
    sprintf(missingdisk_alert, "%s drive %i: disk image not found!", type, num);
    missingdiskdlg[DLGMISDSK_ALERT].txt = missingdisk_alert;
    
    sprintf(missingdisk_disk, "%s %i:", type, num);
    missingdiskdlg[DLGMISDSK_DRIVE].txt = missingdisk_disk;
    
    File_ShrinkName(dlgname_missingdisk, imgname, missingdiskdlg[DLGMISDSK_NAME].w);
    
    missingdiskdlg[DLGMISDSK_NAME].txt = dlgname_missingdisk;
    
    
    /* Draw and process the dialog */
    do
    {
        if (*wp)
            missingdiskdlg[DLGMISDSK_PROTECT].txt = "read-only";
        else
            missingdiskdlg[DLGMISDSK_PROTECT].txt = "";

        but = SDLGui_DoDialog(missingdiskdlg, NULL);
        switch (but)
        {
                
            case DLGMISDSK_BROWSE:
                SDLGui_DiskSelect(dlgname_missingdisk, imgname, missingdiskdlg[DLGMISDSK_NAME].w, wp);
                break;
            case DLGMISDSK_REMOVE:
                *inserted = false;
                *wp = false;
                *imgname = '\0';
                break;
            case DLGMISDSK_QUIT:
                bQuitProgram = true;
                break;
                
            default:
                break;
        }
    }
    while (but != DLGMISDSK_SELECT && but != DLGMISDSK_REMOVE &&
           but != SDLGUI_QUIT && but != SDLGUI_ERROR && !bQuitProgram);
}
