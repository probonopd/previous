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


#define  LOG_KEYMAP_LEVEL   LOG_DEBUG

Uint8 modifiers = 0;
bool capslock = false;


void Keymap_Init(void) {

}


/* This function translates the scancodes provided by SDL to 
 * NeXT scancode values.
 */

static Uint8 Keymap_GetKeyFromScancode(SDL_Scancode sdlscancode) {
    Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Scancode: %i (%s)\n", sdlscancode, SDL_GetScancodeName(sdlscancode));

    switch (sdlscancode) {
        case SDL_SCANCODE_ESCAPE: return 0x49;
        case SDL_SCANCODE_1: return 0x4a;
        case SDL_SCANCODE_2: return 0x4b;
        case SDL_SCANCODE_3: return 0x4c;
        case SDL_SCANCODE_4: return 0x4d;
        case SDL_SCANCODE_5: return 0x50;
        case SDL_SCANCODE_6: return 0x4f;
        case SDL_SCANCODE_7: return 0x4e;
        case SDL_SCANCODE_8: return 0x1e;
        case SDL_SCANCODE_9: return 0x1f;
        case SDL_SCANCODE_0: return 0x20;
        case SDL_SCANCODE_MINUS: return 0x1d;
        case SDL_SCANCODE_EQUALS: return 0x1c;
        case SDL_SCANCODE_BACKSPACE: return 0x1b;
            
        case SDL_SCANCODE_TAB: return 0x41;
        case SDL_SCANCODE_Q: return 0x42;
        case SDL_SCANCODE_W: return 0x43;
        case SDL_SCANCODE_E: return 0x44;
        case SDL_SCANCODE_R: return 0x45;
        case SDL_SCANCODE_T: return 0x48;
        case SDL_SCANCODE_Y: return 0x47;
        case SDL_SCANCODE_U: return 0x46;
        case SDL_SCANCODE_I: return 0x06;
        case SDL_SCANCODE_O: return 0x07;
        case SDL_SCANCODE_P: return 0x08;
        case SDL_SCANCODE_LEFTBRACKET: return 0x05;
        case SDL_SCANCODE_RIGHTBRACKET: return 0x04;
        case SDL_SCANCODE_BACKSLASH: return 0x03;
			
        case SDL_SCANCODE_A: return 0x39;
        case SDL_SCANCODE_S: return 0x3a;
        case SDL_SCANCODE_D: return 0x3b;
        case SDL_SCANCODE_F: return 0x3c;
        case SDL_SCANCODE_G: return 0x3d;
        case SDL_SCANCODE_H: return 0x40;
        case SDL_SCANCODE_J: return 0x3f;
        case SDL_SCANCODE_K: return 0x3e;
        case SDL_SCANCODE_L: return 0x2d;
        case SDL_SCANCODE_SEMICOLON: return 0x2c;
        case SDL_SCANCODE_APOSTROPHE: return 0x2b;
        case SDL_SCANCODE_RETURN: return 0x2a;
            
        case SDL_SCANCODE_Z: return 0x31;
        case SDL_SCANCODE_X: return 0x32;
        case SDL_SCANCODE_C: return 0x33;
        case SDL_SCANCODE_V: return 0x34;
        case SDL_SCANCODE_B: return 0x35;
        case SDL_SCANCODE_N: return 0x37;
        case SDL_SCANCODE_M: return 0x36;
        case SDL_SCANCODE_COMMA: return 0x2e;
        case SDL_SCANCODE_PERIOD: return 0x2f;
        case SDL_SCANCODE_SLASH: return 0x30;
        case SDL_SCANCODE_SPACE: return 0x38;
            
        case SDL_SCANCODE_GRAVE:
        case SDL_SCANCODE_NUMLOCKCLEAR: return 0x26;
        case SDL_SCANCODE_KP_EQUALS: return 0x27;
        case SDL_SCANCODE_KP_DIVIDE: return 0x28;
        case SDL_SCANCODE_KP_MULTIPLY: return 0x25;
        case SDL_SCANCODE_KP_7: return 0x21;
        case SDL_SCANCODE_KP_8: return 0x22;
        case SDL_SCANCODE_KP_9: return 0x23;
        case SDL_SCANCODE_KP_MINUS: return 0x24;
        case SDL_SCANCODE_KP_4: return 0x12;
        case SDL_SCANCODE_KP_5: return 0x18;
        case SDL_SCANCODE_KP_6: return 0x13;
        case SDL_SCANCODE_KP_PLUS: return 0x15;
        case SDL_SCANCODE_KP_1: return 0x11;
        case SDL_SCANCODE_KP_2: return 0x17;
        case SDL_SCANCODE_KP_3: return 0x14;
        case SDL_SCANCODE_KP_0: return 0x0b;
        case SDL_SCANCODE_KP_PERIOD: return 0x0c;
        case SDL_SCANCODE_KP_ENTER: return 0x0d;
            
        case SDL_SCANCODE_LEFT: return 0x09;
        case SDL_SCANCODE_RIGHT: return 0x10;
        case SDL_SCANCODE_UP: return 0x16;
        case SDL_SCANCODE_DOWN: return 0x0f;
            
        /* Special keys */
        case SDL_SCANCODE_F10:
        case SDL_SCANCODE_DELETE: return 0x58;   /* Power */
        case SDL_SCANCODE_F5:
        case SDL_SCANCODE_END: return 0x02;      /* Sound down */
        case SDL_SCANCODE_F6:
        case SDL_SCANCODE_HOME: return 0x1a;     /* Sound up */
        case SDL_SCANCODE_F1:
        case SDL_SCANCODE_PAGEDOWN: return 0x01; /* Brightness down */
        case SDL_SCANCODE_F2:
        case SDL_SCANCODE_PAGEUP: return 0x19;   /* Brightness up */
            
        default: return 0x00;
    }
}


