/*
  Hatari - dlgRom.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
const char DlgMouse_fileid[] = "Previous dlgMouse.c : " __DATE__ " " __TIME__;

#include "main.h"
#include "configuration.h"
#include "dialog.h"
#include "sdlgui.h"
#include "file.h"
#include "paths.h"


#define DLGMOUSE_AUTOLOCK       15
#define DLGMOUSE_EXIT           16

char lin_normal[8];
char lin_locked[8];
char exp_normal[8];
char exp_locked[8];

/* The Boot options dialog: */
static SGOBJ mousedlg[] =
{
	{ SGBOX, 0, 0, 0,0, 45,24, NULL },
    { SGTEXT, 0, 0, 16,1, 13,1, "Mouse options" },
    
    { SGTEXT, 0, 0, 2,4, 30,1, "Mouse motion speed adjustment:" },

	{ SGBOX, 0, 0, 1,6, 43,5, NULL },
    { SGTEXT, 0, 0, 2,7, 32,1, "Linear adjustment (0.1 to 10.0):" },
	{ SGTEXT, 0, 0, 2,9, 12,1, "Normal mode:" },
    { SGEDITFIELD, 0, 0, 15,9, 5,1, lin_normal },
    { SGTEXT, 0, 0, 25,9, 12,1, "Locked mode:" },
    { SGEDITFIELD, 0, 0, 38,9, 5,1, lin_locked },
    
    { SGBOX, 0, 0, 1,12, 43,5, NULL },
    { SGTEXT, 0, 0, 2,13, 38,1, "Exponential adjustment (0.50 to 1.00):" },
    { SGTEXT, 0, 0, 2,15, 12,1, "Normal mode:" },
    { SGEDITFIELD, 0, 0, 15,15, 5,1, exp_normal },
    { SGTEXT, 0, 0, 25,15, 12,1, "Locked mode:" },
    { SGEDITFIELD, 0, 0, 38,15, 5,1, exp_locked },
    
	{ SGCHECKBOX, 0, 0, 2,18, 21,1, "Enable auto-locking" },
    
	{ SGBUTTON, SG_DEFAULT, 0, 12,21, 21,1, "Back to main menu" },
	{ -1, 0, 0, 0,0, 0,0, NULL }
};

static float read_float_string(char *s, float min, float max, int prec)
{
    int i;
    float result=0.0;
    
    for (i=0; i<8; i++) {
        if (*s>=(0+'0') && *s<=(9+'0')) {
            result *= 10.0;
            result += (float)(*s-'0');
            s++;
        } else {
            if (i==0 && *s!='.' && *s!=',') /* bad input, default to 1.0 */
                result=1.0;
            break;
        }
    }

    if (*s == '.' || *s == ',') {
        s++;
        for (i=1; i<=prec; i++) {
            if (*s>=(0+'0') && *s<=(9+'0')) {
                result += (float)(*s-'0')/pow(10.0, i);
                s++;
            } else {
                if (result==0.0) { /* bad input, default to 1.0 */
                    result=1.0;
                }
                break;
            }
        }
        if (*s>=(0+'0') && *s<=(9+'0')) {
            if ((*s-'0')>=5) {
                result += 1.0/pow(10.0, i-1);
            }
        }
    }

    if (result<min)
        result=min;
    if (result>max)
        result=max;
    
    return result;
}


/*-----------------------------------------------------------------------*/
/**
 * Show and process the Mouse options dialog.
 */
void Dialog_MouseDlg(void)
{
	int but;

	SDLGui_CenterDlg(mousedlg);

    /* Set up the dialog from actual values */
    
    mousedlg[DLGMOUSE_AUTOLOCK].state &= ~SG_SELECTED;
    if (ConfigureParams.Mouse.bEnableAutoGrab)
        mousedlg[DLGMOUSE_AUTOLOCK].state |= SG_SELECTED;
    
    sprintf(lin_normal, "%#.1f", ConfigureParams.Mouse.fLinSpeedNormal);
    sprintf(lin_locked, "%#.1f", ConfigureParams.Mouse.fLinSpeedLocked);
    
    sprintf(exp_normal, "%#.2f", ConfigureParams.Mouse.fExpSpeedNormal);
    sprintf(exp_locked, "%#.2f", ConfigureParams.Mouse.fExpSpeedLocked);
    
    /* Draw and process the dialog */
    
	do
	{
		but = SDLGui_DoDialog(mousedlg, NULL);
    }
	while (but != DLGMOUSE_EXIT && but != SDLGUI_QUIT
	       && but != SDLGUI_ERROR && !bQuitProgram);
    

    /* Read values from dialog */
    ConfigureParams.Mouse.bEnableAutoGrab = mousedlg[DLGMOUSE_AUTOLOCK].state&SG_SELECTED ? true : false;
    ConfigureParams.Mouse.fLinSpeedNormal = read_float_string(lin_normal, MOUSE_LIN_MIN, MOUSE_LIN_MAX, 1);
    ConfigureParams.Mouse.fLinSpeedLocked = read_float_string(lin_locked, MOUSE_LIN_MIN, MOUSE_LIN_MAX, 1);
    ConfigureParams.Mouse.fExpSpeedNormal = read_float_string(exp_normal, MOUSE_EXP_MIN, MOUSE_EXP_MAX, 2);
    ConfigureParams.Mouse.fExpSpeedLocked = read_float_string(exp_locked, MOUSE_EXP_MIN, MOUSE_EXP_MAX, 2);
}