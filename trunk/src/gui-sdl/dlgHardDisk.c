/*
  Hatari - dlgHardDisk.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgHardDisk_fileid[] = "Hatari dlgHardDisk.c : " __DATE__ " " __TIME__;

#include <assert.h>
#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"


#define DISKDLG_CDROM0             3
#define DISKDLG_SCSIEJECT0         4
#define DISKDLG_SCSIBROWSE0        5
#define DISKDLG_SCSINAME0          6

#define DISKDLG_CDROM1             8
#define DISKDLG_SCSIEJECT1         9
#define DISKDLG_SCSIBROWSE1        10
#define DISKDLG_SCSINAME1          11

#define DISKDLG_CDROM2             13
#define DISKDLG_SCSIEJECT2         14
#define DISKDLG_SCSIBROWSE2        15
#define DISKDLG_SCSINAME2          16

#define DISKDLG_CDROM3             18
#define DISKDLG_SCSIEJECT3         19
#define DISKDLG_SCSIBROWSE3        20
#define DISKDLG_SCSINAME3          21

#define DISKDLG_CDROM4             23
#define DISKDLG_SCSIEJECT4         24
#define DISKDLG_SCSIBROWSE4        25
#define DISKDLG_SCSINAME4          26

#define DISKDLG_CDROM5             28
#define DISKDLG_SCSIEJECT5         29
#define DISKDLG_SCSIBROWSE5        30
#define DISKDLG_SCSINAME5          31

#define DISKDLG_CDROM6             33
#define DISKDLG_SCSIEJECT6         34
#define DISKDLG_SCSIBROWSE6        35
#define DISKDLG_SCSINAME6          36

#define DISKDLG_EXIT               37


/* The disks dialog: */
static SGOBJ diskdlg[] =
{
    { SGBOX, 0, 0, 0,0, 64,29, NULL },
	{ SGTEXT, 0, 0, 27,1, 10,1, "SCSI disks" },

	{ SGTEXT, 0, 0, 2,3, 14,1, "SCSI Disk 0:" },
    { SGCHECKBOX, 0, 0, 36, 3, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,3, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,3, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,4, 58,1, NULL },
   
    { SGTEXT, 0, 0, 2, 6, 14,1, "SCSI Disk 1:" },
    { SGCHECKBOX, 0, 0, 36, 6, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,6, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,6, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 3,7, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 9, 14,1, "SCSI Disk 2:" },
    { SGCHECKBOX, 0, 0, 36, 9, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,9, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,9, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,10, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 12, 14,1, "SCSI Disk 3:" },
    { SGCHECKBOX, 0, 0, 36, 12, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,12, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,12, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,13, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 15, 14,1, "SCSI Disk 4:" },
    { SGCHECKBOX, 0, 0, 36, 15, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,15, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,15, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,16, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 18, 14,1, "SCSI Disk 5:" },
    { SGCHECKBOX, 0, 0, 36, 18, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,18, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,18, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,19, 58,1, NULL },

    { SGTEXT, 0, 0, 2, 21, 14,1, "SCSI Disk 6:" },
    { SGCHECKBOX, 0, 0, 36, 21, 8, 1, "CD-ROM" },
	{ SGBUTTON, 0, 0, 46,21, 7,1, "Eject" },
	{ SGBUTTON, 0, 0, 54,21, 8,1, "Browse" },
    { SGTEXT, 0, 0, 3,22, 58,1, NULL },


    { SGBUTTON, SG_DEFAULT, 0, 22,26, 20,1, "Back to main menu" },
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
void DlgHardDisk_Main(void)
{
    int but;
    char dlgname_scsi[ESP_MAX_DEVS][64];
    int dialog_offset = DISKDLG_SCSINAME1 - DISKDLG_SCSINAME0;

	SDLGui_CenterDlg(diskdlg);

	/* Set up dialog to actual values: */

	/* SCSI hard disk image: */
    int target;
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (ConfigureParams.SCSI.target[target].bAttached) {
            File_ShrinkName(dlgname_scsi[target], ConfigureParams.SCSI.target[target].szImageName,
                            diskdlg[DISKDLG_SCSINAME0+target*dialog_offset].w);
        } else {
            dlgname_scsi[target][0] = '\0';
        }
        diskdlg[DISKDLG_SCSINAME0+target*dialog_offset].txt = dlgname_scsi[target];
    }

    /* CD-ROM true or false? */
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        if (ConfigureParams.SCSI.target[target].bCDROM)
            diskdlg[DISKDLG_CDROM0+target*dialog_offset].state |= SG_SELECTED;
        else
            diskdlg[DISKDLG_CDROM0+target*dialog_offset].state &= ~SG_SELECTED;
    }

    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(diskdlg, NULL);
		switch (but)
		{
            case DISKDLG_SCSIEJECT0:
                ConfigureParams.SCSI.target[0].bAttached = false;
                ConfigureParams.SCSI.target[0].szImageName[0] = '\0';
                dlgname_scsi[0][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE0:
                if (SDLGui_FileConfSelect(dlgname_scsi[0], ConfigureParams.SCSI.target[0].szImageName, diskdlg[DISKDLG_SCSINAME0].w, false))
                    ConfigureParams.SCSI.target[0].bAttached = true;
                break;
                
            case DISKDLG_SCSIEJECT1:
                ConfigureParams.SCSI.target[1].bAttached = false;
                ConfigureParams.SCSI.target[1].szImageName[0] = '\0';
                dlgname_scsi[1][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE1:
                if (SDLGui_FileConfSelect(dlgname_scsi[1], ConfigureParams.SCSI.target[1].szImageName, diskdlg[DISKDLG_SCSINAME1].w, false))
                    ConfigureParams.SCSI.target[1].bAttached = true;
                break;

            case DISKDLG_SCSIEJECT2:
                ConfigureParams.SCSI.target[2].bAttached = false;
                ConfigureParams.SCSI.target[2].szImageName[0] = '\0';
                dlgname_scsi[2][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE2:
                if (SDLGui_FileConfSelect(dlgname_scsi[2], ConfigureParams.SCSI.target[2].szImageName, diskdlg[DISKDLG_SCSINAME2].w, false))
                    ConfigureParams.SCSI.target[2].bAttached = true;
                break;

            case DISKDLG_SCSIEJECT3:
                ConfigureParams.SCSI.target[3].bAttached = false;
                ConfigureParams.SCSI.target[3].szImageName[3] = '\0';
                dlgname_scsi[3][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE3:
                if (SDLGui_FileConfSelect(dlgname_scsi[3], ConfigureParams.SCSI.target[3].szImageName, diskdlg[DISKDLG_SCSINAME3].w, false))
                    ConfigureParams.SCSI.target[3].bAttached = true;
                break;

            case DISKDLG_SCSIEJECT4:
                ConfigureParams.SCSI.target[4].bAttached = false;
                ConfigureParams.SCSI.target[4].szImageName[4] = '\0';
                dlgname_scsi[4][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE4:
                if (SDLGui_FileConfSelect(dlgname_scsi[4], ConfigureParams.SCSI.target[4].szImageName, diskdlg[DISKDLG_SCSINAME4].w, false))
                    ConfigureParams.SCSI.target[4].bAttached = true;
                break;
                
            case DISKDLG_SCSIEJECT5:
                ConfigureParams.SCSI.target[5].bAttached = false;
                ConfigureParams.SCSI.target[5].szImageName[0] = '\0';
                dlgname_scsi[5][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE5:
                if (SDLGui_FileConfSelect(dlgname_scsi[5], ConfigureParams.SCSI.target[5].szImageName, diskdlg[DISKDLG_SCSINAME5].w, false))
                    ConfigureParams.SCSI.target[5].bAttached = true;
                break;
                
            case DISKDLG_SCSIEJECT6:
                ConfigureParams.SCSI.target[6].bAttached = false;
                ConfigureParams.SCSI.target[6].szImageName[0] = '\0';
                dlgname_scsi[6][0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE6:
                if (SDLGui_FileConfSelect(dlgname_scsi[6], ConfigureParams.SCSI.target[6].szImageName, diskdlg[DISKDLG_SCSINAME6].w, false))
                    ConfigureParams.SCSI.target[6].bAttached = true;
                break;
        }
	}
	while (but != DISKDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog: */
    for (target = 0; target < ESP_MAX_DEVS; target++) {
        ConfigureParams.SCSI.target[target].bCDROM = (diskdlg[DISKDLG_CDROM0+target*dialog_offset].state & SG_SELECTED);
    }
}

