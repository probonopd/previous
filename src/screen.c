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
#include "dimension.h"
#include "paths.h"
#include "screen.h"
#include "control.h"
#include "convert/routines.h"
#include "resolution.h"
#include "statusbar.h"
#include "video.h"


/* extern for several purposes */
SDL_Window *sdlWindow;
SDL_Renderer *sdlRenderer;
SDL_Texture *sdlTexture;
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

#if 1 /* Translating to SDL2 */
void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects)
{
	//fprintf(stderr,"rendering %i %i %i %i %i\n", numrects, rects->x, rects->w, rects->w, rects->h);
	SDL_UpdateTexture(sdlTexture, NULL, screen->pixels, screen->pitch);
	SDL_RenderClear(sdlRenderer);
	SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, NULL);
	SDL_RenderPresent(sdlRenderer);
}

void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h)
{
	SDL_Rect rect = { x, y, w, h };
	SDL_UpdateRects(screen, 1, &rect);
}
#endif

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

	/* Check if we really have to change the video mode: */
	if (!sdlscrn || sdlscrn->w != Width || sdlscrn->h != Height
	    || (BitCount && sdlscrn->format->BitsPerPixel != BitCount))
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
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

		fprintf(stderr, "SDL screen request: %d x %d @ %d (%s)\n", Width, Height, BitCount, bInFullScreen?"fullscreen":"windowed");
		sdlWindow = SDL_CreateWindow(PROG_NAME,
		                             SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		                             Width, Height, 0);
		sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
		if (!sdlWindow || !sdlRenderer)
		{
			fprintf(stderr,"Failed to create window or renderer!\n");
			exit(-1);
		}
		SDL_RenderSetLogicalSize(sdlRenderer, Width, Height);
		sdlscrn = SDL_CreateRGBSurface(SDL_SWSURFACE, Width, Height, 32,
						0x00FF0000, 0x0000FF00,
						0x000000FF, 0x00000000);
		sdlTexture = SDL_CreateTexture(sdlRenderer,
						SDL_PIXELFORMAT_RGB888,
						SDL_TEXTUREACCESS_STREAMING,
						Width, Height);
		fprintf(stderr, "SDL screen granted: %d x %d @ %d\n", sdlscrn->w, sdlscrn->h, sdlscrn->format->BitsPerPixel);

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

	/* Set initial window resolution */
	bInFullScreen = ConfigureParams.Screen.bFullScreen;
	Screen_SetResolution();

	if (bGrabMouse) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
    }

	Video_SetScreenRasters();                       /* Set rasters ready for first screen */

	Screen_CreatePalette();
	/* Configure some SDL stuff: */
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
            SDL_SetWindowFullscreen(sdlWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			//Screen_SetResolution();
			//Screen_ClearScreen();       /* Black out screen bitmap as will be invalid when return */
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
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
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
            SDL_SetWindowFullscreen(sdlWindow, 0);
			//Screen_SetResolution();
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
			SDL_SetRelativeMouseMode(SDL_FALSE);
            SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
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
	if (bInFullScreen || bGrabMouse) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(sdlWindow, SDL_TRUE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
    }
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

