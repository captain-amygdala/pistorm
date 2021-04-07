#ifndef CPUMMU030_H
#define CPUMMU030_H

#include "mmu_common.h"

extern uae_u64 srp_030, crp_030;
extern uae_u32 tt0_030, tt1_030, tc_030;
extern uae_u16 mmusr_030;

#define MAX_MMU030_ACCESS 10
extern uae_u32 mm030_stageb_address;
extern int mmu030_idx;
extern bool mmu030_retry;
extern int mmu030_opcode, mmu030_opcode_stageb;
extern uae_u16 mmu030_state[3];
extern uae_u32 mmu030_data_buffer;
extern uae_u32 mmu030_disp_store[2];
extern uae_u32 mmu030_fmovem_store[2];

#define MMU030_STATEFLAG1_FMOVEM 0x2000
#define MMU030_STATEFLAG1_MOVEM1 0x4000
#define MMU030_STATEFLAG1_MOVEM2 0x8000
#define MMU030_STATEFLAG1_DISP0 0x0001
#define MMU030_STATEFLAG1_DISP1 0x0002

struct mmu030_access
{
	bool done;
	uae_u32 val;
};
extern struct mmu030_access mmu030_ad[MAX_MMU030_ACCESS];

uae_u32 REGPARAM3 get_disp_ea_020_mmu030 (uae_u32 base, int idx) REGPARAM;

void mmu_op30_pmove (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra);
void mmu_op30_ptest (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra);
void mmu_op30_pload (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra);
void mmu_op30_pflush (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra);

uae_u32 mmu_op30_helper_get_fc(uae_u16 next);

void mmu030_ptest_atc_search(uaecptr logical_addr, uae_u32 fc, bool write);
uae_u32 mmu030_ptest_table_search(uaecptr extra, uae_u32 fc, bool write, int level);
uae_u32 mmu030_table_search(uaecptr addr, uae_u32 fc, bool write, int level);


typedef struct {
    uae_u32 addr_base;
    uae_u32 addr_mask;
    uae_u32 fc_base;
    uae_u32 fc_mask;
} TT_info;

TT_info mmu030_decode_tt(uae_u32 TT);
void mmu030_decode_tc(uae_u32 TC);
void mmu030_decode_rp(uae_u64 RP);

int mmu030_logical_is_in_atc(uaecptr addr, uae_u32 fc, bool write);
void mmu030_atc_handle_history_bit(int entry_num);

void mmu030_put_long_atc(uaecptr addr, uae_u32 val, int l, uae_u32 fc);
void mmu030_put_word_atc(uaecptr addr, uae_u16 val, int l, uae_u32 fc);
void mmu030_put_byte_atc(uaecptr addr, uae_u8 val, int l, uae_u32 fc);
uae_u32 mmu030_get_long_atc(uaecptr addr, int l, uae_u32 fc);
uae_u16 mmu030_get_word_atc(uaecptr addr, int l, uae_u32 fc);
uae_u8 mmu030_get_byte_atc(uaecptr addr, int l, uae_u32 fc);

void mmu030_flush_atc_fc(uae_u32 fc_base, uae_u32 fc_mask);
void mmu030_flush_atc_page(uaecptr logical_addr);
void mmu030_flush_atc_page_fc(uaecptr logical_addr, uae_u32 fc_base, uae_u32 fc_mask);
void mmu030_flush_atc_all(void);
void mmu030_reset(int hardreset);
uaecptr mmu030_translate(uaecptr addr, bool super, bool data, bool write);

int mmu030_match_ttr(uaecptr addr, uae_u32 fc, bool write);
int mmu030_match_ttr_access(uaecptr addr, uae_u32 fc, bool write);
int mmu030_match_lrmw_ttr(uaecptr addr, uae_u32 fc);
int mmu030_do_match_ttr(uae_u32 tt, TT_info masks, uaecptr addr, uae_u32 fc, bool write);
int mmu030_do_match_lrmw_ttr(uae_u32 tt, TT_info masks, uaecptr addr, uae_u32 fc);

