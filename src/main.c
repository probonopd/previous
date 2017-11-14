/*
  Hatari - main.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Main initialization and event handling routines.
*/
const char Main_fileid[] = "Hatari main.c : " __DATE__ " " __TIME__;

#include <time.h>
#include <errno.h>
#include <signal.h>

#include "main.h"
#include "configuration.h"
#include "control.h"
#include "options.h"
#include "dialog.h"
#include "ioMem.h"
#include "keymap.h"
#include "log.h"
#include "m68000.h"
#include "paths.h"
#include "reset.h"
#include "screen.h"
#include "sdlgui.h"
#include "shortcut.h"
#include "snd.h"
#include "statusbar.h"
#include "nextMemory.h"
#include "str.h"
#include "video.h"
#include "audio.h"
#include "debugui.h"
#include "file.h"
#include "dsp.h"
#include "host.h"
#include "dimension.h"

#include "hatari-glue.h"

#if HAVE_GETTIMEOFDAY
#include <sys/time.h>
#endif

int nFrameSkips;

bool bQuitProgram = false;                /* Flag to quit program cleanly */

static bool bEmulationActive = true;      /* Run emulation when started */
static bool bAccurateDelays;              /* Host system has an accurate SDL_Delay()? */
static bool bIgnoreNextMouseMotion = false;  /* Next mouse motion will be ignored (needed after SDL_WarpMouse) */

volatile int mainPauseEmulation;

typedef const char* (*report_func)(double realTime, double hostTime);

typedef struct {
    const char*       label;
    const report_func report;
} report_t;

static double lastRT;
static Uint64 lastCycles;
static double speedFactor;
static char   speedMsg[32];

void Main_Speed(double realTime, double hostTime) {
    double dRT = realTime - lastRT;
    speedFactor = nCyclesMainCounter - lastCycles;
    speedFactor /= ConfigureParams.System.nCpuFreq;
    speedFactor /= 1000 * 1000;
    speedFactor /= dRT;
    lastRT     = realTime;
    lastCycles = nCyclesMainCounter;
}

void Main_SpeedReset(void) {
    double realTime, hostTime;
    host_time(&realTime, &hostTime);
    lastRT     = realTime;
    lastCycles = nCyclesMainCounter;
}

const char* Main_SpeedMsg() {
    speedMsg[0] = 0;
    if(speedFactor > 0) {
        if(ConfigureParams.System.bRealtime) {
            sprintf(speedMsg, "%dMHz/", (int)(ConfigureParams.System.nCpuFreq * speedFactor + 0.5));
        } else {
            if ((speedFactor < 0.9) || (speedFactor > 1.1))
                sprintf(speedMsg, "%.1fx%dMHz/", speedFactor, ConfigureParams.System.nCpuFreq);
            else
                sprintf(speedMsg, "%dMHz/",                   ConfigureParams.System.nCpuFreq);
        }
    }
    return speedMsg;
}

#if ENABLE_TESTING
static const report_t reports[] = {
    {"ND",    nd_reports},
    {"Host",  host_report},
};
#endif

/*-----------------------------------------------------------------------*/
/**
 * Pause emulation, stop sound.  'visualize' should be set true,
 * unless unpause will be called immediately afterwards.
 * 
 * @return true if paused now, false if was already paused
 */
bool Main_PauseEmulation(bool visualize) {
	if ( !bEmulationActive )
		return false;

	bEmulationActive = false;
    host_pause_time(!(bEmulationActive));
    Screen_Pause(true);
    Sound_Pause(true);
    if (ConfigureParams.Dimension.bEnabled) {
        dimension_pause(true);
    }
    
	if (visualize) {
		Statusbar_AddMessage("Emulation paused", 100);
		/* make sure msg gets shown */
		Statusbar_Update(sdlscrn);

		if (bGrabMouse && !bInFullScreen) {
			/* Un-grab mouse pointer in windowed mode */
			SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
        }
	}
	return true;
}

/*-----------------------------------------------------------------------*/
/**
 * Start/continue emulation
 * 
 * @return true if continued, false if was already running
 */
bool Main_UnPauseEmulation(void) {
	if ( bEmulationActive )
		return false;

	bEmulationActive = true;
    host_pause_time(!(bEmulationActive));
    Screen_Pause(false);
    Sound_Pause(false);
    if (ConfigureParams.Dimension.bEnabled) {
        dimension_pause(false);
    }

	if (bGrabMouse) {
		/* Grab mouse pointer again */
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
    }
	return true;
}

