/*
  Hatari - screen.c

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.

*/

const char Screen_fileid[] = "Hatari screen.c : " __DATE__ " " __TIME__;

#include <SDL.h>
#include <SDL_endian.h>

#include "main.h"
#include "configuration.h"
#include "log.h"
#include "m68000.h"
#include "paths.h"
#include "screen.h"
#include "control.h"
#include "convert/routines.h"
#include "resolution.h"
#include "statusbar.h"
#include "video.h"


/* extern for several purposes */
SDL_Surface *sdlscrn = NULL;                /* The SDL screen surface */
int nScreenZoomX, nScreenZoomY;             /* Zooming factors, used for scaling mouse motions */
int nBorderPixelsLeft, nBorderPixelsRight;  /* Pixels in left and right border */
int nBorderPixelsTop, nBorderPixelsBottom;  /* Lines in top and bottom border */

/* extern for shortcuts and falcon/hostscreen.c */
bool bGrabMouse = false;      /* Grab the mouse cursor in the window */
bool bInFullScreen = false;   /* true if in full screen */


/* extern for video.c */
Uint8 pNEXTScreen[(1120*832)*2];
FRAMEBUFFER *pFrameBuffer;    /* Pointer into current 'FrameBuffer' */

static FRAMEBUFFER FrameBuffers[NUM_FRAMEBUFFERS]; /* Store frame buffer details to tell how to update */
static Uint8 *pNEXTScreenCopy;                       /* Keep track of current and previous ST screen data */
static Uint8 *pPCScreenDest;                       /* Destination PC buffer */
static int NEXTScreenEndHorizLine;                   /* End lines to be converted */
static int PCScreenBytesPerLine;
static int NEXTScreenWidthBytes;
static SDL_Rect NEXTScreenRect;                      /* screen size without statusbar */

static int NEXTScreenLineOffset[910];  /* Offsets for ST screen lines eg, 0,160,320... */

static void (*ScreenDrawFunctionsNormal[3])(void); /* Screen draw functions */

static bool bScreenContentsChanged;     /* true if buffer changed and requires blitting */
static bool bScrDoubleY;                /* true if double on Y */
static int ScrUpdateFlag;               /* Bit mask of how to update screen */


static bool Screen_DrawFrame(bool bForceFlip);


/*-----------------------------------------------------------------------*/
/**
 * Create ST 0x777 / STe 0xfff color format to 16 or 32 bits per pixel
 * conversion table. Called each time when changed resolution or to/from
 * fullscreen mode.
 */
static void Screen_SetupRGBTable(void)
{
}

SDL_Color sdlColors[16];
Uint32 colors[32];

Uint32 hicolors[4096];

/*-----------------------------------------------------------------------*/
/**
 * Create new palette for display.
 */
