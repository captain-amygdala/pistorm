/*
* UAE - The Un*x Amiga Emulator
*
* MC68000 emulation
*
* Copyright 1995 Bernd Schmidt
*/

#ifndef NEWCPU_H
#define NEWCPU_H

#include "options_cpu.h"

#include "readcpu.h"
//#include "machdep/m68k.h"
#include "m68k.h"
#include "compat.h"
#include "maccess.h"

/* Possible exceptions sources for M68000_Exception() and Exception() */
#define M68000_EXC_SRC_CPU	    1  /* Direct CPU exception */
#define M68000_EXC_SRC_AUTOVEC  2  /* Auto-vector exception (e.g. VBL) */
#define M68000_EXC_SRC_INT_MFP	3  /* MFP interrupt exception */
#define M68000_EXC_SRC_INT_DSP  4  /* DSP interrupt exception */


/* Special flags */
#define SPCFLAG_DEBUGGER 1
#define SPCFLAG_STOP 2
#define SPCFLAG_BUSERROR 4
#define SPCFLAG_INT 8
#define SPCFLAG_BRK 0x10
#define SPCFLAG_EXTRA_CYCLES 0x20
#define SPCFLAG_TRACE 0x40
#define SPCFLAG_DOTRACE 0x80
#define SPCFLAG_DOINT 0x100
#define SPCFLAG_MFP 0x200
#define SPCFLAG_EXEC 0x400
#define SPCFLAG_MODE_CHANGE 0x800


#ifndef SET_CFLG

#define SET_CFLG(x) (CFLG() = (x))
#define SET_NFLG(x) (NFLG() = (x))
#define SET_VFLG(x) (VFLG() = (x))
#define SET_ZFLG(x) (ZFLG() = (x))
#define SET_XFLG(x) (XFLG() = (x))

#define GET_CFLG() CFLG()
#define GET_NFLG() NFLG()
#define GET_VFLG() VFLG()
#define GET_ZFLG() ZFLG()
#define GET_XFLG() XFLG()

#define CLEAR_CZNV() do { \
	SET_CFLG (0); \
	SET_ZFLG (0); \
	SET_NFLG (0); \
	SET_VFLG (0); \
} while (0)

#define COPY_CARRY() (SET_XFLG (GET_CFLG ()))
#endif

extern const int areg_byteinc[];
extern const int imm8_table[];

extern int movem_index1[256];
extern int movem_index2[256];
extern int movem_next[256];

#ifdef FPUEMU
extern int fpp_movem_index1[256];
extern int fpp_movem_index2[256];
extern int fpp_movem_next[256];
#endif

extern int OpcodeFamily;

typedef uae_u32 REGPARAM3 cpuop_func (uae_u32) REGPARAM;
typedef void REGPARAM3 cpuop_func_ce (uae_u32) REGPARAM;

struct cputbl {
	cpuop_func *handler;
	uae_u16 opcode;
};

#ifdef JIT
typedef uae_u32 REGPARAM3 compop_func (uae_u32) REGPARAM;

struct comptbl {
	compop_func *handler;
	uae_u32 opcode;
	int specific;
};
#endif

extern uae_u32 REGPARAM3 op_illg (uae_u32) REGPARAM;
extern void REGPARAM3 op_unimpl (uae_u16) REGPARAM;

typedef uae_u8 flagtype;

#ifdef FPUEMU
/* You can set this to long double to be more accurate. However, the
resulting alignment issues will cost a lot of performance in some
apps */
#if 1   /* set to 1 if your system supports long extended precision long double */
#define USE_LONG_DOUBLE 1
#else
#define USE_LONG_DOUBLE 0
#define USE_SOFT_LONG_DOUBLE 1
#endif

#if USE_LONG_DOUBLE
typedef long double fptype;
#define LDPTR tbyte ptr
#else
typedef double fptype;
#define LDPTR qword ptr
#endif
#endif

#define CPU_PIPELINE_MAX 2
#define CPU000_MEM_CYCLE 4
#define CPU000_CLOCK_MULT 2
#define CPU020_MEM_CYCLE 3
#define CPU020_CLOCK_MULT 4

#define CACHELINES020 64
struct cache020
{
	uae_u32 data;
	uae_u32 tag;
	bool valid;
};

