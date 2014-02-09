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
#include "kms.h"

#include "SDL.h"


void Keymap_Init(void) {
    if(ConfigureParams.Keyboard.bDisableKeyRepeat)
        SDL_EnableKeyRepeat(0, 0);
    else
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}

Uint8 translate_key(SDLKey sdlkey) {
    
    switch (sdlkey) {
        case SDLK_BACKSLASH: return 0x03;
        case SDLK_RIGHTBRACKET: return 0x04;
        case SDLK_LEFTBRACKET: return 0x05;
        case SDLK_i: return 0x06;
        case SDLK_o: return 0x07;
        case SDLK_p: return 0x08;
        case SDLK_LEFT: return 0x09;
        case SDLK_KP0: return 0x0B;
        case SDLK_KP_PERIOD: return 0x0C;
        case SDLK_KP_ENTER: return 0x0D;
        case SDLK_DOWN: return 0x0F;
        case SDLK_RIGHT: return 0x10;
        case SDLK_KP1: return 0x11;
        case SDLK_KP4: return 0x12;
        case SDLK_KP6: return 0x13;
        case SDLK_KP3: return 0x14;
        case SDLK_KP_PLUS: return 0x15;
        case SDLK_UP: return 0x16;
        case SDLK_KP2: return 0x17;
        case SDLK_KP5: return 0x18;
        case SDLK_BACKSPACE: return 0x1B;
        case SDLK_EQUALS: return 0x1C;
        case SDLK_MINUS: return 0x1D;
        case SDLK_8: return 0x1E;
        case SDLK_9: return 0x1F;
        case SDLK_0: return 0x20;
        case SDLK_KP7: return 0x21;
        case SDLK_KP8: return 0x22;
        case SDLK_KP9: return 0x23;
        case SDLK_KP_MINUS: return 0x24;
        case SDLK_KP_MULTIPLY: return 0x25;
        case SDLK_BACKQUOTE: return 0x26;
        case SDLK_KP_EQUALS: return 0x27;
        case SDLK_KP_DIVIDE: return 0x28;
        case SDLK_RETURN: return 0x2A;
        case SDLK_QUOTE: return 0x2B;
        case SDLK_SEMICOLON: return 0x2C;
        case SDLK_l: return 0x2D;
        case SDLK_COMMA: return 0x2E;
        case SDLK_PERIOD: return 0x2F;
        case SDLK_SLASH: return 0x30;
        case SDLK_z: return 0x31;
        case SDLK_x: return 0x32;
        case SDLK_c: return 0x33;
        case SDLK_v: return 0x34;
        case SDLK_b: return 0x35;
        case SDLK_m: return 0x36;
        case SDLK_n: return 0x37;
        case SDLK_SPACE: return 0x38;
        case SDLK_a: return 0x39;
        case SDLK_s: return 0x3A;
        case SDLK_d: return 0x3B;
        case SDLK_f: return 0x3C;
        case SDLK_g: return 0x3D;
        case SDLK_k: return 0x3E;
        case SDLK_j: return 0x3F;
        case SDLK_h: return 0x40;
        case SDLK_TAB: return 0x41;
        case SDLK_q: return 0x42;
        case SDLK_w: return 0x43;
        case SDLK_e: return 0x44;
        case SDLK_r: return 0x45;
        case SDLK_u: return 0x46;
        case SDLK_y: return 0x47;
        case SDLK_t: return 0x48;
        case SDLK_ESCAPE: return 0x49;
        case SDLK_1: return 0x4A;
        case SDLK_2: return 0x4B;
        case SDLK_3: return 0x4C;
        case SDLK_4: return 0x4D;
        case SDLK_7: return 0x4E;
        case SDLK_6: return 0x4F;
        case SDLK_5: return 0x50;
                        
            /* Special Keys */
        case SDLK_F10: return 0x58;
            
            
            /* Special keys not yet emulated:
             SOUND_UP_KEY           0x1A
             SOUND_DOWN_KEY         0x02
             BRIGHTNESS_UP_KEY      0x19
             BRIGHTNESS_DOWN_KEY	0x01
             POWER_KEY              0x58
             */
            
        default: return 0x00;
            break;
    }
}

Uint8 translate_modifiers(SDLMod modifiers) {

    Uint8 mod = 0x00;
    
    if (modifiers&KMOD_RMETA) {
        mod |= 0x01;
    }
    if (modifiers&KMOD_LMETA) {
        mod |= 0x01;
    }
    if (modifiers&KMOD_LSHIFT) {
        mod |= 0x02;
    }
    if (modifiers&KMOD_RSHIFT) {
        mod |= 0x04;
    }
    if (modifiers&KMOD_LCTRL) {
        mod |= 0x08;
    }
    if (modifiers&KMOD_RCTRL) {
        mod |= 0x10;
    }
    if (modifiers&KMOD_LALT) {
        mod |= 0x20;
    }
    if (modifiers&KMOD_RALT) {
        mod |= 0x40;
    }
    if (modifiers&KMOD_CAPS) {
        mod |= (0x02|0x04);
    }

    return mod;
}


/*-----------------------------------------------------------------------*/
/**
 * User pressed key down
 */
void Keymap_KeyDown(SDL_keysym *sdlkey)
{
    Log_Printf(LOG_WARN, "Key pressed: %s\n", SDL_GetKeyName(sdlkey->sym));
    
    int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;
    if (ShortCut_CheckKeys(modkey, symkey, 1)) // Check if we pressed a shortcut
        ShortCut_ActKey();
    
    Uint8 keycode = translate_key(symkey);
    Uint8 modifiers = translate_modifiers(modkey);
    
    Log_Printf(LOG_WARN, "Keycode: $%02x, Modifiers: $%02x\n", keycode, modifiers);
    
    kms_keydown(modifiers, keycode);
}


/*-----------------------------------------------------------------------*/
/**
 * User released key
 */
void Keymap_KeyUp(SDL_keysym *sdlkey)
{
    Log_Printf(LOG_WARN, "Key released: %s\n", SDL_GetKeyName(sdlkey->sym));
    
    int symkey = sdlkey->sym;
	int modkey = sdlkey->mod;
    if (ShortCut_CheckKeys(modkey, symkey, 0))
		return;
    
    Uint8 keycode = translate_key(symkey);
    Uint8 modifiers = translate_modifiers(modkey);
    
    Log_Printf(LOG_WARN, "Keycode: $%02x, Modkeys: $%02x\n", keycode, modifiers);
    
    kms_keyup(modifiers, keycode);
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
