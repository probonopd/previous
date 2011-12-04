/*
  Hatari - keymap.c

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  Here we process a key press and the remapping of the scancodes.
*/
const char Keymap_fileid[] = "Hatari keymap.c : " __DATE__ " " __TIME__;

#include <ctype.h>
#include "main.h"
#include "keymap.h"
#include "configuration.h"
#include "file.h"
#include "shortcut.h"
#include "str.h"
#include "screen.h"
#include "debugui.h"
#include "log.h"
#include "ioMem.h"
#include "m68000.h"

#include "SDL.h"


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


void Keymap_Init(void) {
    if(ConfigureParams.Keyboard.bDisableKeyRepeat)
        SDL_EnableKeyRepeat(0, 0);
    else
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}

Uint8 nextkeycode;
Uint8 modkeys;
Uint8 ralt;
Uint8 lalt;
Uint8 rcom;
Uint8 lcom;
Uint8 rshift;
Uint8 lshift;
Uint8 ctrl;


void Keyboard_Read0(void) {
//    Log_Printf(LOG_WARN, "Keyboard read at $%08x PC=$%08x\n", IoAccessCurrentAddress, m68k_getpc());
    IoMem[IoAccessCurrentAddress & 0x1FFFF]=0xA0;
}

void Keyboard_Read1(void) {
//    Log_Printf(LOG_WARN, "Keyboard read at $%08x PC=$%08x\n", IoAccessCurrentAddress, m68k_getpc());
    IoMem[IoAccessCurrentAddress & 0x1FFFF]=0xF0;
}

void Keyboard_Read2(void) {
//    Log_Printf(LOG_WARN, "Keyboard read at $%08x PC=$%08x\n", IoAccessCurrentAddress, m68k_getpc());
    IoMem[IoAccessCurrentAddress & 0x1FFFF]=0x13;
}

void Keycode_Read(void) {
//    Log_Printf(LOG_WARN, "Keycode read at $%08x PC=$%08x\n", IoAccessCurrentAddress, m68k_getpc());
    IoMem[0xe002]=0x93;
    IoMem[0xe008]=0x10;
    
    IoMem[0xe00a]=modkeys; // Set modifier Keys
    IoMem[0xe00b]=nextkeycode; // Press virtual Key
    nextkeycode=nextkeycode | 0x80; // Automatically release virtual Key
}


