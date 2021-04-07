/*
  Hatari - compat.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains all the includes and defines specific to windows (such as TCHAR)
  needed by WinUae CPU core. 
  The aim is to have minimum changes in WinUae CPU core for next updates
*/

#ifndef HATARI_COMPAT_H
#define HATARI_COMPAT_H

#include <stdbool.h>

#include "sysconfig.h"

/* This define is here to remove some Amiga specific code when compiling */
/* It results in ' #if 0 ' code in newcpu.c code */
#define AMIGA_ONLY 0

/* this defione is here for newcpu.c compatibility.
 * In WinUae, it's defined in debug.h" */
#ifndef MAX_LINEWIDTH
#define MAX_LINEWIDTH 100
#endif

#define RTAREA_DEFAULT 0xf00000

/* Laurent */
/* here only to allow newcpu.c to compile */
/* Should be removed when newcpu.c 'll be relooked for hatari only*/

// Laurent : I don't know what to do with these variables, so, for now, I've put them here !!!
int kickstart_rom, cloanto_rom;
int config_changed;
int vpos;
int quit_program;  // declared as "int quit_program = 0;" in main.c 
//WinUae ChangeLog: Improve quitting/resetting behaviour: Move quit_program from GUI
//WinUae ChangeLog: quit_program is now handled in vsync_handler() and m68k_go().

#ifndef REGPARAM
#define REGPARAM
#endif

#ifndef REGPARAM2
#define REGPARAM2
#endif

#ifndef REGPARAM3
#define REGPARAM3
#endif

#ifndef TCHAR
#define TCHAR char
#endif

#ifndef STATIC_INLINE
#define STATIC_INLINE static inline
#endif

#define _vsnprintf vsnprintf
#define _tcsncmp strncmp
#define _istspace isspace
#define _tcscmp strcmp
#define _tcslen strlen
#define _tcsstr strstr
#define _tcscpy strcpy
#define _tcsncpy strncpy
#define _tcscat strcat
#define _stprintf sprintf

#define _vsntprintf printf

#define f_out fprintf

#endif