/*-----------------------------------------------------------------------*/
/**
 * Optionally ask user whether to quit and set bQuitProgram accordingly
 */
void Main_RequestQuit(void) {
    if (ConfigureParams.Log.bConfirmQuit) {
		bQuitProgram = false;	/* if set true, dialog exits */
		bQuitProgram = DlgAlert_Query("All unsaved data will be lost.\nDo you really want to quit?");
	}
	else {
		bQuitProgram = true;
	}

	if (bQuitProgram) {
		/* Assure that CPU core shuts down */
		M68000_SetSpecial(SPCFLAG_BRK);
	}
}

/*-----------------------------------------------------------------------*/
/**
 * Since SDL_Delay and friends are very inaccurate on some systems, we have
 * to check if we can rely on this delay function.
 */
static void Main_CheckForAccurateDelays(void) {
	int nStartTicks, nEndTicks;

	/* Force a task switch now, so we have a longer timeslice afterwards */
	SDL_Delay(10);

	nStartTicks = SDL_GetTicks();
	SDL_Delay(1);
	nEndTicks = SDL_GetTicks();

	/* If the delay took longer than 10ms, we are on an inaccurate system! */
	bAccurateDelays = ((nEndTicks - nStartTicks) < 9);

	if (bAccurateDelays)
		Log_Printf(LOG_WARN, "Host system has accurate delays. (%d)\n", nEndTicks - nStartTicks);
	else
		Log_Printf(LOG_WARN, "Host system does not have accurate delays. (%d)\n", nEndTicks - nStartTicks);
}


/* ----------------------------------------------------------------------- */
/**
 * Set mouse pointer to new coordinates and set flag to ignore the mouse event
 * that is generated by SDL_WarpMouse().
 */
void Main_WarpMouse(int x, int y) {
    SDL_WarpMouseInWindow(sdlWindow, x, y); /* Set mouse pointer to new position */
	bIgnoreNextMouseMotion = true;          /* Ignore mouse motion event from SDL_WarpMouse */
}


/* ----------------------------------------------------------------------- */
/**
 * Handle mouse motion event.
 */
SDL_Event mymouse[100];
static void Main_HandleMouseMotion(SDL_Event *pEvent) {
	int dx, dy;
	int i,nb;

	dx = pEvent->motion.xrel;
	dy = pEvent->motion.yrel;

	/* get all mouse event to clean the queue and sum them */
	nb=SDL_PeepEvents(&mymouse[0], 100, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEMOTION);

	for (i=0;i<nb;i++) {
        dx += mymouse[i].motion.xrel;
        dy += mymouse[i].motion.yrel;
	}

	if (bGrabMouse) {
    	Keymap_MouseMove(dx,dy,ConfigureParams.Mouse.fLinSpeedLocked,ConfigureParams.Mouse.fExpSpeedLocked);
	} else {
    	Keymap_MouseMove(dx,dy,ConfigureParams.Mouse.fLinSpeedNormal,ConfigureParams.Mouse.fLinSpeedNormal);
	}
}

static int statusBarUpdate;

/* ----------------------------------------------------------------------- */
/**
 * SDL message handler.
 * Here we process the SDL events (keyboard, mouse, ...)
 */
