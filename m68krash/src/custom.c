/*
* UAE - The Un*x Amiga Emulator
*
* Custom chip emulation
*
* Copyright 1995-2002 Bernd Schmidt
* Copyright 1995 Alessandro Bissacco
* Copyright 2000-2010 Toni Wilen
*/

#include "sysconfig.h"
#include "sysdeps.h"
#include "compat.h"
#include "hatari-glue.h"
#include "memory.h"
#include "events.h"
#include "options_cpu.h"
#include "custom.h"
#include "newcpu.h"
#include "main.h"
#include "cpummu.h"
#include "cpu_prefetch.h"
#include "m68000.h"

#define WRITE_LOG_BUF_SIZE 4096

extern struct regstruct mmu_backup_regs;
static uae_u32 mmu_struct, mmu_callback, mmu_regs;
static uae_u32 mmu_fault_bank_addr, mmu_fault_addr;
static int mmu_fault_size, mmu_fault_rw;
static int mmu_slots;
static struct regstruct mmur;
static int userdtsc = 0;
int qpcdivisor = 0;
volatile frame_time_t vsyncmintime;

void do_cycles_ce (long cycles);

unsigned long int event_cycles, nextevent, is_lastline, currcycle;
uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode);
void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v);
void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v);
uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode);
frame_time_t read_processor_time_qpf (void);
frame_time_t read_processor_time_rdtsc (void);


typedef struct _LARGE_INTEGER
{
     union
     {
          struct
          {
               unsigned long LowPart;
               long HighPart;
          };
          int64_t QuadPart;
     };
} LARGE_INTEGER, *PLARGE_INTEGER;

uae_u16 dmacon;
uae_u8 cycle_line[256];


void do_cycles_ce (long cycles)
{
	static int extra_cycle;

	cycles += extra_cycle;
/*
	while (cycles >= CYCLE_UNIT) {
		int hpos = current_hpos () + 1;
		sync_copper (hpos);
		decide_line (hpos);
		decide_fetch_ce (hpos);
		if (bltstate != BLT_done)
			decide_blitter (hpos);
		do_cycles (1 * CYCLE_UNIT);
		cycles -= CYCLE_UNIT;
	}
*/
	extra_cycle = cycles;
}

void wait_cpu_cycle_write_ce020 (uaecptr addr, int mode, uae_u32 v)
{
	int hpos;

/*
	hpos = dma_cycle ();
	do_cycles_ce (CYCLE_UNIT);
*/
#ifdef DEBUGGER
	if (debug_dma) {
		int reg = 0x1100;
		if (mode < 0)
			reg |= 4;
		else if (mode > 0)
			reg |= 2;
		else
			reg |= 1;
		record_dma (reg, v, addr, hpos, vpos, DMARECORD_CPU);
		checknasty (hpos, vpos);
	}
#endif

	if (mode < 0)
		put_long (addr, v);
	else if (mode > 0)
		put_word (addr, v);
	else if (mode == 0)
		put_byte (addr, v);

	regs.ce020memcycles -= CYCLE_UNIT;
}

uae_u32 wait_cpu_cycle_read_ce020 (uaecptr addr, int mode)
{
	uae_u32 v = 0;
	int hpos;
	struct dma_rec *dr;

/*
	hpos = dma_cycle ();
	do_cycles_ce (CYCLE_UNIT);
*/

#ifdef DEBUGGER
	if (debug_dma) {
		int reg = 0x1000;
		if (mode < 0)
			reg |= 4;
		else if (mode > 0)
			reg |= 2;
		else
			reg |= 1;
		dr = record_dma (reg, v, addr, hpos, vpos, DMARECORD_CPU);
		checknasty (hpos, vpos);
	}
#endif
	if (mode < 0)
		v = get_long (addr);
	else if (mode > 0)
		v = get_word (addr);
	else if (mode == 0)
		v = get_byte (addr);

#ifdef DEBUGGER
	if (debug_dma)
		dr->dat = v;
#endif

	regs.ce020memcycles -= CYCLE_UNIT;
	return v;
}

