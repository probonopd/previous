/*
  Hatari - main.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_MAIN_H
#define HATARI_MAIN_H


/* Name and version for window title: */
#define PROG_NAME "Previous 1.6"

/* Messages for window title: */
#ifdef _WIN32
#define MOUSE_LOCK_MSG "Mouse is locked. Press right_ctrl-alt-m to release."
#elif __linux__
#define MOUSE_LOCK_MSG "Mouse is locked. Press right_ctrl-alt-m to release."
#elif __APPLE__
#define MOUSE_LOCK_MSG "Mouse is locked. Press ctrl-alt-m to release."
#else
#define MOUSE_LOCK_MSG "Mouse is locked. Press shortcut-m to release."
#endif

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include <SDL.h>
#include <stdbool.h>

#if __GNUC__ >= 3
# define likely(x)      __builtin_expect (!!(x), 1)
# define unlikely(x)    __builtin_expect (!!(x), 0)
#else
# define likely(x)      (x)
# define unlikely(x)    (x)
#endif

#ifdef WIN32
#define PATHSEP '\\'
#else
#define PATHSEP '/'
#endif

#define CALL_VAR(func)  { ((void(*)(void))func)(); }

#define ARRAYSIZE(x) (int)(sizeof(x)/sizeof(x[0]))

/* 68000 operand sizes */
#define SIZE_BYTE  1
#define SIZE_WORD  2
#define SIZE_LONG  4

enum {
    PAUSE_NONE,
    PAUSE_EMULATION,
    UNPAUSE_EMULATION,
};

/* Flag for pausing m68k thread (used by i860 debugger) */
extern volatile int mainPauseEmulation;

extern bool bQuitProgram;

bool Main_PauseEmulation(bool visualize);
bool Main_UnPauseEmulation(void);
void Main_RequestQuit(void);
void Main_WarpMouse(int x, int y);
void Main_EventHandler(void);
void Main_EventHandlerInterrupt(void);
void Main_SetTitle(const char *title);
void Main_SpeedReset(void);
const char* Main_SpeedMsg(void);

#endif /* ifndef HATARI_MAIN_H */
