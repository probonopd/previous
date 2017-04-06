/*
  Hatari - keymap.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

//#ifndef HATARI_KEYMAP_H
//#define HATARI_KEYMAP_H

#include <SDL_keyboard.h>
#include "SDL.h"


void Keymap_Init(void);
char Keymap_RemapKeyToSTScanCode(SDL_Keysym* pKeySym);
void Keymap_LoadRemapFile(char *pszFileName);
void Keymap_DebounceAllKeys(void);
void Keymap_KeyDown(SDL_Keysym *sdlkey);
void Keymap_MouseWheel(SDL_MouseWheelEvent* event);
void Keymap_KeyUp(SDL_Keysym *sdlkey);
void Keymap_SimulateCharacter(char asckey, bool press);

void Keymap_MouseMove(int dx, int dy, float lin, float exp);
void Keymap_MouseDown(bool left);
void Keymap_MouseUp(bool left);

/* Definitions for NeXT scancodes */

/*
 
 Byte at address 0x0200e00a contains modifier key values:
 
 x--- ----  valid
 -x-- ----  alt_right
 --x- ----  alt_left
 ---x ----  command_right
 ---- x---  command_left
 ---- -x--  shift_right
 ---- --x-  shift_left
 ---- ---x  control
 
 
 Byte at address 0x0200e00b contains key values:
 
 x--- ----  key up (1) or down (0)
 -xxx xxxx  key_code
 
 */


#define NEXTKEY_NONE            0x00
#define NEXTKEY_BRGHTNESS_DOWN  0x01
#define NEXTKEY_VOLUME_DOWN     0x02
#define NEXTKEY_BACKSLASH       0x03
#define NEXTKEY_CLOSEBRACKET    0x04
#define NEXTKEY_OPENBRACKET     0x05
#define NEXTKEY_i               0x06
#define NEXTKEY_o               0x07
#define NEXTKEY_p               0x08
#define NEXTKEY_LEFT_ARROW      0x09
/* missing */
#define NEXTKEY_KEYPAD_0        0x0B
#define NEXTKEY_KEYPAD_PERIOD   0x0C
#define NEXTKEY_KEYPAD_ENTER    0x0D
/* missing */
#define NEXTKEY_DOWN_ARROW      0x0F
#define NEXTKEY_RIGHT_ARROW     0x10
#define NEXTKEY_KEYPAD_1        0x11
#define NEXTKEY_KEYPAD_4        0x12
#define NEXTKEY_KEYPAD_6        0x13
#define NEXTKEY_KEYPAD_3        0x14
#define NEXTKEY_KEYPAD_PLUS     0x15
#define NEXTKEY_UP_ARROW        0x16
#define NEXTKEY_KEYPAD_2        0x17
#define NEXTKEY_KEYPAD_5        0x18
#define NEXTKEY_BRIGHTNESS_UP   0x19
#define NEXTKEY_VOLUME_UP       0x1A
#define NEXTKEY_DELETE          0x1B
#define NEXTKEY_EQUALS          0x1C
#define NEXTKEY_MINUS           0x1D
#define NEXTKEY_8               0x1E
#define NEXTKEY_9               0x1F
#define NEXTKEY_0               0x20
#define NEXTKEY_KEYPAD_7        0x21
#define NEXTKEY_KEYPAD_8        0x22
#define NEXTKEY_KEYPAD_9        0x23
#define NEXTKEY_KEYPAD_MINUS    0x24
#define NEXTKEY_KEYPAD_MULTIPLY 0x25
#define NEXTKEY_BACKQUOTE       0x26
#define NEXTKEY_KEYPAD_EQUALS   0x27
#define NEXTKEY_KEYPAD_DIVIDE   0x28
/* missing */
#define NEXTKEY_RETURN          0x2A
#define NEXTKEY_QUOTE           0x2B
#define NEXTKEY_SEMICOLON       0x2C
#define NEXTKEY_l               0x2D
#define NEXTKEY_COMMA           0x2E
#define NEXTKEY_PERIOD          0x2F
#define NEXTKEY_SLASH           0x30
#define NEXTKEY_z               0x31
#define NEXTKEY_x               0x32
#define NEXTKEY_c               0x33
#define NEXTKEY_v               0x34
#define NEXTKEY_b               0x35
#define NEXTKEY_m               0x36
#define NEXTKEY_n               0x37
#define NEXTKEY_SPACE           0x38
#define NEXTKEY_a               0x39
#define NEXTKEY_s               0x3A
#define NEXTKEY_d               0x3B
#define NEXTKEY_f               0x3C
#define NEXTKEY_g               0x3D
#define NEXTKEY_k               0x3E
#define NEXTKEY_j               0x3F
#define NEXTKEY_h               0x40
#define NEXTKEY_TAB             0x41
#define NEXTKEY_q               0x42
#define NEXTKEY_w               0x43
#define NEXTKEY_e               0x44
#define NEXTKEY_r               0x45
#define NEXTKEY_u               0x46
#define NEXTKEY_y               0x47
#define NEXTKEY_t               0x48
#define NEXTKEY_ESC             0x49
#define NEXTKEY_1               0x4A
#define NEXTKEY_2               0x4B
#define NEXTKEY_3               0x4C
#define NEXTKEY_4               0x4D
#define NEXTKEY_7               0x4E
#define NEXTKEY_6               0x4F
#define NEXTKEY_5               0x50
#define NEXTKEY_POWER           0x58

#define NEXTKEY_MOD_META        0x01
#define NEXTKEY_MOD_LSHIFT      0x02
#define NEXTKEY_MOD_RSHIFT      0x04
#define NEXTKEY_MOD_LCTRL       0x08
#define NEXTKEY_MOD_RCTRL       0x10
#define NEXTKEY_MOD_LALT        0x20
#define NEXTKEY_MOD_RALT        0x40


//#endif
