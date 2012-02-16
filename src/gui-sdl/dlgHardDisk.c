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
    char dlgname_scsi0[64];
    char dlgname_scsi1[64];
    char dlgname_scsi2[64];
    char dlgname_scsi3[64];
    char dlgname_scsi4[64];
    char dlgname_scsi5[64];
    char dlgname_scsi6[64];

	SDLGui_CenterDlg(diskdlg);

	/* Set up dialog to actual values: */

	/* SCSI hard disk image: */
	if (ConfigureParams.HardDisk.bSCSIImageAttached0) {
		File_ShrinkName(dlgname_scsi0, ConfigureParams.HardDisk.szSCSIDiskImage0,
		                diskdlg[DISKDLG_SCSINAME0].w);
    } else {
        dlgname_scsi0[0] = '\0';
    }
        
    if (ConfigureParams.HardDisk.bSCSIImageAttached1) {
        File_ShrinkName(dlgname_scsi1, ConfigureParams.HardDisk.szSCSIDiskImage1,
                    diskdlg[DISKDLG_SCSINAME1].w);
    } else {
        dlgname_scsi1[0] = '\0';
    }
        
    if (ConfigureParams.HardDisk.bSCSIImageAttached2) {
        File_ShrinkName(dlgname_scsi2, ConfigureParams.HardDisk.szSCSIDiskImage2,
                        diskdlg[DISKDLG_SCSINAME2].w);
	} else {
        dlgname_scsi2[0] = '\0';
    }
    
    if (ConfigureParams.HardDisk.bSCSIImageAttached3) {
        File_ShrinkName(dlgname_scsi3, ConfigureParams.HardDisk.szSCSIDiskImage3,
                        diskdlg[DISKDLG_SCSINAME3].w);
	} else {
        dlgname_scsi3[0] = '\0';
    }
    
    if (ConfigureParams.HardDisk.bSCSIImageAttached4) {
        File_ShrinkName(dlgname_scsi4, ConfigureParams.HardDisk.szSCSIDiskImage4,
                        diskdlg[DISKDLG_SCSINAME4].w);
	} else {
        dlgname_scsi4[0] = '\0';
    }

    if (ConfigureParams.HardDisk.bSCSIImageAttached5) {
        File_ShrinkName(dlgname_scsi5, ConfigureParams.HardDisk.szSCSIDiskImage5,
                        diskdlg[DISKDLG_SCSINAME5].w);
	} else {
        dlgname_scsi5[0] = '\0';
    }

    if (ConfigureParams.HardDisk.bSCSIImageAttached6) {
        File_ShrinkName(dlgname_scsi6, ConfigureParams.HardDisk.szSCSIDiskImage6,
                        diskdlg[DISKDLG_SCSINAME6].w);
	} else {
        dlgname_scsi6[0] = '\0';
    }

	diskdlg[DISKDLG_SCSINAME0].txt = dlgname_scsi0;
    diskdlg[DISKDLG_SCSINAME1].txt = dlgname_scsi1;
    diskdlg[DISKDLG_SCSINAME2].txt = dlgname_scsi2;
	diskdlg[DISKDLG_SCSINAME3].txt = dlgname_scsi3;
    diskdlg[DISKDLG_SCSINAME4].txt = dlgname_scsi4;
    diskdlg[DISKDLG_SCSINAME5].txt = dlgname_scsi5;
    diskdlg[DISKDLG_SCSINAME6].txt = dlgname_scsi6;

    /* CD-ROM true or false? */
    if (ConfigureParams.HardDisk.bCDROM0)
        diskdlg[DISKDLG_CDROM0].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM0].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM1)
        diskdlg[DISKDLG_CDROM1].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM1].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM2)
        diskdlg[DISKDLG_CDROM2].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM2].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM3)
        diskdlg[DISKDLG_CDROM3].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM3].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM4)
        diskdlg[DISKDLG_CDROM4].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM4].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM5)
        diskdlg[DISKDLG_CDROM5].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM5].state &= ~SG_SELECTED;
    
    if (ConfigureParams.HardDisk.bCDROM6)
        diskdlg[DISKDLG_CDROM6].state |= SG_SELECTED;
    else
        diskdlg[DISKDLG_CDROM6].state &= ~SG_SELECTED;

    
	/* Draw and process the dialog */
	do
	{
		but = SDLGui_DoDialog(diskdlg, NULL);
		switch (but)
		{
            case DISKDLG_SCSIEJECT0:
                ConfigureParams.HardDisk.bSCSIImageAttached0 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage0[0] = '\0';
                dlgname_scsi0[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE0:
                if (SDLGui_FileConfSelect(dlgname_scsi0,
                                          ConfigureParams.HardDisk.szSCSIDiskImage0,
                                          diskdlg[DISKDLG_SCSINAME0].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached0 = true;
                break;
                
            case DISKDLG_SCSIEJECT1:
//                diskdlg[DISKDLG_CDROM1].state &= ~SG_SELECTED; // useful?
                ConfigureParams.HardDisk.bSCSIImageAttached1 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage1[0] = '\0';

                dlgname_scsi1[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE1:
                if (SDLGui_FileConfSelect(dlgname_scsi1, ConfigureParams.HardDisk.szSCSIDiskImage1, diskdlg[DISKDLG_SCSINAME1].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached1 = true;
                break;
                
            case DISKDLG_SCSIEJECT2:
                ConfigureParams.HardDisk.bSCSIImageAttached2 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage2[0] = '\0';
                dlgname_scsi2[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE2:
                if (SDLGui_FileConfSelect(dlgname_scsi2, ConfigureParams.HardDisk.szSCSIDiskImage2, diskdlg[DISKDLG_SCSINAME2].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached2 = true;
                break;
                
            case DISKDLG_SCSIEJECT3:
                ConfigureParams.HardDisk.bSCSIImageAttached3 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage3[0] = '\0';
                dlgname_scsi3[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE3:
                if (SDLGui_FileConfSelect(dlgname_scsi3, ConfigureParams.HardDisk.szSCSIDiskImage3, diskdlg[DISKDLG_SCSINAME3].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached3 = true;
                break;
                
            case DISKDLG_SCSIEJECT4:
                ConfigureParams.HardDisk.bSCSIImageAttached4 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage4[0] = '\0';
                dlgname_scsi4[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE4:
                if (SDLGui_FileConfSelect(dlgname_scsi4, ConfigureParams.HardDisk.szSCSIDiskImage4, diskdlg[DISKDLG_SCSINAME4].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached4 = true;
                break;

            case DISKDLG_SCSIEJECT5:
                ConfigureParams.HardDisk.bSCSIImageAttached5 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage5[0] = '\0';
                dlgname_scsi5[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE5:
                if (SDLGui_FileConfSelect(dlgname_scsi5, ConfigureParams.HardDisk.szSCSIDiskImage5, diskdlg[DISKDLG_SCSINAME5].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached5 = true;
                break;

            case DISKDLG_SCSIEJECT6:
                ConfigureParams.HardDisk.bSCSIImageAttached6 = false;
                ConfigureParams.HardDisk.szSCSIDiskImage6[0] = '\0';
                dlgname_scsi6[0] = '\0';
                break;
            case DISKDLG_SCSIBROWSE6:
                if (SDLGui_FileConfSelect(dlgname_scsi6, ConfigureParams.HardDisk.szSCSIDiskImage6, diskdlg[DISKDLG_SCSINAME6].w, false))
                    ConfigureParams.HardDisk.bSCSIImageAttached6 = true;
                break;
		}
	}
	while (but != DISKDLG_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);
    
    /* Read values from dialog: */
    ConfigureParams.HardDisk.bCDROM0 = (diskdlg[DISKDLG_CDROM0].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM1 = (diskdlg[DISKDLG_CDROM1].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM2 = (diskdlg[DISKDLG_CDROM2].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM3 = (diskdlg[DISKDLG_CDROM3].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM4 = (diskdlg[DISKDLG_CDROM4].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM5 = (diskdlg[DISKDLG_CDROM5].state & SG_SELECTED);
    ConfigureParams.HardDisk.bCDROM6 = (diskdlg[DISKDLG_CDROM6].state & SG_SELECTED);
}

