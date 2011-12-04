/*
  Hatari - keymap.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_KEYMAP_H
#define HATARI_KEYMAP_H

#include <SDL_keyboard.h>
#include "SDL.h"

extern void Keymap_Init(void);
extern char Keymap_RemapKeyToSTScanCode(SDL_keysym* pKeySym);
extern void Keymap_LoadRemapFile(char *pszFileName);
extern void Keymap_DebounceAllKeys(void);
extern void Keymap_KeyDown(SDL_keysym *sdlkey);
extern void Keymap_KeyUp(SDL_keysym *sdlkey);
extern void Keymap_SimulateCharacter(char asckey, bool press);
void Keyboard_Read0(void);
void Keyboard_Read1(void);
void Keyboard_Read2(void);
void Keycode_Read(void);
void KeyTranslator(SDL_keysym *sdlkey);
void KeyRelease(SDL_keysym *sdlkey);

#endif
