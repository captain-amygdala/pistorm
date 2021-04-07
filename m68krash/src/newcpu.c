/*
* UAE - The Un*x Amiga Emulator
*
* MC68000 emulation
*
* (c) 1995 Bernd Schmidt
*/

#define MMUOP_DEBUG 2
#define DEBUG_CD32CDTVIO 0

//#include "main.h"
//#include "compat.h"
#include "sysconfig.h"
#include "sysdeps.h"
#include "hatari-glue.h"
//#include "options_cpu.h"
#include "events.h"
//#include "custom.h"
#include "maccess.h"
#include "memory.h"
#include "newcpu.h"
#include "cpummu.h"
#include "cpummu030.h"
#include "cpu_prefetch.h"
#include "main.h"
#include "m68000.h"
//#include "reset.h"
//#include "cycInt.h"
//#include "mfp.h"
//#include "dialog.h"
//#include "screen.h"
//#include "video.h"
//#include "options.h"
//#include "log.h"
//#include "debugui.h"
//#include "debugcpu.h"

/* For faster JIT cycles handling */
signed long pissoff = 0;

uaecptr rtarea_base = RTAREA_DEFAULT;

/* Opcode of faulting instruction */
static uae_u16 last_op_for_exception_3;
/* PC at fault time */
static uaecptr last_addr_for_exception_3;
/* Address that generated the exception */
static uaecptr last_fault_for_exception_3;
/* read (0) or write (1) access */
static int last_writeaccess_for_exception_3;
/* instruction (1) or data (0) access */
static int last_instructionaccess_for_exception_3;
unsigned long irqcycles[15];
int irqdelay[15];
int mmu_enabled, mmu_triggered;
int cpu_cycles;
static int baseclock;
int cpucycleunit;

const int areg_byteinc[] = { 1, 1, 1, 1, 1, 1, 1, 2 };
const int imm8_table[] = { 8, 1, 2, 3, 4, 5, 6, 7 };

int movem_index1[256];
int movem_index2[256];
int movem_next[256];

cpuop_func *cpufunctbl[65536];

int OpcodeFamily;
struct mmufixup mmufixup[2];

extern uae_u32 get_fpsr (void);

#define COUNT_INSTRS 0
#define MC68060_PCR   0x04300000
#define MC68EC060_PCR 0x04310000
//moved to newcpu.h
//static uae_u64 srp_030, crp_030;
//static uae_u32 tt0_030, tt1_030, tc_030;
//static uae_u16 mmusr_030;

static struct cache020 caches020[CACHELINES020];
static struct cache030 icaches030[CACHELINES030];
static struct cache030 dcaches030[CACHELINES030];
static struct cache040 caches040[CACHESETS040];
static void InterruptAddJitter (int Level , int Pending);

static void m68k_disasm_2 (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr, int safemode);


void dump_counts (void)
{
}


uae_u32 (*x_prefetch)(int);
uae_u32 (*x_next_iword)(void);
uae_u32 (*x_next_ilong)(void);
uae_u32 (*x_get_ilong)(int);
uae_u32 (*x_get_iword)(int);
uae_u32 (*x_get_ibyte)(int);
uae_u32 (*x_get_long)(uaecptr);
uae_u32 (*x_get_word)(uaecptr);
uae_u32 (*x_get_byte)(uaecptr);
void (*x_put_long)(uaecptr,uae_u32);
void (*x_put_word)(uaecptr,uae_u32);
void (*x_put_byte)(uaecptr,uae_u32);

uae_u32 (*x_cp_next_iword)(void);
uae_u32 (*x_cp_next_ilong)(void);
uae_u32 (*x_cp_get_long)(uaecptr);
uae_u32 (*x_cp_get_word)(uaecptr);
uae_u32 (*x_cp_get_byte)(uaecptr);
void (*x_cp_put_long)(uaecptr,uae_u32);
void (*x_cp_put_word)(uaecptr,uae_u32);
void (*x_cp_put_byte)(uaecptr,uae_u32);
uae_u32 (REGPARAM3 *x_cp_get_disp_ea_020)(uae_u32 base, int idx) REGPARAM;

// shared memory access functions
static void set_x_funcs (void)
{
	if (currprefs.mmu_model) {
		if (currprefs.cpu_model == 68060) {
			x_prefetch = get_iword_mmu060;
			x_get_ilong = get_ilong_mmu060;
			x_get_iword = get_iword_mmu060;
			x_get_ibyte = get_ibyte_mmu060;
			x_next_iword = next_iword_mmu060;
			x_next_ilong = next_ilong_mmu060;
			x_put_long = put_long_mmu060;
			x_put_word = put_word_mmu060;
			x_put_byte = put_byte_mmu060;
			x_get_long = get_long_mmu060;
			x_get_word = get_word_mmu060;
			x_get_byte = get_byte_mmu060;
		} else if (currprefs.cpu_model == 68040) {
			x_prefetch = get_iword_mmu040;
			x_get_ilong = get_ilong_mmu040;
			x_get_iword = get_iword_mmu040;
			x_get_ibyte = get_ibyte_mmu040;
			x_next_iword = next_iword_mmu040;
			x_next_ilong = next_ilong_mmu040;
			x_put_long = put_long_mmu040;
			x_put_word = put_word_mmu040;
			x_put_byte = put_byte_mmu040;
			x_get_long = get_long_mmu040;
			x_get_word = get_word_mmu040;
			x_get_byte = get_byte_mmu040;
		} else {
			x_prefetch = get_iword_mmu030;
			x_get_ilong = get_ilong_mmu030;
			x_get_iword = get_iword_mmu030;
			x_get_ibyte = get_ibyte_mmu030;
			x_next_iword = next_iword_mmu030;
			x_next_ilong = next_ilong_mmu030;
			x_put_long = put_long_mmu030;
			x_put_word = put_word_mmu030;
			x_put_byte = put_byte_mmu030;
			x_get_long = get_long_mmu030;
			x_get_word = get_word_mmu030;
			x_get_byte = get_byte_mmu030;
		}
	}

	x_cp_put_long = x_put_long;
	x_cp_put_word = x_put_word;
	x_cp_put_byte = x_put_byte;
	x_cp_get_long = x_get_long;
	x_cp_get_word = x_get_word;
	x_cp_get_byte = x_get_byte;
	x_cp_next_iword = x_next_iword;
	x_cp_next_ilong = x_next_ilong;
	x_cp_get_disp_ea_020 = x_get_disp_ea_020;

	if (currprefs.mmu_model == 68030) {
		x_cp_put_long = put_long_mmu030_state;
		x_cp_put_word = put_word_mmu030_state;
		x_cp_put_byte = put_byte_mmu030_state;
		x_cp_get_long = get_long_mmu030_state;
		x_cp_get_word = get_word_mmu030_state;
		x_cp_get_byte = get_byte_mmu030_state;
		x_cp_next_iword = next_iword_mmu030_state;
		x_cp_next_ilong = next_ilong_mmu030_state;
		x_cp_get_disp_ea_020 = get_disp_ea_020_mmu030;
	}
}

void set_cpu_caches (bool flush)
{
	int i;

	for (i = 0; i < CPU_PIPELINE_MAX; i++)
		regs.prefetch020addr[i] = 0xffffffff;

	if (currprefs.cpu_model == 68020) {
		if (regs.cacr & 0x08) { // clear instr cache
			for (i = 0; i < CACHELINES020; i++)
				caches020[i].valid = 0;
		}
		if (regs.cacr & 0x04) { // clear entry in instr cache
			caches020[(regs.caar >> 2) & (CACHELINES020 - 1)].valid = 0;
			regs.cacr &= ~0x04;
		}
	} else if (currprefs.cpu_model == 68030) {
		//regs.cacr |= 0x100;
		if (regs.cacr & 0x08) { // clear instr cache
			for (i = 0; i < CACHELINES030; i++) {
				icaches030[i].valid[0] = 0;
				icaches030[i].valid[1] = 0;
				icaches030[i].valid[2] = 0;
				icaches030[i].valid[3] = 0;
			}
		}
		if (regs.cacr & 0x04) { // clear entry in instr cache
			icaches030[(regs.caar >> 4) & (CACHELINES030 - 1)].valid[(regs.caar >> 2) & 3] = 0;
			regs.cacr &= ~0x04;
		}
		if (regs.cacr & 0x800) { // clear data cache
			for (i = 0; i < CACHELINES030; i++) {
				dcaches030[i].valid[0] = 0;
				dcaches030[i].valid[1] = 0;
				dcaches030[i].valid[2] = 0;
				dcaches030[i].valid[3] = 0;
			}
			regs.cacr &= ~0x800;
		}
		if (regs.cacr & 0x400) { // clear entry in data cache
			dcaches030[(regs.caar >> 4) & (CACHELINES030 - 1)].valid[(regs.caar >> 2) & 3] = 0;
			regs.cacr &= ~0x400;
		}
	} else if (currprefs.cpu_model == 68040) {
		if (!(regs.cacr & 0x8000)) {
			for (i = 0; i < CACHESETS040; i++) {
				caches040[i].valid[0] = 0;
				caches040[i].valid[1] = 0;
				caches040[i].valid[2] = 0;
				caches040[i].valid[3] = 0;
			}
		}
	}
}

STATIC_INLINE void count_instr (unsigned int opcode)
{
}

//static unsigned long REGPARAM3 op_illg_1 (uae_u32 opcode) REGPARAM;

static unsigned long REGPARAM2 op_illg_1 (uae_u32 opcode)
{
	op_illg (opcode);
	return 4;
}

void build_cpufunctbl (void)
{
	int i, opcnt;
	unsigned long opcode;
	const struct cputbl *tbl = 0;
	int lvl;
	
	printf("ARNE %i\n", currprefs.cpu_model);
	
	switch (currprefs.cpu_model)
	{
	case 68060:
		lvl = 5;
//		tbl = op_smalltbl_0_ff;
//		if (!currprefs.cachesize) {
//			if (currprefs.cpu_cycle_exact)
//				tbl = op_smalltbl_22_ff;
//			if (currprefs.mmu_model)
				//tbl = op_smalltbl_33_ff;
//		}
		break;
	case 68040:
		lvl = 4;
//		tbl = op_smalltbl_1_ff;
//		if (!currprefs.cachesize) {
//			if (currprefs.cpu_cycle_exact)
//				tbl = op_smalltbl_23_ff;
//			if (currprefs.mmu_model)
				tbl = op_smalltbl_31_ff;
//		}
		break;
	case 68030:
		lvl = 3;
//		tbl = op_smalltbl_2_ff;
//		if (!currprefs.cachesize) {
//			if (currprefs.cpu_cycle_exact)
//				tbl = op_smalltbl_24_ff;
//			if (currprefs.mmu_model)
				tbl = op_smalltbl_32_ff;
//		}
		break;
#if 0
	case 68020:
		lvl = 2;
		tbl = op_smalltbl_3_ff;
		if (currprefs.cpu_cycle_exact)
			tbl = op_smalltbl_20_ff;
		break;
	case 68010:
		lvl = 1;
		tbl = op_smalltbl_4_ff;
		break;
#endif
	default:
		changed_prefs.cpu_model = currprefs.cpu_model = 68000;
	}

	if (tbl == 0) {
		write_log ("no CPU emulation cores available CPU=%d!", currprefs.cpu_model);
		abort ();
	}

	for (opcode = 0; opcode < 65536; opcode++)
		cpufunctbl[opcode] = op_illg_1;
	for (i = 0; tbl[i].handler != NULL; i++) {
		opcode = tbl[i].opcode;
		cpufunctbl[opcode] = tbl[i].handler;
	}

	opcnt = 0;
	for (opcode = 0; opcode < 65536; opcode++) {
		cpuop_func *f;

		if (table68k[opcode].mnemo == i_ILLG)
			continue;
		if (table68k[opcode].clev > lvl) {
			continue;
		}

		if (table68k[opcode].handler != -1) {
			int idx = table68k[opcode].handler;
			f = cpufunctbl[idx];
			if (f == op_illg_1)
				abort ();
			cpufunctbl[opcode] = f;
			opcnt++;
		}
	}
	write_log ("Building CPU, %d opcodes (%d %d %d)\n",
		opcnt, lvl,
		currprefs.cpu_cycle_exact ? -1 : currprefs.cpu_compatible ? 1 : 0, currprefs.address_space_24);
	write_log ("CPU=%d, MMU=%d, FPU=%d ($%02x), JIT%s=%d.\n",
               currprefs.cpu_model, currprefs.mmu_model,
               currprefs.fpu_model, currprefs.fpu_revision,
		currprefs.cachesize ? (currprefs.compfpu ? "=CPU/FPU" : "=CPU") : "",
		currprefs.cachesize);
	
	set_cpu_caches (0);
	if (currprefs.mmu_model) {
        if (currprefs.cpu_model >= 68040) {
            mmu_reset ();
            mmu_set_tc (regs.tcr);
            mmu_set_super (regs.s != 0);
        } else {
            mmu030_reset(1);
        }
    }
}

void fill_prefetch_slow (void)
{
	if (currprefs.mmu_model)
		return;
	regs.ir = x_get_word (m68k_getpc ());
	regs.irc = x_get_word (m68k_getpc () + 2);
}

unsigned long cycles_mask, cycles_val;