void KeyTranslator(SDL_keysym *sdlkey) { // Translate SDL Keys to NeXT Keycodes
    Log_Printf(LOG_WARN, "Key pressed: %s\n", SDL_GetKeyName(sdlkey->sym));

    int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;
    if (ShortCut_CheckKeys(modkey, symkey, 1)) // Check if we pressed a shortcut
        ShortCut_ActKey();
    else

    switch (sdlkey->sym) {
        
        case SDLK_RIGHTBRACKET: nextkeycode = 0x04;
            break;
        case SDLK_LEFTBRACKET: nextkeycode = 0x05;
            break;
        case SDLK_i: nextkeycode = 0x06;
            break;
        case SDLK_o: nextkeycode = 0x07;
            break;
        case SDLK_p: nextkeycode = 0x08;
            break;
        case SDLK_LEFT: nextkeycode = 0x09;
            break;
        case SDLK_KP0: nextkeycode = 0x0B;
            break;
        case SDLK_KP_PERIOD: nextkeycode = 0x0C;
            break;
        case SDLK_KP_ENTER: nextkeycode = 0x0D;
            break;
        case SDLK_DOWN: nextkeycode = 0x0F;
            break;
        case SDLK_RIGHT: nextkeycode = 0x10;
            break;
        case SDLK_KP1: nextkeycode = 0x11;
            break;
        case SDLK_KP4: nextkeycode = 0x12;
            break;
        case SDLK_KP6: nextkeycode = 0x13;
            break;
        case SDLK_KP3: nextkeycode = 0x14;
            break;
        case SDLK_KP_PLUS: nextkeycode = 0x15;
            break;
        case SDLK_UP: nextkeycode = 0x16;
            break;
        case SDLK_KP2: nextkeycode = 0x17;
            break;
        case SDLK_KP5: nextkeycode = 0x18;
            break;
        case SDLK_BACKSPACE: nextkeycode = 0x1B;
            break;
        case SDLK_EQUALS: nextkeycode = 0x1C;
            break;
        case SDLK_MINUS: nextkeycode = 0x1D;
            break;
        case SDLK_8: nextkeycode = 0x1E;
            break;
        case SDLK_9: nextkeycode = 0x1F;
            break;
        case SDLK_0: nextkeycode = 0x20;
            break;
        case SDLK_KP7: nextkeycode = 0x21;
            break;
        case SDLK_KP8: nextkeycode = 0x22;
            break;
        case SDLK_KP9: nextkeycode = 0x23;
            break;
        case SDLK_KP_MINUS: nextkeycode = 0x24;
            break;
        case SDLK_KP_MULTIPLY: nextkeycode = 0x25;
            break;
        case SDLK_BACKQUOTE: nextkeycode = 0x26;
            break;
        case SDLK_KP_EQUALS: nextkeycode = 0x27;
            break;
        case SDLK_KP_DIVIDE: nextkeycode = 0x28;
            break;            
        case SDLK_RETURN: nextkeycode = 0x2A;
            break;
        case SDLK_BACKSLASH: nextkeycode = 0x2B;
            break;
        case SDLK_SEMICOLON: nextkeycode = 0x2C;
            break;
        case SDLK_l: nextkeycode = 0x2D;
            break;
        case SDLK_COMMA: nextkeycode = 0x2E;
            break;
        case SDLK_PERIOD: nextkeycode = 0x2F;
            break;
        case SDLK_SLASH: nextkeycode = 0x30;
            break;
        case SDLK_z: nextkeycode = 0x31;
            break;
        case SDLK_x: nextkeycode = 0x32;
            break;
        case SDLK_c: nextkeycode = 0x33;
            break;
        case SDLK_v: nextkeycode = 0x34;
            break;
        case SDLK_b: nextkeycode = 0x35;
            break;
        case SDLK_m: nextkeycode = 0x36;
            break;
        case SDLK_n: nextkeycode = 0x37;
            break;
        case SDLK_SPACE: nextkeycode = 0x38;
            break;
        case SDLK_a: nextkeycode = 0x39;
            break;
        case SDLK_s: nextkeycode = 0x3A;
            break;
        case SDLK_d: nextkeycode = 0x3B;
            break;
        case SDLK_f: nextkeycode = 0x3C;
            break;            
        case SDLK_g: nextkeycode = 0x3D;
            break;
        case SDLK_k: nextkeycode = 0x3E;
            break;
        case SDLK_j: nextkeycode = 0x3F;
            break;
        case SDLK_h: nextkeycode = 0x40;
            break;
        case SDLK_TAB: nextkeycode = 0x41;
            break;
        case SDLK_q: nextkeycode = 0x42;
            break;
        case SDLK_w: nextkeycode = 0x43;
            break;
        case SDLK_e: nextkeycode = 0x44;
            break;
        case SDLK_r: nextkeycode = 0x45;
            break;
        case SDLK_u: nextkeycode = 0x46;
            break;
        case SDLK_y: nextkeycode = 0x47;
            break;
        case SDLK_t: nextkeycode = 0x48;
            break;
        case SDLK_ESCAPE: nextkeycode = 0x49;
            break;
        case SDLK_1: nextkeycode = 0x4A;
            break;
        case SDLK_2: nextkeycode = 0x4B;
            break;
        case SDLK_3: nextkeycode = 0x4C;
            break;
        case SDLK_4: nextkeycode = 0x4D;
            break;
        case SDLK_7: nextkeycode = 0x4E;
            break;
        case SDLK_6: nextkeycode = 0x4F;
            break;
        case SDLK_5: nextkeycode = 0x50;
            break;
            
        /* Modifier Keys */
        case SDLK_RMETA: ctrl = 0x01;
            break;
        case SDLK_LMETA: ctrl = 0x01;
            break;
        case SDLK_LSHIFT: lshift = 0x02;
            break;
        case SDLK_RSHIFT: rshift = 0x04;
            break;
        case SDLK_LCTRL: lcom = 0x08;
            break;
        case SDLK_RCTRL: rcom = 0x10;
            break;
        case SDLK_LALT: lalt = 0x20;
            break;
        case SDLK_RALT: ralt = 0x40;
            break;
        case SDLK_CAPSLOCK: lshift = 0x02, rshift = 0x04;
            break;
            
        /* Special keys not yet emulated:
         SOUND_UP_KEY           0x1A
         SOUND_DOWN_KEY         0x02
         BRIGHTNESS_UP_KEY      0x19
         BRIGHTNESS_DOWN_KEY	0x01
         POWER_KEY              0x58
         */

        default: break;
    }
    modkeys = 0x80 | ralt | lalt | rcom | lcom | rshift | lshift | ctrl;
    Log_Printf(LOG_WARN, "Modkeys: $%02x\n", modkeys);
}