uae_u32 wait_cpu_cycle_read (uaecptr addr, int mode)
{
	uae_u32 v = 0;
	int hpos;
	struct dma_rec *dr;

/*
	hpos = dma_cycle ();
	do_cycles_ce (CYCLE_UNIT);
*/

#ifdef DEBUGGER
	if (debug_dma) {
		int reg = 0x1000;
		if (mode < 0)
			reg |= 4;
		else if (mode > 0)
			reg |= 2;
		else
			reg |= 1;
		dr = record_dma (reg, v, addr, hpos, vpos, DMARECORD_CPU);
		checknasty (hpos, vpos);
	}
#endif
	if (mode < 0)
		v = get_long (addr);
	else if (mode > 0)
		v = get_word (addr);
	else if (mode == 0)
		v = get_byte (addr);

#ifdef DEBUGGER
	if (debug_dma)
		dr->dat = v;
#endif

	do_cycles_ce (CYCLE_UNIT);
	return v;
}

void wait_cpu_cycle_write (uaecptr addr, int mode, uae_u32 v)
{
	int hpos;

/*
	hpos = dma_cycle ();
	do_cycles_ce (CYCLE_UNIT);
*/

#ifdef DEBUGGER
	if (debug_dma) {
		int reg = 0x1100;
		if (mode < 0)
			reg |= 4;
		else if (mode > 0)
			reg |= 2;
		else
			reg |= 1;
		record_dma (reg, v, addr, hpos, vpos, DMARECORD_CPU);
		checknasty (hpos, vpos);
	}
#endif

	if (mode < 0)
		put_long (addr, v);
	else if (mode > 0)
		put_word (addr, v);
	else if (mode == 0)
		put_byte (addr, v);
	do_cycles_ce (CYCLE_UNIT);

}

int is_cycle_ce (void)
{
	int hpos = current_hpos ();
	return cycle_line[hpos];
}

void reset_frame_rate_hack (void)
{
/*	Laurent : should it be adapted or removed ?
	if (currprefs.m68k_speed != -1)
		return;

	if (! rpt_available) {
		currprefs.m68k_speed = 0;
		return;
	}

	rpt_did_reset = 1;
	is_lastline = 0;
	vsyncmintime = read_processor_time () + vsynctime;
	write_log ("Resetting frame rate hack\n");
*/
}

/* Code taken from main.cpp */
void fixup_cpu (struct uae_prefs *p)
{
	if (p->cpu_frequency == 1000000)
		p->cpu_frequency = 0;
	switch (p->cpu_model)
	{
	case 68000:
		p->address_space_24 = 1;
		if (p->cpu_compatible || p->cpu_cycle_exact)
			p->fpu_model = 0;
		break;
	case 68010:
		p->address_space_24 = 1;
		if (p->cpu_compatible || p->cpu_cycle_exact)
			p->fpu_model = 0;
		break;
	case 68020:
		break;
	case 68030:
		p->address_space_24 = 0;
		break;
	case 68040:
		p->address_space_24 = 0;
		if (p->fpu_model)
			p->fpu_model = 68040;
		break;
	case 68060:
		p->address_space_24 = 0;
		if (p->fpu_model)
			p->fpu_model = 68060;
		break;
	}
	if ((p->cpu_model != 68040) && (p->cpu_model != 68030))
		p->mmu_model = 0;
}

/* Code taken from main.cpp*/
void uae_reset (int hardreset)
{
	currprefs.quitstatefile[0] = changed_prefs.quitstatefile[0] = 0;

	if (quit_program == 0) {
		quit_program = -2;
		if (hardreset)
			quit_program = -3;
	}

}