void mmu030_put_long(uaecptr addr, uae_u32 val, uae_u32 fc);
void mmu030_put_word(uaecptr addr, uae_u16 val, uae_u32 fc);
void mmu030_put_byte(uaecptr addr, uae_u8  val, uae_u32 fc);
uae_u32 mmu030_get_long(uaecptr addr, uae_u32 fc);
uae_u16 mmu030_get_word(uaecptr addr, uae_u32 fc);
uae_u8  mmu030_get_byte(uaecptr addr, uae_u32 fc);

uae_u32 uae_mmu030_get_lrmw(uaecptr addr, int size);
void uae_mmu030_put_lrmw(uaecptr addr, uae_u32 val, int size);

uae_u32 mmu030_get_generic(uaecptr addr, uae_u32 fc, int size, int accesssize, int flags);

extern uae_u16 REGPARAM3 mmu030_get_word_unaligned(uaecptr addr, uae_u32 fc, int flags) REGPARAM;
extern uae_u32 REGPARAM3 mmu030_get_long_unaligned(uaecptr addr, uae_u32 fc, int flags) REGPARAM;
extern uae_u16 REGPARAM3 mmu030_get_lrmw_word_unaligned(uaecptr addr, uae_u32 fc, int flags) REGPARAM;
extern uae_u32 REGPARAM3 mmu030_get_lrmw_long_unaligned(uaecptr addr, uae_u32 fc, int flags) REGPARAM;
extern void REGPARAM3 mmu030_put_word_unaligned(uaecptr addr, uae_u16 val, uae_u32 fc, int flags) REGPARAM;
extern void REGPARAM3 mmu030_put_long_unaligned(uaecptr addr, uae_u32 val, uae_u32 fc, int flags) REGPARAM;

static ALWAYS_INLINE uae_u32 uae_mmu030_get_ilong(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 2;

	if (unlikely(is_unaligned(addr, 4)))
		return mmu030_get_long_unaligned(addr, fc, 0);
	return mmu030_get_long(addr, fc);
}
static ALWAYS_INLINE uae_u16 uae_mmu030_get_iword(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 2;

	if (unlikely(is_unaligned(addr, 2)))
		return mmu030_get_word_unaligned(addr, fc, 0);
	return mmu030_get_word(addr, fc);
}
static ALWAYS_INLINE uae_u16 uae_mmu030_get_ibyte(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 2;

	return mmu030_get_byte(addr, fc);
}
static ALWAYS_INLINE uae_u32 uae_mmu030_get_long(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;

	if (unlikely(is_unaligned(addr, 4)))
		return mmu030_get_long_unaligned(addr, fc, 0);
	return mmu030_get_long(addr, fc);
}
static ALWAYS_INLINE uae_u16 uae_mmu030_get_word(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;

	if (unlikely(is_unaligned(addr, 2)))
		return mmu030_get_word_unaligned(addr, fc, 0);
	return mmu030_get_word(addr, fc);
}
static ALWAYS_INLINE uae_u8 uae_mmu030_get_byte(uaecptr addr)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;

	return mmu030_get_byte(addr, fc);
}
static ALWAYS_INLINE void uae_mmu030_put_long(uaecptr addr, uae_u32 val)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;
    
	if (unlikely(is_unaligned(addr, 4)))
		mmu030_put_long_unaligned(addr, val, fc, 0);
	else
		mmu030_put_long(addr, val, fc);
}
static ALWAYS_INLINE void uae_mmu030_put_word(uaecptr addr, uae_u16 val)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;

	if (unlikely(is_unaligned(addr, 2)))
		mmu030_put_word_unaligned(addr, val, fc, 0);
	else
		mmu030_put_word(addr, val, fc);
}
static ALWAYS_INLINE void uae_mmu030_put_byte(uaecptr addr, uae_u8 val)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;

	mmu030_put_byte(addr, val, fc);
}

static ALWAYS_INLINE uae_u32 sfc030_get_long(uaecptr addr)
{
    uae_u32 fc = regs.sfc;
#if MMUDEBUG > 2
	write_log(_T("sfc030_get_long: FC = %i\n"),fc);
#endif
	if (unlikely(is_unaligned(addr, 4)))
		return mmu030_get_long_unaligned(addr, fc, 0);
	return mmu030_get_long(addr, fc);
}

