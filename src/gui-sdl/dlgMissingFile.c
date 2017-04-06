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
#define DLGMISROM_ALERT     1

#define DLGMISROM_BROWSE    4
#define DLGMISROM_DEFAULT   5
#define DLGMISROM_NAME      6

#define DLGMISROM_SELECT    7
#define DLGMISROM_REMOVE    8
#define DLGMISROM_QUIT      9


static SGOBJ missingromdlg[] =
{
    { SGBOX, 0, 0, 0,0, 52,15, NULL },
    { SGTEXT, 0, 0, 2,1, 38,1, NULL },
    { SGTEXT, 0, 0, 2,4, 43,1, "Please select a valid ROM file:" },
    
    { SGBOX, 0, 0, 1,6, 50,4, NULL },
    { SGBUTTON, 0, 0, 2,7, 8,1, "Browse" },
    { SGBUTTON, 0, 0, 11,7, 9,1, "Default" },
    { SGTEXT, 0, 0, 2,8, 46,1, NULL },
    
    { SGBUTTON, SG_DEFAULT, 0, 4,12, 10,1, "Select" },
    { SGBUTTON, 0, 0, 16,12, 10,1, "Remove" },
    { SGBUTTON, 0, 0, 38,12, 10,1, "Quit" },
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
void DlgMissing_Rom(const char* type, char *imgname, const char *defname, bool *enabled) {
    int but;
    
    char dlgname_missingrom[64];
    char missingrom_alert[64];
    
    bool bOldMouseVisibility;
    bOldMouseVisibility = SDL_ShowCursor(SDL_QUERY);
    SDL_ShowCursor(SDL_ENABLE);
    
    SDLGui_CenterDlg(missingromdlg);
    
    /* Set up dialog to actual values: */
    sprintf(missingrom_alert, "%s: ROM file not found!", type);
    missingromdlg[DLGMISROM_ALERT].txt = missingrom_alert;
    
    File_ShrinkName(dlgname_missingrom, imgname, missingromdlg[DLGMISROM_NAME].w);
    
    missingromdlg[DLGMISROM_NAME].txt = dlgname_missingrom;
    
    if (*enabled) {
        missingromdlg[DLGMISROM_REMOVE].type = SGBUTTON;
        missingromdlg[DLGMISROM_REMOVE].txt = "Remove";
    } else {
        missingromdlg[DLGMISROM_REMOVE].type = SGTEXT;
        missingromdlg[DLGMISROM_REMOVE].txt = "";
    }
    
    
    /* Draw and process the dialog */
    do
    {
        but = SDLGui_DoDialog(missingromdlg, NULL);
        switch (but)
        {
                
            case DLGMISROM_BROWSE:
                SDLGui_FileConfSelect(dlgname_missingrom, imgname,
                                      missingromdlg[DLGMISROM_NAME].w,
                                      false);
                break;
            case DLGMISROM_DEFAULT:
                sprintf(imgname, "%s",defname);
                File_ShrinkName(dlgname_missingrom, imgname, missingromdlg[DLGMISROM_NAME].w);
                break;
            case DLGMISROM_REMOVE:
                *enabled = false;
                *imgname = '\0';
                break;
            case DLGMISROM_QUIT:
                bQuitProgram = true;
                break;
                
            default:
                break;
        }
    }
    while (but != DLGMISROM_SELECT && but != DLGMISROM_REMOVE &&
           but != SDLGUI_QUIT && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    SDL_ShowCursor(bOldMouseVisibility);
}


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Missing Disk dialog.
 */
void DlgMissing_Disk(const char* type, int num, char *imgname, bool *inserted, bool *wp)
{
    int but;
    
    char dlgname_missingdisk[64];
    char missingdisk_alert[64];
    char missingdisk_disk[64];
    
    bool bOldMouseVisibility;
    bOldMouseVisibility = SDL_ShowCursor(SDL_QUERY);
    SDL_ShowCursor(SDL_ENABLE);

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
    
    SDL_ShowCursor(bOldMouseVisibility);
}