/* Code taken from debug.cpp*/
void mmu_do_hit (void)
{
	int i;
	uaecptr p;
	uae_u32 pc;

	mmu_triggered = 0;
	pc = m68k_getpc ();
	p = mmu_regs + 18 * 4;
	put_long (p, pc);
	regs = mmu_backup_regs;
	regs.intmask = 7;
	regs.t0 = regs.t1 = 0;
	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
		if (currprefs.cpu_model >= 68020)
			m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
		else
			m68k_areg (regs, 7) = regs.isp;
		regs.s = 1;
	}
	MakeSR ();
	m68k_setpc (mmu_callback);
	fill_prefetch_slow ();

	if (currprefs.cpu_model > 68000) {
		for (i = 0 ; i < 9; i++) {
			m68k_areg (regs, 7) -= 4;
			put_long (m68k_areg (regs, 7), 0);
		}
		m68k_areg (regs, 7) -= 4;
		put_long (m68k_areg (regs, 7), mmu_fault_addr);
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB1S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB2S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0); /* WB3S */
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7),
			(mmu_fault_rw ? 0 : 0x100) | (mmu_fault_size << 5)); /* SSW */
		m68k_areg (regs, 7) -= 4;
		put_long (m68k_areg (regs, 7), mmu_fault_bank_addr);
		m68k_areg (regs, 7) -= 2;
		put_word (m68k_areg (regs, 7), 0x7002);
	}
	m68k_areg (regs, 7) -= 4;
	put_long (m68k_areg (regs, 7), get_long (p - 4));
	m68k_areg (regs, 7) -= 2;
	put_word (m68k_areg (regs, 7), mmur.sr);
#ifdef JIT
	set_special(SPCFLAG_END_COMPILE);
#endif
}

/* Code taken from win32.cpp*/
void fpux_restore (int *v)
{
/*#ifndef _WIN64
	if (v)
		_controlfp (*v, _MCW_IC | _MCW_RC | _MCW_PC);
	else
		_controlfp (fpucontrol, _MCW_IC | _MCW_RC | _MCW_PC);
#endif
*/
}

/* Code taken from win32.cpp*/
frame_time_t read_processor_time (void)
{
#if 0
	static int cnt;

	cnt++;
	if (cnt > 1000000) {
		write_log(L"**************\n");
		cnt = 0;
	}
#endif
	if (userdtsc)
		return read_processor_time_rdtsc ();
	else
		return read_processor_time_qpf ();
}

/* Code taken from win32.cpp*/
frame_time_t read_processor_time_qpf (void)
{
/* Laurent : may be coded later
	LARGE_INTEGER counter;
	QueryPerformanceCounter (&counter);
	if (qpcdivisor == 0)
		return (frame_time_t)(counter.LowPart);
	return (frame_time_t)(counter.QuadPart >> qpcdivisor);
*/
}

/* Code taken from win32.cpp*/
frame_time_t read_processor_time_rdtsc (void)
{
	frame_time_t foo = 0;
#if defined(X86_MSVC_ASSEMBLY)
	frame_time_t bar;
	__asm
	{
		rdtsc
			mov foo, eax
			mov bar, edx
	}
	/* very high speed CPU's RDTSC might overflow without this.. */
	foo >>= 6;
	foo |= bar << 26;
#endif
	return foo;
}

/* Code taken from win32.cpp*/
void sleep_millis (int ms)
{
/* Laurent : may be coded later (DSL-Delay ?)
	unsigned int TimerEvent;
	int start;
	int cnt;

	start = read_processor_time ();
	EnterCriticalSection (&cs_time);
	cnt = timehandlecounter++;
	if (timehandlecounter >= MAX_TIMEHANDLES)
		timehandlecounter = 0;
	LeaveCriticalSection (&cs_time);
	TimerEvent = timeSetEvent (ms, 0, (LPTIMECALLBACK)timehandle[cnt], 0, TIME_ONESHOT | TIME_CALLBACK_EVENT_SET);
	WaitForSingleObject (timehandle[cnt], ms);
	ResetEvent (timehandle[cnt]);
	timeKillEvent (TimerEvent);
	idletime += read_processor_time () - start;
*/
}


/* Code just here to let newcpu.c link (original function is in inprec.cpp) */
int inprec_open(char *fname, int record)
{
	return 0;
}

int current_hpos (void)
{
    return (get_cycles () /*- eventtab[ev_hsync].oldcycles*/) / CYCLE_UNIT;
}