static void update_68k_cycles (void)
{
	cycles_mask = 0;
	cycles_val = currprefs.m68k_speed;
	if (currprefs.m68k_speed < 1) {
		cycles_mask = 0xFFFFFFFF;
		cycles_val = 0;
	}
	currprefs.cpu_clock_multiplier = changed_prefs.cpu_clock_multiplier;
	currprefs.cpu_frequency = changed_prefs.cpu_frequency;

	baseclock = currprefs.ntscmode ? 28636360 : 28375160;
	cpucycleunit = CYCLE_UNIT / 2;
	if (currprefs.cpu_clock_multiplier) {
		if (currprefs.cpu_clock_multiplier >= 256) {
			cpucycleunit = CYCLE_UNIT / (currprefs.cpu_clock_multiplier >> 8);
		} else {
			cpucycleunit = CYCLE_UNIT * currprefs.cpu_clock_multiplier;
		}
	} else if (currprefs.cpu_frequency) {
		cpucycleunit = CYCLE_UNIT * baseclock / currprefs.cpu_frequency;
	}
	if (cpucycleunit < 1)
		cpucycleunit = 1;
	if (currprefs.cpu_cycle_exact)
		write_log ("CPU cycleunit: %d (%.3f)\n", cpucycleunit, (float)cpucycleunit / CYCLE_UNIT);
	config_changed = 1;
}

static void prefs_changed_cpu (void)
{
	fixup_cpu (&changed_prefs);
	currprefs.cpu_model = changed_prefs.cpu_model;
	currprefs.fpu_model = changed_prefs.fpu_model;
    currprefs.fpu_revision = changed_prefs.fpu_revision;
	currprefs.mmu_model = changed_prefs.mmu_model;
	currprefs.cpu_compatible = changed_prefs.cpu_compatible;
	currprefs.cpu_cycle_exact = changed_prefs.cpu_cycle_exact;
	currprefs.blitter_cycle_exact = changed_prefs.cpu_cycle_exact;
}

void check_prefs_changed_cpu (void)
{
	bool changed = 0;

	if (!config_changed)
		return;

	if (changed
		|| currprefs.cpu_model != changed_prefs.cpu_model
		|| currprefs.fpu_model != changed_prefs.fpu_model
        || currprefs.fpu_revision != changed_prefs.fpu_revision
		|| currprefs.mmu_model != changed_prefs.mmu_model
		|| currprefs.cpu_compatible != changed_prefs.cpu_compatible
		|| currprefs.cpu_cycle_exact != changed_prefs.cpu_cycle_exact) {

			prefs_changed_cpu ();
			if (!currprefs.cpu_compatible && changed_prefs.cpu_compatible)
				fill_prefetch_slow ();
			build_cpufunctbl ();
			changed = 1;
	}
	if (changed
		|| currprefs.m68k_speed != changed_prefs.m68k_speed
		|| currprefs.cpu_clock_multiplier != changed_prefs.cpu_clock_multiplier
		|| currprefs.cpu_frequency != changed_prefs.cpu_frequency) {
			currprefs.m68k_speed = changed_prefs.m68k_speed;
			reset_frame_rate_hack ();
			update_68k_cycles ();
			changed = 1;
	}

	if (currprefs.cpu_idle != changed_prefs.cpu_idle) {
		currprefs.cpu_idle = changed_prefs.cpu_idle;
	}
	if (changed)
		set_special (SPCFLAG_MODE_CHANGE);

}

void init_m68k (void)
{
	int i;

	//prefs_changed_cpu ();
	update_68k_cycles ();

	for (i = 0 ; i < 256 ; i++) {
		int j;
		for (j = 0 ; j < 8 ; j++) {
			if (i & (1 << j)) break;
		}
		movem_index1[i] = j;
		movem_index2[i] = 7-j;
		movem_next[i] = i & (~(1 << j));
	}

	write_log ("Building CPU table for configuration: %d", currprefs.cpu_model);
	regs.address_space_mask = 0xffffffff;
//	if (currprefs.cpu_compatible) {
//		if (currprefs.address_space_24 && currprefs.cpu_model >= 68030)
//			currprefs.address_space_24 = false;
//	}
	if (currprefs.fpu_model > 0)
		write_log ("/%d", currprefs.fpu_model);
	if (currprefs.cpu_cycle_exact) {
		if (currprefs.cpu_model == 68000)
			write_log (" prefetch and cycle-exact");
		else
			write_log (" ~cycle-exact");
	} else if (currprefs.cpu_compatible)
		write_log (" prefetch");
	if (currprefs.address_space_24) {
		regs.address_space_mask = 0x00ffffff;
		write_log (" 24-bit");
	}
	write_log ("\n");

	read_table68k ();
	do_merges ();

	write_log ("%d CPU functions\n", nr_cpuop_funcs);

	build_cpufunctbl ();
	set_x_funcs ();

}

struct regstruct regs, mmu_backup_regs;
struct flag_struct regflags;
static struct regstruct regs_backup[16];
static int backup_pointer = 0;
static long int m68kpc_offset;

#define get_ibyte_1(o) get_byte (regs.pc + (regs.pc_p - regs.pc_oldp) + (o) + 1)
#define get_iword_1(o) get_word (regs.pc + (regs.pc_p - regs.pc_oldp) + (o))
#define get_ilong_1(o) get_long (regs.pc + (regs.pc_p - regs.pc_oldp) + (o))

static uae_s32 ShowEA (FILE *f, uae_u16 opcode, int reg, amodes mode, wordsizes size, TCHAR *buf, uae_u32 *eaddr, int safemode)
{
	uae_u16 dp;
	uae_s8 disp8;
	uae_s16 disp16;
	int r;
	uae_u32 dispreg;
	uaecptr addr = 0;
	uae_s32 offset = 0;
	TCHAR buffer[80];

	switch (mode){
	case Dreg:
		_stprintf (buffer, "D%d", reg);
		break;
	case Areg:
		_stprintf (buffer, "A%d", reg);
		break;
	case Aind:
		_stprintf (buffer, "(A%d)", reg);
		addr = regs.regs[reg + 8];
		break;
	case Aipi:
		_stprintf (buffer, "(A%d)+", reg);
		addr = regs.regs[reg + 8];
		break;
	case Apdi:
		_stprintf (buffer, "-(A%d)", reg);
		addr = regs.regs[reg + 8];
		break;
	case Ad16:
		{
			TCHAR offtxt[80];
			disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
			if (disp16 < 0)
				_stprintf (offtxt, "-$%04x", -disp16);
			else
				_stprintf (offtxt, "$%04x", disp16);
			addr = m68k_areg (regs, reg) + disp16;
			_stprintf (buffer, "(A%d, %s) == $%08lx", reg, offtxt, (unsigned long)addr);
		}
		break;
	case Ad8r:
		dp = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? m68k_areg (regs, r) : m68k_dreg (regs, r);
		if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
		dispreg <<= (dp >> 9) & 3;

		if (dp & 0x100) {
			uae_s32 outer = 0, disp = 0;
			uae_s32 base = m68k_areg (regs, reg);
			TCHAR name[10];
			_stprintf (name, "A%d, ", reg);
			if (dp & 0x80) { base = 0; name[0] = 0; }
			if (dp & 0x40) dispreg = 0;
			if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x30) == 0x30) { disp = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }
			base += disp;

			if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x3) == 0x3) { outer = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }

			if (!(dp & 4)) base += dispreg;
			if ((dp & 3) && !safemode) base = get_long (base);
			if (dp & 4) base += dispreg;

			addr = base + outer;
			_stprintf (buffer, "(%s%c%d.%c*%d+%d)+%d == $%08lx", name,
				dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
				1 << ((dp >> 9) & 3),
				disp, outer,
				(unsigned long)addr);
		} else {
			addr = m68k_areg (regs, reg) + (uae_s32)((uae_s8)disp8) + dispreg;
			_stprintf (buffer, "(A%d, %c%d.%c*%d, $%02x) == $%08lx", reg,
				dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
				1 << ((dp >> 9) & 3), disp8,
				(unsigned long)addr);
		}
		break;
	case PC16:
		addr = m68k_getpc () + m68kpc_offset;
		disp16 = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		addr += (uae_s16)disp16;
		_stprintf (buffer, "(PC,$%04x) == $%08lx", disp16 & 0xffff, (unsigned long)addr);
		break;
	case PC8r:
		addr = m68k_getpc () + m68kpc_offset;
		dp = get_iword_1 (m68kpc_offset); m68kpc_offset += 2;
		disp8 = dp & 0xFF;
		r = (dp & 0x7000) >> 12;
		dispreg = dp & 0x8000 ? m68k_areg (regs, r) : m68k_dreg (regs, r);
		if (!(dp & 0x800)) dispreg = (uae_s32)(uae_s16)(dispreg);
		dispreg <<= (dp >> 9) & 3;

		if (dp & 0x100) {
			uae_s32 outer = 0, disp = 0;
			uae_s32 base = addr;
			TCHAR name[10];
			_stprintf (name, "PC, ");
			if (dp & 0x80) { base = 0; name[0] = 0; }
			if (dp & 0x40) dispreg = 0;
			if ((dp & 0x30) == 0x20) { disp = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x30) == 0x30) { disp = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }
			base += disp;

			if ((dp & 0x3) == 0x2) { outer = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset); m68kpc_offset += 2; }
			if ((dp & 0x3) == 0x3) { outer = get_ilong_1 (m68kpc_offset); m68kpc_offset += 4; }

			if (!(dp & 4)) base += dispreg;
			if ((dp & 3) && !safemode) base = get_long (base);
			if (dp & 4) base += dispreg;

			addr = base + outer;
			_stprintf (buffer, "(%s%c%d.%c*%d+%d)+%d == $%08lx", name,
				dp & 0x8000 ? 'A' : 'D', (int)r, dp & 0x800 ? 'L' : 'W',
				1 << ((dp >> 9) & 3),
				disp, outer,
				(unsigned long)addr);
		} else {
			addr += (uae_s32)((uae_s8)disp8) + dispreg;
			_stprintf (buffer, "(PC, %c%d.%c*%d, $%02x) == $%08lx", dp & 0x8000 ? 'A' : 'D',
				(int)r, dp & 0x800 ? 'L' : 'W',  1 << ((dp >> 9) & 3),
				disp8, (unsigned long)addr);
		}
		break;
	case absw:
		addr = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset);
		_stprintf (buffer, "$%08lx", (unsigned long)addr);
		m68kpc_offset += 2;
		break;
	case absl:
		addr = get_ilong_1 (m68kpc_offset);
		_stprintf (buffer, "$%08lx", (unsigned long)addr);
		m68kpc_offset += 4;
		break;
	case imm:
		switch (size){
		case sz_byte:
			_stprintf (buffer, "#$%02x", (unsigned int)(get_iword_1 (m68kpc_offset) & 0xff));
			m68kpc_offset += 2;
			break;
		case sz_word:
			_stprintf (buffer, "#$%04x", (unsigned int)(get_iword_1 (m68kpc_offset) & 0xffff));
			m68kpc_offset += 2;
			break;
		case sz_long:
			_stprintf (buffer, "#$%08lx", (unsigned long)(get_ilong_1 (m68kpc_offset)));
			m68kpc_offset += 4;
			break;
		default:
			break;
		}
		break;
	case imm0:
		offset = (uae_s32)(uae_s8)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		_stprintf (buffer, "#$%02x", (unsigned int)(offset & 0xff));
		break;
	case imm1:
		offset = (uae_s32)(uae_s16)get_iword_1 (m68kpc_offset);
		m68kpc_offset += 2;
		buffer[0] = 0;
		_stprintf (buffer, "#$%04x", (unsigned int)(offset & 0xffff));
		break;
	case imm2:
		offset = (uae_s32)get_ilong_1 (m68kpc_offset);
		m68kpc_offset += 4;
		_stprintf (buffer, "#$%08lx", (unsigned long)offset);
		break;
	case immi:
		offset = (uae_s32)(uae_s8)(reg & 0xff);
		_stprintf (buffer, "#$%08lx", (unsigned long)offset);
		break;
	default:
		break;
	}
	if (buf == 0)
		f_out (f, "%s", buffer);
	else
		_tcscat (buf, buffer);
	if (eaddr)
		*eaddr = addr;
	return offset;
}


int get_cpu_model (void)
{
	return currprefs.cpu_model;
}


void REGPARAM2 MakeSR (void)
{
	regs.sr = ((regs.t1 << 15) | (regs.t0 << 14)
		| (regs.s << 13) | (regs.m << 12) | (regs.intmask << 8)
		| (GET_XFLG () << 4) | (GET_NFLG () << 3)
		| (GET_ZFLG () << 2) | (GET_VFLG () << 1)
		|  GET_CFLG ());
}

void REGPARAM2 MakeFromSR (void)
{
	int oldm = regs.m;
	int olds = regs.s;

	if (currprefs.cpu_cycle_exact && currprefs.cpu_model >= 68020) {
		do_cycles_ce (6 * CYCLE_UNIT);
		regs.ce020memcycles = 0;
	}

	SET_XFLG ((regs.sr >> 4) & 1);
	SET_NFLG ((regs.sr >> 3) & 1);
	SET_ZFLG ((regs.sr >> 2) & 1);
	SET_VFLG ((regs.sr >> 1) & 1);
	SET_CFLG (regs.sr & 1);
	if (regs.t1 == ((regs.sr >> 15) & 1) &&
		regs.t0 == ((regs.sr >> 14) & 1) &&
		regs.s  == ((regs.sr >> 13) & 1) &&
		regs.m  == ((regs.sr >> 12) & 1) &&
		regs.intmask == ((regs.sr >> 8) & 7))
		return;
	regs.t1 = (regs.sr >> 15) & 1;
	regs.t0 = (regs.sr >> 14) & 1;
	regs.s  = (regs.sr >> 13) & 1;
	regs.m  = (regs.sr >> 12) & 1;
	regs.intmask = (regs.sr >> 8) & 7;
	if (currprefs.cpu_model >= 68020) {
		/* 68060 does not have MSP but does have M-bit.. */
		if (currprefs.cpu_model >= 68060)
			regs.msp = regs.isp;
		if (olds != regs.s) {
			if (olds) {
				if (oldm)
					regs.msp = m68k_areg (regs, 7);
				else
					regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
			}
		} else if (olds && oldm != regs.m) {
			if (oldm) {
				regs.msp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.isp;
			} else {
				regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.msp;
			}
		}
		if (currprefs.cpu_model >= 68060)
			regs.t0 = 0;
	} else {
		regs.t0 = regs.m = 0;
		if (olds != regs.s) {
			if (olds) {
				regs.isp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.usp;
			} else {
				regs.usp = m68k_areg (regs, 7);
				m68k_areg (regs, 7) = regs.isp;
			}
		}
	}
	if (currprefs.mmu_model)
		mmu_set_super (regs.s != 0);

	doint ();
	if (regs.t1 || regs.t0)
		set_special (SPCFLAG_TRACE);
	else
		/* Keep SPCFLAG_DOTRACE, we still want a trace exception for
		SR-modifying instructions (including STOP).  */
		unset_special (SPCFLAG_TRACE);
}