static void Screen_CreatePalette(void)
{

	sdlColors[0].r = sdlColors[0].g = sdlColors[0].b = 255;
	sdlColors[1].r = sdlColors[1].g = sdlColors[1].b = 170;
	sdlColors[2].r = sdlColors[2].g = sdlColors[2].b = 85;
	sdlColors[3].r = sdlColors[3].g = sdlColors[3].b = 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Create 8-Bit palette for display if needed.
 */
static void Screen_Handle8BitPalettes(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Set screen draw functions.
 */
static void Screen_SetDrawFunctions(int nBitCount, bool bDoubleLowRes)
{
		ScreenDrawFunctionsNormal[ST_HIGH_RES] = ConvertHighRes_640x8Bit;
}


/*-----------------------------------------------------------------------*/
/**
 * Set amount of border pixels
 */
static void Screen_SetBorderPixels(int leftX, int leftY)
{
}

/*-----------------------------------------------------------------------*/
/**
 * store Y offset for each horizontal line in our source ST screen for
 * reference in the convert functions.
 */
static void Screen_SetSTScreenOffsets(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Initialize SDL screen surface / set resolution.
 */
static void Screen_SetResolution(void)
{
	int Width, Height, nZoom, SBarHeight, BitCount, maxW, maxH;
	Uint32 sdlVideoFlags;
	bool bDoubleLowRes = false;

	BitCount = 0; /* host native */

	nBorderPixelsTop = nBorderPixelsBottom = 0;
	nBorderPixelsLeft = nBorderPixelsRight = 0;

	nScreenZoomX = 1;
	nScreenZoomY = 1;

	Width = 1120;
	Height = 832;
	nZoom = 1;

	/* Statusbar height for doubled screen size */
	SBarHeight = Statusbar_GetHeightForSize(1120, 832);
	Resolution_GetLimits(&maxW, &maxH, &BitCount);
		
	
	Screen_SetSTScreenOffsets();  
	Height += Statusbar_SetHeight(Width, Height);

	/* SDL Video attributes: */
	if (bInFullScreen)
	{
		sdlVideoFlags  = SDL_HWSURFACE|SDL_FULLSCREEN|SDL_HWPALETTE/*|SDL_DOUBLEBUF*/;
		/* SDL_DOUBLEBUF helps avoiding tearing and can be faster on suitable HW,
		 * but it doesn't work with partial screen updates done by the ST screen
		 * update code or the Hatari GUI, so double buffering is disabled.
		 */
	}
	else
	{
		sdlVideoFlags  = SDL_SWSURFACE|SDL_HWPALETTE;
	}

	/* Check if we really have to change the video mode: */
	if (!sdlscrn || sdlscrn->w != Width || sdlscrn->h != Height
	    || (BitCount && sdlscrn->format->BitsPerPixel != BitCount)
	    || (sdlscrn->flags&SDL_FULLSCREEN) != (sdlVideoFlags&SDL_FULLSCREEN))
	{
#ifdef _MUDFLAP
		if (sdlscrn) {
			__mf_unregister(sdlscrn->pixels, sdlscrn->pitch*sdlscrn->h, __MF_TYPE_GUESS);
		}
#endif
		if (bInFullScreen)
		{
			/* unhide the Hatari WM window for fullscreen */
			Control_ReparentWindow(Width, Height, bInFullScreen);
		}
		
		/* Set new video mode */
		//fprintf(stderr,"Requesting video mode %i %i %i\n", Width, Height, BitCount);
		sdlscrn = SDL_SetVideoMode(Width, Height, BitCount, sdlVideoFlags);
		//fprintf(stderr,"Got video mode %i %i %i\n", sdlscrn->w, sdlscrn->h, sdlscrn->format->BitsPerPixel);

		/* By default ConfigureParams.Screen.nForceBpp and therefore
		 * BitCount is zero which means "SDL color depth autodetection".
		 * In this case the SDL_SetVideoMode() call might return
		 * a 24 bpp resolution
		 */
		if (sdlscrn && sdlscrn->format->BitsPerPixel == 24)
		{
			fprintf(stderr, "Unsupported color depth 24, trying 32 bpp instead...\n");
			sdlscrn = SDL_SetVideoMode(Width, Height, 32, sdlVideoFlags);
		}

		/* Exit if we can not open a screen */
		if (!sdlscrn)
		{
			fprintf(stderr, "Could not set video mode:\n %s\n", SDL_GetError() );
			SDL_Quit();
			exit(-2);
		}
#ifdef _MUDFLAP
		__mf_register(sdlscrn->pixels, sdlscrn->pitch*sdlscrn->h, __MF_TYPE_GUESS, "SDL pixels");
#endif

		if (!bInFullScreen)
		{
			/* re-embed the new Hatari SDL window */
			Control_ReparentWindow(Width, Height, bInFullScreen);
		}
		
		/* Re-init screen palette: */
		if (sdlscrn->format->BitsPerPixel == 8)
			Screen_Handle8BitPalettes();    /* Initialize new 8 bit palette */
		else
			Screen_SetupRGBTable();         /* Create color convertion table */

		Statusbar_Init(sdlscrn);
		
		/* screen area without the statusbar */
		NEXTScreenRect.x = 0;
		NEXTScreenRect.y = 0;
		NEXTScreenRect.w = sdlscrn->w;
		NEXTScreenRect.h = sdlscrn->h - Statusbar_GetHeight();
	}

	/* Set drawing functions */
	Screen_SetDrawFunctions(sdlscrn->format->BitsPerPixel, bDoubleLowRes);

	Screen_SetFullUpdate();           /* Cause full update of screen */
}


/*-----------------------------------------------------------------------*/
/**
 * Init Screen bitmap and buffers/tables needed for ST to PC screen conversion
 */
void Screen_Init(void)
{
	int i;
	SDL_Surface *pIconSurf;
	char sIconFileName[FILENAME_MAX];

	/* Clear frame buffer structures and set current pointer */
	memset(FrameBuffers, 0, NUM_FRAMEBUFFERS * sizeof(FRAMEBUFFER));

	/* Allocate previous screen check workspace. We are going to double-buffer a double-buffered screen. Oh. */
	for (i = 0; i < NUM_FRAMEBUFFERS; i++)
	{
		FrameBuffers[i].pNEXTScreen = malloc(((1024)/8)*768);
		FrameBuffers[i].pNEXTScreenCopy = malloc(((1024)/8)*768);
		if (!FrameBuffers[i].pNEXTScreen || !FrameBuffers[i].pNEXTScreenCopy)
		{
			fprintf(stderr, "Failed to allocate frame buffer memory.\n");
			exit(-1);
		}
	}
	pFrameBuffer = &FrameBuffers[0];

	/* Load and set icon */
	snprintf(sIconFileName, sizeof(sIconFileName), "%s%cprevious-icon.bmp",
	         Paths_GetDataDir(), PATHSEP);
	pIconSurf = SDL_LoadBMP(sIconFileName);
	if (pIconSurf)
	{
		SDL_SetColorKey(pIconSurf, SDL_SRCCOLORKEY, SDL_MapRGB(pIconSurf->format, 255, 255, 255));
		SDL_WM_SetIcon(pIconSurf, NULL);
		SDL_FreeSurface(pIconSurf);
	}

	/* Set initial window resolution */
	bInFullScreen = ConfigureParams.Screen.bFullScreen;
	Screen_SetResolution();

	if (bGrabMouse)
		SDL_WM_GrabInput(SDL_GRAB_ON);

	Video_SetScreenRasters();                       /* Set rasters ready for first screen */

	Screen_CreatePalette();
	/* Configure some SDL stuff: */
	SDL_WM_SetCaption(PROG_NAME, "Previous");
	SDL_ShowCursor(SDL_DISABLE);
}


/*-----------------------------------------------------------------------*/
/**
 * Free screen bitmap and allocated resources
 */
void Screen_UnInit(void)
{
	int i;

	/* Free memory used for copies */
	for (i = 0; i < NUM_FRAMEBUFFERS; i++)
	{
		free(FrameBuffers[i].pNEXTScreen);
		free(FrameBuffers[i].pNEXTScreenCopy);
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Reset screen
 */
void Screen_Reset(void)
{
	/* Cause full update */
	Screen_ModeChanged();
}


/*-----------------------------------------------------------------------*/
/**
 * Set flags so screen will be TOTALLY re-drawn (clears whole of full-screen)
 * next time around
 */
void Screen_SetFullUpdate(void)
{
	int i;

	/* Update frame buffers */
	for (i = 0; i < NUM_FRAMEBUFFERS; i++)
		FrameBuffers[i].bFullUpdate = true;
}


/*-----------------------------------------------------------------------*/
/**
 * Clear Window display memory
 */
static void Screen_ClearScreen(void)
{
	SDL_FillRect(sdlscrn, &NEXTScreenRect, SDL_MapRGB(sdlscrn->format, 0, 0, 0));
}


/*-----------------------------------------------------------------------*/
/**
 * Return true if (falcon/tt) hostscreen functions need to be used
 * instead of the (st/ste) functions here.
 */
static bool Screen_UseHostScreen(void)
{
	return false;
}

/*-----------------------------------------------------------------------*/
/**
 * Force screen redraw.  Does the right thing regardless of whether
 * we're in ST/STe, Falcon or TT mode.  Needed when switching modes
 * while emulation is paused.
 */
static void Screen_Refresh(void)
{
	Screen_DrawFrame(true);
}


/*-----------------------------------------------------------------------*/
/**
 * Enter Full screen mode
 */
void Screen_EnterFullScreen(void)
{
	bool bWasRunning;

	if (!bInFullScreen)
	{
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = true;

		if (Screen_UseHostScreen())
		{
//			HostScreen_toggleFullScreen();
		}
		else
		{
			Screen_SetResolution();
			Screen_ClearScreen();       /* Black out screen bitmap as will be invalid when return */
		}

		SDL_Delay(20);                  /* To give monitor time to change to new resolution */
		
		if (bWasRunning)
		{
			/* And off we go... */
			Main_UnPauseEmulation();
		}
		else
		{
			Screen_Refresh();
		}
		SDL_WM_GrabInput(SDL_GRAB_ON);  /* Grab mouse pointer in fullscreen */
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Return from Full screen mode back to a window
 */
void Screen_ReturnFromFullScreen(void)
{
	bool bWasRunning;

	if (bInFullScreen)
	{
		/* Hold things... */
		bWasRunning = Main_PauseEmulation(false);
		bInFullScreen = false;

		if (Screen_UseHostScreen())
		{
//			HostScreen_toggleFullScreen();
		}
		else
		{
			Screen_SetResolution();
		}
		SDL_Delay(20);                /* To give monitor time to switch resolution */

		if (bWasRunning)
		{
			/* And off we go... */
			Main_UnPauseEmulation();
		}
		else
		{
			Screen_Refresh();
		}

		if (!bGrabMouse)
		{
			/* Un-grab mouse pointer in windowed mode */
			SDL_WM_GrabInput(SDL_GRAB_OFF);
		}
	}
}


/*-----------------------------------------------------------------------*/
/**
 * Have we changed between low/med/high res?
 */
static void Screen_DidResolutionChange(int new_res)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Force things associated with changing between low/medium/high res.
 */
void Screen_ModeChanged(void)
{
	if (!sdlscrn)
	{
		/* screen not yet initialized */
		return;
	}
		/* Set new display mode, if differs from current */
		Screen_SetResolution();
		Screen_SetFullUpdate();
	if (bInFullScreen || bGrabMouse)
		SDL_WM_GrabInput(SDL_GRAB_ON);
	else
		SDL_WM_GrabInput(SDL_GRAB_OFF);
}


/*-----------------------------------------------------------------------*/
/**
 * Compare current resolution on line with previous, and set 'UpdateLine' accordingly
 * Return if swap between low/medium resolution
 */
static bool Screen_CompareResolution(int y, int *pUpdateLine, int oldres)
{
	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Check to see if palette changes cause screen update and keep 'HBLPalette[]' up-to-date
 */
static void Screen_ComparePalette(int y, int *pUpdateLine)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Check for differences in Palette and Resolution from Mask table and update
 * and store off which lines need updating and create full-screen palette.
 * (It is very important for these routines to check for colour changes with
 * the previous screen so only the very minimum parts are updated).
 * Return new STRes value.
 */
static int Screen_ComparePaletteMask(int res)
{
	return 0;
}


/*-----------------------------------------------------------------------*/
/**
 * Update Palette Mask to show 'full-update' required. This is usually done after a resolution change
 * or when going between a Window and full-screen display
 */
static void Screen_SetFullUpdateMask(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Set details for ST screen conversion.
 */
static void Screen_SetConvertDetails(void)
{
}


/*-----------------------------------------------------------------------*/
/**
 * Lock full-screen for drawing
 */
static bool Screen_Lock(void)
{
	if (SDL_MUSTLOCK(sdlscrn))
	{
		if (SDL_LockSurface(sdlscrn))
		{
			Screen_ReturnFromFullScreen();   /* All OK? If not need to jump back to a window */
			return false;
		}
	}

	return true;
}

/*-----------------------------------------------------------------------*/
/**
 * UnLock full-screen
 */
static void Screen_UnLock(void)
{
	if ( SDL_MUSTLOCK(sdlscrn) )
		SDL_UnlockSurface(sdlscrn);
}


/*-----------------------------------------------------------------------*/
/**
 * Blit our converted ST screen to window/full-screen
 */
static void Screen_Blit(void)
{
	unsigned char *pTmpScreen;

	{
		SDL_UpdateRects(sdlscrn, 1, &NEXTScreenRect);
	}

	/* Swap copy/raster buffers in screen. */
	pTmpScreen = pFrameBuffer->pNEXTScreenCopy;
	pFrameBuffer->pNEXTScreenCopy = pFrameBuffer->pNEXTScreen;
	pFrameBuffer->pNEXTScreen = pTmpScreen;
}


/*-----------------------------------------------------------------------*/
/**
 */
static bool Screen_DrawFrame(bool bForceFlip)
{
	int new_res;
	void (*pDrawFunction)(void);
	
	/* Lock screen ready for drawing */
	if (Screen_Lock())
	{
		
		pDrawFunction = ScreenDrawFunctionsNormal[ST_HIGH_RES];

		if (pDrawFunction)
			CALL_VAR(pDrawFunction);

		/* Unlock screen */
		Screen_UnLock();

		/* draw statusbar or overlay led(s) after unlock */
		Statusbar_OverlayBackup(sdlscrn);
		Statusbar_Update(sdlscrn);

		Screen_Blit();

		return bScreenContentsChanged;
	}

	return false;
}


/*-----------------------------------------------------------------------*/
/**
 * Draw ST screen to window/full-screen
 */
bool Screen_Draw(void)
{
	if (!bQuitProgram)
	{
		/* And draw (if screen contents changed) */
		Screen_DrawFrame(false);
		return true;
	}

	return false;
}


/* -------------- screen conversion routines --------------------------------
*/


/*-----------------------------------------------------------------------*/
/**
 */
static int AdjustLinePaletteRemap(int y)
{
	return true;
}


/*-----------------------------------------------------------------------*/
/**
 */
static void Convert_StartFrame(void)
{
}

/* lookup tables and conversion macros */
#include "convert/macros.h"

/* Conversion routines */

#include "convert/high640x8.c"    /* HighRes To 640xH x 8-bit color */

