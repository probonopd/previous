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

extern volatile bool bGrabMouse;
extern volatile bool bInFullScreen;
extern struct SDL_Window *sdlWindow;
extern SDL_Surface *sdlscrn;

void Screen_Init(void);
void Screen_UnInit(void);
void Screen_Pause(bool pause);
void Screen_EnterFullScreen(void);
void Screen_ReturnFromFullScreen(void);
void Screen_ModeChanged(void);
bool Update_StatusBar(void);
void SDL_UpdateRects(SDL_Surface *screen, int numrects, SDL_Rect *rects);
void SDL_UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h);
void blitDimension(SDL_Texture* tex);

#endif  /* ifndef HATARI_SCREEN_H */