void Main_EventHandler(void) {
    bool bContinueProcessing;
    SDL_Event event;
    int events;
    int remotepause;
    
    if(++statusBarUpdate > 400) {
        double vt;
        double rt;
        host_time(&rt, &vt);
#if ENABLE_TESTING
        fprintf(stderr, "[reports]");
        for(int i = 0; i < sizeof(reports)/sizeof(report_t); i++) {
            const char* msg = reports[i].report(rt, vt);
            if(msg[0]) fprintf(stderr, " %s:%s", reports[i].label, msg);
        }
        fprintf(stderr, "\n");
#else
        Main_Speed(rt, vt);
#endif
        Statusbar_UpdateInfo();
        statusBarUpdate = 0;
    }
    
    do {
        bContinueProcessing = false;
        
        /* check remote process control from different thread (e.g. i860) */
        switch(mainPauseEmulation) {
            case PAUSE_EMULATION:
                mainPauseEmulation = PAUSE_NONE;
                Main_PauseEmulation(true);
                break;
            case UNPAUSE_EMULATION:
                mainPauseEmulation = PAUSE_NONE;
                Main_UnPauseEmulation();
                break;
        }
        
        /* check remote process control */
        remotepause = Control_CheckUpdates();
        
        if ( bEmulationActive || remotepause ) {
            double time_offset = host_real_time_offset() * 1000;
            if(time_offset > 10)
                events = SDL_WaitEventTimeout(&event, time_offset);
            else
                events = SDL_PollEvent(&event);
        }
        else {
            ShortCut_ActKey();
            /* last (shortcut) event activated emulation? */
            if ( bEmulationActive )
                break;
            events = SDL_WaitEvent(&event);
        }
        if (!events) {
            /* no events -> if emulation is active or
             * user is quitting -> return from function.
             */
            continue;
        }
        switch (event.type) {
            case SDL_WINDOWEVENT:
                if(event.window.event == SDL_WINDOWEVENT_CLOSE) {
                    SDL_WaitEventTimeout(&event, 100); // grab SDL_Quit if pending
                    Main_RequestQuit();
                }
                continue;

            case SDL_QUIT:
                Main_RequestQuit();
                break;
                
            case SDL_MOUSEMOTION:               /* Read/Update internal mouse position */
                Main_HandleMouseMotion(&event);
                bContinueProcessing = false;
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    if (ConfigureParams.Mouse.bEnableAutoGrab && !bGrabMouse) {
                        bGrabMouse = true;        /* Toggle flag */
                        
                        /* If we are in windowed mode, toggle the mouse cursor mode now: */
                        if (!bInFullScreen)
                        {
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                            SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
                            Main_SetTitle(MOUSE_LOCK_MSG);
                        }
                    }
                    
                    Keymap_MouseDown(true);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    Keymap_MouseDown(false);
                }
                break;
                
            case SDL_MOUSEBUTTONUP:
                if (event.button.button == SDL_BUTTON_LEFT) {
                    Keymap_MouseUp(true);
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    Keymap_MouseUp(false);
                }
                break;
                
            case SDL_MOUSEWHEEL:
                Keymap_MouseWheel(&event.wheel);
                break;
                
            case SDL_KEYDOWN:
                if (event.key.repeat)
                    break;
                
                Keymap_KeyDown(&event.key.keysym);
                break;
                
            case SDL_KEYUP:
                Keymap_KeyUp(&event.key.keysym);
                break;
                
                
            default:
                /* don't let unknown events delay event processing */
                bContinueProcessing = true;
                break;
        }
    } while (bContinueProcessing || !(bEmulationActive || bQuitProgram));
}


void Main_EventHandlerInterrupt() {
    CycInt_AcknowledgeInterrupt();
    Main_EventHandler();
    CycInt_AddRelativeInterruptUs((1000*1000)/200, 0, INTERRUPT_EVENT_LOOP); // poll events with 200 Hz
}

/*-----------------------------------------------------------------------*/
/**
 * Set Hatari window title. Use NULL for default
 */
void Main_SetTitle(const char *title) {
    if (title)
        SDL_SetWindowTitle(sdlWindow, title);
    else
        SDL_SetWindowTitle(sdlWindow, PROG_NAME);
}

/*-----------------------------------------------------------------------*/
/**
 * Initialise emulation
 */
static void Main_Init(void) {
	/* Open debug log file */
	if (!Log_Init()) {
		fprintf(stderr, "Logging/tracing initialization failed\n");
		exit(-1);
	}
	Log_Printf(LOG_INFO, PROG_NAME ", compiled on:  " __DATE__ ", " __TIME__ "\n");

	/* Init SDL's video subsystem. Note: Audio and joystick subsystems
	   will be initialized later (failures there are not fatal). */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | Opt_GetNoParachuteFlag()) < 0)
	{
		fprintf(stderr, "Could not initialize the SDL library:\n %s\n", SDL_GetError() );
		exit(-1);
	}
	SDLGui_Init();
	Screen_Init();
	Main_SetTitle(NULL);
	DSP_Init();
	M68000_Init();                /* Init CPU emulation */
	Keymap_Init();

    /* call menu at startup */
    if (!File_Exists(sConfigFileName) || ConfigureParams.ConfigDialog.bShowConfigDialogAtStartup) {
        Dialog_DoProperty();
        if (bQuitProgram) {
            SDL_Quit();
            exit(-2);
        }
    }

    Dialog_CheckFiles();
    
    if (bQuitProgram) {
        SDL_Quit();
        exit(-2);
    }
    
    Reset_Cold();
    
	IoMem_Init();
	
    /* Start EventHandler */
    CycInt_AddRelativeInterruptUs(500*1000, 0, INTERRUPT_EVENT_LOOP);
    
	/* done as last, needs CPU & DSP running... */
	DebugUI_Init();
}


