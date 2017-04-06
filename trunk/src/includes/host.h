/*
 Host system dependencies such as (real)time synchronizaton and events that
 occur on host threads (e.g. VBL & timers) which need to be synchronized
 to Previous CPU threads.
 */

#pragma once

#ifndef __HOST_H__
#define __HOST_H__

#include <SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
#define REALTIME_INT_LVL 1

#ifdef __MINGW32__
#define FMT_ll "I64"
#define FMT_zu "u"
#else
#define FMT_ll "ll"
#define FMT_zu "zu"
#endif

    enum {
        MAIN_DISPLAY,
        ND_DISPLAY,
        ND_VIDEO,
    };
    
    typedef SDL_SpinLock       lock_t;
    typedef SDL_Thread         thread_t;
    typedef SDL_ThreadFunction thread_func_t;

    void        host_reset(void);
    void        host_realtime(bool state);
    void        host_blank(int slot, int src, bool state);
    bool        host_blank_state(int slot, int src);
    Uint64      host_time_us(void);
    Uint32      host_time_ms(void);
    double      host_time_sec(void);
    void        host_time(double* realTime, double* hostTime);
    time_t      host_unix_time(void);
    void        host_set_unix_time(time_t now);
    void        host_sleep_sec(double sec);
    void        host_sleep_ms(Uint32 ms);
    void        host_sleep_us(Uint64 us);
    int         host_num_cpus(void);
    void        host_hardclock(int expected, int actual);
    double      host_real_time_offset(void);
    void        host_pause_time(bool pausing);
    const char* host_report(double realTime, double hostTime);
    void        host_darkmatter(bool state);
    
    void        host_lock(lock_t* lock);
    void        host_unlock(lock_t* lock);
    int         host_trylock(lock_t* lock);
    thread_t*   host_thread_create(thread_func_t, void* data);
    int         host_thread_wait(thread_t* thread);

#ifdef __cplusplus
}
#endif

#endif /* __HOST_H__ */