#define CACHELINES030 16
struct cache030
{
	uae_u32 data[4];
	bool valid[4];
	uae_u32 tag;
};

#define CACHESETS040 64
#define CACHELINES040 4
struct cache040
{
	uae_u32 data[CACHELINES040][4];
	bool valid[CACHELINES040];
	uae_u32 tag[CACHELINES040];
};

uae_u64 srp_030, crp_030;
uae_u32 tt0_030, tt1_030, tc_030;
uae_u16 mmusr_030;

struct mmufixup
{
    int reg;
    uae_u32 value;
};
extern struct mmufixup mmufixup[2];

typedef struct
{
	fptype fp;
#ifdef USE_SOFT_LONG_DOUBLE
	bool fpx;
	uae_u32 fpm;
	uae_u64 fpe;
#endif
} fpdata;

struct regstruct
{
	uae_u32 regs[16];

	uae_u32 pc;
	uae_u8 *pc_p;
	uae_u8 *pc_oldp;
	uae_u32 instruction_pc;

	uae_u16 irc, ir;
	uae_u32 spcflags;

	uaecptr usp, isp, msp;
	uae_u16 sr;
	flagtype t1;
	flagtype t0;
	flagtype s;
	flagtype m;
	flagtype x;
	flagtype stopped;
	int intmask;
	int ipl, ipl_pin;

	uae_u32 vbr, sfc, dfc;

#ifdef FPUEMU
	fpdata fp[8];
	fpdata fp_result;
      uae_u32 fp_result_status;
	uae_u32 fpcr, fpsr, fpiar;
	uae_u32 fpu_state;
    uae_u32 fpu_exp_state;
    fpdata exp_src1, exp_src2;
    uae_u32 exp_pack[3];
    uae_u16 exp_opcode, exp_extra, exp_type;
	bool fp_exception;
#endif
#ifndef CPUEMU_68000_ONLY
	uae_u32 cacr, caar;
	uae_u32 itt0, itt1, dtt0, dtt1;
	uae_u32 tcr, mmusr, urp, srp, buscr;
	uae_u32 mmu_fslw;
	uae_u32 mmu_fault_addr, mmu_effective_addr;
	uae_u16 mmu_ssw;
    uae_u32 wb2_address;
	uae_u32 wb3_data;
	uae_u16 wb3_status, wb2_status;
	int mmu_enabled;
	int mmu_page_size;
#endif

	uae_u32 pcr;
	uae_u32 address_space_mask;

	uae_u8 panic;
	uae_u32 panic_pc, panic_addr;

	uae_u32 prefetch020data[CPU_PIPELINE_MAX];
	uae_u32 prefetch020addr[CPU_PIPELINE_MAX];
	int ce020memcycles;
};

extern struct regstruct regs;

STATIC_INLINE uae_u32 munge24 (uae_u32 x)
{
	return x & regs.address_space_mask;
}

extern int mmu_enabled, mmu_triggered;
extern int cpu_cycles;
extern int cpucycleunit;
STATIC_INLINE void set_special (uae_u32 x)
{
	regs.spcflags |= x;
	cycles_do_special ();
}

STATIC_INLINE void unset_special (uae_u32 x)
{
	regs.spcflags &= ~x;
}

#define m68k_dreg(r,num) ((r).regs[(num)])
#define m68k_areg(r,num) (((r).regs + 8)[(num)])

STATIC_INLINE void m68k_setpc (uaecptr newpc)
{
    regs.pc_p = regs.pc_oldp = 0;
	regs.instruction_pc = regs.pc = newpc;
}

STATIC_INLINE uaecptr m68k_getpc (void)
{
	return (uaecptr)(regs.pc + ((uae_u8*)regs.pc_p - (uae_u8*)regs.pc_oldp));
}
#define M68K_GETPC m68k_getpc()

STATIC_INLINE uaecptr m68k_getpc_p (uae_u8 *p)
{
	return (uaecptr)(regs.pc + ((uae_u8*)p - (uae_u8*)regs.pc_oldp));
}

STATIC_INLINE void fill_prefetch_0 (void)
{
}

#define fill_prefetch_2 fill_prefetch_0

