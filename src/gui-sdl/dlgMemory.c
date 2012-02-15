/*
  Hatari - dlgMemory.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMemory_fileid[] = "Hatari dlgMemory.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "dialog.h"
#include "sdlgui.h"
#include "memorySnapShot.h"
#include "file.h"
#include "screen.h"


#define DLGMEM_8MB      4
#define DLGMEM_16MB     5
#define DLGMEM_32MB     6
#define DLGMEM_64MB     7
#define DLGMEM_128MB    8

#define DLGMEM_FILENAME 12
#define DLGMEM_SAVE     13
#define DLGMEM_RESTORE  14
#define DLGMEM_AUTOSAVE 15

#define DLGMEM_EXIT     16


static char dlgSnapShotName[36+1];


/* The memory dialog: */
static SGOBJ memorydlg[] =
{
	{ SGBOX, 0, 0, 0,0, 40,27, NULL },
    { SGTEXT, 0, 0, 14,1, 12,1, "Memory options" },

	{ SGBOX, 0, 0, 1,3, 16,9, NULL },
	{ SGTEXT, 0, 0, 2,4, 12,1, "Memory size" },
	{ SGRADIOBUT, 0, 0, 3,6, 9,1, "8 MB" },
	{ SGRADIOBUT, 0, 0, 3,7, 7,1, "16 MB" },
	{ SGRADIOBUT, 0, 0, 3,8, 7,1, "32 MB" },
	{ SGRADIOBUT, 0, 0, 3,9, 7,1, "64 MB" },
	{ SGRADIOBUT, 0, 0, 3,10, 7,1, "128 MB" },

	{ SGBOX, 0, 0, 1,13, 38,10, NULL },
	{ SGTEXT, 0, 0, 2,14, 17,1, "Load/Save memory state (untested)" },
	{ SGTEXT, 0, 0, 2,16, 20,1, "Snap-shot file name:" },
	{ SGTEXT, 0, 0, 2,17, 36,1, dlgSnapShotName },
	{ SGBUTTON, 0, 0, 8,19, 10,1, "Save" },
	{ SGBUTTON, 0, 0, 22,19, 10,1, "Restore" },
	{ SGCHECKBOX, 0, 0, 2,21, 37,1, "Load/save state at start-up/exit" },

	{ SGBUTTON, SG_DEFAULT, 0, 10,25, 20,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

/* Variable objects */
SGOBJ disable_128mb_option = { SGTEXT, 0, 0, 3,10, 7,1, " " };
//SGOBJ enable_128mb_option = { SGRADIOBUT, 0, 0, 3,10, 7,1, "128 MB" };

/**
 * Show and process the memory dialog.
 * @return  true if a memory snapshot has been loaded, false otherwise
 */
bool Dialog_MemDlg(void)
{
	int i;
	int but;

	SDLGui_CenterDlg(memorydlg);

	for (i = DLGMEM_8MB; i <= DLGMEM_128MB; i++)
	{
		memorydlg[i].state &= ~SG_SELECTED;
	}

	switch (ConfigureParams.Memory.nMemorySize)
	{
	 case 8:
		memorydlg[DLGMEM_8MB].state |= SG_SELECTED;
		break;
	 case 16:
		memorydlg[DLGMEM_16MB].state |= SG_SELECTED;
		break;
	 case 32:
		memorydlg[DLGMEM_32MB].state |= SG_SELECTED;
		break;
	 case 64:
		memorydlg[DLGMEM_64MB].state |= SG_SELECTED;
		break;
	 case 128:
            if (ConfigureParams.System.nMachineType == NEXT_STATION) {
                memorydlg[DLGMEM_128MB].state |= SG_SELECTED;
            } else {
                ConfigureParams.Memory.nMemorySize = 64;
                memorydlg[DLGMEM_64MB].state |= SG_SELECTED;
            }
		
		break;
	 default:
		memorydlg[DLGMEM_64MB].state |= SG_SELECTED;
		break;
	}
    /* Remove 128MB option if system is not NeXTstation */
    if (ConfigureParams.System.nMachineType != NEXT_STATION) {
        memorydlg[DLGMEM_128MB] = disable_128mb_option;
    }

	File_ShrinkName(dlgSnapShotName, ConfigureParams.Memory.szMemoryCaptureFileName, memorydlg[DLGMEM_FILENAME].w);


	if (ConfigureParams.Memory.bAutoSave)
		memorydlg[DLGMEM_AUTOSAVE].state |= SG_SELECTED;
	else
		memorydlg[DLGMEM_AUTOSAVE].state &= ~SG_SELECTED;

	do
	{
		but = SDLGui_DoDialog(memorydlg, NULL);

		switch (but)
		{
		 case DLGMEM_SAVE:              /* Save memory snap-shot */
			if (SDLGui_FileConfSelect(dlgSnapShotName,
			                          ConfigureParams.Memory.szMemoryCaptureFileName,
			                          memorydlg[DLGMEM_FILENAME].w, true))
			{
				MemorySnapShot_Capture(ConfigureParams.Memory.szMemoryCaptureFileName, true);
			}
			break;
		 case DLGMEM_RESTORE:           /* Load memory snap-shot */
			if (SDLGui_FileConfSelect(dlgSnapShotName,
			                          ConfigureParams.Memory.szMemoryCaptureFileName,
			                          memorydlg[DLGMEM_FILENAME].w, false))
			{
				MemorySnapShot_Restore(ConfigureParams.Memory.szMemoryCaptureFileName, true);
				return true;
			}
			break;
		}
	}
	while (but != DLGMEM_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram );

	/* Read new values from dialog: */

	if (memorydlg[DLGMEM_8MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 8;
	else if (memorydlg[DLGMEM_16MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 16;
	else if (memorydlg[DLGMEM_32MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 32;
	else if (memorydlg[DLGMEM_64MB].state & SG_SELECTED)
		ConfigureParams.Memory.nMemorySize = 64;
	else
		ConfigureParams.Memory.nMemorySize = 128;

	ConfigureParams.Memory.bAutoSave = (memorydlg[DLGMEM_AUTOSAVE].state & SG_SELECTED);

	return false;
}