static void exception_trace (int nr)
{
	unset_special (SPCFLAG_TRACE | SPCFLAG_DOTRACE);
	if (regs.t1 && !regs.t0) {
		/* trace stays pending if exception is div by zero, chk,
		* trapv or trap #x
		*/
		if (nr == 5 || nr == 6 || nr == 7 || (nr >= 32 && nr <= 47))
			set_special (SPCFLAG_DOTRACE);
	}
	regs.t1 = regs.t0 = regs.m = 0;
}

static void exception_debug (int nr)
{
#ifdef DEBUGGER
	if (!exception_debugging)
		return;
	console_out_f ("Exception %d, PC=%08X\n", nr, M68K_GETPC);
	abort();
#endif
}

#ifdef CPUEMU_12

/* cycle-exact exception handler, 68000 only */

/*

Address/Bus Error:

- 6 idle cycles
- write PC low word
- write SR
- write PC high word
- write instruction word
- write fault address low word
- write status code
- write fault address high word
- 2 idle cycles
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

Division by Zero:

- 6 idle cycles
- write PC low word
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

Traps:

- 2 idle cycles
- write PC low word
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

TrapV:

- write PC low word
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

CHK:

- 6 idle cycles
- write PC low word
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

Illegal Instruction:

- 2 idle cycles
- write PC low word
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

Interrupt cycle diagram:

- 6 idle cycles
- write PC low word
- read exception number byte from (0xfffff1 | (interrupt number << 1))
- 4 idle cycles
- write SR
- write PC high word
- read exception address high word
- read exception address low word
- prefetch
- 2 idle cycles
- prefetch

*/

static void Exception_ce000 (int nr, uaecptr oldpc)
{
	uae_u32 currpc = m68k_getpc (), newpc;
	int sv = regs.s;
	int start, interrupt;

	start = 6;
	if (nr == 7) // TRAPV
		start = 0;
	else if (nr >= 32 && nr < 32 + 16) // TRAP #x
		start = 2;
	else if (nr == 4 || nr == 8) // ILLG & PRIVIL VIOL
		start = 2;
	interrupt = nr >= 24 && nr < 24 + 8;

	if (start)
		do_cycles_ce000 (start);

	exception_debug (nr);
	MakeSR ();

	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
		m68k_areg (regs, 7) = regs.isp;
		regs.s = 1;
	}
	if (nr == 2 || nr == 3) { /* 2=bus error, 3=address error */
		uae_u16 mode = (sv ? 4 : 0) | (last_instructionaccess_for_exception_3 ? 2 : 1);
		mode |= last_writeaccess_for_exception_3 ? 0 : 16;
		m68k_areg (regs, 7) -= 14;
		/* fixme: bit3=I/N */
		put_word_ce (m68k_areg (regs, 7) + 12, last_addr_for_exception_3);
		put_word_ce (m68k_areg (regs, 7) + 8, regs.sr);
		put_word_ce (m68k_areg (regs, 7) + 10, last_addr_for_exception_3 >> 16);
		put_word_ce (m68k_areg (regs, 7) + 6, last_op_for_exception_3);
		put_word_ce (m68k_areg (regs, 7) + 4, last_fault_for_exception_3);
		put_word_ce (m68k_areg (regs, 7) + 0, mode);
		put_word_ce (m68k_areg (regs, 7) + 2, last_fault_for_exception_3 >> 16);
		do_cycles_ce000 (2);
		write_log ("Exception %d (%x) at %x -> %x!\n", nr, oldpc, currpc, get_long (4 * nr));
		goto kludge_me_do;
	}
	m68k_areg (regs, 7) -= 6;
	put_word_ce (m68k_areg (regs, 7) + 4, currpc); // write low address
	if (interrupt) {
		// fetch interrupt vector number
		nr = get_byte_ce (0x00fffff1 | ((nr - 24) << 1));
		do_cycles_ce000 (4);
	}
	put_word_ce (m68k_areg (regs, 7) + 0, regs.sr); // write SR
	put_word_ce (m68k_areg (regs, 7) + 2, currpc >> 16); // write high address
kludge_me_do:
	newpc = get_word_ce (4 * nr) << 16; // read high address
	newpc |= get_word_ce (4 * nr + 2); // read low address
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			Reset_Cold(); /* there is nothing else we can do.. */
		else
			exception3 (regs.ir, newpc);
		return;
	}
	m68k_setpc (newpc);
	regs.ir = get_word_ce (m68k_getpc ()); // prefetch 1
	do_cycles_ce000 (2);
	regs.irc = get_word_ce (m68k_getpc () + 2); // prefetch 2
#ifdef JIT
	set_special (SPCFLAG_END_COMPILE);
#endif
	exception_trace (nr);
}
#endif

void cpu_halt (int e)
{
	write_log("HALT! @ 0x%X\n", m68k_getpc());
	abort();
}

static void Exception_build_stack_frame (uae_u32 oldpc, uae_u32 currpc, uae_u32 ssw, int nr, int format)
{
    int i;

    switch (format) {
        case 0x0: // four word stack frame
        case 0x1: // throwaway four word stack frame
            break;
        case 0x2: // six word stack frame
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), oldpc);
            break;
        case 0x7: // access error stack frame (68040)

			for (i = 3; i >= 0; i--) {
				// WB1D/PD0,PD1,PD2,PD3
                m68k_areg (regs, 7) -= 4;
                x_put_long (m68k_areg (regs, 7), mmu040_move16[i]);
			}

            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0); // WB1A
			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0); // WB2D
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.wb2_address); // WB2A
			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.wb3_data); // WB3D
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // WB3A

			m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // FA
            
			m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), 0);
            m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), regs.wb2_status);
            regs.wb2_status = 0;
            m68k_areg (regs, 7) -= 2;
            x_put_word (m68k_areg (regs, 7), regs.wb3_status);
            regs.wb3_status = 0;

			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), ssw);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), regs.mmu_effective_addr);
            break;
        case 0x9: // coprocessor mid-instruction stack frame (68020, 68030)
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), 0);
            m68k_areg (regs, 7) -= 4;
            x_put_long (m68k_areg (regs, 7), oldpc);
            break;
        case 0x3: // floating point post-instruction stack frame (68040)
        case 0x8: // bus and address error stack frame (68010)
            write_log(_T("Exception stack frame format %X not implemented\n"), format);
            return;
        case 0x4: // floating point unimplemented stack frame (68LC040, 68EC040)
				// or 68060 bus access fault stack frame
			if (currprefs.cpu_model == 68040) {
				// this is actually created in fpp.c
				write_log(_T("Exception stack frame format %X not implemented\n"), format);
				return;
			}
			// 68060 bus access fault
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fslw);
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);
			break;
		case 0xB: // long bus cycle fault stack frame (68020, 68030)
			// We always use B frame because it is easier to emulate,
			// our PC always points at start of instruction but A frame assumes
			// it is + 2 and handling this properly is not easy.
			// Store state information to internal register space
			for (i = 0; i < mmu030_idx + 1; i++) {
				m68k_areg (regs, 7) -= 4;
				x_put_long (m68k_areg (regs, 7), mmu030_ad[i].val);
			}
			while (i < 9) {
                uae_u32 v = 0;
				m68k_areg (regs, 7) -= 4;
                // mmu030_idx is always small enough instruction is FMOVEM.
                if (mmu030_state[1] & MMU030_STATEFLAG1_FMOVEM) {
                    if (i == 7)
                        v = mmu030_fmovem_store[0];
                    else if (i == 8)
                        v = mmu030_fmovem_store[1];
                }
                x_put_long (m68k_areg (regs, 7), v);
                i++;
			}
			 // version & internal information (We store index here)
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_idx);
			// 3* internal registers
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[2]);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[1]);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), mmu030_state[0]);
			// data input buffer = fault address
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);
			// 2xinternal
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);
			// stage b address
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mm030_stageb_address);
			// 2xinternal
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_disp_store[1]);
		/* fall through */
		case 0xA: // short bus cycle fault stack frame (68020, 68030)
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_disp_store[0]);
			m68k_areg (regs, 7) -= 4;
			 // Data output buffer = value that was going to be written
			x_put_long (m68k_areg (regs, 7), (mmu030_state[1] & MMU030_STATEFLAG1_MOVEM1) ? mmu030_data_buffer : mmu030_ad[mmu030_idx].val);
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), mmu030_opcode);  // Internal register (opcode storage)
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr); // data cycle fault address
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Instr. pipe stage B
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Instr. pipe stage C
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), ssw);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0);  // Internal register
			break;
		default:
            write_log(_T("Unknown exception stack frame format: %X\n"), format);
            return;
    }
    m68k_areg (regs, 7) -= 2;
    x_put_word (m68k_areg (regs, 7), (format << 12) | (nr * 4));
    m68k_areg (regs, 7) -= 4;
    x_put_long (m68k_areg (regs, 7), currpc);
    m68k_areg (regs, 7) -= 2;
    x_put_word (m68k_areg (regs, 7), regs.sr);
}

// 68030 MMU
static void Exception_mmu030 (int nr, uaecptr oldpc)
{
    uae_u32 currpc = m68k_getpc (), newpc;
    int sv = regs.s;
    
    exception_debug (nr);
    MakeSR ();
    
    if (!regs.s) {
        regs.usp = m68k_areg (regs, 7);
        m68k_areg(regs, 7) = regs.m ? regs.msp : regs.isp;
        regs.s = 1;
        mmu_set_super (1);
    }

    newpc = x_get_long (regs.vbr + 4 * nr);

	if (regs.m && nr >= 24 && nr < 32) { /* M + Interrupt */
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
        MakeSR(); /* this sets supervisor bit in status reg */
        regs.m = 0; /* clear the M bit (but frame 0x1 uses sr with M bit set) */
        regs.msp = m68k_areg (regs, 7);
        m68k_areg (regs, 7) = regs.isp;
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x1);
    } else if (nr ==5 || nr == 6 || nr == 7 || nr == 9 || nr == 56) {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x2);
    } else if (nr == 2) {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr,  0xB);
    } else if (nr == 3) {
		regs.mmu_fault_addr = last_fault_for_exception_3;
		mmu030_state[0] = mmu030_state[1] = 0;
		mmu030_data_buffer = 0;
        Exception_build_stack_frame (last_fault_for_exception_3, currpc, MMU030_SSW_RW | MMU030_SSW_SIZE_W | (regs.s ? 6 : 2), nr,  0xA);
    } else {
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x0);
    }
    
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			cpu_halt (2);
		else
			exception3 (regs.ir, newpc);
		return;
	}
	m68k_setpc (newpc);
	exception_trace (nr);
}


// 68040/060 MMU
static void Exception_mmu (int nr, uaecptr oldpc)
{
	uae_u32 currpc = m68k_getpc (), newpc;
	int sv = regs.s;

	exception_debug (nr);
	MakeSR ();

	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
		if (currprefs.cpu_model == 68060) {
			m68k_areg (regs, 7) = regs.isp;
			if (nr >= 24 && nr < 32)
				regs.m = 0;
		} else if (currprefs.cpu_model >= 68020) {
			m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
		} else {
			m68k_areg (regs, 7) = regs.isp;
		}
		regs.s = 1;
		mmu_set_super (1);
	}
    
	newpc = x_get_long (regs.vbr + 4 * nr);

	if (nr == 2) { // bus error
        //write_log (_T("Exception_mmu %08x %08x %08x\n"), currpc, oldpc, regs.mmu_fault_addr);
        if (currprefs.mmu_model == 68040)
			Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x7);
		else
			Exception_build_stack_frame(oldpc, currpc, regs.mmu_fslw, nr, 0x4);
	} else if (nr == 3) { // address error
        Exception_build_stack_frame(last_fault_for_exception_3, currpc, 0, nr, 0x2);
		write_log (_T("Exception %d (%x) at %x -> %x!\n"), nr, last_fault_for_exception_3, currpc, get_long (regs.vbr + 4 * nr));
	} else if (nr == 5 || nr == 6 || nr == 7 || nr == 9) {
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x2);
	} else if (regs.m && nr >= 24 && nr < 32) { /* M + Interrupt */
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
        MakeSR(); /* this sets supervisor bit in status reg */
        regs.m = 0; /* clear the M bit (but frame 0x1 uses sr with M bit set) */
        regs.msp = m68k_areg (regs, 7);
        m68k_areg (regs, 7) = regs.isp;
        Exception_build_stack_frame (oldpc, currpc, regs.mmu_ssw, nr, 0x1);
	} else if (nr == 61) {
        Exception_build_stack_frame(oldpc, regs.instruction_pc, regs.mmu_ssw, nr, 0x0);
	} else {
        Exception_build_stack_frame(oldpc, currpc, regs.mmu_ssw, nr, 0x0);
	}
    
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			cpu_halt (2);
		else
			exception3 (regs.ir, newpc);
		return;
	}
	m68k_setpc (newpc);
	exception_trace (nr);
}

