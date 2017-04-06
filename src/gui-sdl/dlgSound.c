/*
  Previous - dlgSound.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgSound_fileid[] = "Previous dlgSound.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"


#define DLGSOUND_ENABLE     3

#define DLGSOUND_EXIT       4



/* The Sound options dialog: */
static SGOBJ sounddlg[] =
{
    { SGBOX, 0, 0, 0,0, 40,13, NULL },
    { SGTEXT, 0, 0, 13,1, 16,1, "Sound options" },
    
    { SGBOX, 0, 0, 1,3, 38,5, NULL },
    { SGCHECKBOX, 0, 0, 4,5, 15,1, "Sound enabled" },
    
    { SGBUTTON, SG_DEFAULT, 0, 10,10, 21,1, "Back to main menu" },
    { -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the Boot options dialog.
 */
void DlgSound_Main(void)
{
    int but;
    
    SDLGui_CenterDlg(sounddlg);
    
    /* Set up the dialog from actual values */
    if (ConfigureParams.Sound.bEnableSound)
        sounddlg[DLGSOUND_ENABLE].state |= SG_SELECTED;
    else
        sounddlg[DLGSOUND_ENABLE].state &= ~SG_SELECTED;
    
    /* Draw and process the dialog */
    
    do
    {
        but = SDLGui_DoDialog(sounddlg, NULL);
    }
    while (but != DLGSOUND_EXIT && but != SDLGUI_QUIT
           && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog */
    ConfigureParams.Sound.bEnableSound = sounddlg[DLGSOUND_ENABLE].state & SG_SELECTED;
}