/*
  Hatari - screen.h

  This file is distributed under the GNU Public License, version 2 or at your
  option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_SCREEN_H
#define HATARI_SCREEN_H

#include <SDL_video.h>    /* for SDL_Surface */

#if 1
void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h);
void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects);
#endif

/* The 'screen' is a representation of the ST video memory	*/
/* taking into account all the border tricks. Data are stored	*/
/* in 'planar' format (1 word per plane) and are then converted	*/
/* to an SDL buffer that will be displayed.			*/
/* So, all video lines are converted to a unique line of	*/
/* SCREENBYTES_LINE bytes in planar format.			*/
/* SCREENBYTES_LINE should always be a multiple of 8.		*/

/* left/right borders must be multiple of 8 bytes */
#define SCREENBYTES_LEFT    (nBorderPixelsLeft/2)  /* Bytes for left border */
#define SCREENBYTES_MIDDLE  160                    /* Middle (320 pixels) */
#define SCREENBYTES_RIGHT   (nBorderPixelsRight/2) /* Right border */
#define SCREENBYTES_LINE    (SCREENBYTES_LEFT+SCREENBYTES_MIDDLE+SCREENBYTES_RIGHT)
#define SCREENBYTES_MONOLINE 80         /* Byte per line in ST-high resolution */



/* Frame buffer, used to store details in screen conversion */
typedef struct
{
  Uint8 *pNEXTScreen;             /* Copy of screen built up during frame (copy each line on HBL to simulate monitor raster) */
  Uint8 *pNEXTScreenCopy;         /* Previous frames copy of above  */
  bool bFullUpdate;             /* Set TRUE to cause full update on next draw */
} FRAMEBUFFER;

/* Number of frame buffers (1 or 2) - should be 2 for supporting screen flipping */
#define NUM_FRAMEBUFFERS  2


/* ST/TT resolution defines */
enum
{
  ST_HIGH_RES,
};
#define ST_MEDIUM_RES_BIT 0x1
#define ST_RES_MASK 0x3


/* Palette mask values for 'HBLPaletteMask[]' */
#define PALETTEMASK_RESOLUTION  0x00040000
#define PALETTEMASK_PALETTE     0x0000ffff
#define PALETTEMASK_UPDATERES   0x20000000
#define PALETTEMASK_UPDATEPAL   0x40000000
#define PALETTEMASK_UPDATEFULL  0x80000000
#define PALETTEMASK_UPDATEMASK  (PALETTEMASK_UPDATEFULL|PALETTEMASK_UPDATEPAL|PALETTEMASK_UPDATERES)

/* Overscan values */
enum
{
  OVERSCANMODE_NONE,     /* 0x00 */
  OVERSCANMODE_TOP,      /* 0x01 */
  OVERSCANMODE_BOTTOM    /* 0x02 (Top+Bottom) 0x03 */
};

extern bool bGrabMouse;
extern bool bInFullScreen;
extern int nScreenZoomX, nScreenZoomY;
extern int nBorderPixelsLeft, nBorderPixelsRight;
extern int NEXTScreenStartHorizLine;
extern int NEXTScreenLeftSkipBytes;
extern FRAMEBUFFER *pFrameBuffer;
extern Uint8 pNEXTScreen[(1120*832)*2];
extern struct SDL_Window *sdlWindow;
extern SDL_Surface *sdlscrn;
extern Uint32 STRGBPalette[16];
extern Uint32 ST2RGB[4096];

extern void Screen_Init(void);
extern void Screen_UnInit(void);
extern void Screen_Reset(void);
extern void Screen_SetFullUpdate(void);
extern void Screen_EnterFullScreen(void);
extern void Screen_ReturnFromFullScreen(void);
extern void Screen_ModeChanged(void);
extern bool Screen_Draw(void);

#endif  /* ifndef HATARI_SCREEN_H */