STATIC_INLINE void m68k_incpc (int o)
{
	regs.pc_p += o;
}

STATIC_INLINE void m68k_setpc_mmu (uaecptr newpc)
{
	regs.instruction_pc = regs.pc = newpc;
	regs.pc_p = regs.pc_oldp = 0;
}
STATIC_INLINE void m68k_setpci (uaecptr newpc)
{
	regs.instruction_pc = regs.pc = newpc;
}
STATIC_INLINE uaecptr m68k_getpci (void)
{
	return regs.pc;
}
STATIC_INLINE void m68k_incpci (int o)
{
	regs.pc += o;
}

STATIC_INLINE void m68k_do_rts (void)
{
	uae_u32 newpc = get_long (m68k_areg (regs, 7));
	m68k_setpc (newpc);
	m68k_areg (regs, 7) += 4;
}
STATIC_INLINE void m68k_do_rtsi (void)
{
	m68k_setpci (get_long (m68k_areg (regs, 7)));
	m68k_areg (regs, 7) += 4;
}

STATIC_INLINE void m68k_do_bsr (uaecptr oldpc, uae_s32 offset)
{
	m68k_areg (regs, 7) -= 4;
	put_long (m68k_areg (regs, 7), oldpc);
	m68k_incpc (offset);
}
STATIC_INLINE void m68k_do_bsri (uaecptr oldpc, uae_s32 offset)
{
	m68k_areg (regs, 7) -= 4;
	put_long (m68k_areg (regs, 7), oldpc);
	m68k_incpci (offset);
}

STATIC_INLINE uae_u32 get_ibyte (int o)
{
	return do_get_mem_byte((uae_u8 *)((regs).pc_p + (o) + 1));
}
STATIC_INLINE uae_u32 get_iword (int o)
{
	return do_get_mem_word((uae_u16 *)((regs).pc_p + (o)));
}
STATIC_INLINE uae_u32 get_ilong (int o)
{
	return do_get_mem_long((uae_u32 *)((regs).pc_p + (o)));
}

#define get_iwordi(o) get_wordi(o)
#define get_ilongi(o) get_longi(o)

/* These are only used by the 68020/68881 code, and therefore don't
* need to handle prefetch.  */
STATIC_INLINE uae_u32 next_ibyte (void)
{
	uae_u32 r = get_ibyte (0);
	m68k_incpc (2);
	return r;
}
STATIC_INLINE uae_u32 next_iword (void)
{
	uae_u32 r = get_iword (0);
	m68k_incpc (2);
	return r;
}
STATIC_INLINE uae_u32 next_iwordi (void)
{
	uae_u32 r = get_iwordi (m68k_getpci ());
	m68k_incpc (2);
	return r;
}
STATIC_INLINE uae_u32 next_ilong (void)
{
	uae_u32 r = get_ilong (0);
	m68k_incpc (4);
	return r;
}
STATIC_INLINE uae_u32 next_ilongi (void)
{
	uae_u32 r = get_ilongi (m68k_getpci ());
	m68k_incpc (4);
	return r;
}

extern uae_u32 (*x_prefetch)(int);
extern uae_u32 (*x_prefetch_long)(int);
extern uae_u32 (*x_get_byte)(uaecptr addr);
extern uae_u32 (*x_get_word)(uaecptr addr);
extern uae_u32 (*x_get_long)(uaecptr addr);
extern void (*x_put_byte)(uaecptr addr, uae_u32 v);
extern void (*x_put_word)(uaecptr addr, uae_u32 v);
extern void (*x_put_long)(uaecptr addr, uae_u32 v);
extern uae_u32 (*x_next_iword)(void);
extern uae_u32 (*x_next_ilong)(void);
extern uae_u32 (*x_get_ilong)(int);
extern uae_u32 (*x_get_iword)(int);
extern uae_u32 (*x_get_ibyte)(int);

