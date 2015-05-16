/*
 Previous - dlgFloppy.c
 
 This file is distributed under the GNU Public License, version 2 or at
 your option any later version. Read the file gpl.txt for details.
 */
const char DlgFloppy_fileid[] = "Previous dlgFloppy.c : " __DATE__ " " __TIME__;

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "floppy.h"


#define DUAL_FLOPPY_DRIVE 0 /* Real NeXT hardware only supports one drive */

#define FLPDLG_CONNECTED0       3
#define FLPDLG_INSERT0          6
#define FLPDLG_READONLY0        7
#define FLPDLG_DISKNAME0        8
#if DUAL_FLOPPY_DRIVE
#define FLPDLG_CONNECTED1       10
#define FLPDLG_INSERT1          13
#define FLPDLG_READONLY1        14
#define FLPDLG_DISKNAME1        15

#define FLPDLG_EXIT             17
#else
#define FLPDLG_EXIT             10
#endif

/* Constant strings */
#define FLPDLG_EJECT_WARNING     "WARNING: Don't eject manually if a guest system is running. Risk of data loss. Eject now?"
#define FLPDLG_BADSIZE_ERROR     "ERROR: Can't insert floppy disk. Bad disk size. Must be 720K, 1440K or 2880K."

/* Variable strings */
char insrtejct0[16] = "Insert";
char insrtejct1[16] = "Insert";


/* The floppy drive dialog: */
static SGOBJ flpdlg[] =
{
#if DUAL_FLOPPY_DRIVE
    { SGBOX, 0, 0, 0,0, 64,30, NULL },
#else
    { SGBOX, 0, 0, 0,0, 64,20, NULL },
#endif
    { SGTEXT, 0, 0, 23,1, 10,1, "Floppy disk drives" },
    
    { SGTEXT, 0, 0, 2,4, 16,1, "Floppy Drive 0:" },
    { SGCHECKBOX, 0, 0, 20, 4, 11, 1, "Connected" },
    
    { SGBOX, 0, 0, 2,6, 60,6, NULL },
    { SGTEXT, 0, 0, 4,7, 14,1, "Floppy disk:" },
    { SGBUTTON, 0, 0, 20,7, 10,1, insrtejct0 },
    { SGTEXT, 0, 0, 32,7, 17,1, NULL },
    { SGTEXT, 0, 0, 4,9, 56,1, NULL },
#if DUAL_FLOPPY_DRIVE
    { SGTEXT, 0, 0, 2,14, 16,1, "Floppy Drive 1:" },
    { SGCHECKBOX, 0, 0, 20, 14, 11, 1, "Connected" },
    
    { SGBOX, 0, 0, 2,16, 60,6, NULL },
    { SGTEXT, 0, 0, 4,17, 14,1, "Floppy disk:" },
    { SGBUTTON, 0, 0, 20,17, 10,1, insrtejct1 },
    { SGTEXT, 0, 0, 32,17, 17,1, NULL },
    { SGTEXT, 0, 0, 4,19, 56,1, NULL },
    
    { SGTEXT, 0, 0, 2,24, 14,1, "Note: Floppy disk drives do not work with 68030 based Cubes." },
    
    { SGBUTTON, SG_DEFAULT, 0, 21,27, 21,1, "Back to main menu" },
#else
    { SGTEXT, 0, 0, 2,14, 14,1, "Note: Floppy disk drives do not work with 68030 based Cubes." },
    
    { SGBUTTON, SG_DEFAULT, 0, 21,17, 21,1, "Back to main menu" },
#endif
    
    { -1, 0, 0, 0,0, 0,0, NULL }
};


/**
 * Let user browse given directory, set directory if one selected.
 * return false if none selected, otherwise return true.
 */