static ALWAYS_INLINE uae_u16 sfc030_get_word(uaecptr addr)
{
    uae_u32 fc = regs.sfc;
#if MMUDEBUG > 2
	write_log(_T("sfc030_get_word: FC = %i\n"),fc);
#endif
    if (unlikely(is_unaligned(addr, 2)))
		return mmu030_get_word_unaligned(addr, fc, 0);
	return mmu030_get_word(addr, fc);
}

static ALWAYS_INLINE uae_u8 sfc030_get_byte(uaecptr addr)
{
    uae_u32 fc = regs.sfc;
#if MMUDEBUG > 2
	write_log(_T("sfc030_get_byte: FC = %i\n"),fc);
#endif
	return mmu030_get_byte(addr, fc);
}

static ALWAYS_INLINE void dfc030_put_long(uaecptr addr, uae_u32 val)
{
    uae_u32 fc = regs.dfc;
#if MMUDEBUG > 2
	write_log(_T("dfc030_put_long: FC = %i\n"),fc);
#endif
    if (unlikely(is_unaligned(addr, 4)))
		mmu030_put_long_unaligned(addr, val, fc, 0);
	else
		mmu030_put_long(addr, val, fc);
}

static ALWAYS_INLINE void dfc030_put_word(uaecptr addr, uae_u16 val)
{
    uae_u32 fc = regs.dfc;
#if MMUDEBUG > 2
	write_log(_T("dfc030_put_word: FC = %i\n"),fc);
#endif
	if (unlikely(is_unaligned(addr, 2)))
		mmu030_put_word_unaligned(addr, val, fc, 0);
	else
		mmu030_put_word(addr, val, fc);
}

static ALWAYS_INLINE void dfc030_put_byte(uaecptr addr, uae_u8 val)
{
    uae_u32 fc = regs.dfc;
#if MMUDEBUG > 2
	write_log(_T("dfc030_put_byte: FC = %i\n"),fc);
#endif
	mmu030_put_byte(addr, val, fc);
}

#define ACCESS_CHECK_PUT \
	if (!mmu030_ad[mmu030_idx].done) { \
		mmu030_ad[mmu030_idx].val = v; \
	} else if (mmu030_ad[mmu030_idx].done) { \
		mmu030_idx++; \
		return; \
	}

#define ACCESS_CHECK_GET \
	if (mmu030_ad[mmu030_idx].done) { \
		v = mmu030_ad[mmu030_idx].val; \
		mmu030_idx++; \
		return v; \
	}

#define ACCESS_CHECK_GET_PC(pc) \
	if (mmu030_ad[mmu030_idx].done) { \
		v = mmu030_ad[mmu030_idx].val; \
		mmu030_idx++; \
		m68k_incpci (pc); \
		return v; \
	}

#define ACCESS_EXIT_PUT \
	mmu030_ad[mmu030_idx].done = true; \
	mmu030_idx++; \
	mmu030_ad[mmu030_idx].done = false;

#define ACCESS_EXIT_GET \
	mmu030_ad[mmu030_idx].val = v; \
	mmu030_ad[mmu030_idx].done = true; \
	mmu030_idx++; \
	mmu030_ad[mmu030_idx].done = false;

STATIC_INLINE void put_byte_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_byte (addr, v);
	ACCESS_EXIT_PUT
}
STATIC_INLINE void put_lrmw_byte_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_lrmw (addr, v, sz_byte);
	ACCESS_EXIT_PUT
}
STATIC_INLINE void put_word_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_word (addr, v);
	ACCESS_EXIT_PUT
}
STATIC_INLINE void put_lrmw_word_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_lrmw (addr, v, sz_word);
	ACCESS_EXIT_PUT
}
STATIC_INLINE void put_long_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_long (addr, v);
	ACCESS_EXIT_PUT
}
STATIC_INLINE void put_lrmw_long_mmu030_state (uaecptr addr, uae_u32 v)
{
	ACCESS_CHECK_PUT
    uae_mmu030_put_lrmw (addr, v, sz_long);
	ACCESS_EXIT_PUT
}