/* These functions translate the scancodes provided by SDL to
 * NeXT modifier bits.
 */

static Uint8 Keymap_Keydown_GetModFromScancode(SDL_Scancode sdlscancode) {
    switch (sdlscancode) {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            modifiers|=0x01;
            break;
        case SDL_SCANCODE_LSHIFT:
            modifiers|=0x02;
            break;
        case SDL_SCANCODE_RSHIFT:
            modifiers|=0x04;
            break;
        case SDL_SCANCODE_LGUI:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x20:0x08;
            break;
        case SDL_SCANCODE_RGUI:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x40:0x10;
            break;
        case SDL_SCANCODE_LALT:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x08:0x20;
            break;
        case SDL_SCANCODE_RALT:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x10:0x40;
            break;
        case SDL_SCANCODE_CAPSLOCK:
            capslock=capslock?false:true;
            break;
        default:
            break;
    }
    
    return modifiers|(capslock?0x02:0x00);
}

static Uint8 Keymap_Keyup_GetModFromScancode(SDL_Scancode sdlscancode) {
    
    switch (sdlscancode) {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            modifiers&=~0x01;
            break;
        case SDL_SCANCODE_LSHIFT:
            modifiers&=~0x02;
            break;
        case SDL_SCANCODE_RSHIFT:
            modifiers&=~0x04;
            break;
        case SDL_SCANCODE_LGUI:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x20:0x08);
            break;
        case SDL_SCANCODE_RGUI:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x40:0x10);
            break;
        case SDL_SCANCODE_LALT:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x08:0x20);
            break;
        case SDL_SCANCODE_RALT:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x10:0x40);
            break;
        case SDL_SCANCODE_CAPSLOCK:
            //capslock=false;
            break;
        default:
            break;
    }
    
    return modifiers|(capslock?0x02:0x00);
}


/* This function translates the key symbols provided by SDL to
 * NeXT scancode values.
 */