/*static bool DlgDisk_BrowseDir(char *dlgname, char *confname, int maxlen)
 {
	char *str, *selname;
 
	selname = SDLGui_FileSelect(confname, NULL, false);
	if (selname)
	{
 strcpy(confname, selname);
 free(selname);
 
 str = strrchr(confname, PATHSEP);
 if (str != NULL)
 str[1] = 0;
 File_CleanFileName(confname);
 File_ShrinkName(dlgname, confname, maxlen);
 return true;
	}
	return false;
 }*/


/**
 * Show and process the hard disk dialog.
 */
void DlgFloppy_Main(void)
{
    int but;
    char dlgname_flp[FLP_MAX_DRIVES][64];
    
    SDLGui_CenterDlg(flpdlg);
    
    /* Set up dialog to actual values: */
    
    /* Floppy disk image: */
    if (ConfigureParams.Floppy.drive[0].bDriveConnected && ConfigureParams.Floppy.drive[0].bDiskInserted) {
        File_ShrinkName(dlgname_flp[0], ConfigureParams.Floppy.drive[0].szImageName,
                        flpdlg[FLPDLG_DISKNAME0].w);
        sprintf(insrtejct0, "Eject");
    } else {
        dlgname_flp[0][0] = '\0';
        sprintf(insrtejct0, "Insert");
    }
    flpdlg[FLPDLG_DISKNAME0].txt = dlgname_flp[0];
#if DUAL_FLOPPY_DRIVE
    if (ConfigureParams.Floppy.drive[1].bDriveConnected && ConfigureParams.Floppy.drive[1].bDiskInserted) {
        File_ShrinkName(dlgname_flp[1], ConfigureParams.Floppy.drive[1].szImageName,
                        flpdlg[FLPDLG_DISKNAME1].w);
        sprintf(insrtejct1, "Eject");
    } else {
        dlgname_flp[1][0] = '\0';
        sprintf(insrtejct1, "Insert");
    }
    flpdlg[FLPDLG_DISKNAME1].txt = dlgname_flp[1];
#endif
    
    /* Drive connected true or false? */
    if (ConfigureParams.Floppy.drive[0].bDriveConnected)
        flpdlg[FLPDLG_CONNECTED0].state |= SG_SELECTED;
    else
        flpdlg[FLPDLG_CONNECTED0].state &= ~SG_SELECTED;
#if DUAL_FLOPPY_DRIVE
    if (ConfigureParams.Floppy.drive[1].bDriveConnected)
        flpdlg[FLPDLG_CONNECTED1].state |= SG_SELECTED;
    else
        flpdlg[FLPDLG_CONNECTED1].state &= ~SG_SELECTED;
#endif
    
    /* Draw and process the dialog */
    do
    {
        /* Write protection true or false? */
        if (ConfigureParams.Floppy.drive[0].bWriteProtected)
            flpdlg[FLPDLG_READONLY0].txt = "read-only";
        else
            flpdlg[FLPDLG_READONLY0].txt = "";
#if DUAL_FLOPPY_DRIVE
        if (ConfigureParams.Floppy.drive[1].bWriteProtected)
            flpdlg[FLPDLG_READONLY1].txt = "read-only";
        else
            flpdlg[FLPDLG_READONLY1].txt = "";
#endif

        but = SDLGui_DoDialog(flpdlg, NULL);
        
        switch (but)
        {
            case FLPDLG_INSERT0:
                if (!ConfigureParams.Floppy.drive[0].bDiskInserted) {
                    if (SDLGui_DiskSelect(dlgname_flp[0], ConfigureParams.Floppy.drive[0].szImageName,
                                          flpdlg[FLPDLG_DISKNAME0].w, &ConfigureParams.Floppy.drive[0].bWriteProtected)) {
                        if (Floppy_Insert(0)) {
                            DlgAlert_Notice(FLPDLG_BADSIZE_ERROR);
                            ConfigureParams.Floppy.drive[0].bWriteProtected = false;
                            ConfigureParams.Floppy.drive[0].szImageName[0] = '\0';
                            dlgname_flp[0][0] = '\0';
                            break;
                        }
                        ConfigureParams.Floppy.drive[0].bDiskInserted = true;
                        sprintf(insrtejct0, "Eject");
                        if (!ConfigureParams.Floppy.drive[0].bDriveConnected) {
                            ConfigureParams.Floppy.drive[0].bDriveConnected = true;
                            flpdlg[FLPDLG_CONNECTED0].state |= SG_SELECTED;
                        }
                    }
                } else {
                    if (DlgAlert_Query(FLPDLG_EJECT_WARNING)) {
                        ConfigureParams.Floppy.drive[0].bDiskInserted = false;
                        ConfigureParams.Floppy.drive[0].bWriteProtected = false;
                        sprintf(insrtejct0, "Insert");
                        ConfigureParams.Floppy.drive[0].szImageName[0] = '\0';
                        dlgname_flp[0][0] = '\0';
                        Floppy_Eject(0);
                    }
                }
                break;
            case FLPDLG_CONNECTED0:
                if (ConfigureParams.Floppy.drive[0].bDriveConnected) {
                    ConfigureParams.Floppy.drive[0].bDriveConnected = false;
                    ConfigureParams.Floppy.drive[0].bDiskInserted = false;
                    sprintf(insrtejct0, "Insert");
                    ConfigureParams.Floppy.drive[0].bWriteProtected = false;
                    ConfigureParams.Floppy.drive[0].szImageName[0] = '\0';
                    dlgname_flp[0][0] = '\0';
                } else {
                    ConfigureParams.Floppy.drive[0].bDriveConnected = true;
                }
                break;
#if DUAL_FLOPPY_DRIVE
            case FLPDLG_INSERT1:
                if (!ConfigureParams.Floppy.drive[1].bDiskInserted) {
                    if (SDLGui_DiskSelect(dlgname_flp[1], ConfigureParams.Floppy.drive[1].szImageName,
                                          flpdlg[FLPDLG_DISKNAME1].w, &ConfigureParams.Floppy.drive[1].bWriteProtected)) {
                        if (Floppy_Insert(1)) {
                            DlgAlert_Notice(FLPDLG_BADSIZE_ERROR);
                            ConfigureParams.Floppy.drive[1].bWriteProtected = false;
                            ConfigureParams.Floppy.drive[1].szImageName[0] = '\0';
                            dlgname_flp[1][0] = '\0';
                            break;
                        }
                        ConfigureParams.Floppy.drive[1].bDiskInserted = true;
                        sprintf(insrtejct1, "Eject");
                        if (!ConfigureParams.Floppy.drive[1].bDriveConnected) {
                            ConfigureParams.Floppy.drive[1].bDriveConnected = true;
                            flpdlg[FLPDLG_CONNECTED1].state |= SG_SELECTED;
                        }
                    }
                } else {
                    if (DlgAlert_Query(FLPDLG_EJECT_WARNING)) {
                        ConfigureParams.Floppy.drive[1].bDiskInserted = false;
                        ConfigureParams.Floppy.drive[1].bWriteProtected = false;
                        sprintf(insrtejct1, "Insert");
                        ConfigureParams.Floppy.drive[1].szImageName[0] = '\0';
                        dlgname_flp[1][0] = '\0';
                        Floppy_Eject(1);
                    }
                }
                break;
            case FLPDLG_CONNECTED1:
                if (ConfigureParams.Floppy.drive[1].bDriveConnected) {
                    ConfigureParams.Floppy.drive[1].bDriveConnected = false;
                    ConfigureParams.Floppy.drive[1].bDiskInserted = false;
                    sprintf(insrtejct1, "Insert");
                    ConfigureParams.Floppy.drive[1].bWriteProtected = false;
                    ConfigureParams.Floppy.drive[1].szImageName[0] = '\0';
                    dlgname_flp[1][0] = '\0';
                } else {
                    ConfigureParams.Floppy.drive[1].bDriveConnected = true;
                }
                break;
#endif
            default:
                break;
        }
    }
    while (but != FLPDLG_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
}