/* Handle exceptions. We need a special case to handle MFP exceptions */
/* on Atari ST, because it's possible to change the MFP's vector base */
/* and get a conflict with 'normal' cpu exceptions. */
static void Exception_normal (int nr, uaecptr oldpc, int ExceptionSource)
{
	uae_u32 currpc = m68k_getpc (), newpc;
	int sv = regs.s;



	exception_debug (nr);
	MakeSR ();

	/* Change to supervisor mode if necessary */
	if (!regs.s) {
		regs.usp = m68k_areg (regs, 7);
		if (currprefs.cpu_model >= 68020)
			m68k_areg (regs, 7) = regs.m ? regs.msp : regs.isp;
		else
			m68k_areg (regs, 7) = regs.isp;
		regs.s = 1;
		if (currprefs.mmu_model)
			mmu_set_super (regs.s != 0);
	}
	if (currprefs.cpu_model > 68000) {
		/* Build additional exception stack frame for 68010 and higher */
		/* (special case for MFP) */
		
		if (nr == 2 || nr == 3) {
			int i;
			if (currprefs.cpu_model >= 68040) {
				if (nr == 2) {
					// bus error
					if (currprefs.mmu_model) {

						for (i = 0 ; i < 7 ; i++) {
							m68k_areg (regs, 7) -= 4;
							x_put_long (m68k_areg (regs, 7), 0);
						}
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), regs.wb3_data);
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), regs.wb3_status);
						regs.wb3_status = 0;
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), regs.mmu_ssw);
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), regs.mmu_fault_addr);

						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0x7000 + nr * 4);
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), oldpc);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), regs.sr);
						newpc = x_get_long (regs.vbr + 4 * nr);
						if (newpc & 1) {
							if (nr == 2 || nr == 3)
								uae_reset (1); /* there is nothing else we can do.. */
							else
								exception3 (regs.ir, newpc);
							return;
						}
						m68k_setpc (newpc);

						exception_trace (nr);
						return;

					} else {

						for (i = 0 ; i < 18 ; i++) {
							m68k_areg (regs, 7) -= 2;
							x_put_word (m68k_areg (regs, 7), 0);
						}
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), last_fault_for_exception_3);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0x0140 | (sv ? 6 : 2)); /* SSW */
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), last_addr_for_exception_3);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), 0x7000 + nr * 4);
						m68k_areg (regs, 7) -= 4;
						x_put_long (m68k_areg (regs, 7), oldpc);
						m68k_areg (regs, 7) -= 2;
						x_put_word (m68k_areg (regs, 7), regs.sr);
						goto kludge_me_do;

					}

				} else {
					m68k_areg (regs, 7) -= 4;
					x_put_long (m68k_areg (regs, 7), last_fault_for_exception_3);
					m68k_areg (regs, 7) -= 2;
					x_put_word (m68k_areg (regs, 7), 0x2000 + nr * 4);
				}
			} else {
				// address error
				uae_u16 ssw = (sv ? 4 : 0) | (last_instructionaccess_for_exception_3 ? 2 : 1);
				ssw |= last_writeaccess_for_exception_3 ? 0 : 0x40;
				ssw |= 0x20;
				for (i = 0 ; i < 36; i++) {
					m68k_areg (regs, 7) -= 2;
					x_put_word (m68k_areg (regs, 7), 0);
				}
				m68k_areg (regs, 7) -= 4;
				x_put_long (m68k_areg (regs, 7), last_fault_for_exception_3);
				m68k_areg (regs, 7) -= 2;
				x_put_word (m68k_areg (regs, 7), 0);
				m68k_areg (regs, 7) -= 2;
				x_put_word (m68k_areg (regs, 7), 0);
				m68k_areg (regs, 7) -= 2;
				x_put_word (m68k_areg (regs, 7), 0);
				m68k_areg (regs, 7) -= 2;
				x_put_word (m68k_areg (regs, 7), ssw);
				m68k_areg (regs, 7) -= 2;
				x_put_word (m68k_areg (regs, 7), 0xb000 + nr * 4);
			}
			write_log ("Exception %d (%x) at %x -> %x!\n", nr, oldpc, currpc, x_get_long (regs.vbr + 4*nr));
		} else if (nr ==5 || nr == 6 || nr == 7 || nr == 9) {
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), oldpc);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0x2000 + nr * 4);
		} else if (regs.m && nr >= 24 && nr < 32) { /* M + Interrupt */
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), nr * 4);
			m68k_areg (regs, 7) -= 4;
			x_put_long (m68k_areg (regs, 7), currpc);
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), regs.sr);
			regs.sr |= (1 << 13);
			regs.msp = m68k_areg (regs, 7);
            regs.m = 0;
			m68k_areg (regs, 7) = regs.isp;
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), 0x1000 + nr * 4);
		} else {
			m68k_areg (regs, 7) -= 2;
			x_put_word (m68k_areg (regs, 7), nr * 4);
		}
	} else if (nr == 2 || nr == 3) {
		uae_u16 mode = (sv ? 4 : 0) | (last_instructionaccess_for_exception_3 ? 2 : 1);
		mode |= last_writeaccess_for_exception_3 ? 0 : 16;
		m68k_areg (regs, 7) -= 14;
		/* fixme: bit3=I/N */
		x_put_word (m68k_areg (regs, 7) + 0, mode);
		x_put_long (m68k_areg (regs, 7) + 2, last_fault_for_exception_3);
		x_put_word (m68k_areg (regs, 7) + 6, last_op_for_exception_3);
		x_put_word (m68k_areg (regs, 7) + 8, regs.sr);
		x_put_long (m68k_areg (regs, 7) + 10, last_addr_for_exception_3);
		write_log ("Exception %d (%x) at %x -> %x!\n", nr, oldpc, currpc, x_get_long (regs.vbr + 4*nr));
		goto kludge_me_do;
	}

	/* Push PC on stack: */
	m68k_areg (regs, 7) -= 4;
	x_put_long (m68k_areg (regs, 7), currpc);
	/* Push SR on stack: */
	m68k_areg (regs, 7) -= 2;
	x_put_word (m68k_areg (regs, 7), regs.sr);
kludge_me_do:
	newpc = x_get_long (regs.vbr + 4 * nr);
	if (newpc & 1) {
		if (nr == 2 || nr == 3)
			uae_reset (1); /* there is nothing else we can do.. */
		else
			exception3 (regs.ir, newpc);
		return;
	}
	m68k_setpc (newpc);
	fill_prefetch_slow ();
	exception_trace (nr);
	
    /* Handle exception cycles (special case for MFP) */
    if (ExceptionSource == M68000_EXC_SRC_INT_MFP)
    {
      M68000_AddCycles(44+12);			/* MFP interrupt, 'nr' can be in a different range depending on $fffa17 */
    }
    else if (nr >= 24 && nr <= 31)
    {
      if ( nr == 26 )				/* HBL */
      {
        /* store current cycle pos when then interrupt was received (see video.c) */
  //      LastCycleHblException = Cycles_GetCounter(CYCLES_COUNTER_VIDEO);
        M68000_AddCycles(44+12);		/* Video Interrupt */
      }
      else if ( nr == 28 ) 			/* VBL */
        M68000_AddCycles(44+12);		/* Video Interrupt */
      else
        M68000_AddCycles(44+4);			/* Other Interrupts */
    }
    else if(nr >= 32 && nr <= 47)
    {
      M68000_AddCycles(34-4);			/* Trap (total is 34, but cpuemu.c already adds 4) */
    }
    else switch(nr)
    {
      case 2: M68000_AddCycles(50); break;	/* Bus error */
      case 3: M68000_AddCycles(50); break;	/* Address error */
      case 4: M68000_AddCycles(34); break;	/* Illegal instruction */
      case 5: M68000_AddCycles(38); break;	/* Div by zero */
      case 6: M68000_AddCycles(40); break;	/* CHK */
      case 7: M68000_AddCycles(34); break;	/* TRAPV */
      case 8: M68000_AddCycles(34); break;	/* Privilege violation */
      case 9: M68000_AddCycles(34); break;	/* Trace */
      case 10: M68000_AddCycles(34); break;	/* Line-A - probably wrong */
      case 11: M68000_AddCycles(34); break;	/* Line-F - probably wrong */
      default:
        /* FIXME: Add right cycles value for MFP interrupts and copro exceptions ... */
        if(nr < 64)
          M68000_AddCycles(4);			/* Coprocessor and unassigned exceptions (???) */
        else
          M68000_AddCycles(44+12);		/* Must be a MFP or DSP interrupt */
        break;
}

}

/* Handle exceptions. We need a special case to handle MFP exceptions */
/* on Atari ST, because it's possible to change the MFP's vector base */
/* and get a conflict with 'normal' cpu exceptions. */
void REGPARAM2 ExceptionL (int nr/*, int ExceptionSource*/)
{
    int ExceptionSource = 0;
	uaecptr oldpc = m68k_getpc ();
    if (currprefs.mmu_model && currprefs.cpu_model == 68030)
        Exception_mmu030(nr, oldpc);
    else if (currprefs.mmu_model)
        Exception_mmu (nr, oldpc); // Todo: add ExceptionSource
    else
        Exception_normal (nr, oldpc, ExceptionSource);
}
void REGPARAM2 Exception (int nr)
{
	ExceptionL (nr/*, -1*/);
}

STATIC_INLINE void do_interrupt (int nr, int Pending)
{

	regs.stopped = 0;
	unset_special (SPCFLAG_STOP);
	assert (nr < 8 && nr >= 0);

	/* On Hatari, only video ints are using SPCFLAG_INT (see m68000.c) */
	Exception (nr + 24);

	regs.intmask = nr;
	doint ();

	set_special (SPCFLAG_INT);
	/* Handle Atari ST's specific jitter for hbl/vbl */
	InterruptAddJitter (nr , Pending);
}


void NMI (void)
{
	do_interrupt (7, false);
}

void m68k_reset (int hardreset)
{
    regs.spcflags &= (SPCFLAG_MODE_CHANGE | SPCFLAG_BRK);
	regs.ipl = regs.ipl_pin = 0;
	regs.s = 1;
	regs.m = 0;
	regs.stopped = 0;
	regs.t1 = 0;
	regs.t0 = 0;
	SET_ZFLG (0);
	SET_XFLG (0);
	SET_CFLG (0);
	SET_VFLG (0);
	SET_NFLG (0);
	regs.intmask = 7;
	regs.vbr = regs.sfc = regs.dfc = 0;
	regs.irc = 0xffff;
	
	m68k_areg (regs, 7) = get_long (0);
	m68k_setpc (get_long (4));

	fpu_reset ();

	regs.caar = regs.cacr = 0;
	regs.itt0 = regs.itt1 = regs.dtt0 = regs.dtt1 = 0;
	regs.tcr = regs.mmusr = regs.urp = regs.srp = regs.buscr = 0;
	if (currprefs.cpu_model == 68020) {
		regs.cacr |= 8;
		set_cpu_caches (0);
	}

	mmufixup[0].reg = -1;
	mmufixup[1].reg = -1;
	if (currprefs.mmu_model) {
        if (currprefs.cpu_model >= 68040) {
            mmu_reset ();
            mmu_set_tc (regs.tcr);
            mmu_set_super (regs.s != 0);
        }
        else {
            mmu030_reset(hardreset);
        }
    }

	/* 68060 FPU is not compatible with 68040,
     * 68060 accelerators' boot ROM disables the FPU
     */
	regs.pcr = 0;
	if (currprefs.cpu_model == 68060) {
		regs.pcr = currprefs.fpu_model == 68060 ? MC68060_PCR : MC68EC060_PCR;
		regs.pcr |= (currprefs.cpu060_revision & 0xff) << 8;
	}
	fill_prefetch_slow ();
}

void REGPARAM2 op_unimpl (uae_u16 opcode)
{
	static int warned;
	if (warned < 20) {
		write_log (_T("68060 unimplemented opcode %04X, PC=%08x\n"), opcode, regs.instruction_pc);
		warned++;
	}
	ExceptionL (61/*, regs.instruction_pc*/);
}

uae_u32 REGPARAM2 op_illg (uae_u32 opcode)
{
	uaecptr pc = m68k_getpc ();
	static int warned;

	if (warned < 20) {
		write_log ("Illegal instruction: %04x at %08X -> %08X\n", opcode, pc, get_long (regs.vbr + 0x10));
		warned++;
		//activate_debugger();
	}

	if ((opcode & 0xF000)==0xA000)
		Exception (10);
	else 
	if ((opcode & 0xF000)==0xF000)
		Exception (11);
	else
		Exception (4);
	return 4;
}

void mmu_op30 (uaecptr pc, uae_u32 opcode, uae_u16 extra, uaecptr extraa)
{
	if (currprefs.cpu_model != 68030) {
		m68k_setpc (pc);
		op_illg (opcode);
		return;
	}
	if (extra & 0x8000)
		mmu_op30_ptest (pc, opcode, extra, extraa);
	else if ((extra&0xE000)==0x2000 && (extra & 0x1C00))
		mmu_op30_pflush (pc, opcode, extra, extraa);
    else if ((extra&0xE000)==0x2000 && !(extra & 0x1C00))
        mmu_op30_pload (pc, opcode, extra, extraa);
	else
		mmu_op30_pmove (pc, opcode, extra, extraa);
}

