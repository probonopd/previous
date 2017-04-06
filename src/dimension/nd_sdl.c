#include "main.h"
#include "nd_sdl.h"
#include "configuration.h"
#include "dimension.h"
#include "screen.h"
#include "host.h"
#include "cycInt.h"

/* Because of SDL time (in)accuracy, timing is very approximative */
const int DISPLAY_VBL_MS = 1000 / 68; // main display at 68Hz, actually this is 71.42 Hz because (int)1000/(int)68Hz=14ms
const int VIDEO_VBL_MS   = 1000 / 60; // NTSC display at 60Hz, actually this is 62.5 Hz because (int)1000/(int)60Hz=16ms
const int BLANK_MS       = 2;         // Give some blank time for both

static volatile bool doRepaint     = true;
static SDL_Thread*   repaintThread = NULL;
static SDL_Window*   ndWindow      = NULL;
static SDL_Renderer* ndRenderer    = NULL;

void blitDimension(SDL_Texture* tex);

static int repainter(void* unused) {
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
    
    SDL_Texture*  ndTexture  = NULL;
    
    SDL_Rect r = {0,0,1120,832};
    
    ndRenderer = SDL_CreateRenderer(ndWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!ndRenderer) {
        fprintf(stderr,"[ND] Failed to create renderer!\n");
        exit(-1);
    }
    
    SDL_RenderSetLogicalSize(ndRenderer, r.w, r.h);
    ndTexture = SDL_CreateTexture(ndRenderer, SDL_PIXELFORMAT_UNKNOWN, SDL_TEXTUREACCESS_STREAMING, r.w, r.h);
    
    while(doRepaint) {
        blitDimension(ndTexture);
        SDL_RenderCopy(ndRenderer, ndTexture, NULL, NULL);
        SDL_RenderPresent(ndRenderer);
    }

    SDL_DestroyTexture(ndTexture);
    SDL_DestroyRenderer(ndRenderer);
    SDL_DestroyWindow(ndWindow);

    return 0;
}

static bool ndVBLtoggle;
void nd_vbl_handler() {
    CycInt_AcknowledgeInterrupt();
    
    host_blank(ND_SLOT, ND_DISPLAY, ndVBLtoggle);
    ndVBLtoggle = !ndVBLtoggle;
    
    CycInt_AddRelativeInterruptUs((1000*1000)/136, INTERRUPT_ND_VBL); // 136Hz with toggle gives 68Hz, blank time is 1/2 frame time
}

bool ndVideoVBLtoggle;
void nd_video_vbl_handler() {
    CycInt_AcknowledgeInterrupt();
    
    host_blank(ND_SLOT, ND_VIDEO, ndVideoVBLtoggle);
    ndVideoVBLtoggle = !ndVideoVBLtoggle;
    
    CycInt_AddRelativeInterruptUs((1000*1000)/120, INTERRUPT_ND_VIDEO_VBL); // 120Hz with toggle gives 60Hz NTSC, blank time is 1/2 frame time
}

void nd_sdl_init() {
    if(!(repaintThread)) {
        int x, y, w, h;
        SDL_GetWindowPosition(sdlWindow, &x, &y);
        SDL_GetWindowSize(sdlWindow, &w, &h);
        ndWindow   = SDL_CreateWindow("NeXTdimension",(x-w)+1, y, 1120, 832, SDL_WINDOW_HIDDEN);
        
        if (!ndWindow) {
            fprintf(stderr,"[ND] Failed to create window!\n");
            exit(-1);
        }
    }
    
    if(ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL) {
        SDL_ShowWindow(ndWindow);
    } else {
        SDL_HideWindow(ndWindow);
    }
}

void nd_start_interrupts() {
    if(!(repaintThread) && ConfigureParams.Screen.nMonitorType == MONITOR_TYPE_DUAL)
        repaintThread = SDL_CreateThread(repainter, "[ND] repainter", NULL);
    
    // if this is a cube and we have an ND configured, install ND VBL handlers
    if (ConfigureParams.Dimension.bEnabled && (ConfigureParams.System.nMachineType == NEXT_CUBE030 || ConfigureParams.System.nMachineType == NEXT_CUBE040)) {
        CycInt_AddRelativeInterruptUs(1000, INTERRUPT_ND_VBL);
        CycInt_AddRelativeInterruptUs(1000, INTERRUPT_ND_VIDEO_VBL);
    }
}

void nd_sdl_uninit() {
    SDL_HideWindow(ndWindow);
}

void nd_sdl_destroy() {
    doRepaint = false; // stop repaint thread
    int s;
    SDL_WaitThread(repaintThread, &s);
    nd_sdl_uninit();
}

