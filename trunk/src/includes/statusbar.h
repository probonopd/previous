/*
  Hatari - statusbar.h
  
  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
#ifndef HATARI_STATUSBAR_H
#define HATARI_STATUSBAR_H

#include <SDL.h>
#include "main.h"

typedef enum {
	DEVICE_LED_ENET,
	DEVICE_LED_OD,
	DEVICE_LED_SCSI,
    DEVICE_LED_FD,
    NUM_DEVICE_LEDS
} drive_index_t;

int Statusbar_SetHeight(int ScreenWidth, int ScreenHeight);
int Statusbar_GetHeightForSize(int width, int height);
int Statusbar_GetHeight(void);
void Statusbar_BlinkLed(drive_index_t drive);
void Statusbar_SetSystemLed(bool state);
void Statusbar_SetDspLed(bool state);
void Statusbar_SetNdLed(int state);

void Statusbar_Init(SDL_Surface *screen);
void Statusbar_UpdateInfo(void);
void Statusbar_AddMessage(const char *msg, Uint32 msecs);
void Statusbar_OverlayBackup(SDL_Surface *screen);
void Statusbar_Update(SDL_Surface *screen);
void Statusbar_OverlayRestore(SDL_Surface *screen);

#endif /* HATARI_STATUSBAR_H */