void KeyRelease(SDL_keysym *sdlkey) { //release modifier Keys
    int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;
    if (ShortCut_CheckKeys(modkey, symkey, 0))
		return;

    switch (sdlkey->sym) {
            
        case SDLK_RMETA: ctrl = 0x00;
            break;
        case SDLK_LMETA: ctrl = 0x00;
            break;
        case SDLK_LSHIFT: lshift = 0x00;
            break;
        case SDLK_RSHIFT: rshift = 0x00;
            break;
        case SDLK_LCTRL: lcom = 0x00;
            break;
        case SDLK_RCTRL: rcom = 0x00;
            break;
        case SDLK_LALT: lalt = 0x00;
            break;
        case SDLK_RALT: ralt = 0x00;
            break;
        case SDLK_CAPSLOCK: lshift = 0x00, rshift = 0x00;
            break;
        default: break;
    }
    modkeys = 0x80 | ralt | lalt | rcom | lcom | rshift | lshift | ctrl;
    Log_Printf(LOG_WARN, "Modkeys: $%02x\n", modkeys);
}



/*-----------------------------------------------------------------------*/
/**
 * User press key down
 */
void Keymap_KeyDown(SDL_keysym *sdlkey)
{
	bool bPreviousKeyState;
	char STScanCode;
	int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;

	/*fprintf(stderr, "keydown: sym=%i scan=%i mod=$%x\n",symkey, sdlkey->scancode, modkey);*/

	if (ShortCut_CheckKeys(modkey, symkey, 1))
		return;

}


/*-----------------------------------------------------------------------*/
/**
 * User released key
 */
void Keymap_KeyUp(SDL_keysym *sdlkey)
{
	char STScanCode;
	int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;

	/*fprintf(stderr, "keyup: sym=%i scan=%i mod=$%x\n",symkey, sdlkey->scancode, modkey);*/

	/* Ignore short-cut keys here */

	if (ShortCut_CheckKeys(modkey, symkey, 0))
		return;


}

/*-----------------------------------------------------------------------*/
/**
 * Simulate press or release of a key corresponding to given character
 */
void Keymap_SimulateCharacter(char asckey, bool press)
{
	SDL_keysym sdlkey;

	sdlkey.mod = KMOD_NONE;
	sdlkey.scancode = 0;
	if (isupper(asckey)) {
		if (press) {
			sdlkey.sym = SDLK_LSHIFT;
			Keymap_KeyDown(&sdlkey);
		}
		sdlkey.sym = tolower(asckey);
		sdlkey.mod = KMOD_LSHIFT;
	} else {
		sdlkey.sym = asckey;
	}
	if (press) {
		Keymap_KeyDown(&sdlkey);
	} else {
		Keymap_KeyUp(&sdlkey);
		if (isupper(asckey)) {
			sdlkey.sym = SDLK_LSHIFT;
			Keymap_KeyUp(&sdlkey);
		}
	}
}