/*-----------------------------------------------------------------------*/
/**
 * Un-Initialise emulation
 */
static void Main_UnInit(void) {
	Screen_ReturnFromFullScreen();
	IoMem_UnInit();
	SDLGui_UnInit();
	Screen_UnInit();
	Exit680x0();

	/* SDL uninit: */
	SDL_Quit();

	/* Close debug log file */
	Log_UnInit();
}


/*-----------------------------------------------------------------------*/
/**
 * Load initial configuration file(s)
 */
static void Main_LoadInitialConfig(void) {
	char *psGlobalConfig;

	psGlobalConfig = malloc(FILENAME_MAX);
	if (psGlobalConfig)
	{
#if defined(__AMIGAOS4__)
		strncpy(psGlobalConfig, CONFDIR"previous.cfg", FILENAME_MAX);
#else
		snprintf(psGlobalConfig, FILENAME_MAX, CONFDIR"%cprevious.cfg", PATHSEP);
#endif
		/* Try to load the global configuration file */
		Configuration_Load(psGlobalConfig);

		free(psGlobalConfig);
	}

	/* Now try the users configuration file */
	Configuration_Load(NULL);
}

/*-----------------------------------------------------------------------*/
/**
 * Set TOS etc information and initial help message
 */
static void Main_StatusbarSetup(void) {
	const char *name = NULL;
	SDL_Keycode key;

	key = ConfigureParams.Shortcut.withoutModifier[SHORTCUT_OPTIONS];
	if (!key)
		key = ConfigureParams.Shortcut.withModifier[SHORTCUT_OPTIONS];
	if (key)
		name = SDL_GetKeyName(key);
	if (name)
	{
		char message[24], *keyname;
#ifdef _MUDFLAP
		__mf_register(name, 32, __MF_TYPE_GUESS, "SDL keyname");
#endif
		keyname = Str_ToUpper(strdup(name));
		snprintf(message, sizeof(message), "Press %s for Options", keyname);
		free(keyname);

		Statusbar_AddMessage(message, 6000);
	}
	/* update information loaded by Main_Init() */
	Statusbar_UpdateInfo();
}

#ifdef WIN32
	extern void Win_OpenCon(void);
#endif

/*-----------------------------------------------------------------------*/
/**
 * Set signal handlers to catch signals
 */
static void Main_SetSignalHandlers(void) {
#ifndef _WIN32
    signal(SIGPIPE, SIG_IGN);
#endif
    signal(SIGFPE, SIG_IGN);
}


/*-----------------------------------------------------------------------*/
/**
 * Main
 * 
 * Note: 'argv' cannot be declared const, MinGW would then fail to link.
 */
int main(int argc, char *argv[]) {
	/* Generate random seed */
	srand(time(NULL));
    
    /* Set signal handlers */
    Main_SetSignalHandlers();

	/* Initialize directory strings */
	Paths_Init(argv[0]);

	/* Set default configuration values: */
	Configuration_SetDefault();

	/* Now load the values from the configuration file */
	Main_LoadInitialConfig();
    
#if 0 /* FIXME: This sometimes causes exits when starting from application bundles */
	/* Check for any passed parameters */
	if (!Opt_ParseParameters(argc, (const char * const *)argv))
	{
		return 1;
	}
#endif
	/* monitor type option might require "reset" -> true */
	Configuration_Apply(true);

#ifdef WIN32
	Win_OpenCon();
#endif

	/* Needed on maemo but useful also with normal X11 window managers
	 * for window grouping when you have multiple Hatari SDL windows open
	 */
#if HAVE_SETENV
	setenv("SDL_VIDEO_X11_WMCLASS", "previous", 1);
#endif

	/* Init emulator system */
	Main_Init();

	/* Set initial Statusbar information */
	Main_StatusbarSetup();
	
	/* Check if SDL_Delay is accurate */
	Main_CheckForAccurateDelays();


	/* Run emulation */
	Main_UnPauseEmulation();
	M68000_Start();                 /* Start emulation */


	/* Un-init emulation system */
	Main_UnInit();

	return 0;
}
