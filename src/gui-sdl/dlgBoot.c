/*
  Hatari - dlgRom.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgBoot_fileid[] = "Hatari dlgBoot.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGBOOT_SCSI           4
#define DLGBOOT_ETHERNET       5
#define DLGBOOT_MO             6
#define DLGBOOT_FLOPPY         7
#define DLGBOOT_ROMMONITOR     8

#define DLGBOOT_DRAMTEST      11
#define DLGBOOT_ENABLE_POT    12
#define DLGBOOT_SOUNDTEST     13
#define DLGBOOT_SCSITEST      14
#define DLGBOOT_LOOP          15
#define DLGBOOT_VERBOSE       16
#define DLGBOOT_EXTENDED_POT  17    

#define DLGBOOT_EXIT          19



/* The Boot options dialog: */
static SGOBJ bootdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 52,22, NULL },
    { SGTEXT, 0, 0, 20,1, 9,1, "Boot options" },

	{ SGBOX, 0, 0, 1,4, 19,11, NULL },
	{ SGTEXT, 0, 0, 2,5, 30,1, "Boot device:" },
	{ SGRADIOBUT, 0, 0, 2,7, 11,1, "SCSI disk" },
    { SGRADIOBUT, 0, 0, 2,8, 10,1, "Ethernet" },
    { SGRADIOBUT, 0, 0, 2,9, 9,1, "MO disk" },
    { SGRADIOBUT, 0, 0, 2,10, 8,1, "Floppy" },
    { SGRADIOBUT, 0, 0, 2,12, 13,1, "ROM monitor" },
    
	{ SGBOX, 0, 0, 21,4, 30,11, NULL },
	{ SGTEXT, 0, 0, 22,5, 30,1, "Power-on test options:" },
	{ SGCHECKBOX, 0, 0, 22,7, 19,1, "perform DRAM test" },
	{ SGCHECKBOX, 0, 0, 22,8, 23,1, "perform power-on test" },
    { SGCHECKBOX, 0, 0, 24,9, 16,1, "sound out test" },
	{ SGCHECKBOX, 0, 0, 24,10, 12,1, "SCSI tests" },
	{ SGCHECKBOX, 0, 0, 24,11, 21,1, "loop until keypress" },
	{ SGCHECKBOX, 0, 0, 24,12, 19,1, "verbose test mode" },
	{ SGCHECKBOX, 0, 0, 22,13, 27,1, "boot extended diagnostics" },
    
    { SGTEXT, 0, 0, 2,16, 25,1, "For advanced options boot to ROM monitor prompt." },

	{ SGBUTTON, SG_DEFAULT, 0, 15,19, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the Boot options dialog.
 */
void DlgBoot_Main(void)
{
    int i;
	int but;

	SDLGui_CenterDlg(bootdlg);

    /* Set up the dialog from actual values */
    for (i = DLGBOOT_ROMMONITOR; i <= DLGBOOT_FLOPPY; i++)
	{
		bootdlg[i].state &= ~SG_SELECTED;
	}
    switch (ConfigureParams.Boot.nBootDevice) {
        case BOOT_ROM:
            bootdlg[DLGBOOT_ROMMONITOR].state |= SG_SELECTED;
            break;
        case BOOT_SCSI:
            bootdlg[DLGBOOT_SCSI].state |= SG_SELECTED;
            break;
        case BOOT_ETHERNET:
            bootdlg[DLGBOOT_ETHERNET].state |= SG_SELECTED;
            break;
        case BOOT_MO:
            bootdlg[DLGBOOT_MO].state |= SG_SELECTED;
            break;
        case BOOT_FLOPPY:
            bootdlg[DLGBOOT_FLOPPY].state |= SG_SELECTED;
            break;
            
        default:
            break;
    }
    
    for (i = DLGBOOT_DRAMTEST; i <= DLGBOOT_EXTENDED_POT; i++) {
        bootdlg[i].state &= ~SG_SELECTED;
    }
    if (ConfigureParams.Boot.bEnableDRAMTest)
        bootdlg[DLGBOOT_DRAMTEST].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bEnablePot)
        bootdlg[DLGBOOT_ENABLE_POT].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bEnableSoundTest)
        bootdlg[DLGBOOT_SOUNDTEST].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bEnableSCSITest)
        bootdlg[DLGBOOT_SCSITEST].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bLoopPot)
        bootdlg[DLGBOOT_LOOP].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bVerbose)
        bootdlg[DLGBOOT_VERBOSE].state |= SG_SELECTED;
    if (ConfigureParams.Boot.bExtendedPot)
        bootdlg[DLGBOOT_EXTENDED_POT].state |= SG_SELECTED;
    

    /* Draw and process the dialog */
    
	do
	{
		but = SDLGui_DoDialog(bootdlg, NULL);
		switch (but)
		{
            case DLGBOOT_ENABLE_POT:
                if (!(bootdlg[DLGBOOT_ENABLE_POT].state & SG_SELECTED)) {
                    bootdlg[DLGBOOT_SOUNDTEST].state &= ~SG_SELECTED;
                    bootdlg[DLGBOOT_SCSITEST].state &= ~SG_SELECTED;
                    bootdlg[DLGBOOT_LOOP].state &= ~SG_SELECTED;
                    bootdlg[DLGBOOT_VERBOSE].state &= ~SG_SELECTED;
                }
                
            case DLGBOOT_SOUNDTEST:
            case DLGBOOT_SCSITEST:
            case DLGBOOT_LOOP:
            case DLGBOOT_VERBOSE:
                if (bootdlg[but].state & SG_SELECTED) {
                    bootdlg[DLGBOOT_ENABLE_POT].state |= SG_SELECTED;
                }
                break;
            default: break;
		}
	}
	while (but != DLGBOOT_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog */
    if (bootdlg[DLGBOOT_ROMMONITOR].state & SG_SELECTED)
        ConfigureParams.Boot.nBootDevice = BOOT_ROM;
    else if (bootdlg[DLGBOOT_SCSI].state & SG_SELECTED)
        ConfigureParams.Boot.nBootDevice = BOOT_SCSI;
    else if (bootdlg[DLGBOOT_ETHERNET].state & SG_SELECTED)
        ConfigureParams.Boot.nBootDevice = BOOT_ETHERNET;
    else if (bootdlg[DLGBOOT_MO].state & SG_SELECTED)
        ConfigureParams.Boot.nBootDevice = BOOT_MO;
    else if (bootdlg[DLGBOOT_FLOPPY].state & SG_SELECTED)
        ConfigureParams.Boot.nBootDevice = BOOT_FLOPPY;

    ConfigureParams.Boot.bEnableDRAMTest = bootdlg[DLGBOOT_DRAMTEST].state & SG_SELECTED;
    ConfigureParams.Boot.bEnablePot = bootdlg[DLGBOOT_ENABLE_POT].state & SG_SELECTED;
    ConfigureParams.Boot.bEnableSoundTest = bootdlg[DLGBOOT_SOUNDTEST].state & SG_SELECTED;
    ConfigureParams.Boot.bEnableSCSITest = bootdlg[DLGBOOT_SCSITEST].state & SG_SELECTED;
    ConfigureParams.Boot.bLoopPot = bootdlg[DLGBOOT_LOOP].state & SG_SELECTED;
    ConfigureParams.Boot.bVerbose = bootdlg[DLGBOOT_VERBOSE].state & SG_SELECTED;
    ConfigureParams.Boot.bExtendedPot = bootdlg[DLGBOOT_EXTENDED_POT].state & SG_SELECTED;
}