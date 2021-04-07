/*
  Hatari - m68000.h

  This file is distributed under the GNU Public License, version 2 or at
  your option any later version. Read the file gpl.txt for details.
*/

/* 2007/11/10	[NP]	Add pairing for lsr / dbcc (and all variants	*/
/*			working on register, not on memory).		*/
/* 2008/01/07	[NP]	Use PairingArray to store all valid pairing	*/
/*			combinations (in m68000.c)			*/
/* 2010/04/05	[NP]	Rework the pairing code to take BusCyclePenalty	*/
/*			into account when using d8(an,ix).		*/
/* 2010/05/07	[NP]	Add BusCyclePenalty to LastInstrCycles to detect*/
/*			a possible pairing between add.l (a5,d1.w),d0	*/
/*			and move.b 7(a5,d1.w),d5.			*/


#ifndef HATARI_M68000_H
#define HATARI_M68000_H

#include <stdint.h>

#include "cycles.h"     /* for nCyclesMainCounter */
#include "sysdeps.h"
#include "memory.h"
#include "newcpu.h"     /* for regs */
#include "cycInt.h"
#include "log.h"
#include "m68krash.h"


/* 68000 Register defines */
enum {
  REG_D0,    /* D0.. */
  REG_D1,
  REG_D2,
  REG_D3,
  REG_D4,
  REG_D5,
  REG_D6,
  REG_D7,    /* ..D7 */
  REG_A0,    /* A0.. */
  REG_A1,
  REG_A2,
  REG_A3,
  REG_A4,
  REG_A5,
  REG_A6,
  REG_A7,    /* ..A7 (also SP) */
};

/* 68000 Condition code's */
#define SR_AUX              0x0010
#define SR_NEG              0x0008
#define SR_ZERO             0x0004
#define SR_OVERFLOW         0x0002
#define SR_CARRY            0x0001

#define SR_CLEAR_AUX        0xffef
#define SR_CLEAR_NEG        0xfff7
#define SR_CLEAR_ZERO       0xfffb
#define SR_CLEAR_OVERFLOW   0xfffd
#define SR_CLEAR_CARRY      0xfffe

#define SR_CCODE_MASK       (SR_AUX|SR_NEG|SR_ZERO|SR_OVERFLOW|SR_CARRY)
#define SR_MASK             0xFFE0

#define SR_TRACEMODE        0x8000
#define SR_SUPERMODE        0x2000
#define SR_IPL              0x0700

#define SR_CLEAR_IPL        0xf8ff
#define SR_CLEAR_TRACEMODE  0x7fff
#define SR_CLEAR_SUPERMODE  0xdfff

/* Exception vectors */
#define  EXCEPTION_BUSERROR   0x00000008
#define  EXCEPTION_ADDRERROR  0x0000000c
#define  EXCEPTION_ILLEGALINS 0x00000010
#define  EXCEPTION_DIVZERO    0x00000014
#define  EXCEPTION_CHK        0x00000018
#define  EXCEPTION_TRAPV      0x0000001c
#define  EXCEPTION_TRACE      0x00000024
#define  EXCEPTION_LINE_A     0x00000028
#define  EXCEPTION_LINE_F     0x0000002c
#define  EXCEPTION_HBLANK     0x00000068
#define  EXCEPTION_VBLANK     0x00000070
#define  EXCEPTION_TRAP0      0x00000080
#define  EXCEPTION_TRAP1      0x00000084
#define  EXCEPTION_TRAP2      0x00000088
#define  EXCEPTION_TRAP13     0x000000B4
#define  EXCEPTION_TRAP14     0x000000B8


/* Size of 68000 instructions */
#define MAX_68000_INSTRUCTION_SIZE  10  /* Longest 68000 instruction is 10 bytes(6+4) */
#define MIN_68000_INSTRUCTION_SIZE  2   /* Smallest 68000 instruction is 2 bytes(ie NOP) */