extern uae_u32 REGPARAM3 x_get_disp_ea_020 (uae_u32 base, int idx) REGPARAM;
extern uae_u32 REGPARAM3 x_get_disp_ea_ce020 (uae_u32 base, int idx) REGPARAM;
extern uae_u32 REGPARAM3 x_get_disp_ea_ce030 (uae_u32 base, int idx) REGPARAM;
extern uae_u32 REGPARAM3 x_get_bitfield (uae_u32 src, uae_u32 bdata[2], uae_s32 offset, int width) REGPARAM;
extern void REGPARAM3 x_put_bitfield (uae_u32 dst, uae_u32 bdata[2], uae_u32 val, uae_s32 offset, int width) REGPARAM;

extern uae_u32 (*x_cp_get_byte)(uaecptr addr);
extern uae_u32 (*x_cp_get_word)(uaecptr addr);
extern uae_u32 (*x_cp_get_long)(uaecptr addr);
extern void (*x_cp_put_byte)(uaecptr addr, uae_u32 v);
extern void (*x_cp_put_word)(uaecptr addr, uae_u32 v);
extern void (*x_cp_put_long)(uaecptr addr, uae_u32 v);
extern uae_u32 (*x_cp_next_iword)(void);
extern uae_u32 (*x_cp_next_ilong)(void);

extern uae_u32 (REGPARAM3 *x_cp_get_disp_ea_020)(uae_u32 base, int idx) REGPARAM;

extern void m68k_setstopped (void);
extern void m68k_resumestopped (void);

extern uae_u32 REGPARAM3 get_disp_ea_020 (uae_u32 base, int idx) REGPARAM;
extern uae_u32 REGPARAM3 get_bitfield (uae_u32 src, uae_u32 bdata[2], uae_s32 offset, int width) REGPARAM;
extern void REGPARAM3 put_bitfield (uae_u32 dst, uae_u32 bdata[2], uae_u32 val, uae_s32 offset, int width) REGPARAM;

extern void m68k_disasm_ea (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt, uae_u32 *seaddr, uae_u32 *deaddr);
extern void m68k_disasm (FILE *f, uaecptr addr, uaecptr *nextpc, int cnt);
extern int get_cpu_model (void);

extern void set_cpu_caches (bool flush);
extern void REGPARAM3 MakeSR (void) REGPARAM;
extern void REGPARAM3 MakeFromSR (void) REGPARAM;
extern void MakeSR (void);
extern void MakeFromSR (void);
extern void REGPARAM3 Exception (int) REGPARAM;
extern void REGPARAM3 ExceptionL (int/*, uaecptr*/) REGPARAM;
//extern void REGPARAM3 Exception (int, uaecptr, int) REGPARAM;
extern void NMI (void);
extern void NMI_delayed (void);
extern void prepare_interrupt (uae_u32);
extern void doint (void);
extern void dump_counts (void);
extern int m68k_move2c (int, uae_u32 *);
extern int m68k_movec2 (int, uae_u32 *);
extern bool m68k_divl (uae_u32, uae_u32, uae_u16);
extern bool m68k_mull (uae_u32, uae_u32, uae_u16);
//extern void m68k_divl (uae_u32, uae_u32, uae_u16, uaecptr);
//extern void m68k_mull (uae_u32, uae_u32, uae_u16);
extern void init_m68k (void);
extern void init_m68k_full (void);
extern void m68k_go (int);
extern void m68k_dumpstate (FILE *, uaecptr *);
extern void m68k_disasm (FILE *, uaecptr, uaecptr *, int);
extern void sm68k_disasm (TCHAR*, TCHAR*, uaecptr addr, uaecptr *nextpc);
extern void m68k_reset (int);
extern int getDivu68kCycles (uae_u32 dividend, uae_u16 divisor);
extern int getDivs68kCycles (uae_s32 dividend, uae_s16 divisor);
extern void divbyzero_special (bool issigned, uae_s32 dst);
extern void m68k_do_rte (void);

extern void mmu_op (uae_u32, uae_u32);
extern void mmu_op30 (uaecptr, uae_u32, uae_u16, uaecptr);

extern void fpuop_arithmetic(uae_u32, uae_u16);
extern void fpuop_dbcc(uae_u32, uae_u16);
extern void fpuop_scc(uae_u32, uae_u16);
extern void fpuop_trapcc(uae_u32, uaecptr, uae_u16);
extern void fpuop_bcc(uae_u32, uaecptr, uae_u32);
extern void fpuop_save(uae_u32);
extern void fpuop_restore(uae_u32);
extern uae_u32 fpp_get_fpsr (void);
extern void fpu_reset (void);
extern void fpux_save (int*);
extern void fpux_restore (int*);