static Uint8 Keymap_GetKeyFromSymbol(SDL_Keycode sdlkey) {
    Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] Symkey: %s\n", SDL_GetKeyName(sdlkey));
    
    switch (sdlkey) {
        case SDLK_BACKSLASH: return 0x03;
        case SDLK_RIGHTBRACKET: return 0x04;
        case SDLK_LEFTBRACKET: return 0x05;
        case SDLK_i: return 0x06;
        case SDLK_o: return 0x07;
        case SDLK_p: return 0x08;
        case SDLK_LEFT: return 0x09;
        case SDLK_KP_0: return 0x0B;
        case SDLK_KP_PERIOD: return 0x0C;
        case SDLK_KP_ENTER: return 0x0D;
        case SDLK_DOWN: return 0x0F;
        case SDLK_RIGHT: return 0x10;
        case SDLK_KP_1: return 0x11;
        case SDLK_KP_4: return 0x12;
        case SDLK_KP_6: return 0x13;
        case SDLK_KP_3: return 0x14;
        case SDLK_KP_PLUS: return 0x15;
        case SDLK_UP: return 0x16;
        case SDLK_KP_2: return 0x17;
        case SDLK_KP_5: return 0x18;
        case SDLK_BACKSPACE: return 0x1B;
        case SDLK_EQUALS: return 0x1C;
        case SDLK_MINUS: return 0x1D;
        case SDLK_8: return 0x1E;
        case SDLK_9: return 0x1F;
        case SDLK_0: return 0x20;
        case SDLK_KP_7: return 0x21;
        case SDLK_KP_8: return 0x22;
        case SDLK_KP_9: return 0x23;
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
        case SDLK_F10:
        case SDLK_DELETE: return 0x58;   /* Power */
        case SDLK_F5:
        case SDLK_END: return 0x02;      /* Sound down */
        case SDLK_F6:
        case SDLK_HOME: return 0x1a;     /* Sound up */
        case SDLK_F1:
        case SDLK_PAGEDOWN: return 0x01; /* Brightness down */
        case SDLK_F2:
        case SDLK_PAGEUP: return 0x19;   /* Brightness up */
            
            
        default: return 0x00;
            break;
    }
}


/* These functions translate the key symbols provided by SDL to
 * NeXT modifier bits.
 */

static Uint8 Keymap_Keydown_GetModFromSymbol(SDL_Keycode sdl_modifier) {
    
    switch (sdl_modifier) {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            modifiers|=0x01;
            break;
        case SDLK_LSHIFT:
            modifiers|=0x02;
            break;
        case SDLK_RSHIFT:
            modifiers|=0x04;
            break;
        case SDLK_LGUI:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x20:0x08;
            break;
        case SDLK_RGUI:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x40:0x10;
            break;
        case SDLK_LALT:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x08:0x20;
            break;
        case SDLK_RALT:
            modifiers|=ConfigureParams.Keyboard.bSwapCmdAlt?0x10:0x40;
            break;
        case SDLK_CAPSLOCK:
            capslock=capslock?false:true;
            break;
        default:
            break;
    }
    
    return modifiers|(capslock?0x02:0x00);
}

static Uint8 Keymap_Keyup_GetModFromSymbol(SDL_Keycode sdl_modifier) {
    
    switch (sdl_modifier) {
        case SDLK_LCTRL:
        case SDLK_RCTRL:
            modifiers&=~0x01;
            break;
        case SDLK_LSHIFT:
            modifiers&=~0x02;
            break;
        case SDLK_RSHIFT:
            modifiers&=~0x04;
            break;
        case SDLK_LGUI:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x20:0x08);
            break;
        case SDLK_RGUI:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x40:0x10);
            break;
        case SDLK_LALT:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x08:0x20);
            break;
        case SDLK_RALT:
            modifiers&=~(ConfigureParams.Keyboard.bSwapCmdAlt?0x10:0x40);
            break;
        case SDLK_CAPSLOCK:
            //capslock=false;
            break;
        default:
            break;
    }
    
    return modifiers|(capslock?0x02:0x00);
}


/*-----------------------------------------------------------------------*/
/**
 * Mouse wheel mapped to cursor keys (currently disabled)
 */

static bool pendingX = true;
static bool pendingY = true;

static void post_key_event(int sym, int scan) {
    SDL_Event sdlevent;
    sdlevent.type = SDL_KEYDOWN;
    sdlevent.key.keysym.sym      = sym;
    sdlevent.key.keysym.scancode = scan;
    SDL_PushEvent(&sdlevent);
    sdlevent.type = SDL_KEYUP;
    sdlevent.key.keysym.sym      = sym;
    sdlevent.key.keysym.scancode = scan;
    SDL_PushEvent(&sdlevent);
}

void Keymap_MouseWheel(SDL_MouseWheelEvent* event) {
    if(!(pendingX)) {
        pendingX = true;
        if     (event->x > 0) post_key_event(SDLK_LEFT,  SDL_SCANCODE_LEFT);
        else if(event->x < 0) post_key_event(SDLK_RIGHT, SDL_SCANCODE_RIGHT);
    }
    
    if(!(pendingY)) {
        pendingY = true;
        if     (event->y < 0) post_key_event(SDLK_UP,   SDL_SCANCODE_UP);
        else if(event->y > 0) post_key_event(SDLK_DOWN, SDL_SCANCODE_DOWN);
    }
}