/* Illegal Opcode used to help emulation. eg. free entries are 8 to 15 inc' */
#define  GEMDOS_OPCODE        8  /* Free op-code to intercept GemDOS trap */
#define  SYSINIT_OPCODE      10  /* Free op-code to initialize system (connected drives etc.) */
#define  VDI_OPCODE          12  /* Free op-code to call VDI handlers AFTER Trap#2 */



/* Ugly hacks to adapt the main code to the different CPU cores: */

#define Regs regs.regs

# define M68000_GetPC()     m68k_getpc()
# define M68000_SetPC(val)  m68k_setpc(val)

static inline uint16_t M68000_GetSR(void)
{
	MakeSR();
	return regs.sr;
}
static inline void M68000_SetSR(uint16_t v)
{
	regs.sr = v;
	MakeFromSR();
}

# define M68000_SetSpecial(flags)   set_special(flags)
# define M68000_UnsetSpecial(flags) unset_special(flags)


/* bus error mode */
#define BUS_ERROR_WRITE 0
#define BUS_ERROR_READ 1


/* bus access mode */
#define	BUS_MODE_CPU		0			/* bus is owned by the cpu */
#define	BUS_MODE_BLITTER	1			/* bus is owned by the blitter */


extern uint32_t BusErrorAddress;
extern uint32_t BusErrorPC;
extern bool bBusErrorReadWrite;
extern int nCpuFreqShift;
extern int nWaitStateCycles;
extern int BusMode;

extern int	LastOpcodeFamily;
extern int	LastInstrCycles;
extern int	Pairing;
extern char	PairingArray[ MAX_OPCODE_FAMILY ][ MAX_OPCODE_FAMILY ];
extern const char *OpcodeName[];


/*-----------------------------------------------------------------------*/
/**
 * Add CPU cycles.
 * NOTE: All times are rounded up to nearest 4 cycles.
 */
static inline void M68000_AddCycles(int cycles)
{
	cycles = (cycles + 3) & ~3;
	cycles = cycles >> nCpuFreqShift;

	PendingInterruptCount -= INT_CONVERT_TO_INTERNAL(cycles, INT_CPU_CYCLE);
	nCyclesMainCounter += cycles;
}


/*-----------------------------------------------------------------------*/
/**
 * Add CPU cycles, take cycles pairing into account. Pairing will make
 * some specific instructions take 4 cycles less when run one after the other.
 * Pairing happens when the 2 instructions are "aligned" on different bus accesses.
 * Candidates are :
 *  - 2 instructions taking 4n+2 cycles
 *  - 1 instruction taking 4n+2 cycles, followed by 1 instruction using d8(an,ix)
 *
 * Not all the candidate instructions can pair, only the opcodes listed in PairingArray.
 * On ST, when using d8(an,ix), we get an extra 2 cycle penalty for misaligned bus access.
 * The only instruction that can generate BusCyclePenalty=4 is move d8(an,ix),d8(an,ix)
 * and although it takes 4n cycles (24 for .b/.w or 32 for .l) it can pair with
 * a previous 4n+2 instruction (but it will still have 1 misaligned bus access in the end).
 *
 * Verified pairing on an STF :
 *  - lsl.w #4,d1 + move.w 0(a4,d2.w),d1		motorola=14+14=28  stf=28
 *  - lsl.w #4,d1 + move.w 0(a4,d2.w),(a4)		motorola=14+18=32  stf=32
 *  - lsl.w #4,d1 + move.w 0(a4,d2.w),0(a4,d2.w)	motorola=14+24=38  stf=40
 *  - add.l (a5,d1.w),d0 + move.b 7(a5,d1.w),d5)	motorola=20+14=34  stf=36
 *
 * d8(an,ix) timings without pairing (2 cycles penalty) :
 *  - add.l   0(a4,d2.w),a1				motorola=20  stf=24
 *  - move.w  0(a4,d2.w),d1				motorola=14  stf=16
 *  - move.w  0(a4,d2.w),(a4)				motorola=18  stf=20
 *  - move.w  0(a4,d2.w),0(a4,d2.w)			motorola=24  stf=28
 *
 * NOTE: All times are rounded up to nearest 4 cycles.
 */
