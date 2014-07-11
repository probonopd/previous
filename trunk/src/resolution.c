/*
  Hatari - resolution.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  SDL resolution limitation and selection routines.
*/
const char Resolution_fileid[] = "Hatari resolution.c : " __DATE__ " " __TIME__;

#include <SDL.h>
#include "main.h"
#include "configuration.h"
#include "resolution.h"
#include "screen.h"

#define RESOLUTION_DEBUG 0

#if RESOLUTION_DEBUG
#define Dprintf(a) printf a
#else
#define Dprintf(a)
#endif

static int DesktopWidth, DesktopHeight;

/**
  * Initilizes resolution settings (gets current desktop
  * resolution, sets max Falcon/TT Videl zooming resolution).
  */
void Resolution_Init(void)
{
 	/* Needs to be called after SDL video and configuration
 	 * initialization, but before Hatari Screen init is called
 	 * for the first time!
 	 */
	fprintf(stderr,"FIXME: Resolution init!\n");
	DesktopWidth = 800;
	DesktopHeight = 600;

 	/* if user hasn't set own max zoom size, use desktop size */
 	if (!(ConfigureParams.Screen.nMaxWidth &&
 	      ConfigureParams.Screen.nMaxHeight)) {
 		ConfigureParams.Screen.nMaxWidth = DesktopWidth;
 		ConfigureParams.Screen.nMaxHeight = DesktopHeight;
 	}
 	Dprintf(("Desktop resolution: %dx%d\n",DesktopWidth, DesktopHeight));
 	Dprintf(("Configured Max res: %dx%d\n",ConfigureParams.Screen.nMaxWidth,ConfigureParams.Screen.nMaxHeight));
}

/**
  * Get current desktop resolution
  */
void Resolution_GetDesktopSize(int *width, int *height)
{
 	*width = DesktopWidth;
 	*height = DesktopHeight;
}


/**
 * Select best resolution from given SDL video modes.
 * - If width and height are given, select the smallest mode larger
 *   or equal to requested size
 * - Otherwise select the largest available mode
 * return true for success and false if no matching mode was found.
 */
static bool Resolution_Select(SDL_Rect **modes, int *width, int *height)
{
#define TOO_LARGE 0x7fff
	int i, bestw, besth;

	if (!(*width && *height)) {
		/* search the largest mode (prefer wider ones) */
		for (i = 0; modes[i]; i++) {
			if ((modes[i]->w > *width) && (modes[i]->h >= *height)) {
				*width = modes[i]->w;
				*height = modes[i]->h;
			}
		}
		Dprintf(("resolution: largest found video mode: %dx%d\n",*width,*height));
		return true;
	}

	/* Search the smallest mode larger or equal to requested size */
	bestw = TOO_LARGE;
	besth = TOO_LARGE;
	for (i = 0; modes[i]; i++) {
		if ((modes[i]->w >= *width) && (modes[i]->h >= *height)) {
			if ((modes[i]->w < bestw) || (modes[i]->h < besth)) {
				bestw = modes[i]->w;
				besth = modes[i]->h;
			}
		}
	}
	if (bestw == TOO_LARGE || besth == TOO_LARGE) {
		return false;
	}
	*width = bestw;
	*height = besth;
	Dprintf(("resolution: video mode found: %dx%d\n",*width,*height));
	return true;
#undef TOO_LARGE
}


/**
 * Search video mode size that best suits the given width/height/bpp
 * constraints and set them into given arguments.  With zeroed arguments,
 * return largest video mode.
 */
void Resolution_Search(int *width, int *height, int *bpp)
{
	fprintf(stderr,"FIXME: Resolution_Search\n");
	Resolution_GetDesktopSize(width, height);
}


/**
  * Set given width & height arguments to maximum size allowed in the
  * configuration, or if that's too large for the requested bit depth,
  * to the largest available video mode size.
 */
void Resolution_GetLimits(int *width, int *height, int *bpp)
{
	*width = *height = 0;
	/* constrain max size to what HW/SDL offers */
    Dprintf(("resolution: request limits for: %dx%dx%d\n", *width, *height, *bpp));
	Resolution_Search(width, height, bpp);

 	if (bInFullScreen && ConfigureParams.Screen.bKeepResolution) {
 		/* resolution change not allowed */
 		Dprintf(("resolution: limit to desktop size\n"));
 		Resolution_GetDesktopSize(width, height);
 		return;
 	}
	if (!(*width && *height) ||

	    (ConfigureParams.Screen.nMaxWidth < *width &&
	     ConfigureParams.Screen.nMaxHeight < *height)) {
		Dprintf(("resolution: limit to user configured max\n"));
		*width = ConfigureParams.Screen.nMaxWidth;
		*height = ConfigureParams.Screen.nMaxHeight;
	}
}
