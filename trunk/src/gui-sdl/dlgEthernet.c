/*
  Previous - dlgEthernet.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgEthernet_fileid[] = "Previous dlgEthernet.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGENET_ENABLE      3
#define DLGENET_THIN        4
#define DLGENET_TWISTED     5

#define DLGENET_EXIT        6



/* The Boot options dialog: */
static SGOBJ enetdlg[] =
{
	{ SGBOX, 0, 0, 0,0, 40,17, NULL },
	{ SGTEXT, 0, 0, 12,1, 16,1, "Ethernet options" },

	{ SGBOX, 0, 0, 1,3, 38,9, NULL },
	{ SGCHECKBOX, 0, 0, 4,5, 20,1, "Ethernet connected" },
	{ SGRADIOBUT, 0, 0, 6,7, 11,1, "Thin wire" },
	{ SGRADIOBUT, 0, 0, 6,9, 14,1, "Twisted pair" },
    
	{ SGBUTTON, SG_DEFAULT, 0, 10,14, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};



/*-----------------------------------------------------------------------*/
/**
 * Show and process the Boot options dialog.
 */
void DlgEthernet_Main(void)
{
	int but;

	SDLGui_CenterDlg(enetdlg);

    /* Set up the dialog from actual values */
    
    enetdlg[DLGENET_ENABLE].state &= ~SG_SELECTED;
    enetdlg[DLGENET_THIN].state &= ~SG_SELECTED;
    enetdlg[DLGENET_TWISTED].state &= ~SG_SELECTED;

    if (ConfigureParams.Ethernet.bEthernetConnected) {
        enetdlg[DLGENET_ENABLE].state |= SG_SELECTED;
        if (ConfigureParams.Ethernet.bTwistedPair) {
            enetdlg[DLGENET_TWISTED].state |= SG_SELECTED;
        } else {
            enetdlg[DLGENET_THIN].state |= SG_SELECTED;
        }
    }
    
    /* Draw and process the dialog */
    
	do
	{
		but = SDLGui_DoDialog(enetdlg, NULL);
		
		switch (but) {
			case DLGENET_ENABLE:
				if (enetdlg[DLGENET_ENABLE].state & SG_SELECTED) {
					if (ConfigureParams.Ethernet.bTwistedPair) {
						enetdlg[DLGENET_TWISTED].state |= SG_SELECTED;
					} else {
						enetdlg[DLGENET_THIN].state |= SG_SELECTED;
					}
				} else {
					enetdlg[DLGENET_THIN].state &= ~SG_SELECTED;
					enetdlg[DLGENET_TWISTED].state &= ~SG_SELECTED;
                }
				break;
			case DLGENET_THIN:
			case DLGENET_TWISTED:
				if (!(enetdlg[DLGENET_ENABLE].state & SG_SELECTED)) {
					enetdlg[DLGENET_ENABLE].state |= SG_SELECTED;
				}
				break;
				
			default:
				break;
		}
	}
    while (but != DLGENET_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    
    
    /* Read values from dialog */
    ConfigureParams.Ethernet.bEthernetConnected = enetdlg[DLGENET_ENABLE].state & SG_SELECTED;
    ConfigureParams.Ethernet.bTwistedPair = enetdlg[DLGENET_TWISTED].state & SG_SELECTED;
}