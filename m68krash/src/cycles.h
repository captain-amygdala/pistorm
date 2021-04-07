/*
  Hatari - cycles.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

#ifndef HATARI_CYCLES_H
#define HATARI_CYCLES_H

#include <stdbool.h>

enum
{
	CYCLES_COUNTER_SOUND,
	CYCLES_COUNTER_VIDEO,
	CYCLES_COUNTER_CPU,

	CYCLES_COUNTER_MAX
};


extern int nCyclesMainCounter;

extern int CurrentInstrCycles;


extern void Cycles_MemorySnapShot_Capture(bool bSave);
extern void Cycles_SetCounter(int nId, int nValue);
extern int Cycles_GetCounter(int nId);
extern int Cycles_GetCounterOnReadAccess(int nId);
extern int Cycles_GetCounterOnWriteAccess(int nId);

#endif  /* HATARI_CYCLES_H */
