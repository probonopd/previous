/*
  Hatari - shortcut.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Shortcut keys
*/
const char ShortCut_fileid[] = "Hatari shortcut.c : " __DATE__ " " __TIME__;

#include <SDL.h>

#include "main.h"
#include "dialog.h"
#include "file.h"
#include "m68000.h"
#include "dimension.h"
#include "reset.h"
#include "screen.h"
#include "configuration.h"
#include "shortcut.h"
#include "debugui.h"
#include "sdlgui.h"
#include "video.h"
#include "snd.h"
#include "statusbar.h"

static SHORTCUTKEYIDX ShortCutKey = SHORTCUT_NONE;  /* current shortcut key */


/*-----------------------------------------------------------------------*/
/**
 * Shortcut to toggle full-screen
 */
static void ShortCut_FullScreen(void)
{
	if (!bInFullScreen)
	{
		Screen_EnterFullScreen();
	}
	else
	{
		Screen_ReturnFromFullScreen();
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Shortcut to toggle mouse grabbing mode
 */
static void ShortCut_MouseGrab(void)
{

	bGrabMouse = !bGrabMouse;        /* Toggle flag */

	/* If we are in windowed mode, toggle the mouse cursor mode now: */
	if (!bInFullScreen)
	{
		if (bGrabMouse)
		{
			SDL_SetRelativeMouseMode(SDL_TRUE);
            SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
            Main_SetTitle(MOUSE_LOCK_MSG);
		}
		else
		{
			SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
            Main_SetTitle(NULL);
		}
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Shortcut to sound on/off
 */
static void ShortCut_SoundOnOff(void)
{
    ConfigureParams.Sound.bEnableSound = !ConfigureParams.Sound.bEnableSound;
    
    Sound_Reset();
}

/*-----------------------------------------------------------------------*/
/**
 * Shorcut to M68K debug interface
 */
void ShortCut_Debug_M68K(void)
{
	int running;

	running = Main_PauseEmulation(true);
    /* Call the debugger */
	DebugUI();
	if (running)
		Main_UnPauseEmulation();
}

/*-----------------------------------------------------------------------*/
/**
 * Shorcut to I860 debug interface
 */
void ShortCut_Debug_I860(void) {
    int running;
    
    if (bInFullScreen)
        Screen_ReturnFromFullScreen();

    running = Main_PauseEmulation(true);
    
    /* override paused message so that user knows to look into console
     * on how to continue in case he invoked the debugger by accident.
     */
    Statusbar_AddMessage("I860 Console Debugger", 100);
    Statusbar_Update(sdlscrn);
    
    /* disable normal GUI alerts while on console */
    int alertLevel = Log_SetAlertLevel(LOG_FATAL);
    
    /* Call the debugger */
    nd_start_debugger();
    Log_SetAlertLevel(alertLevel);

    if (running)
        Main_UnPauseEmulation();
}

/*-----------------------------------------------------------------------*/
/**
 * Shorcut to pausing
 */
static void ShortCut_Pause(void)
{
	if (!Main_UnPauseEmulation())
		Main_PauseEmulation(true);
}

/**
 * Shorcut to switch monochrome and dimension screen
 */
static void ShortCut_Dimension(void)
{
	if (ConfigureParams.System.nMachineType==NEXT_STATION || !ConfigureParams.Dimension.bEnabled) {
		return;
	}
	
    if (ConfigureParams.Screen.nMonitorType != MONITOR_TYPE_DUAL) {
        if (ConfigureParams.Screen.nMonitorType==MONITOR_TYPE_DIMENSION) {
            ConfigureParams.Screen.nMonitorType=MONITOR_TYPE_CPU;
        } else {
            ConfigureParams.Screen.nMonitorType=MONITOR_TYPE_DIMENSION;
        }
    }
	Statusbar_UpdateInfo();
}


/*-----------------------------------------------------------------------*/
/**
 * Check to see if pressed any shortcut keys, and call handling function
 */
void ShortCut_ActKey(void)
{
	if (ShortCutKey == SHORTCUT_NONE)
		return;

	switch (ShortCutKey)
	{
	 case SHORTCUT_OPTIONS:
		Dialog_DoProperty();           /* Show options dialog */
		break;
	 case SHORTCUT_FULLSCREEN:
		ShortCut_FullScreen();         /* Switch between fullscreen/windowed mode */
		break;
	 case SHORTCUT_MOUSEGRAB:
		ShortCut_MouseGrab();          /* Toggle mouse grab */
		break;
	 case SHORTCUT_COLDRESET:
		Main_UnPauseEmulation();
		Reset_Cold();                  /* Reset emulator with 'cold' (clear all) */
		break;
	 case SHORTCUT_SOUND:
		ShortCut_SoundOnOff();         /* Enable/disable sound */
		break;
	 case SHORTCUT_DEBUG_M68K:
		ShortCut_Debug_M68K();         /* Invoke the Debug UI */
		break;
	 case SHORTCUT_DEBUG_I860:
		ShortCut_Debug_I860();         /* Invoke the M68K UI */
		break;
	 case SHORTCUT_PAUSE:
		ShortCut_Pause();              /* Invoke Pause */
		break;
	 case SHORTCUT_QUIT:
		Main_RequestQuit();
		break;
	 case SHORTCUT_DIMENSION:
		ShortCut_Dimension();
		break;
	 case SHORTCUT_KEYS:
	 case SHORTCUT_NONE:
		/* ERROR: cannot happen, just make compiler happy */
	 default:
		break;
	}
	ShortCutKey = SHORTCUT_NONE;
}


/*-----------------------------------------------------------------------*/
/**
 * Invoke shortcut identified by name. This supports only keys for
 * functionality that cannot be invoked with command line options
 * or otherwise for remote GUIs etc.
 */
bool Shortcut_Invoke(const char *shortcut)
{
	struct {
		SHORTCUTKEYIDX id;
		const char *name;
	} shortcuts[] = {
		{ SHORTCUT_MOUSEGRAB, "mousegrab" },
		{ SHORTCUT_COLDRESET, "coldreset" },
		{ SHORTCUT_WARMRESET, "warmreset" },
		{ SHORTCUT_QUIT, "quit" },
		{ SHORTCUT_NONE, NULL }
	};
	int i;

	if (ShortCutKey != SHORTCUT_NONE)
	{
		fprintf(stderr, "Shortcut invocation failed, shortcut already active\n");
		return false;
	}
	for (i = 0; shortcuts[i].name; i++)
	{
		if (strcmp(shortcut, shortcuts[i].name) == 0)
		{
			ShortCutKey = shortcuts[i].id;
			ShortCut_ActKey();
			ShortCutKey = SHORTCUT_NONE;
			return true;
		}
	}
	fprintf(stderr, "WARNING: unknown shortcut '%s'\n\n", shortcut);
	fprintf(stderr, "Hatari shortcuts are:\n");
	for (i = 0; shortcuts[i].name; i++)
	{
		fprintf(stderr, "- %s\n", shortcuts[i].name);
	}
	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Check whether given key was any of the ones in given shortcut array.
 * Return corresponding array index or SHORTCUT_NONE for no match
 */
static SHORTCUTKEYIDX ShortCut_CheckKey(int symkey, int *keys)
{
	SHORTCUTKEYIDX key;
	for (key = SHORTCUT_OPTIONS; key < SHORTCUT_KEYS; key++)
	{
		if (symkey == keys[key])
			return key;
	}
	return SHORTCUT_NONE;
}

/*-----------------------------------------------------------------------*/
/**
 * Check which Shortcut key is pressed/released.
 * If press is set, store the key array index.
 * Return zero if key didn't match to a shortcut
 */
int ShortCut_CheckKeys(int modkey, int symkey, bool press)
{
	SHORTCUTKEYIDX key;

#if defined(__APPLE__)
    if ((modkey&(KMOD_RCTRL|KMOD_LCTRL)) && (modkey&(KMOD_RALT|KMOD_LALT)))
#else
	if (modkey & (KMOD_RALT|KMOD_LGUI|KMOD_RGUI|KMOD_MODE))
#endif
		key = ShortCut_CheckKey(symkey, ConfigureParams.Shortcut.withModifier);
	else
		key = ShortCut_CheckKey(symkey, ConfigureParams.Shortcut.withoutModifier);

	if (key == SHORTCUT_NONE)
		return 0;
	if (press) {
		ShortCutKey = key;
		fprintf(stderr,"Short :%x\n",ShortCutKey);
	}
	return 1;
}