/*-----------------------------------------------------------------------*/
/**
 * User pressed key down
 */
void Keymap_KeyDown(SDL_Keysym *sdlkey)
{
    Uint8 next_mod, next_key;

    if (ShortCut_CheckKeys(sdlkey->mod, sdlkey->sym, 1)) { // Check if we pressed a shortcut
        ShortCut_ActKey();
        return;
    }
    
    if (ConfigureParams.Keyboard.nKeymapType==KEYMAP_SYMBOLIC) {
        next_key = Keymap_GetKeyFromSymbol(sdlkey->sym);
        next_mod = Keymap_Keydown_GetModFromSymbol(sdlkey->sym);
    } else {
        next_key = Keymap_GetKeyFromScancode(sdlkey->scancode);
        next_mod = Keymap_Keydown_GetModFromScancode(sdlkey->scancode);
    }
    
    Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] NeXT Keycode: $%02x, Modifiers: $%02x\n", next_key, next_mod);
    
    kms_keydown(next_mod, next_key);
}


/*-----------------------------------------------------------------------*/
/**
 * User released key
 */
void Keymap_KeyUp(SDL_Keysym *sdlkey) {
    Uint8 next_mod, next_key;

    if (ShortCut_CheckKeys(sdlkey->mod, sdlkey->sym, 0))
		return;
    
    if (ConfigureParams.Keyboard.nKeymapType==KEYMAP_SYMBOLIC) {
        next_key = Keymap_GetKeyFromSymbol(sdlkey->sym);
        next_mod = Keymap_Keyup_GetModFromSymbol(sdlkey->sym);
    } else {
        next_key = Keymap_GetKeyFromScancode(sdlkey->scancode);
        next_mod = Keymap_Keyup_GetModFromScancode(sdlkey->scancode);
    }
    
    Log_Printf(LOG_KEYMAP_LEVEL, "[Keymap] NeXT Keycode: $%02x, Modifiers: $%02x\n", next_key, next_mod);
    
    kms_keyup(next_mod, next_key);
}

/*-----------------------------------------------------------------------*/
/**
 * Simulate press or release of a key corresponding to given character
 */
void Keymap_SimulateCharacter(char asckey, bool press)
{
	SDL_Keysym sdlkey;

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


/*-----------------------------------------------------------------------*/
/**
 * User moved mouse
 */
void Keymap_MouseMove(int dx, int dy, float lin, float exp)
{
    static bool s_left=false;
    static bool s_up=false;
    static float s_fdx=0.0;
    static float s_fdy=0.0;
    
    bool left=false;
    bool up=false;
    float fdx;
    float fdy;
    
    if ((dx!=0) || (dy!=0)) {
        /* Remove the sign */
        if (dx<0) {
            dx=-dx;
            left=true;
        }
        if (dy<0) {
            dy=-dy;
            up=true;
        }
        
        /* Exponential adjustmend */
        fdx = pow(dx, exp);
        fdy = pow(dy, exp);
        
        /* Linear adjustment */
        fdx *= lin;
        fdy *= lin;
        
        /* Add residuals */
        if (left==s_left) {
            s_fdx+=fdx;
        } else {
            s_fdx=fdx;
            s_left=left;
        }
        if (up==s_up) {
            s_fdy+=fdy;
        } else {
            s_fdy=fdy;
            s_up=up;
        }
        
        /* Convert to integer and save residuals */
        dx=s_fdx;
        s_fdx-=dx;
        dy=s_fdy;
        s_fdy-=dy;
        //printf("adjusted: dx=%i, dy=%i\n",dx,dy);
        kms_mouse_move(dx, left, dy, up);
    }
}

/*-----------------------------------------------------------------------*/
/**
 * User pressed mouse button
 */
void Keymap_MouseDown(bool left)
{
    kms_mouse_button(left,true);
}

/*-----------------------------------------------------------------------*/
/**
 * User released mouse button
 */
void Keymap_MouseUp(bool left)
{
    kms_mouse_button(left,false);
}
