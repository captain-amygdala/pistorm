/*
  Hatari - sysconfig.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.

  This file contains needed auto generated includes and defines needed by WinUae CPU core. 
  The aim is to have minimum changes in WinUae CPU core for next updates
*/

#ifndef HATARI_SYSCONFIG_H
#define HATARI_SYSCONFIG_H

#define SUPPORT_THREADS
#define MAX_DPATH 1000

//#define X86_MSVC_ASSEMBLY
//#define X86_MSVC_ASSEMBLY_MEMACCESS
#define OPTIMIZED_FLAGS
//#define __i386__

#ifndef UAE_MINI

//#define DEBUGGER
//#define AUTOCONFIG /* autoconfig support, fast ram, harddrives etc.. */

#define NATMEM_OFFSET natmem_offset
#define USE_NORMAL_CALLING_CONVENTION 0
#define USE_X86_FPUCW 1

#define FPUEMU /* FPU emulation */
#define FPU_UAE

#if 0
#define CPUEMU_0 /* generic 680x0 emulation */
#define CPUEMU_11 /* 68000+prefetch emulation */
#define CPUEMU_12 /* 68000 cycle-exact cpu&blitter */
#define CPUEMU_20 /* 68020 "cycle-exact" + blitter */
#define CPUEMU_21 /* 68030 (040/060) "cycle-exact" + blitter */
#endif


#define FULLMMU
#define MMUEMU
#define CPUEMU_31 /* 68040 Aranym MMU */
#define CPUEMU_32 /* 68030 with MMU */

#define MMU030_OP_DBG_MSG 0
#define MMU030_ATC_DBG_MSG 0
#define MMU030_REG_DBG_MSG 0

#else

/* #define SINGLEFILE */

#define CUSTOM_SIMPLE /* simplified custom chipset emulation */
#define CPUEMU_0
#define CPUEMU_68000_ONLY /* drop 68010+ commands from CPUEMU_0 */
#define ADDRESS_SPACE_24BIT
#ifndef UAE_NOGUI
#define D3D
#define OPENGL
#endif
#define CAPS
#define CPUEMU_12
#define CPUEMU_11


#endif

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

#ifdef WIN64
#undef X86_MSVC_ASSEMBLY
#undef JIT
#define X64_MSVC_ASSEMBLY
#define CPU_64_BIT
#define SIZEOF_VOID_P 8
#else
#define SIZEOF_VOID_P 4
#endif

#if !defined(AHI)
#undef ENFORCER
#endif


/* Define if utime(file, NULL) sets file's timestamp to the present.  */
#define HAVE_UTIME_NULL 1

/* Define as __inline if that's what the C compiler calls it.  */
/* #undef inline */
#define __inline__ __inline
#define __volatile__ volatile

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#ifdef __GNUC__
#define TIME_WITH_SYS_TIME 1
#endif

#ifdef _WIN32_WCE
#define NO_TIME_H 1
#endif

/* Define if the X Window System is missing or not being used.  */
#define X_DISPLAY_MISSING 1

/* The number of bytes in a __int64.  */
#define SIZEOF___INT64 8

/* The number of bytes in a char.  */
#define SIZEOF_CHAR 1

/* The number of bytes in a int.  */
#define SIZEOF_INT 4

/* The number of bytes in a long.  */
#define SIZEOF_LONG 4

/* The number of bytes in a long long.  */
#define SIZEOF_LONG_LONG 8

/* The number of bytes in a short.  */
#define SIZEOF_SHORT 2

#define SIZEOF_FLOAT 4
#define SIZEOF_DOUBLE 8

#define HAVE_ISNAN
#define HAVE_ISINF


#endif