void mmu_op (uae_u32 opcode, uae_u32 extra)
{
	if (currprefs.cpu_model) {
		mmu_op_real (opcode, extra);
		return;
	}
#if MMUOP_DEBUG > 1
	write_log ("mmu_op %04X PC=%08X\n", opcode, m68k_getpc ());
#endif
	if ((opcode & 0xFE0) == 0x0500) {
		/* PFLUSH */
		regs.mmusr = 0;
#if MMUOP_DEBUG > 0
		write_log ("PFLUSH\n");
#endif
		return;
	} else if ((opcode & 0x0FD8) == 0x548) {
		if (currprefs.cpu_model < 68060) { /* PTEST not in 68060 */
			/* PTEST */
#if MMUOP_DEBUG > 0
			write_log ("PTEST\n");
#endif
			return;
		}
	} else if ((opcode & 0x0FB8) == 0x588) {
		/* PLPA */
		if (currprefs.cpu_model == 68060) {
#if MMUOP_DEBUG > 0
			write_log ("PLPA\n");
#endif
			return;
		}
	}
#if MMUOP_DEBUG > 0
	write_log ("Unknown MMU OP %04X\n", opcode);
#endif
	m68k_setpc (m68k_getpc () - 2);
	op_illg (opcode);
}

static uaecptr last_trace_ad = 0;

static void do_trace (void)
{
	if (regs.t0 && currprefs.cpu_model >= 68020) {
		uae_u16 opcode;
		/* should also include TRAP, CHK, SR modification FPcc */
		/* probably never used so why bother */
		/* We can afford this to be inefficient... */
		m68k_setpc (m68k_getpc ());
		fill_prefetch_slow ();
		opcode = x_get_word (regs.pc);
		if (opcode == 0x4e73 			/* RTE */
			|| opcode == 0x4e74 		/* RTD */
			|| opcode == 0x4e75 		/* RTS */
			|| opcode == 0x4e77 		/* RTR */
			|| opcode == 0x4e76 		/* TRAPV */
			|| (opcode & 0xffc0) == 0x4e80 	/* JSR */
			|| (opcode & 0xffc0) == 0x4ec0 	/* JMP */
			|| (opcode & 0xff00) == 0x6100	/* BSR */
			|| ((opcode & 0xf000) == 0x6000	/* Bcc */
			&& cctrue ((opcode >> 8) & 0xf))
			|| ((opcode & 0xf0f0) == 0x5050	/* DBcc */
			&& !cctrue ((opcode >> 8) & 0xf)
			&& (uae_s16)m68k_dreg (regs, opcode & 7) != 0))
		{
			last_trace_ad = m68k_getpc ();
			unset_special (SPCFLAG_TRACE);
			set_special (SPCFLAG_DOTRACE);
		}
	} else if (regs.t1) {
		last_trace_ad = m68k_getpc ();
		unset_special (SPCFLAG_TRACE);
		set_special (SPCFLAG_DOTRACE);
	}
}


// handle interrupt delay (few cycles)
STATIC_INLINE int time_for_interrupt (void)
{
	if (regs.ipl > regs.intmask || regs.ipl == 7) {
		return 1;
	}
	return 0;
}

void doint (void)
{
	if (currprefs.cpu_cycle_exact) {
		regs.ipl_pin = intlev ();
		set_special (SPCFLAG_INT);
		return;
	}
	if (currprefs.cpu_compatible)
		set_special (SPCFLAG_INT);
	else
		set_special (SPCFLAG_DOINT);
}

#define IDLETIME (currprefs.cpu_idle * sleep_resolution / 700)

/*
 * Compute the number of jitter cycles to add when a video interrupt occurs
 * (this is specific to the Atari ST)
 */
STATIC_INLINE void InterruptAddJitter (int Level , int Pending)
{
	}


/*
 * Handle special flags
 */

static bool do_specialties_interrupt (int Pending)
{
    return false;					/* no interrupt was found */
}

STATIC_INLINE int do_specialties (int cycles)
{
    
	if(regs.spcflags & SPCFLAG_EXTRA_CYCLES) {
		/* Add some extra cycles to simulate a wait state */
		unset_special(SPCFLAG_EXTRA_CYCLES);
		M68000_AddCycles(nWaitStateCycles);
		nWaitStateCycles = 0;
	}

	if (regs.spcflags & SPCFLAG_DOTRACE)
		Exception (9);

	/*if (regs.spcflags & SPCFLAG_TRAP) {
		unset_special (SPCFLAG_TRAP);
		Exception (3);
	}*/

    /* Handle the STOP instruction */
    if ( regs.spcflags & SPCFLAG_STOP ) {
        /* We first test if there's a pending interrupt that would */
        /* allow to immediatly leave the STOP state */
        if ( do_specialties_interrupt(true) ) {		/* test if there's an interrupt and add pending jitter */
            regs.stopped = 0;
            unset_special (SPCFLAG_STOP);
	}

	while (regs.spcflags & SPCFLAG_STOP) {

	    /* Take care of quit event if needed */
	    if (regs.spcflags & SPCFLAG_BRK)
			return 1;
	
		do_cycles (currprefs.cpu_cycle_exact ? 2 * CYCLE_UNIT : 4 * CYCLE_UNIT);
		M68000_AddCycles(cpu_cycles * 2 / CYCLE_UNIT);

	    /* It is possible one or more ints happen at the same time */
	    /* We must process them during the same cpu cycle until the special INT flag is set */
		while (PendingInterruptCount<=0 && PendingInterruptFunction) {
			/* 1st, we call the interrupt handler */
			CALL_VAR(PendingInterruptFunction);
		
			/* Then we check if this handler triggered an interrupt to process */
			if ( do_specialties_interrupt(false) ) {	/* test if there's an interrupt and add non pending jitter */
				regs.stopped = 0;
				unset_special (SPCFLAG_STOP);
				break;
			}

		if (currprefs.cpu_cycle_exact) {
			ipl_fetch ();
			if (time_for_interrupt ()) {
					do_interrupt (regs.ipl, true);
			}
		} else {

		}
		if ((regs.spcflags & (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE))) {
			unset_special (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE);
			// SPCFLAG_BRK breaks STOP condition, need to prefetch
			m68k_resumestopped ();
			return 1;
		}

		if (currprefs.cpu_idle && currprefs.m68k_speed != 0 && ((regs.spcflags & SPCFLAG_STOP)) == SPCFLAG_STOP) {
			/* sleep 1ms if STOP-instruction is executed */
			if (1) {
				static int sleepcnt, lvpos, zerocnt;
				if (vpos != lvpos) {
					sleepcnt--;
					lvpos = vpos;
					if (sleepcnt < 0) {
							/*sleepcnt = IDLETIME / 2; */  /* Laurent : badly removed for now */
						sleep_millis (1);
					}
				}
			}
		}
	}
	}
	}

	if (regs.spcflags & SPCFLAG_TRACE)
		do_trace ();

	if (currprefs.cpu_cycle_exact) {
		if (time_for_interrupt ()) {
			do_interrupt (regs.ipl, true);
		}
	} else {
	}

	if (regs.spcflags & SPCFLAG_DOINT) {
		unset_special (SPCFLAG_DOINT);
		set_special (SPCFLAG_INT);
	}

	if ( do_specialties_interrupt(false) ) {	/* test if there's an interrupt and add non pending jitter */
		/* TODO: Always do do_specialties_interrupt() in m68k_run_x instead? */
		regs.stopped = 0;
	}

	/*if (regs.spcflags & SPCFLAG_DEBUGGER)
		DebugCpu_Check();*/

	if ((regs.spcflags & (SPCFLAG_BRK | SPCFLAG_MODE_CHANGE))) {
		unset_special(SPCFLAG_MODE_CHANGE);
		return 1;
	}
	return 0;
}



#ifndef CPUEMU_11

static void m68k_run_1 (void)
{
}

#else

/* It's really sad to have two almost identical functions for this, but we
do it all for performance... :(
This version emulates 68000's prefetch "cache" */
static void m68k_run_1 (void)
{
	struct regstruct *r = &regs;

	for (;;) {
		uae_u32 opcode = r->ir;

		count_instr (opcode);

	/*m68k_dumpstate(stderr, NULL);*/
	if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
	{
	    int FrameCycles, HblCounterVideo, LineCycles;

	    Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );

	    LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
	    m68k_disasm(stderr, m68k_getpc (), NULL, 1);
	}


		do_cycles (cpu_cycles);
		cpu_cycles = (*cpufunctbl[opcode])(opcode);
		cpu_cycles &= cycles_mask;
		cpu_cycles |= cycles_val;
		
		/* We can have several interrupts at the same time before the next CPU instruction */
		/* We must check for pending interrupt and call do_specialties_interrupt() only */
		/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
		/* and prevent exiting the STOP state when calling do_specialties() after. */
		/* For performance, we first test PendingInterruptCount, then regs.spcflags */
		while ( ( PendingInterruptCount <= 0 ) && ( PendingInterruptFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
			CALL_VAR(PendingInterruptFunction);		/* call the interrupt handler */
			do_specialties_interrupt(false);		/* test if there's an mfp/video interrupt and add non pending jitter */
		}
		
		if (r->spcflags) {
			if (do_specialties (cpu_cycles))
				return;
		}
		regs.ipl = regs.ipl_pin;
		if (!currprefs.cpu_compatible || (currprefs.cpu_cycle_exact && currprefs.cpu_model == 68000))
			return;
	}
}

#endif /* CPUEMU_11 */

#ifndef CPUEMU_12

static void m68k_run_1_ce (void)
{
}

#else

/* cycle-exact m68k_run () */

static void m68k_run_1_ce (void)
{
	struct regstruct *r = &regs;

	ipl_fetch ();
	for (;;) {
		uae_u32 opcode = r->ir;

		/*m68k_dumpstate(stderr, NULL);*/
		if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
			m68k_disasm(stderr, m68k_getpc (), NULL, 1);
		}


		(*cpufunctbl[opcode])(opcode);
		if (r->spcflags) {
			if (do_specialties (0))
				return;
		}
		if (!currprefs.cpu_cycle_exact || currprefs.cpu_model > 68000)
			return;
	}
}
#endif

