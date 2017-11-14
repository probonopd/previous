/*
  Previous - dlgKeyboard.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgKeyboard_fileid[] = "Previous dlgKeyboard.c : " __DATE__ " " __TIME__;

#include <unistd.h>

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "screen.h"
#include "dimension.h"


#define ENABLE_LOADED_OPTION 0

#define DLGKEY_SCANCODE  4
#define DLGKEY_SYMBOLIC  5
#define DLGKEY_SWAP      8
#define DLGKEY_EXIT      20


/* The keyboard dialog: */
static SGOBJ keyboarddlg[] =
{
	{ SGBOX, 0, 0, 0,0, 47,28, NULL },
	{ SGTEXT, 0, 0, 15,1, 16,1, "Keyboard options" },
    
    { SGBOX, 0, 0, 2,3, 21,7, NULL },
	{ SGTEXT, 0, 0, 4,4, 17,1, "Keyboard mapping:" },
	{ SGRADIOBUT, 0, 0, 6,6, 10,1, "Scancode" },
	{ SGRADIOBUT, 0, 0, 6,8, 10,1, "Symbolic" },
    
    { SGBOX, 0, 0, 24,3, 21,7, NULL },
    { SGTEXT, 0, 0, 26,4, 12,1, "Key options:" },
    { SGCHECKBOX, 0, 0, 26,6, 18,1, "Swap cmd and alt" },
    
    { SGBOX, 0, 0, 2,11, 43,12, NULL },
    { SGTEXT, 0, 0, 4,12, 10,1, "Shortcuts:" },
    { SGTEXT, 0, 0, 17,12, 10,1, "ctrl-alt-X (Fn)" },
    { SGTEXT, 0, 0, 6,14, 17,1, "Show main menu     -O (F12)" },
    { SGTEXT, 0, 0, 6,15, 17,1, "Pause              -P" },
    { SGTEXT, 0, 0, 6,16, 17,1, "Cold reset         -C" },
    { SGTEXT, 0, 0, 6,17, 17,1, "Lock/unlock mouse  -M" },
    { SGTEXT, 0, 0, 6,18, 17,1, "Screen toggle      -N" },
    { SGTEXT, 0, 0, 6,19, 17,1, "Fullscreen on/off  -F" },
    { SGTEXT, 0, 0, 6,20, 17,1, "Sound on/off       -S" },
    { SGTEXT, 0, 0, 6,21, 17,1, "Quit               -Q" },
    
    { SGBUTTON, SG_DEFAULT, 0, 13,25, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};


/*-----------------------------------------------------------------------*/
/**
 * Show and process the "Keyboard" dialog.
 */
void Dialog_KeyboardDlg(void)
{
	int but;
#if ENABLE_LOADED_OPTION
	char dlgmapfile[44];
#endif
	SDLGui_CenterDlg(keyboarddlg);

	/* Set up dialog from actual values: */
    keyboarddlg[DLGKEY_SCANCODE].state &= ~SG_SELECTED;
    keyboarddlg[DLGKEY_SYMBOLIC].state &= ~SG_SELECTED;
    
    switch (ConfigureParams.Keyboard.nKeymapType) {
        case KEYMAP_SCANCODE:
            keyboarddlg[DLGKEY_SCANCODE].state |= SG_SELECTED;
            break;
        case KEYMAP_SYMBOLIC:
            keyboarddlg[DLGKEY_SYMBOLIC].state |= SG_SELECTED;
            break;
            
        default:
            break;
    }
#if ENABLE_LOADED_OPTION
	File_ShrinkName(dlgmapfile, ConfigureParams.Keyboard.szMappingFileName, keyboarddlg[DLGKEY_MAPNAME].w);
	keyboarddlg[DLGKEY_MAPNAME].txt = dlgmapfile;
#endif
    
    if (ConfigureParams.Keyboard.bSwapCmdAlt)
        keyboarddlg[DLGKEY_SWAP].state |= SG_SELECTED;
    else
        keyboarddlg[DLGKEY_SWAP].state &= ~SG_SELECTED;

	/* Show the dialog: */
	do
	{
		but = SDLGui_DoDialog(keyboarddlg, NULL);
#if ENABLE_LOADED_OPTION
		if (but == DLGKEY_MAPBROWSE)
		{
			SDLGui_FileConfSelect(dlgmapfile,
			                      ConfigureParams.Keyboard.szMappingFileName,
			                      keyboarddlg[DLGKEY_MAPNAME].w, false);
		}
#endif
	}
	while (but != DLGKEY_EXIT && but != SDLGUI_QUIT
	        && but != SDLGUI_ERROR && !bQuitProgram);

	/* Read values from dialog: */
	if (keyboarddlg[DLGKEY_SCANCODE].state & SG_SELECTED)
		ConfigureParams.Keyboard.nKeymapType = KEYMAP_SCANCODE;
	else
		ConfigureParams.Keyboard.nKeymapType = KEYMAP_SYMBOLIC;
#if ENABLE_LOADED_OPTION
	else
		ConfigureParams.Keyboard.nKeymapType = KEYMAP_LOADED;
#endif
    ConfigureParams.Keyboard.bSwapCmdAlt = (keyboarddlg[DLGKEY_SWAP].state & SG_SELECTED);
}
