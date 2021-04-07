/*
  Hatari - main.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_MAIN_H
#define HATARI_MAIN_H


/* Name and version for window title: */
#define PROG_NAME "Previous 0.5"

/* Messages for window title: */
#ifdef _WIN32
#define MOUSE_LOCK_MSG "Mouse is locked. Press left_ctrl-alt-m to release."
#elif __linux__
#define MOUSE_LOCK_MSG "Mouse is locked. Press shortcut-m to release."
#elif __APPLE__
#define MOUSE_LOCK_MSG "Mouse is locked. Press alt-m to release."
#else
#define MOUSE_LOCK_MSG "Mouse is locked. Press shortcut-m to release."
#endif

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

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

/* The 8 MHz CPU frequency */
#define CPU_FREQ   8012800

extern bool bQuitProgram;

extern bool Main_PauseEmulation(bool visualize);
extern bool Main_UnPauseEmulation(void);
extern void Main_RequestQuit(void);
extern void Main_SetRunVBLs(uint32_t vbls);
extern void Main_WaitOnVbl(void);
extern void Main_WarpMouse(int x, int y);
extern void Main_EventHandler(void);
extern void Main_SetTitle(const char *title);

#endif /* ifndef HATARI_MAIN_H */