// Previous MMU 68030
static void m68k_run_mmu030 (void)
{
	uae_u16 opcode;
	uaecptr pc;
	struct flag_struct f;
	m68k_exception save_except;
    int intr = 0;
    int lastintr = 0;

	mmu030_opcode_stageb = -1;
retry:
	TRY (prb) {
		for (;;) {
			int cnt;
insretry:
			pc = regs.instruction_pc = m68k_getpc ();
			//printf("PC: 0x%X\n", pc);
			f.cznv = regflags.cznv;
			f.x = regflags.x;

			mmu030_state[0] = mmu030_state[1] = mmu030_state[2] = 0;
			mmu030_opcode = -1;
			if (mmu030_opcode_stageb < 0) {
				opcode = get_iword_mmu030 (0);
			} else {
				opcode = mmu030_opcode_stageb;
				mmu030_opcode_stageb = -1;
			}

			mmu030_opcode = opcode;
			mmu030_ad[0].done = false;

			cnt = 50;
			for (;;) {
				opcode = mmu030_opcode;
				mmu030_idx = 0;
				count_instr (opcode);
				do_cycles (cpu_cycles);
				mmu030_retry = false;
				cpu_cycles = (*cpufunctbl[opcode])(opcode);
				cnt--; // so that we don't get in infinite loop if things go horribly wrong
				if (!mmu030_retry)
					break;
				if (cnt < 0) {
					cpu_halt (9);
					break;
				}
				if (mmu030_retry && mmu030_opcode == -1)
					goto insretry; // urgh
			}

			mmu030_opcode = -1;

			M68000_AddCycles(cpu_cycles * 2 / CYCLE_UNIT);

			if (regs.spcflags & SPCFLAG_EXTRA_CYCLES) {
				/* Add some extra cycles to simulate a wait state */
				unset_special(SPCFLAG_EXTRA_CYCLES);
				M68000_AddCycles(nWaitStateCycles);
				nWaitStateCycles = 0;
			}

			/* We can have several interrupts at the same time before the next CPU instruction */
			/* We must check for pending interrupt and call do_specialties_interrupt() only */
			/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
			/* and prevent exiting the STOP state when calling do_specialties() after. */
			/* For performance, we first test PendingInterruptCount, then regs.spcflags */
			while ( ( PendingInterruptCount <= 0 ) && ( PendingInterruptFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
				CALL_VAR(PendingInterruptFunction);		/* call the interrupt handler */
				do_specialties_interrupt(false);		/* test if there's an mfp/video interrupt and add non pending jitter */
			}

            /* Previous: for now we poll the interrupt pins with every instruction.
             * TODO: only do this when an actual interrupt is active to not
             * unneccessarily slow down emulation.
             */
            intr = intlev ();
            if (intr>regs.intmask || (intr==7 && intr>lastintr)) {
                do_interrupt (intr, false);
            }
            lastintr = intr;

			if (regs.spcflags) {
				if (do_specialties (cpu_cycles* 2 / CYCLE_UNIT))
					return;
			}
		}
	} CATCH (prb) {
		save_except = __exvalue;

		regflags.cznv = f.cznv;
		regflags.x = f.x;

		m68k_setpc (regs.instruction_pc);

		if (mmufixup[0].reg >= 0) {
			m68k_areg (regs, mmufixup[0].reg) = mmufixup[0].value;
			mmufixup[0].reg = -1;
		}
		if (mmufixup[1].reg >= 0) {
			m68k_areg (regs, mmufixup[1].reg) = mmufixup[1].value;
			mmufixup[1].reg = -1;
		}

		TRY (prb2) {
			Exception (save_except);
		} CATCH (prb2) {
			cpu_halt (1);
			return;
		} ENDTRY
	} ENDTRY
    goto retry;
}

#ifndef CPUEMU_0

static void m68k_run_2 (void)
{
}




/* Aranym MMU 68040  */
static void m68k_run_mmu040 (void)
{
	uae_u16 opcode;
	struct flag_struct f;
	uaecptr pc;
	m68k_exception save_except;
    int intr = 0;
    int lastintr = 0;
	
	for (;;) {
	TRY (prb) {
		for (;;) {
			f.cznv = regflags.cznv;
			f.x = regflags.x;
			mmu_restart = true;
			pc = regs.instruction_pc = m68k_getpc ();

			do_cycles (cpu_cycles);

			mmu_opcode = -1;
			mmu_opcode = opcode = x_prefetch (0);
			count_instr (opcode);
			cpu_cycles = (*cpufunctbl[opcode])(opcode);

			//DSP_Run(cpu_cycles * 4 / CYCLE_UNIT);

			M68000_AddCycles(cpu_cycles * 2 / CYCLE_UNIT);

			if (regs.spcflags & SPCFLAG_EXTRA_CYCLES) {
				/* Add some extra cycles to simulate a wait state */
				unset_special(SPCFLAG_EXTRA_CYCLES);
				M68000_AddCycles(nWaitStateCycles);
				nWaitStateCycles = 0;
			}

			/* We can have several interrupts at the same time before the next CPU instruction */
			/* We must check for pending interrupt and call do_specialties_interrupt() only */
			/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
			/* and prevent exiting the STOP state when calling do_specialties() after. */
			/* For performance, we first test PendingInterruptCount, then regs.spcflags */
			while ( ( PendingInterruptCount <= 0 ) && ( PendingInterruptFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
				CALL_VAR(PendingInterruptFunction);		/* call the interrupt handler */
				do_specialties_interrupt(false);		/* test if there's an mfp/video interrupt and add non pending jitter */
			}

            /* Previous: for now we poll the interrupt pins with every instruction.
             * TODO: only do this when an actual interrupt is active to not
             * unneccessarily slow down emulation.
             */
            intr = intlev ();
            if (intr>regs.intmask || (intr==7 && intr>lastintr)) {
                do_interrupt (intr, false);
            }
            lastintr = intr;
            
            
			if (regs.spcflags) {
				if (do_specialties (cpu_cycles* 2 / CYCLE_UNIT))
					return;
			}
		} // end of for(;;)
	} CATCH (prb) {
		save_except = __exvalue;

		if (mmu_restart) {
			/* restore state if instruction restart */
			regflags.cznv = f.cznv;
			regflags.x = f.x;
			m68k_setpc (regs.instruction_pc);
		}

		if (mmufixup[0].reg >= 0) {
			m68k_areg (regs, mmufixup[0].reg) = mmufixup[0].value;
			mmufixup[0].reg = -1;
		}

		TRY (prb) {
			Exception (save_except);
		} CATCH (prb) {
    			fprintf(stderr, "[FATAL] double fault");	
			abort();		
		} ENDTRY

	} ENDTRY
	}
}

#else

/* "cycle exact" 68020/030  */
#define MAX68020CYCLES 4
static void m68k_run_2ce (void)
{
	struct regstruct *r = &regs;
	int tmpcycles = MAX68020CYCLES;

	ipl_fetch ();
	for (;;) {
		uae_u32 opcode = 0;		// 25/12/2013 - Strict C (pre 1999) does not allow variables to be declared after the start of a scoping brace
		/*m68k_dumpstate(stderr, NULL);*/
		if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
			m68k_disasm(stderr, m68k_getpc (), NULL, 1);
		}
		opcode = x_prefetch (0);
		(*cpufunctbl[opcode])(opcode);
		if (r->ce020memcycles > 0) {
			tmpcycles = CYCLE_UNIT * MAX68020CYCLES;
			do_cycles_ce (r->ce020memcycles);
			r->ce020memcycles = 0;
		}
		if (r->spcflags) {
			if (do_specialties (0))
				return;
		}
		tmpcycles -= cpucycleunit;
		if (tmpcycles <= 0) {
			do_cycles_ce (1 * CYCLE_UNIT);
			tmpcycles = CYCLE_UNIT * MAX68020CYCLES;;
		}
		regs.ipl = regs.ipl_pin;
	}
}

/* emulate simple prefetch  */
static void m68k_run_2p (void)
{
	uae_u32 prefetch, prefetch_pc;
	struct regstruct *r = &regs;

	prefetch_pc = m68k_getpc ();
	prefetch = get_longi (prefetch_pc);
	for (;;) {
		uae_u32 opcode;
		uae_u32 pc = m68k_getpc ();

		/*m68k_dumpstate(stderr, NULL);*/
		if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
			m68k_disasm(stderr, m68k_getpc (), NULL, 1);
		}

#if DEBUG_CD32CDTVIO
		out_cd32io (m68k_getpc ());
#endif

		do_cycles (cpu_cycles);

		if (pc == prefetch_pc)
			opcode = prefetch >> 16;
		else if (pc == prefetch_pc + 2)
			opcode = prefetch & 0xffff;
		else
			opcode = get_wordi (pc);

		count_instr (opcode);

		prefetch_pc = m68k_getpc () + 2;
		prefetch = get_longi (prefetch_pc);
		cpu_cycles = (*cpufunctbl[opcode])(opcode);
		cpu_cycles &= cycles_mask;
		cpu_cycles |= cycles_val;

		M68000_AddCycles(cpu_cycles * 2 / CYCLE_UNIT);

		if (regs.spcflags & SPCFLAG_EXTRA_CYCLES) {
			/* Add some extra cycles to simulate a wait state */
			unset_special(SPCFLAG_EXTRA_CYCLES);
			M68000_AddCycles(nWaitStateCycles);
			nWaitStateCycles = 0;
		}
		
		/* We can have several interrupts at the same time before the next CPU instruction */
		/* We must check for pending interrupt and call do_specialties_interrupt() only */
		/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
		/* and prevent exiting the STOP state when calling do_specialties() after. */
		/* For performance, we first test PendingInterruptCount, then regs.spcflags */
		while ( ( PendingInterruptCount <= 0 ) && ( PendingInterruptFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
			CALL_VAR(PendingInterruptFunction);		/* call the interrupt handler */
			do_specialties_interrupt(false);		/* test if there's an mfp/video interrupt and add non pending jitter */
		}
		
		if (r->spcflags) {
			if (do_specialties (cpu_cycles* 2 / CYCLE_UNIT))
				return;
		}
	}
}


//static int used[65536];

/* Same thing, but don't use prefetch to get opcode.  */
static void m68k_run_2 (void)
{
	struct regstruct *r = &regs;

	for (;;) {
		uae_u32 opcode = get_iword (0);
		count_instr (opcode);

		/*m68k_dumpstate(stderr, NULL);*/
		if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
			m68k_disasm(stderr, m68k_getpc (), NULL, 1);
		}

		do_cycles (cpu_cycles);
		cpu_cycles = (*cpufunctbl[opcode])(opcode);
		cpu_cycles &= cycles_mask;
		cpu_cycles |= cycles_val;

		M68000_AddCycles(cpu_cycles * 2 / CYCLE_UNIT);

		if (regs.spcflags & SPCFLAG_EXTRA_CYCLES) {
			/* Add some extra cycles to simulate a wait state */
			unset_special(SPCFLAG_EXTRA_CYCLES);
			M68000_AddCycles(nWaitStateCycles);
			nWaitStateCycles = 0;
		}

		/* We can have several interrupts at the same time before the next CPU instruction */
		/* We must check for pending interrupt and call do_specialties_interrupt() only */
		/* if the cpu is not in the STOP state. Else, the int could be acknowledged now */
		/* and prevent exiting the STOP state when calling do_specialties() after. */
		/* For performance, we first test PendingInterruptCount, then regs.spcflags */
		while ( ( PendingInterruptCount <= 0 ) && ( PendingInterruptFunction ) && ( ( regs.spcflags & SPCFLAG_STOP ) == 0 ) ) {
			CALL_VAR(PendingInterruptFunction);		/* call the interrupt handler */
			do_specialties_interrupt(false);		/* test if there's an mfp/video interrupt and add non pending jitter */
		}
		
		
		if (r->spcflags) {
			if (do_specialties (cpu_cycles* 2 / CYCLE_UNIT))
				return;
		}
	}
}


/* fake MMU 68k  */
static void m68k_run_mmu (void)
{
	for (;;) {
		uae_u32 opcode = 0;		// 25/12/2013 - Strict C adhearance (if your going to use C99 IMO you may as well use C++!)
		/*m68k_dumpstate(stderr, NULL);*/
		if (LOG_TRACE_LEVEL(TRACE_CPU_DISASM))
		{
			int FrameCycles, HblCounterVideo, LineCycles;
			Video_GetPosition ( &FrameCycles , &HblCounterVideo , &LineCycles );
			LOG_TRACE_PRINT ( "cpu video_cyc=%6d %3d@%3d : " , FrameCycles, LineCycles, HblCounterVideo );
			m68k_disasm(stderr, m68k_getpc (), NULL, 1);
		}
		opcode = get_iword (0);
		do_cycles (cpu_cycles);
		mmu_backup_regs = regs;
		cpu_cycles = (*cpufunctbl[opcode])(opcode);
		cpu_cycles &= cycles_mask;
		cpu_cycles |= cycles_val;
		if (mmu_triggered)
			mmu_do_hit ();
		if (regs.spcflags) {
			if (do_specialties (cpu_cycles))
				return;
		}
	}
}

#endif /* CPUEMU_0 */

int in_m68k_go = 0;

static void exception2_handle (uaecptr addr, uaecptr fault)
{
	last_addr_for_exception_3 = addr;
	last_fault_for_exception_3 = fault;
	last_writeaccess_for_exception_3 = 0;
	last_instructionaccess_for_exception_3 = 0;
	Exception (2);
}

void m68k_go (int may_quit)
{
	int hardboot = 1;

	if (in_m68k_go || !may_quit) {
		write_log ("Bug! m68k_go is not reentrant.\n");
		abort ();
	}

	reset_frame_rate_hack ();
	update_68k_cycles ();

	in_m68k_go++;
	for (;;) {
		void (*run_func)(void);
		
		if (regs.spcflags & SPCFLAG_BRK) {
			unset_special(SPCFLAG_BRK);
				break;
		}

			quit_program = 0;
			hardboot = 0;





	set_x_funcs ();
	
#if 0
	if (mmu_enabled && !currprefs.cachesize) {
			//run_func = m68k_run_mmu030;
			run_func = m68k_run_mmu;
		} else { /*
			run_func = currprefs.cpu_cycle_exact && currprefs.cpu_model == 68000 ? m68k_run_1_ce :
				currprefs.cpu_compatible && currprefs.cpu_model == 68000 ? m68k_run_1 :
				currprefs.cpu_model >= 68030 && currprefs.mmu_model ? m68k_run_mmu040 :
				currprefs.cpu_model >= 68020 && currprefs.cpu_cycle_exact ? m68k_run_2ce :
				currprefs.cpu_compatible ? m68k_run_2p : m68k_run_2;
				*/
#endif
			run_func=currprefs.cpu_model == 68040 ? m68k_run_mmu040 : m68k_run_mmu030;
#if 0
		}
#endif
		run_func ();
	}
	in_m68k_go--;
}


static const TCHAR *ccnames[] =
{ "T ","F ","HI","LS","CC","CS","NE","EQ",
"VC","VS","PL","MI","GE","LT","GT","LE" };

static void addmovemreg (TCHAR *out, int *prevreg, int *lastreg, int *first, int reg)
{
	TCHAR *p = out + _tcslen (out);
	if (*prevreg < 0) {
		*prevreg = reg;
		*lastreg = reg;
		return;
	}
	if ((*prevreg) + 1 != reg || (reg & 8) != ((*prevreg & 8))) {
		_stprintf (p, "%s%c%d", (*first) ? "" : "/", (*lastreg) < 8 ? 'D' : 'A', (*lastreg) & 7);
		p = p + _tcslen (p);
		if ((*lastreg) + 2 == reg) {
			_stprintf (p, "/%c%d", (*prevreg) < 8 ? 'D' : 'A', (*prevreg) & 7);
		} else if ((*lastreg) != (*prevreg)) {
			_stprintf (p, "-%c%d", (*prevreg) < 8 ? 'D' : 'A', (*prevreg) & 7);
		}
		*lastreg = reg;
		*first = 0;
	}
	*prevreg = reg;
}

static void movemout (TCHAR *out, uae_u16 mask, int mode)
{
	unsigned int dmask, amask;
	int prevreg = -1, lastreg = -1, first = 1;

	if (mode == Apdi) {
		int i;
		uae_u8 dmask2 = (mask >> 8) & 0xff;
		uae_u8 amask2 = mask & 0xff;
		dmask = 0;
		amask = 0;
		for (i = 0; i < 8; i++) {
			if (dmask2 & (1 << i))
				dmask |= 1 << (7 - i);
			if (amask2 & (1 << i))
				amask |= 1 << (7 - i);
		}
	} else {
		dmask = mask & 0xff;
		amask = (mask >> 8) & 0xff;
	}
	while (dmask) { addmovemreg (out, &prevreg, &lastreg, &first, movem_index1[dmask]); dmask = movem_next[dmask]; }
	while (amask) { addmovemreg (out, &prevreg, &lastreg, &first, movem_index1[amask] + 8); amask = movem_next[amask]; }
	addmovemreg (out, &prevreg, &lastreg, &first, -1);
}

