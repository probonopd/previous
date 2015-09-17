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
#include "memorySnapShot.h"
#include "reset.h"
#include "screen.h"
#include "screenSnapShot.h"
#include "configuration.h"
#include "shortcut.h"
#include "debugui.h"
#include "sdlgui.h"
#include "video.h"
#include "snd.h"
#include "avi_record.h"
#include "clocks_timings.h"

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
 * Shortcut to toggle YM/WAV sound recording
 */
static void ShortCut_RecordSound(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Shortcut to toggle screen animation recording
 */
static void ShortCut_RecordAnimation(void)
{
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
 * Shortcut to fast forward
 */
static void ShortCut_FastForward(void)
{
		/* Set maximum speed */
		ConfigureParams.System.bFastForward = true;
}


/*-----------------------------------------------------------------------*/
/**
 * Shortcut to 'Boss' key, ie minmize Window and switch to another application
 */
static void ShortCut_BossKey(void)
{
	/* If we are in full-screen, then return to a window */
	Screen_ReturnFromFullScreen();

	if (bGrabMouse)
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
		bGrabMouse = false;
	}
	Main_PauseEmulation(true);

	/* Minimize Window and give up processing to next one! */
    fprintf(stderr,"FIXME: minimize window!\n");
}


/*-----------------------------------------------------------------------*/
/**
 * Shorcut to debug interface
 */
static void ShortCut_Debug(void)
{
	int running;

	/* Call the debugger */
	running = Main_PauseEmulation(true);
	DebugUI();
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
 * Shorcut to load a disk image
 */
static void ShortCut_Dimension(void)
{
#if ENABLE_DIMENSION
    enable_dimension_screen = !enable_dimension_screen;        /* Toggle flag */
    
    if (enable_dimension_screen)
    {
        Main_SetTitle("NeXTdimension - Press ctrl-alt-n to return.");
    }
    else
    {
        Main_SetTitle(NULL);
    }
#endif
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
#if 0
	 case SHORTCUT_WARMRESET:
		Main_UnPauseEmulation();
		Reset_Warm();                  /* Emulator 'warm' reset */
		break;
	 case SHORTCUT_SCREENSHOT:
		ScreenSnapShot_SaveScreen();   /* Grab screenshot */
		break;
	 case SHORTCUT_BOSSKEY:
		ShortCut_BossKey();            /* Boss key */
		break;
	 case SHORTCUT_CURSOREMU:          /* Toggle joystick emu on/off */
		Joy_ToggleCursorEmulation();
		break;
	 case SHORTCUT_FASTFORWARD:
		ShortCut_FastForward();       /* Toggle Min/Max speed */
		break;
	 case SHORTCUT_RECANIM:
		ShortCut_RecordAnimation();    /* Record animation */
		break;
	 case SHORTCUT_RECSOUND:
		ShortCut_RecordSound();        /* Toggle sound recording */
		break;
#endif
	 case SHORTCUT_SOUND:
		ShortCut_SoundOnOff();         /* Enable/disable sound */
		break;
#if 0
	 case SHORTCUT_DEBUG:
		ShortCut_Debug();              /* Invoke the Debug UI */
		break;
#endif
	 case SHORTCUT_PAUSE:
		ShortCut_Pause();              /* Invoke Pause */
		break;
	 case SHORTCUT_QUIT:
		Main_RequestQuit();
		break;
#if 0
	 case SHORTCUT_LOADMEM:
		MemorySnapShot_Restore(ConfigureParams.Memory.szMemoryCaptureFileName, true);
		break;
	 case SHORTCUT_SAVEMEM:
		MemorySnapShot_Capture(ConfigureParams.Memory.szMemoryCaptureFileName, true);
		break;
#endif
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
		{ SHORTCUT_SCREENSHOT, "screenshot" },
		{ SHORTCUT_BOSSKEY, "bosskey" },
		{ SHORTCUT_RECANIM, "recanim" },
		{ SHORTCUT_RECSOUND, "recsound" },
		{ SHORTCUT_SAVEMEM, "savemem" },
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
