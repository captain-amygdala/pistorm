/*
  Hatari - hatari-glue.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_GLUE_H
#define HATARI_GLUE_H

#include "sysdeps.h"
#include "options_cpu.h"
#include "cycles.h"

#if 0
extern int pendingInterrupts;

extern int Init680x0(void);
extern void Exit680x0(void);
#ifndef ENABLE_WINUAE_CPU
extern void customreset(void);
#endif
extern int intlev (void);

extern unsigned long OpCode_GemDos(uae_u32 opcode);
extern unsigned long OpCode_SysInit(uae_u32 opcode);
extern unsigned long OpCode_VDI(uae_u32 opcode);

#endif

#define write_log printf


#endif /* HATARI_GLUE_H */
