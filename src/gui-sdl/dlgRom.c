/*
  Hatari - dlgRom.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgRom_fileid[] = "Hatari dlgRom.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGROM_ROM030_DEFAULT     4
#define DLGROM_ROM030_BROWSE      5
#define DLGROM_ROM030_NAME        6

#define DLGROM_ROM040_DEFAULT     9
#define DLGROM_ROM040_BROWSE     10
#define DLGROM_ROM040_NAME       11

#define DLGROM_ROMTURBO_DEFAULT  14
#define DLGROM_ROMTURBO_BROWSE   15
#define DLGROM_ROMTURBO_NAME     16

#define DLGROM_EXIT              18



/* The ROM dialog: */
static SGOBJ romdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 52,28, NULL },
    { SGTEXT, 0, 0, 22,1, 9,1, "ROM setup" },

	{ SGBOX, 0, 0, 1,4, 50,5, NULL },
	{ SGTEXT, 0, 0, 2,5, 30,1, "ROM for 68030 based systems:" },
    { SGBUTTON, 0, 0, 32,5, 9,1, "Default" },
	{ SGBUTTON, 0, 0, 42,5, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 2,7, 46,1, NULL },
    
	{ SGBOX, 0, 0, 1,10, 50,5, NULL },
	{ SGTEXT, 0, 0, 2,11, 30,1, "ROM for 68040 based systems:" },
	{ SGBUTTON, 0, 0, 32,11, 9,1, "Default" },
	{ SGBUTTON, 0, 0, 42,11, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 2,13, 46,1, NULL },
    
    { SGBOX, 0, 0, 1,16, 50,5, NULL },
	{ SGTEXT, 0, 0, 2,17, 30,1, "ROM for Turbo systems:" },
	{ SGBUTTON, 0, 0, 32,17, 9,1, "Default" },
	{ SGBUTTON, 0, 0, 42,17, 8,1, "Browse" },
	{ SGTEXT, 0, 0, 2,19, 46,1, NULL },
    
	{ SGTEXT, 0, 0, 2,23, 25,1, "A reset is needed after changing these options." },
	{ SGBUTTON, SG_DEFAULT, 0, 15,25, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the ROM dialog.
 */
void DlgRom_Main(void)
{
	char szDlgRom030Name[47];
	char szDlgRom040Name[47];
    char szDlgRomTurboName[47];
	int but;

	SDLGui_CenterDlg(romdlg);

	File_ShrinkName(szDlgRom030Name, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgRom030Name)-1);
	romdlg[DLGROM_ROM030_NAME].txt = szDlgRom030Name;

	File_ShrinkName(szDlgRom040Name, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgRom040Name)-1);
	romdlg[DLGROM_ROM040_NAME].txt = szDlgRom040Name;
    
    File_ShrinkName(szDlgRomTurboName, ConfigureParams.Rom.szRomTurboFileName, sizeof(szDlgRomTurboName)-1);
	romdlg[DLGROM_ROMTURBO_NAME].txt = szDlgRomTurboName;

	do
	{
		but = SDLGui_DoDialog(romdlg, NULL);
		switch (but)
		{
            case DLGROM_ROM030_DEFAULT:
                sprintf(ConfigureParams.Rom.szRom030FileName, "%s%cRev_1.0_v41.BIN",
                        Paths_GetWorkingDir(), PATHSEP);
                File_ShrinkName(szDlgRom030Name, ConfigureParams.Rom.szRom030FileName, sizeof(szDlgRom030Name)-1);
                break;
                
            case DLGROM_ROM030_BROWSE:
                /* Show and process the file selection dlg */
                SDLGui_FileConfSelect(szDlgRom030Name,
                                      ConfigureParams.Rom.szRom030FileName,
                                      sizeof(szDlgRom030Name)-1,
                                      false);
                break;
                
            case DLGROM_ROM040_DEFAULT:
                sprintf(ConfigureParams.Rom.szRom040FileName, "%s%cRev_2.5_v66.BIN",
                        Paths_GetWorkingDir(), PATHSEP);
                File_ShrinkName(szDlgRom040Name, ConfigureParams.Rom.szRom040FileName, sizeof(szDlgRom040Name)-1);
                break;
                
            case DLGROM_ROM040_BROWSE:
                /* Show and process the file selection dlg */
                SDLGui_FileConfSelect(szDlgRom040Name,
                                      ConfigureParams.Rom.szRom040FileName,
                                      sizeof(szDlgRom040Name)-1,
                                      false);
                break;
                
            case DLGROM_ROMTURBO_DEFAULT:
                sprintf(ConfigureParams.Rom.szRomTurboFileName, "%s%cRev_3.3_v74.BIN",
                        Paths_GetWorkingDir(), PATHSEP);
                File_ShrinkName(szDlgRomTurboName, ConfigureParams.Rom.szRomTurboFileName, sizeof(szDlgRomTurboName)-1);
                break;
                
            case DLGROM_ROMTURBO_BROWSE:
                /* Show and process the file selection dlg */
                SDLGui_FileConfSelect(szDlgRomTurboName,
                                      ConfigureParams.Rom.szRomTurboFileName,
                                      sizeof(szDlgRomTurboName)-1,
                                      false);
                break;

		}
	}
	while (but != DLGROM_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
}