STATIC_INLINE uae_u32 get_byte_mmu030_state (uaecptr addr)
{
	uae_u32 v;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_byte (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_lrmw_byte_mmu030_state (uaecptr addr)
{
	uae_u32 v;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_lrmw (addr, sz_byte);
	ACCESS_EXIT_GET
	return v;
}

STATIC_INLINE uae_u32 get_word_mmu030_state (uaecptr addr)
{
 	uae_u32 v;
	ACCESS_CHECK_GET
	v = uae_mmu030_get_word (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_lrmw_word_mmu030_state (uaecptr addr)
{
 	uae_u32 v;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_lrmw (addr, sz_word);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_long_mmu030_state (uaecptr addr)
{
 	uae_u32 v;
	ACCESS_CHECK_GET
	v = uae_mmu030_get_long (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_lrmw_long_mmu030_state (uaecptr addr)
{
 	uae_u32 v;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_lrmw (addr, sz_long);
	ACCESS_EXIT_GET
	return v;
}

STATIC_INLINE uae_u32 get_ibyte_mmu030_state (int o)
{
 	uae_u32 v;
    uae_u32 addr = m68k_getpc () + o;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_iword (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_iword_mmu030_state (int o)
{
 	uae_u32 v;
    uae_u32 addr = m68k_getpc () + o;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_iword (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 get_ilong_mmu030_state (int o)
{
 	uae_u32 v;
    uae_u32 addr = m68k_getpc () + o;
	ACCESS_CHECK_GET
    v = uae_mmu030_get_ilong (addr);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 next_iword_mmu030_state (void)
{
 	uae_u32 v;
    uae_u32 addr = m68k_getpc ();
	ACCESS_CHECK_GET_PC(2);
    v = uae_mmu030_get_iword (addr);
    m68k_incpci (2);
	ACCESS_EXIT_GET
	return v;
}
STATIC_INLINE uae_u32 next_ilong_mmu030_state (void)
{
 	uae_u32 v;
    uae_u32 addr = m68k_getpc ();
	ACCESS_CHECK_GET_PC(4);
    v = uae_mmu030_get_ilong (addr);
    m68k_incpci (4);
	ACCESS_EXIT_GET
	return v;
}

STATIC_INLINE uae_u32 get_byte_mmu030 (uaecptr addr)
{
	return uae_mmu030_get_byte (addr);
}
STATIC_INLINE uae_u32 get_word_mmu030 (uaecptr addr)
{
	return uae_mmu030_get_word (addr);
}
STATIC_INLINE uae_u32 get_long_mmu030 (uaecptr addr)
{
	return uae_mmu030_get_long (addr);
}
STATIC_INLINE void put_byte_mmu030 (uaecptr addr, uae_u32 v)
{
    uae_mmu030_put_byte (addr, v);
}

STATIC_INLINE void put_word_mmu030 (uaecptr addr, uae_u32 v)
{
    uae_mmu030_put_word (addr, v);
}
STATIC_INLINE void put_long_mmu030 (uaecptr addr, uae_u32 v)
{
    uae_mmu030_put_long (addr, v);
}

STATIC_INLINE uae_u32 get_ibyte_mmu030 (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu030_get_iword (pc);
}
STATIC_INLINE uae_u32 get_iword_mmu030 (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu030_get_iword (pc);
}
STATIC_INLINE uae_u32 get_ilong_mmu030 (int o)
{
    uae_u32 pc = m68k_getpc () + o;
    return uae_mmu030_get_ilong (pc);
}
STATIC_INLINE uae_u32 next_iword_mmu030 (void)
{
 	uae_u32 v;
    uae_u32 pc = m68k_getpc ();
    v = uae_mmu030_get_iword (pc);
    m68k_incpci (2);
	return v;
}
STATIC_INLINE uae_u32 next_ilong_mmu030 (void)
{
 	uae_u32 v;
    uae_u32 pc = m68k_getpc ();
    v = uae_mmu030_get_ilong (pc);
    m68k_incpci (4);
	return v;
}

extern void m68k_do_rts_mmu030 (void);
extern void m68k_do_rte_mmu030 (uaecptr a7);
extern void flush_mmu030 (uaecptr, int);
extern void m68k_do_bsr_mmu030 (uaecptr oldpc, uae_s32 offset);
extern void mmu030_page_fault(uaecptr addr, bool read, int flags, uae_u32 fc);
#endif
