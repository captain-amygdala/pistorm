/*
  Hatari - log.h
  
  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/
#ifndef HATARI_LOG_H
#define HATARI_LOG_H

#include <stdbool.h>
#include <stdint.h>


/* Logging
 * -------
 * Is always enabled as it's information that can be useful
 * to the Hatari users
 */
typedef enum
{
/* these present user with a dialog and log the issue */
	LOG_FATAL,	/* Hatari can't continue unless user resolves issue */
	LOG_ERROR,	/* something user did directly failed (e.g. save) */
/* these just log the issue */
	LOG_WARN,	/* something failed, but it's less serious */
	LOG_INFO,	/* user action success (e.g. TOS file load) */
	LOG_TODO,	/* functionality not yet being emulated */
	LOG_DEBUG,	/* information about internal Hatari working */
	LOG_NONE	/* invalid LOG level */
} LOGTYPE;

#ifndef __GNUC__
/* assuming attributes work only for GCC */
#define __attribute__(foo)
#endif

extern int Log_Init(void);
extern int Log_SetAlertLevel(int level);
extern void Log_UnInit(void);
extern void Log_Printf(LOGTYPE nType, const char *psFormat, ...)
	__attribute__ ((format (printf, 2, 3)));
extern void Log_AlertDlg(LOGTYPE nType, const char *psFormat, ...)
	__attribute__ ((format (printf, 2, 3)));
extern LOGTYPE Log_ParseOptions(const char *OptionStr);
extern const char* Log_SetTraceOptions(const char *OptionsStr);
extern char *Log_MatchTrace(const char *text, int state);

#ifndef __GNUC__
#undef __attribute__
#endif



/* Tracing
 * -------
 * Tracing outputs information about what happens in the emulated
 * system and slows down the emulation.  As it's intended mainly
 * just for the Hatari developers, tracing support is compiled in
 * by default.
 * 
 * Tracing can be enabled by defining ENABLE_TRACING
 * in the top level config.h
 */
#include "config.h"

/* Up to 64 levels when using Uint32 for HatariTraceFlags */
#define	TRACE_VIDEO_SYNC	 (1<<0)
#define	TRACE_VIDEO_RES 	 (1<<1)
#define	TRACE_VIDEO_COLOR	 (1<<2)
#define	TRACE_VIDEO_BORDER_V	 (1<<3)
#define	TRACE_VIDEO_BORDER_H	 (1<<4)
#define	TRACE_VIDEO_ADDR	 (1<<5)
#define	TRACE_VIDEO_VBL 	 (1<<6)
#define	TRACE_VIDEO_HBL 	 (1<<7)
#define	TRACE_VIDEO_STE 	 (1<<8)

#define	TRACE_MFP_EXCEPTION	 (1<<9)
#define	TRACE_MFP_START 	 (1<<10)
#define	TRACE_MFP_READ  	 (1<<11)
#define	TRACE_MFP_WRITE 	 (1<<12)

#define	TRACE_PSG_READ  	 (1<<13)
#define	TRACE_PSG_WRITE 	 (1<<14)

#define	TRACE_CPU_PAIRING	 (1<<15)
#define	TRACE_CPU_DISASM	 (1<<16)
#define	TRACE_CPU_EXCEPTION	 (1<<17)

#define	TRACE_INT		 (1<<18)

#define	TRACE_FDC		 (1<<19)

#define	TRACE_IKBD_CMDS 	 (1<<20)
#define	TRACE_IKBD_ACIA 	 (1<<21)
#define	TRACE_IKBD_EXEC 	 (1<<22)

#define TRACE_BLITTER		 (1<<23)

#define TRACE_OS_BIOS		 (1<<24)
#define TRACE_OS_XBIOS  	 (1<<25)
#define TRACE_OS_GEMDOS 	 (1<<26)
#define TRACE_OS_VDI		 (1<<27)
#define TRACE_OS_AES		 (1<<28)

#define TRACE_IOMEM_RD  	 (1<<29)
#define TRACE_IOMEM_WR  	 (1<<30)

#define TRACE_DMASND		 (1<<31)

#define TRACE_CROSSBAR		 (1ll<<32)
#define TRACE_VIDEL		 (1ll<<33)

#define TRACE_DSP_HOST_INTERFACE (1ll<<34)
#define TRACE_DSP_HOST_COMMAND	 (1ll<<35)
#define TRACE_DSP_HOST_SSI	 (1ll<<36)
#define TRACE_DSP_DISASM	 (1ll<<37)
#define TRACE_DSP_DISASM_REG	 (1ll<<38)
#define TRACE_DSP_DISASM_MEM	 (1ll<<39)
#define TRACE_DSP_STATE		 (1ll<<40)
#define TRACE_DSP_INTERRUPT	 (1ll<<41)

#define	TRACE_NONE		 (0)
#define	TRACE_ALL		 (~0)


#define	TRACE_VIDEO_ALL		( TRACE_VIDEO_SYNC | TRACE_VIDEO_RES | TRACE_VIDEO_COLOR \
		| TRACE_VIDEO_BORDER_V | TRACE_VIDEO_BORDER_H | TRACE_VIDEO_ADDR \
		| TRACE_VIDEO_VBL | TRACE_VIDEO_HBL | TRACE_VIDEO_STE )

#define TRACE_MFP_ALL		( TRACE_MFP_EXCEPTION | TRACE_MFP_START | TRACE_MFP_READ | TRACE_MFP_WRITE )

#define	TRACE_PSG_ALL		( TRACE_PSG_READ | TRACE_PSG_WRITE )

#define	TRACE_CPU_ALL		( TRACE_CPU_PAIRING | TRACE_CPU_DISASM | TRACE_CPU_EXCEPTION )

#define	TRACE_IKBD_ALL		( TRACE_IKBD_CMDS | TRACE_IKBD_ACIA | TRACE_IKBD_EXEC | TRACE_OS_VDI )

#define	TRACE_OS_ALL		( TRACE_OS_BIOS | TRACE_OS_XBIOS | TRACE_OS_GEMDOS | TRACE_OS_VDI )

#define	TRACE_IOMEM_ALL		( TRACE_IOMEM_RD | TRACE_IOMEM_WR )

#define TRACE_DSP_ALL		( TRACE_DSP_HOST_INTERFACE | TRACE_DSP_HOST_COMMAND | TRACE_DSP_HOST_SSI | TRACE_DSP_DISASM \
    | TRACE_DSP_DISASM_REG | TRACE_DSP_DISASM_MEM | TRACE_DSP_STATE | TRACE_DSP_INTERRUPT )



extern FILE *TraceFile;
extern uint64_t LogTraceFlags;

#if ENABLE_TRACING

#ifndef _VCWIN_
#define	LOG_TRACE(level, args...) \
	if (unlikely(LogTraceFlags & level)) fprintf(TraceFile, args)
#endif
#define LOG_TRACE_LEVEL( level )	(unlikely(LogTraceFlags & level))

#else		/* ENABLE_TRACING */

#ifndef _VCWIN_
#define LOG_TRACE(level, args...)	{}
#endif
#define LOG_TRACE_LEVEL( level )	(0)

#endif		/* ENABLE_TRACING */

/* Always defined in full to avoid compiler warnings about unused variables.
 * In code it's used in such a way that it will be optimized away when tracing
 * is disabled.
 */
#ifndef _VCWIN_
#define LOG_TRACE_PRINT(args...)	fprintf(TraceFile , args)
#endif


#endif		/* HATARI_LOG_H */