static void disasm_size (TCHAR *instrname, struct instr *dp)
{
	switch (dp->size)
	{
	case sz_byte:
		_tcscat (instrname, ".B ");
		break;
	case sz_word:
		_tcscat (instrname, ".W ");
		break;
	case sz_long:
		_tcscat (instrname, ".L ");
		break;
	default:
		_tcscat (instrname, "   ");
		break;
	}
}

static void m68k_disasm_2 (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr, int safemode)
{
	uaecptr newpc = 0;
	m68kpc_offset = addr - m68k_getpc ();

	if (!table68k)
		return;
	while (cnt-- > 0) {
		TCHAR instrname[100], *ccpt;
		int i;
		uae_u32 opcode;
		struct mnemolookup *lookup;
		struct instr *dp;
		int oldpc;

		oldpc = m68kpc_offset;
		opcode = get_iword_1 (m68kpc_offset);
		if (cpufunctbl[opcode] == op_illg_1) {
			opcode = 0x4AFC;
		}
		dp = table68k + opcode;
		for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++)
			;

		fprintf(f, "%08lX ", m68k_getpc () + m68kpc_offset);
		m68kpc_offset += 2;

		if (strcmp(lookup->friendlyname, ""))
			_tcscpy (instrname, lookup->friendlyname);
		else
			_tcscpy (instrname, lookup->name);
		ccpt = _tcsstr (instrname, "cc");
		if (ccpt != 0) {
			_tcsncpy (ccpt, ccnames[dp->cc], 2);
		}
		disasm_size (instrname, dp);

		if (lookup->mnemo == i_MOVEC2 || lookup->mnemo == i_MOVE2C) {
			uae_u16 imm = get_iword_1 (m68kpc_offset);
			uae_u16 creg = imm & 0x0fff;
			uae_u16 r = imm >> 12;
			TCHAR regs[16];
			const TCHAR *cname = "?";
			int i;
			for (i = 0; m2cregs[i].regname; i++) {
				if (m2cregs[i].regno == creg)
					break;
			}
			_stprintf (regs, "%c%d", r >= 8 ? 'A' : 'D', r >= 8 ? r - 8 : r);
			if (m2cregs[i].regname)
				cname = m2cregs[i].regname;
			if (lookup->mnemo == i_MOVE2C) {
				_tcscat (instrname, regs);
				_tcscat (instrname, ",");
				_tcscat (instrname, cname);
			} else {
				_tcscat (instrname, cname);
				_tcscat (instrname, ",");
				_tcscat (instrname, regs);
			}
			m68kpc_offset += 2;
		} else if (lookup->mnemo == i_MVMEL) {
			newpc = m68k_getpc () + m68kpc_offset;
			m68kpc_offset += 2;
			newpc += ShowEA (0, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
			_tcscat (instrname, ",");
			movemout (instrname, get_iword_1 (oldpc + 2), dp->dmode);
		} else if (lookup->mnemo == i_MVMLE) {
			m68kpc_offset += 2;
			movemout (instrname, get_iword_1 (oldpc + 2), dp->dmode);
			_tcscat (instrname, ",");
			newpc = m68k_getpc () + m68kpc_offset;
			newpc += ShowEA (0, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
		} else {
			if (dp->suse) {
				newpc = m68k_getpc () + m68kpc_offset;
				newpc += ShowEA (0, opcode, dp->sreg, dp->smode, dp->size, instrname, seaddr, safemode);
			}
			if (dp->suse && dp->duse)
				_tcscat (instrname, ",");
			if (dp->duse) {
				newpc = m68k_getpc () + m68kpc_offset;
				newpc += ShowEA (0, opcode, dp->dreg, dp->dmode, dp->size, instrname, deaddr, safemode);
			}
		}

		for (i = 0; i < (m68kpc_offset - oldpc) / 2; i++) {
			fprintf(f, "%04x ", get_iword_1 (oldpc + i * 2));
		}

		while (i++ < 5)
			fprintf(f, "%s", "     ");

		fprintf(f, "%s", instrname);

		if (ccpt != 0) {
			if (deaddr)
				*deaddr = newpc;
			if (cctrue (dp->cc))
				fprintf(f, " == $%08X (T)", newpc);
			else
				fprintf(f, " == $%08X (F)", newpc);
		} else if ((opcode & 0xff00) == 0x6100) { /* BSR */
			if (deaddr)
				*deaddr = newpc;
			fprintf(f, " == $%08X", newpc);
		}
		fprintf(f, "%s", "\n");
	}
	if (nextpc)
		*nextpc = m68k_getpc () + m68kpc_offset;
}

void m68k_disasm_ea (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr)
{
	m68k_disasm_2 (f, addr, nextpc, cnt, seaddr, deaddr, 1);
}
void m68k_disasm (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt)
{
	m68k_disasm_2 (f, addr, nextpc, cnt, NULL, NULL, 0);
}

/*************************************************************
Disasm the m68kcode at the given address into instrname
and instrcode
*************************************************************/
void sm68k_disasm (TCHAR *instrname, TCHAR *instrcode, uaecptr addr, uaecptr *nextpc)
{
	TCHAR *ccpt;
	uae_u32 opcode;
	struct mnemolookup *lookup;
	struct instr *dp;
	int oldpc;

	uaecptr newpc = 0;

	m68kpc_offset = addr - m68k_getpc ();

	oldpc = m68kpc_offset;
	opcode = get_iword_1 (m68kpc_offset);
	if (cpufunctbl[opcode] == op_illg_1) {
		opcode = 0x4AFC;
	}
	dp = table68k + opcode;
	for (lookup = lookuptab;lookup->mnemo != dp->mnemo; lookup++);

	m68kpc_offset += 2;

	_tcscpy (instrname, lookup->name);
	ccpt = _tcsstr (instrname, "cc");
	if (ccpt != 0) {
		_tcsncpy (ccpt, ccnames[dp->cc], 2);
	}
	switch (dp->size){
	case sz_byte: _tcscat (instrname, ".B "); break;
	case sz_word: _tcscat (instrname, ".W "); break;
	case sz_long: _tcscat (instrname, ".L "); break;
	default: _tcscat (instrname, "   "); break;
	}

	if (dp->suse) {
		newpc = m68k_getpc () + m68kpc_offset;
		newpc += ShowEA (0, opcode, dp->sreg, dp->smode, dp->size, instrname, NULL, 0);
	}
	if (dp->suse && dp->duse)
		_tcscat (instrname, ",");
	if (dp->duse) {
		newpc = m68k_getpc () + m68kpc_offset;
		newpc += ShowEA (0, opcode, dp->dreg, dp->dmode, dp->size, instrname, NULL, 0);
	}

	if (instrcode)
	{
		int i;
		for (i = 0; i < (m68kpc_offset - oldpc) / 2; i++)
		{
			_stprintf (instrcode, "%04x ", get_iword_1 (oldpc + i * 2));
			instrcode += _tcslen (instrcode);
		}
	}

	if (nextpc)
		*nextpc = m68k_getpc () + m68kpc_offset;
}

struct cpum2c m2cregs[] = {
	{ 0, "SFC" },
	{ 1, "DFC" },
	{ 2, "CACR" },
	{ 3, "TC" },
	{ 4, "ITT0" },
	{ 5, "ITT1" },
	{ 6, "DTT0" },
	{ 7, "DTT1" },
	{ 8, "BUSC" },
	{ 0x800, "USP" },
	{ 0x801, "VBR" },
	{ 0x802, "CAAR" },
	{ 0x803, "MSP" },
	{ 0x804, "ISP" },
	{ 0x805, "MMUS" },
	{ 0x806, "URP" },
	{ 0x807, "SRP" },
	{ 0x808, "PCR" },
	{ -1, NULL }
};

void m68k_dumpstate (FILE *f, uaecptr *nextpc)
{
	int i, j;

	for (i = 0; i < 8; i++){
		f_out (f, "  D%d %08lX ", i, m68k_dreg (regs, i));
		if ((i & 3) == 3) f_out (f, "\n");
	}
	for (i = 0; i < 8; i++){
		f_out (f, "  A%d %08lX ", i, m68k_areg (regs, i));
		if ((i & 3) == 3) f_out (f, "\n");
	}
	if (regs.s == 0)
		regs.usp = m68k_areg (regs, 7);
	if (regs.s && regs.m)
		regs.msp = m68k_areg (regs, 7);
	if (regs.s && regs.m == 0)
		regs.isp = m68k_areg (regs, 7);
	j = 2;
	f_out (f, "USP  %08X ISP  %08X ", regs.usp, regs.isp);
	for (i = 0; m2cregs[i].regno>= 0; i++) {
		if (!movec_illg (m2cregs[i].regno)) {
			if (!_tcscmp (m2cregs[i].regname, "USP") || !_tcscmp (m2cregs[i].regname, "ISP"))
				continue;
			if (j > 0 && (j % 4) == 0)
				f_out (f, "\n");
			f_out (f, "%-4s %08X ", m2cregs[i].regname, val_move2c (m2cregs[i].regno));
			j++;
		}
	}
	if (j > 0)
		f_out (f, "\n");
	f_out (f, "T=%d%d S=%d M=%d X=%d N=%d Z=%d V=%d C=%d IMASK=%d STP=%d\n",
		regs.t1, regs.t0, regs.s, regs.m,
		GET_XFLG (), GET_NFLG (), GET_ZFLG (),
		GET_VFLG (), GET_CFLG (),
		regs.intmask, regs.stopped);
#ifdef FPUEMU
	if (currprefs.fpu_model) {
		uae_u32 fpsr;
		for (i = 0; i < 8; i++){
			f_out (f, "FP%d: %g ", i, regs.fp[i]);
			if ((i & 3) == 3)
				f_out (f, "\n");
		}
		fpsr = get_fpsr ();
		f_out (f, "N=%d Z=%d I=%d NAN=%d\n",
			(fpsr & 0x8000000) != 0,
			(fpsr & 0x4000000) != 0,
			(fpsr & 0x2000000) != 0,
			(fpsr & 0x1000000) != 0);
	}
#endif
    if (currprefs.mmu_model == 68030) {
        f_out (f, "SRP: %llX CRP: %llX\n", srp_030, crp_030);
        f_out (f, "TT0: %08X TT1: %08X TC: %08X\n", tt0_030, tt1_030, tc_030);
    }
	if (currprefs.cpu_compatible && currprefs.cpu_model == 68000) {
		struct instr *dp;
		struct mnemolookup *lookup1, *lookup2;
		dp = table68k + regs.irc;
		for (lookup1 = lookuptab; lookup1->mnemo != dp->mnemo; lookup1++);
		dp = table68k + regs.ir;
		for (lookup2 = lookuptab; lookup2->mnemo != dp->mnemo; lookup2++);
		f_out (f, "Prefetch %04x (%s) %04x (%s)\n", regs.irc, lookup1->name, regs.ir, lookup2->name);
	}

	m68k_disasm (f, m68k_getpc (), nextpc, 1);
	if (nextpc)
		f_out (f, "Next PC: %08lx\n", *nextpc);
}

static void exception3f (uae_u32 opcode, uaecptr addr, int writeaccess, int instructionaccess, uaecptr pc)
{
	if (currprefs.cpu_model >= 68040)
		addr &= ~1;
	if (currprefs.cpu_model >= 68020) {
		if (pc == 0xffffffff)
			last_addr_for_exception_3 = regs.instruction_pc;
		else
			last_addr_for_exception_3 = pc;
	} else if (pc == 0xffffffff) {
		last_addr_for_exception_3 = m68k_getpc () + 2;
	} else {
		last_addr_for_exception_3 = pc;
	}
	last_fault_for_exception_3 = addr;
	last_op_for_exception_3 = opcode;
	last_writeaccess_for_exception_3 = writeaccess;
	last_instructionaccess_for_exception_3 = instructionaccess;
	Exception (3);
}

void exception3 (uae_u32 opcode, uaecptr addr)
{
	exception3f (opcode, addr, 0, 0, 0xffffffff);
}
void exception3i (uae_u32 opcode, uaecptr addr)
{
	exception3f (opcode, addr, 0, 1, 0xffffffff);
}
void exception3b (uae_u32 opcode, uaecptr addr, bool w, bool i, uaecptr pc)
{
	exception3f (opcode, addr, w, i, pc);
}

void exception2 (uaecptr addr, bool read, int size, uae_u32 fc)
{
	if (currprefs.mmu_model) {
		if (currprefs.mmu_model == 68030) {
			uae_u32 flags = size == 1 ? MMU030_SSW_SIZE_B : (size == 2 ? MMU030_SSW_SIZE_W : MMU030_SSW_SIZE_L);
			mmu030_page_fault (addr, read, flags, fc);
		} else {
			//mmu_bus_error (addr, fc, read == false, size, false, 0);
			mmu_bus_error (addr, fc, read == false, size, false, 0, true);
		}
	} else {
		// simple version
		exception2_handle (addr, addr);
	}
}

void exception2_fake (uaecptr addr)
{
	write_log ("delayed exception2!\n");
	regs.panic_pc = m68k_getpc ();
	regs.panic_addr = addr;
	regs.panic = 2;
	set_special (SPCFLAG_BRK);
	m68k_setpc (0xf80000);

	fill_prefetch_slow ();
}

void cpureset (void)
{
	uaecptr pc;
	uaecptr ksboot = 0xf80002 - 2; /* -2 = RESET hasn't increased PC yet */
	uae_u16 ins;

	if (currprefs.cpu_compatible || currprefs.cpu_cycle_exact) {
//		customreset (0);
		customreset ();
		return;
	}
	pc = m68k_getpc ();
	if (pc >= currprefs.chipmem_size) {
//		addrbank *b = &get_mem_bank (pc);
//		if (b->check (pc, 2 + 2)) {
			/* We have memory, hope for the best.. */
//			customreset (0);
//			customreset ();
//			return;
//		}
		write_log ("M68K RESET PC=%x, rebooting..\n", pc);
//		customreset (0);
		customreset ();
		m68k_setpc (ksboot);
		return;
	}
	/* panic, RAM is going to disappear under PC */
	ins = get_word (pc + 2);
	if ((ins & ~7) == 0x4ed0) {
		int reg = ins & 7;
		uae_u32 addr = m68k_areg (regs, reg);
		write_log ("reset/jmp (ax) combination emulated -> %x\n", addr);
//		customreset (0);
		customreset ();
		if (addr < 0x80000)
			addr += 0xf80000;
		m68k_setpc (addr - 2);
		return;
	}
	write_log ("M68K RESET PC=%x, rebooting..\n", pc);
//	customreset (0);
	customreset ();
	m68k_setpc (ksboot);
}


void m68k_setstopped (void)
{
	regs.stopped = 1;
	/* A traced STOP instruction drops through immediately without
	actually stopping.  */
	if ((regs.spcflags & SPCFLAG_DOTRACE) == 0)
		set_special (SPCFLAG_STOP);
	else
		m68k_resumestopped ();
}

void m68k_resumestopped (void)
{
	if (!regs.stopped)
		return;
	regs.stopped = 0;
	if (currprefs.cpu_cycle_exact) {
		if (currprefs.cpu_model == 68000)
			do_cycles_ce000 (6);
	}
	fill_prefetch_slow ();
	unset_special (SPCFLAG_STOP);
}

#if 1

STATIC_INLINE void fill_cache040 (uae_u32 addr)
{
	int index, i, lws;
	uae_u32 tag;
	uae_u32 data;
	struct cache040 *c;
	static int linecnt;
	int line = 0;		// 25/12/2013 - Strict C compliance KVK

	addr &= ~15;
	index = (addr >> 4) & (CACHESETS040 - 1);
	tag = regs.s | (addr & ~((CACHESETS040 << 4) - 1));
	lws = (addr >> 2) & 3;
	c = &caches040[index];
	for (i = 0; i < CACHELINES040; i++) {
		if (c->valid[i] && c->tag[i] == tag) {
			// cache hit
			regs.prefetch020addr[0] = addr;
			regs.prefetch020data[0] = c->data[i][lws];
			return;
		}
	}
	// cache miss
	data = mem_access_delay_longi_read_ce020 (addr);
	line = linecnt;
	for (i = 0; i < CACHELINES040; i++) {
		int line = (linecnt + i) & (CACHELINES040 - 1);
		if (c->tag[i] != tag || c->valid[i] == false) {
			c->tag[i] = tag;
			c->valid[i] = true;
			c->data[i][0] = data;
		}
	}
	regs.prefetch020addr[0] = addr;
	regs.prefetch020data[0] = data;
}

// this one is really simple and easy
STATIC_INLINE void fill_icache020 (uae_u32 addr, int idx)
{
	int index;
	uae_u32 tag;
	uae_u32 data;
	struct cache020 *c;

	addr &= ~3;
	index = (addr >> 2) & (CACHELINES020 - 1);
	tag = regs.s | (addr & ~((CACHELINES020 << 2) - 1));
	c = &caches020[index];
	if (c->valid && c->tag == tag) {
		// cache hit
		regs.prefetch020addr[idx] = addr;
		regs.prefetch020data[idx] = c->data;
		return;
	}
	// cache miss
	data = mem_access_delay_longi_read_ce020 (addr);
	if (!(regs.cacr & 2)) {
		c->tag = tag;
		c->valid = !!(regs.cacr & 1);
		c->data = data;
	}
	regs.prefetch020addr[idx] = addr;
	regs.prefetch020data[idx] = data;
}

uae_u32 get_word_ce020_prefetch (int o)
{
	int i;
	uae_u32 pc = m68k_getpc () + o;

	for (;;) {
		for (i = 0; i < 2; i++) {
			if (pc == regs.prefetch020addr[0]) {
				uae_u32 v = regs.prefetch020data[0] >> 16;
				fill_icache020 (regs.prefetch020addr[0] + 4, 1);
				return v;
			}
			if (pc == regs.prefetch020addr[0] + 2) {
				uae_u32 v = regs.prefetch020data[0] & 0xffff;
				if (regs.prefetch020addr[1] == regs.prefetch020addr[0] + 4) {
					regs.prefetch020addr[0] = regs.prefetch020addr[1];
					regs.prefetch020data[0] = regs.prefetch020data[1];
					fill_icache020 (regs.prefetch020addr[0] + 4, 1);
				} else {
					fill_icache020 (pc + 4, 0);
					fill_icache020 (regs.prefetch020addr[0] + 4, 1);
				}
				return v;
			}
			regs.prefetch020addr[0] = regs.prefetch020addr[1];
			regs.prefetch020data[0] = regs.prefetch020data[1];
		}
		fill_icache020 (pc + 0, 0);
		fill_icache020 (pc + 4, 1);
	}
}

// 68030 caches aren't so simple as 68020 cache..
STATIC_INLINE struct cache030 *getcache030 (struct cache030 *cp, uaecptr addr, uae_u32 *tagp, int *lwsp)
{
	int index, lws;
	uae_u32 tag;
	struct cache030 *c;

	addr &= ~3;
	index = (addr >> 4) & (CACHELINES030 - 1);
	tag = regs.s | (addr & ~((CACHELINES030 << 4) - 1));
	lws = (addr >> 2) & 3;
	c = &cp[index];
	*tagp = tag;
	*lwsp = lws;
	return c;
}

STATIC_INLINE void update_cache030 (struct cache030 *c, uae_u32 val, uae_u32 tag, int lws)
{
	if (c->tag != tag)
		c->valid[0] = c->valid[1] = c->valid[2] = c->valid[3] = false;
	c->tag = tag;
	c->valid[lws] = true;
	c->data[lws] = val;
}

STATIC_INLINE void fill_icache030 (uae_u32 addr, int idx)
{
	int lws;
	uae_u32 tag;
	uae_u32 data;
	struct cache030 *c;

	addr &= ~3;
	c = getcache030 (icaches030, addr, &tag, &lws);
	if (c->valid[lws] && c->tag == tag) {
		// cache hit
		regs.prefetch020addr[idx] = addr;
		regs.prefetch020data[idx] = c->data[lws];
		return;
	}
	// cache miss
	data = mem_access_delay_longi_read_ce020 (addr);
	if ((regs.cacr & 3) == 1) { // not frozen and enabled
		update_cache030 (c, data, tag, lws);
	}
	regs.prefetch020addr[idx] = addr;
	regs.prefetch020data[idx] = data;
}

STATIC_INLINE bool cancache030 (uaecptr addr)
{
	return ce_cachable[addr >> 16] != 0;
}

// and finally the worst part, 68030 data cache..
void write_dcache030 (uaecptr addr, uae_u32 val, int size)
{
	struct cache030 *c1, *c2;
	int lws1, lws2;
	uae_u32 tag1, tag2;
	int aligned = addr & 3;

	if (!(regs.cacr & 0x100) || currprefs.cpu_model == 68040) // data cache disabled? 68040 shares this too.
		return;
	if (!cancache030 (addr))
		return;

	c1 = getcache030 (dcaches030, addr, &tag1, &lws1);
	if (!(regs.cacr & 0x2000)) { // write allocate
		if (c1->tag != tag1 || c1->valid[lws1] == false)
			return;
	}

	// easy one
	if (size == 2 && aligned == 0) {
		update_cache030 (c1, val, tag1, lws1);
		return;
	}
	// argh!! merge partial write
	c2 = getcache030 (dcaches030, addr + 4, &tag2, &lws2);
	if (size == 2) {
		if (c1->valid[lws1] && c1->tag == tag1) {
			c1->data[lws1] &= ~(0xffffffff >> (aligned * 8));
			c1->data[lws1] |= val >> (aligned * 8);
		}
		if (c2->valid[lws2] && c2->tag == tag2) {
			c2->data[lws2] &= 0xffffffff >> ((4 - aligned) * 8);
			c2->data[lws2] |= val << ((4 - aligned) * 8);
		}
	} else if (size == 1) {
		val <<= 16;
		if (c1->valid[lws1] && c1->tag == tag1) {
			c1->data[lws1] &= ~(0xffff0000 >> (aligned * 8));
			c1->data[lws1] |= val >> (aligned * 8);
		}
		if (c2->valid[lws2] && c2->tag == tag2 && aligned == 3) {
			c2->data[lws2] &= 0x00ffffff;
			c2->data[lws2] |= val << 8;
		}
	} else if (size == 0) {
		val <<= 24;
		if (c1->valid[lws1] && c1->tag == tag1) {
			c1->data[lws1] &= ~(0xff000000 >> (aligned * 8));
			c1->data[lws1] |= val >> (aligned * 8);
		}
	}
}

uae_u32 read_dcache030 (uaecptr addr, int size)
{
	struct cache030 *c1, *c2;
	int lws1, lws2;
	uae_u32 tag1, tag2;
	int aligned = addr & 3;
	int len = (1 << size) * 8;
	uae_u32 v1, v2;

	if (!(regs.cacr & 0x100) || currprefs.cpu_model == 68040 || !cancache030 (addr)) { // data cache disabled? shared with 68040 "ce"
		if (size == 2)
			return mem_access_delay_long_read_ce020 (addr);
		else if (size == 1)
			return mem_access_delay_word_read_ce020 (addr);
		else
			return mem_access_delay_byte_read_ce020 (addr);
	}

	c1 = getcache030 (dcaches030, addr, &tag1, &lws1);
	addr &= ~3;
	if (!c1->valid[lws1] || c1->tag != tag1) {
		v1 = mem_access_delay_long_read_ce020 (addr);
		update_cache030 (c1, v1, tag1, lws1);
	} else {
		v1 = c1->data[lws1];
		if (get_long (addr) != v1) {
			write_log ("data cache mismatch %d %d %08x %08x != %08x %08x %d PC=%08x\n",
				size, aligned, addr, get_long (addr), v1, tag1, lws1, M68K_GETPC);
			v1 = get_long (addr);
		}
	}
	// only one long fetch needed?
	if (size == 0) {
		v1 >>= (3 - aligned) * 8;
		return v1;
	} else if (size == 1 && aligned <= 2) {
		v1 >>= (2 - aligned) * 8;
		return v1;
	} else if (size == 2 && aligned == 0) {
		return v1;
	}
	// need two longs
	addr += 4;
	c2 = getcache030 (dcaches030, addr, &tag2, &lws2);
	if (!c2->valid[lws2] || c2->tag != tag2) {
		v2 = mem_access_delay_long_read_ce020 (addr);
		update_cache030 (c2, v2, tag2, lws2);
	} else {
		v2 = c2->data[lws2];
		if (get_long (addr) != v2) {
			write_log ("data cache mismatch %d %d %08x %08x != %08x %08x %d PC=%08x\n",
				size, aligned, addr, get_long (addr), v2, tag2, lws2, M68K_GETPC);
			v2 = get_long (addr);
		}
	}
	if (size == 1 && aligned == 3)
		return (v1 << 8) | (v2 >> 24);
	else if (size == 2 && aligned == 1)
		return (v1 << 8) | (v2 >> 24);
	else if (size == 2 && aligned == 2)
		return (v1 << 16) | (v2 >> 16);
	else if (size == 2 && aligned == 3)
		return (v1 << 24) | (v2 >> 8);

	write_log ("dcache030 weirdness!?\n");
	return 0;
}

uae_u32 get_word_ce030_prefetch (int o)
{
	int i;
	uae_u32 pc = m68k_getpc () + o;

	for (;;) {
		for (i = 0; i < 2; i++) {
			if (pc == regs.prefetch020addr[0]) {
				uae_u32 v = regs.prefetch020data[0] >> 16;
				fill_icache030 (regs.prefetch020addr[0] + 4, 1);
				return v;
			}
			if (pc == regs.prefetch020addr[0] + 2) {
				uae_u32 v = regs.prefetch020data[0] & 0xffff;
				if (regs.prefetch020addr[1] == regs.prefetch020addr[0] + 4) {
					regs.prefetch020addr[0] = regs.prefetch020addr[1];
					regs.prefetch020data[0] = regs.prefetch020data[1];
					fill_icache030 (regs.prefetch020addr[0] + 4, 1);
				} else {
					fill_icache030 (pc + 4, 0);
					fill_icache030 (regs.prefetch020addr[0] + 4, 1);
				}
				return v;
			}
			regs.prefetch020addr[0] = regs.prefetch020addr[1];
			regs.prefetch020data[0] = regs.prefetch020data[1];
		}
		fill_icache030 (pc + 0, 0);
		fill_icache030 (pc + 4, 1);
	}
}

#endif

void flush_dcache (uaecptr addr, int size)
{
	int i;
	if (!currprefs.cpu_cycle_exact)
		return;
	if (currprefs.cpu_model >= 68030) {
		for (i = 0; i < CACHELINES030; i++) {
			dcaches030[i].valid[0] = 0;
			dcaches030[i].valid[1] = 0;
			dcaches030[i].valid[2] = 0;
			dcaches030[i].valid[3] = 0;
		}
	}
}

void do_cycles_ce020 (int clocks)
{
	do_cycles_ce (clocks * cpucycleunit);
}
void do_cycles_ce020_mem (int clocks)
{
	regs.ce020memcycles -= clocks * cpucycleunit;
	do_cycles_ce (clocks * cpucycleunit);
}

void do_cycles_ce000 (int clocks)
{
	do_cycles_ce (clocks * cpucycleunit);
}

