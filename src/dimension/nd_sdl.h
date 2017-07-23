#include <SDL.h>
#include <SDL_thread.h>

#pragma once

#ifndef __ND_SDL_H__
#define __ND_SDL_H__

#ifdef __cplusplus
extern "C" {
#endif

    extern const int DISPLAY_VBL_MS;
    extern const int VIDEO_VBL_MS;
    extern const int BLANK_MS;

    void    nd_sdl_init(void);
    void    nd_sdl_uninit(void);
    void    nd_sdl_pause(bool pause);
    void    nd_sdl_destroy(void);
    void    nd_start_interrupts(void);
    void    nd_vbl_handler(void);
    void    nd_video_vbl_handler(void);

#ifdef __cplusplus
}
#endif
    
#endif /* __ND_SDL_H__ */