static inline void M68000_AddCyclesWithPairing(int cycles)
{
	Pairing = 0;
	/* Check if number of cycles for current instr and for */
	/* the previous one is of the form 4+2n */
	/* If so, a pairing could be possible depending on the opcode */
	/* A pairing is also possible if current instr is 4n but with BusCyclePenalty > 0 */
	if ( ( PairingArray[ LastOpcodeFamily ][ OpcodeFamily ] == 1 )
	    && ( ( LastInstrCycles & 3 ) == 2 )
	    && ( ( ( cycles & 3 ) == 2 ) || ( BusCyclePenalty > 0 ) ) )
	{
		Pairing = 1;
		LOG_TRACE(TRACE_CPU_PAIRING,
		          "cpu pairing detected pc=%x family %s/%s cycles %d/%d\n",
		          m68k_getpc(), OpcodeName[LastOpcodeFamily],
		          OpcodeName[OpcodeFamily], LastInstrCycles, cycles);
	}

	/* [NP] This part is only needed to track possible pairing instructions, */
	/* we can keep it disabled most of the time */
#if 0
	if ( (LastOpcodeFamily!=OpcodeFamily) && ( Pairing == 0 )
		&& ( ( cycles & 3 ) == 2 ) && ( ( LastInstrCycles & 3 ) == 2 ) )
	{
		LOG_TRACE(TRACE_CPU_PAIRING,
		          "cpu could pair pc=%x family %s/%s cycles %d/%d\n",
		          m68k_getpc(), OpcodeName[LastOpcodeFamily],
		          OpcodeName[OpcodeFamily], LastInstrCycles, cycles);
	}
#endif

	/* Store current instr (not rounded) to check next time */
	LastInstrCycles = cycles + BusCyclePenalty;
	LastOpcodeFamily = OpcodeFamily;

	/* If pairing is true, we need to substract 2 cycles for the	*/
	/* previous instr which was rounded to 4 cycles while it wasn't */
	/* needed (and we don't round the current one)			*/
	/* -> both instr will take 4 cycles less on the ST than if ran	*/
	/* separately.							*/
	if (Pairing == 1)
	{
		if ( ( cycles & 3 ) == 2 )		/* pairing between 4n+2 and 4n+2 instructions */
			cycles -= 2;			/* if we have a pairing, we should not count the misaligned bus access */

		else					/* this is the case of move d8(an,ix),d8(an,ix) where BusCyclePenalty=4 */
			/*do nothing */;		/* we gain 2 cycles for the pairing with 1st d8(an,ix) */
							/* and we have 1 remaining misaligned access for the 2nd d8(an,ix). So in the end, we keep */
							/* cycles unmodified as 4n cycles (eg lsl.w #4,d1 + move.w 0(a4,d2.w),0(a4,d2.w) takes 40 cycles) */
	}
	else
	{
		cycles += BusCyclePenalty;		/* >0 if d8(an,ix) was used */
		cycles = (cycles + 3) & ~3;		/* no pairing, round current instr to 4 cycles */
	}

	cycles = cycles >> nCpuFreqShift;

	PendingInterruptCount -= INT_CONVERT_TO_INTERNAL ( cycles , INT_CPU_CYCLE );

	nCyclesMainCounter += cycles;
	BusCyclePenalty = 0;
}

extern void M68000_CheckCpuSettings(void);
extern void M68000_MemorySnapShot_Capture(bool bSave);
extern void M68000_BusError(uint32_t addr, bool bReadWrite);
extern void M68000_Exception(uint32_t ExceptionVector , int ExceptionSource);
extern void M68000_WaitState(int nCycles);

#endif