extern void exception3 (uae_u32 opcode, uaecptr addr);
extern void exception3i (uae_u32 opcode, uaecptr addr);
extern void exception3b (uae_u32 opcode, uaecptr addr, bool w, bool i, uaecptr pc);
extern void exception2 (uaecptr addr, bool read, int size, uae_u32 fc);
//extern void exception3 (uae_u32 opcode, uaecptr addr, uaecptr fault);
//extern void exception3i (uae_u32 opcode, uaecptr addr, uaecptr fault);
//extern void exception2 (uaecptr addr, uaecptr fault);
extern void cpureset (void);
extern void cpu_halt (int id);

extern void fill_prefetch (void);
extern void fill_prefetch_020 (void);
extern void fill_prefetch_030 (void);

#define CPU_OP_NAME(a) op ## a

/* 68060 */
extern const struct cputbl op_smalltbl_0_ff[];
extern const struct cputbl op_smalltbl_22_ff[]; // CE
extern const struct cputbl op_smalltbl_33_ff[]; // MMU
/* 68040 */
extern const struct cputbl op_smalltbl_1_ff[];
extern const struct cputbl op_smalltbl_23_ff[]; // CE
extern const struct cputbl op_smalltbl_31_ff[]; // MMU
/* 68030 */
extern const struct cputbl op_smalltbl_2_ff[];
extern const struct cputbl op_smalltbl_24_ff[]; // CE
extern const struct cputbl op_smalltbl_32_ff[]; // MMU
/* 68020 */
extern const struct cputbl op_smalltbl_3_ff[];
extern const struct cputbl op_smalltbl_20_ff[]; // prefetch
extern const struct cputbl op_smalltbl_21_ff[]; // CE
/* 68010 */
extern const struct cputbl op_smalltbl_4_ff[];
extern const struct cputbl op_smalltbl_11_ff[]; // prefetch
extern const struct cputbl op_smalltbl_13_ff[]; // CE
/* 68000 */
extern const struct cputbl op_smalltbl_5_ff[];
extern const struct cputbl op_smalltbl_12_ff[]; // prefetch
extern const struct cputbl op_smalltbl_14_ff[]; // CE

extern cpuop_func *cpufunctbl[65536] ASM_SYM_FOR_FUNC ("cpufunctbl");

/* Added for hatari_glue.c */
extern void build_cpufunctbl(void);

#ifdef JIT
extern void flush_icache (uaecptr, int);
extern void compemu_reset (void);
extern bool check_prefs_changed_comp (void);
#else
#define flush_icache(uaecptr, int) do {} while (0)
#endif
extern void flush_dcache (uaecptr, int);
extern void flush_mmu (uaecptr, int);

extern int movec_illg (int regno);
extern uae_u32 val_move2c (int regno);
extern void val_move2c2 (int regno, uae_u32 val);
struct cpum2c {
	int regno;
	const TCHAR *regname;
};
extern struct cpum2c m2cregs[];

/* Family of the latest instruction executed (to check for pairing) */
extern int OpcodeFamily;			/* see instrmnem in readcpu.h */

/* How many cycles to add to the current instruction in case a "misaligned" bus acces is made */
/* (used when addressing mode is d8(an,ix)) */
extern int BusCyclePenalty;

STATIC_INLINE uae_u32 get_iword_prefetch (uae_s32 o)
{
/* Laurent : let's see this later
*/
/*
    uae_u32 currpc = m68k_getpc ();
    uae_u32 addr = currpc + o;
    uae_u32 offs = addr - prefetch_pc;
    uae_u32 v;
    if (offs > 3) {
	refill_prefetch (currpc, o);
	offs = addr - prefetch_pc;
    }
    v = do_get_mem_word (((uae_u8 *)&prefetch) + offs);
    if (offs >= 2)
	refill_prefetch (currpc, 2);

     printf ("get_iword PC %lx ADDR %lx OFFS %lx V %lx\n", currpc, addr, offs, v);
*/
    //return v;
printf("prefetch!\n");
    return 0;
}

#endif
