/* Emulation of MC68030 MMU
 * This code has been written for Previous - a NeXT Computer emulator
 *
 * This file is distributed under the GNU General Public License, version 2
 * or at your option any later version. Read the file gpl.txt for details.
 *
 *
 * Written by Andreas Grabher
 *
 * Many thanks go to Thomas Huth and the Hatari community for helping
 * to test and debug this code!
 *
 *
 * Release notes:
 * 01-09-2012: First release
 * 29-09-2012: Improved function code handling
 * 16-11-2012: Improved exception handling
 *
 *
 * - Check if read-modify-write operations are correctly detected for
 *   handling transparent access (see TT matching functions)
 * - If possible, test mmu030_table_search with all kinds of translations
 *   (early termination, invalid descriptors, bus errors, indirect
 *   descriptors, PTEST in different levels, etc).
 * - Check which bits of an ATC entry or the status register should be set 
 *   and which should be un-set, if an invalid translation occurs.
 * - Handle cache inhibit bit when accessing ATC entries
 */


#include "sysconfig.h"
#include "sysdeps.h"

#include "options_cpu.h"
#include "memory.h"
#include "newcpu.h"
#include "cpummu030.h"
#include "hatari-glue.h"

#define TT_FC_MASK      0x00000007
#define TT_FC_BASE      0x00000070
#define TT_RWM          0x00000100
#define TT_RW           0x00000200
#define TT_CI           0x00000400
#define TT_ENABLE       0x00008000

#define TT_ADDR_MASK    0x00FF0000
#define TT_ADDR_BASE    0xFF000000

static int bBusErrorReadWrite;
static int atcindextable[32];
static int tt_enabled;

int mmu030_idx;

uae_u32 mm030_stageb_address;
bool mmu030_retry;
int mmu030_opcode;
int mmu030_opcode_stageb;
uae_u16 mmu030_state[3];
uae_u32 mmu030_data_buffer;
uae_u32 mmu030_disp_store[2];
uae_u32 mmu030_fmovem_store[2];
struct mmu030_access mmu030_ad[MAX_MMU030_ACCESS];

/* for debugging messages */
char table_letter[4] = {'A','B','C','D'};

uae_u64 srp_030, crp_030;
uae_u32 tt0_030, tt1_030, tc_030;
uae_u16 mmusr_030;

/* ATC struct */
#define ATC030_NUM_ENTRIES  22

typedef struct {
    struct {
        uaecptr addr;
        bool modified;
        bool write_protect;
        bool cache_inhibit;
        bool bus_error;
    } physical;
    
    struct {
        uaecptr addr;
        uae_u32 fc;
        bool valid;
    } logical;
    /* history bit */
    int mru;
} MMU030_ATC_LINE;


/* MMU struct for 68030 */
struct {
    
    /* Translation tables */
    struct {
        struct {
            uae_u32 mask;
            uae_u8 shift;
        } table[4];
        
        struct {
            uae_u32 mask;
			uae_u32 imask;
            uae_u8 size;
        } page;
        
        uae_u8 init_shift;
        uae_u8 last_table;
    } translation;
    
    /* Transparent translation */
    struct {
        TT_info tt0;
        TT_info tt1;
    } transparent;
    
    /* Address translation cache */
    MMU030_ATC_LINE atc[ATC030_NUM_ENTRIES];
    
    /* Condition */
    bool enabled;
    uae_u16 status;
} mmu030;



/* MMU Status Register
 *
 * ---x ---x x-xx x---
 * reserved (all 0)
 *
 * x--- ---- ---- ----
 * bus error
 *
 * -x-- ---- ---- ----
 * limit violation
 *
 * --x- ---- ---- ----
 * supervisor only
 *
 * ---- x--- ---- ----
 * write protected
 *
 * ---- -x-- ---- ----
 * invalid
 *
 * ---- --x- ---- ----
 * modified
 *
 * ---- ---- -x-- ----
 * transparent access
 *
 * ---- ---- ---- -xxx
 * number of levels (number of tables accessed during search)
 *
 */

#define MMUSR_BUS_ERROR         0x8000
#define MMUSR_LIMIT_VIOLATION   0x4000
#define MMUSR_SUPER_VIOLATION   0x2000
#define MMUSR_WRITE_PROTECTED   0x0800
#define MMUSR_INVALID           0x0400
#define MMUSR_MODIFIED          0x0200
#define MMUSR_TRANSP_ACCESS     0x0040
#define MMUSR_NUM_LEVELS_MASK   0x0007



/* -- MMU instructions -- */

void mmu_op30_pmove (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
	int preg = (next >> 10) & 31;
	int rw = (next >> 9) & 1;
	int fd = (next >> 8) & 1;
    
#if MMU030_OP_DBG_MSG
    switch (preg) {
        case 0x10:
            write_log(_T("PMOVE: %s TC %08X\n"), rw?"read":"write",
                      rw?tc_030:x_get_long(extra));
            write_log("addr is 0x%p\n", extra);
            break;
        case 0x12:
            write_log(_T("PMOVE: %s SRP %08X%08X\n"), rw?"read":"write",
                      rw?(uae_u32)(srp_030>>32)&0xFFFFFFFF:x_get_long(extra),
                      rw?(uae_u32)srp_030&0xFFFFFFFF:x_get_long(extra+4));
            break;
        case 0x13:
            write_log(_T("PMOVE: %s CRP %08X%08X\n"), rw?"read":"write",
                      rw?(uae_u32)(crp_030>>32)&0xFFFFFFFF:x_get_long(extra),
                      rw?(uae_u32)crp_030&0xFFFFFFFF:x_get_long(extra+4));
            break;
        case 0x18:
            write_log(_T("PMOVE: %s MMUSR %04X\n"), rw?"read":"write",
                      rw?mmusr_030:x_get_word(extra));
            break;
        case 0x02:
            write_log(_T("PMOVE: %s TT0 %08X\n"), rw?"read":"write",
                      rw?tt0_030:x_get_long(extra));
            break;
        case 0x03:
            write_log(_T("PMOVE: %s TT1 %08X\n"), rw?"read":"write",
                      rw?tt1_030:x_get_long(extra));
            break;
        default:
            break;
    }
    if (!fd && !rw && !(preg==0x18)) {
        write_log(_T("PMOVE: flush ATC\n"));
    }
#endif
    
	switch (preg)
	{
        case 0x10: // TC
            if (rw)
                x_put_long (extra, tc_030);
            else {
                tc_030 = x_get_long (extra);
                mmu030_decode_tc(tc_030);
            }
            break;
        case 0x12: // SRP
            if (rw) {
                x_put_long (extra, srp_030 >> 32);
                x_put_long (extra + 4, srp_030);
            } else {
                srp_030 = (uae_u64)x_get_long (extra) << 32;
                srp_030 |= x_get_long (extra + 4);
                mmu030_decode_rp(srp_030);
            }
            break;
        case 0x13: // CRP
            if (rw) {
                x_put_long (extra, crp_030 >> 32);
                x_put_long (extra + 4, crp_030);
            } else {
                crp_030 = (uae_u64)x_get_long (extra) << 32;
                crp_030 |= x_get_long (extra + 4);
                mmu030_decode_rp(crp_030);
            }
            break;
        case 0x18: // MMUSR
            if (rw)
                x_put_word (extra, mmusr_030);
            else
                mmusr_030 = x_get_word (extra);
            break;
        case 0x02: // TT0
            if (rw)
                x_put_long (extra, tt0_030);
            else {
                tt0_030 = x_get_long (extra);
                mmu030.transparent.tt0 = mmu030_decode_tt(tt0_030);
            }
            break;
        case 0x03: // TT1
            if (rw)
                x_put_long (extra, tt1_030);
            else {
                tt1_030 = x_get_long (extra);
                mmu030.transparent.tt1 = mmu030_decode_tt(tt1_030);
            }
            break;
        default:
            write_log (_T("Bad PMOVE at %08x\n"),m68k_getpc());
            op_illg (opcode);
            return;
	}
    
    if (!fd && !rw && !(preg==0x18)) {
        mmu030_flush_atc_all();
    }
	tt_enabled = (tt0_030 & TT_ENABLE) || (tt1_030 & TT_ENABLE);
}

void mmu_op30_ptest (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
    mmu030.status = mmusr_030 = 0;
    
    int level = (next&0x1C00)>>10;
    int rw = (next >> 9) & 1;
    int a = (next >> 8) & 1;
    int areg = (next&0xE0)>>5;
    uae_u32 fc = mmu_op30_helper_get_fc(next);
        
    bool write = rw ? false : true;

    uae_u32 ret = 0;
    
    /* Check this - datasheet says:
     * "When the instruction specifies an address translation cache search
     *  with an address register operand, the MC68030 takes an F-line
     *  unimplemented instruction exception."
     */
    if (!level && a) { /* correct ? */
        write_log(_T("PTEST: Bad instruction causing F-line unimplemented instruction exception!\n"));
        Exception(11); /* F-line unimplemented instruction exception */
        return;
    }
        
#if MMU030_OP_DBG_MSG
    write_log(_T("PTEST%c: addr = %08X, fc = %i, level = %i, "),
              rw?'R':'W', extra, fc, level);
    if (a) {
        write_log(_T("return descriptor to register A%i\n"), areg);
    } else {
        write_log(_T("do not return descriptor\n"));
    }
#endif
    
    if (!level) {
        mmu030_ptest_atc_search(extra, fc, write);
    } else {
        ret = mmu030_ptest_table_search(extra, fc, write, level);
        if (a) {
            m68k_areg (regs, areg) = ret;
        }
    }
    mmusr_030 = mmu030.status;
    
#if MMU030_OP_DBG_MSG
    write_log(_T("PTEST status: %04X, B = %i, L = %i, S = %i, W = %i, I = %i, M = %i, T = %i, N = %i\n"),
              mmusr_030, (mmusr_030&MMUSR_BUS_ERROR)?1:0, (mmusr_030&MMUSR_LIMIT_VIOLATION)?1:0,
              (mmusr_030&MMUSR_SUPER_VIOLATION)?1:0, (mmusr_030&MMUSR_WRITE_PROTECTED)?1:0,
              (mmusr_030&MMUSR_INVALID)?1:0, (mmusr_030&MMUSR_MODIFIED)?1:0,
              (mmusr_030&MMUSR_TRANSP_ACCESS)?1:0, mmusr_030&MMUSR_NUM_LEVELS_MASK);
#endif
}

void mmu_op30_pload (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
    int rw = (next >> 9) & 1;
    uae_u32 fc = mmu_op30_helper_get_fc(next);
    
    bool write = rw ? false : true;

#if 0
    write_log (_T("PLOAD%c: Create ATC entry for %08X, FC = %i\n"), write?'W':'R', extra, fc);
#endif

    mmu030_flush_atc_page(extra);
    mmu030_table_search(extra, fc, write, 0);
}

void mmu_op30_pflush (uaecptr pc, uae_u32 opcode, uae_u16 next, uaecptr extra)
{
    uae_u16 mode = (next&0x1C00)>>10;
    uae_u32 fc_mask = (uae_u32)(next&0x00E0)>>5;
    uae_u32 fc_base = mmu_op30_helper_get_fc(next);
    
#if 0
    switch (mode) {
        case 0x1:
            write_log(_T("PFLUSH: Flush all entries\n"));
            break;
        case 0x4:
            write_log(_T("PFLUSH: Flush by function code only\n"));
            write_log(_T("PFLUSH: function code: base = %08X, mask = %08X\n"), fc_base, fc_mask);
            break;
        case 0x6:
            write_log(_T("PFLUSH: Flush by function code and effective address\n"));
            write_log(_T("PFLUSH: function code: base = %08X, mask = %08X\n"), fc_base, fc_mask);
            write_log(_T("PFLUSH: effective address = %08X\n"), extra);
            break;
        default:
            break;
    }
#endif
    
    switch (mode) {
        case 0x1:
            mmu030_flush_atc_all();
            break;
        case 0x4:
            mmu030_flush_atc_fc(fc_base, fc_mask);
            break;
        case 0x6:
            mmu030_flush_atc_page_fc(extra, fc_base, fc_mask);
            break;
            
        default:
            write_log(_T("PFLUSH ERROR: bad mode! (%i)\n"),mode);
            break;
    }
}

/* -- Helper function for MMU instructions -- */
uae_u32 mmu_op30_helper_get_fc(uae_u16 next) {
    switch (next&0x0018) {
        case 0x0010:
            return (next&0x7);
        case 0x0008:
            return (m68k_dreg(regs, next&0x7)&0x7);
        case 0x0000:
            if (next&1) {
                return (regs.dfc);
            } else {
                return (regs.sfc);
            }
        default:
            write_log(_T("MMU_OP30 ERROR: bad fc source! (%04X)\n"),next&0x0018);
            return 0;
    }
}


/* -- ATC flushing functions -- */

/* This function flushes ATC entries depending on their function code */
void mmu030_flush_atc_fc(uae_u32 fc_base, uae_u32 fc_mask) {
    int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (((fc_base&fc_mask)==(mmu030.atc[i].logical.fc&fc_mask)) &&
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
#if MMU030_OP_DBG_MSG
            write_log(_T("ATC: Flushing %08X\n"), mmu030.atc[i].physical.addr);
#endif
		}
    }
}

/* This function flushes ATC entries depending on their logical address
 * and their function code */
void mmu030_flush_atc_page_fc(uaecptr logical_addr, uae_u32 fc_base, uae_u32 fc_mask) {
    int i;
	logical_addr &= mmu030.translation.page.imask;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (((fc_base&fc_mask)==(mmu030.atc[i].logical.fc&fc_mask)) &&
            (mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
#if MMU030_OP_DBG_MSG
            write_log(_T("ATC: Flushing %08X\n"), mmu030.atc[i].physical.addr);
#endif
		}
    }
}

/* This function flushes ATC entries depending on their logical address */
void mmu030_flush_atc_page(uaecptr logical_addr) {
    int i;
	logical_addr &= mmu030.translation.page.imask;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if ((mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            mmu030.atc[i].logical.valid = false;
#if MMU030_OP_DBG_MSG
            write_log(_T("ATC: Flushing %08X\n"), mmu030.atc[i].physical.addr);
#endif
		}
    }
}

/* This function flushes all ATC entries */
void mmu030_flush_atc_all(void) {
#if MMU030_OP_DBG_MSG
	write_log(_T("ATC: Flushing all entries\n"));
#endif
	int i;
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        mmu030.atc[i].logical.valid = false;
    }
}


/* Transparent Translation Registers (TT0 and TT1)
 *
 * ---- ---- ---- ---- -xxx x--- x--- x---
 * reserved, must be 0
 *
 * ---- ---- ---- ---- ---- ---- ---- -xxx
 * function code mask (FC bits to be ignored)
 *
 * ---- ---- ---- ---- ---- ---- -xxx ----
 * function code base (FC value for transparent block)
 *
 * ---- ---- ---- ---- ---- ---x ---- ----
 * 0 = r/w field used, 1 = read and write is transparently translated
 *
 * ---- ---- ---- ---- ---- --x- ---- ----
 * r/w field: 0 = write ..., 1 = read access transparent
 *
 * ---- ---- ---- ---- ---- -x-- ---- ----
 * cache inhibit: 0 = caching allowed, 1 = caching inhibited
 *
 * ---- ---- ---- ---- x--- ---- ---- ----
 * 0 = transparent translation enabled disabled, 1 = enabled
 *
 * ---- ---- xxxx xxxx ---- ---- ---- ----
 * logical address mask
 *
 * xxxx xxxx ---- ---- ---- ---- ---- ----
 * logical address base
 *
 */

/* TT comparision results */
#define TT_NO_MATCH	0x1
#define TT_OK_MATCH	0x2
#define TT_NO_READ  0x4
#define TT_NO_WRITE	0x8

TT_info mmu030_decode_tt(uae_u32 TT) {
    
    TT_info ret;

    ret.fc_mask = ~((TT&TT_FC_MASK)|0xFFFFFFF8);
    ret.fc_base = (TT&TT_FC_BASE)>>4;
    ret.addr_base = TT & TT_ADDR_BASE;
    ret.addr_mask = ~(((TT&TT_ADDR_MASK)<<8)|0x00FFFFFF);
    
#if 0
    if ((TT&TT_ENABLE) && !(TT&TT_RWM)) {
        write_log(_T("MMU Warning: Transparent translation of read-modify-write cycle is not correctly handled!\n"));
    }
#endif

#if MMU030_REG_DBG_MSG /* enable or disable debugging messages */
    write_log(_T("\n"));
    write_log(_T("TRANSPARENT TRANSLATION: %08X\n"), TT);
    write_log(_T("\n"));
    
    write_log(_T("TT: transparent translation "));
    if (TT&TT_ENABLE) {
        write_log(_T("enabled\n"));
    } else {
        write_log(_T("disabled\n"));
        return ret;
    }

    write_log(_T("TT: caching %s\n"), (TT&TT_CI) ? _T("inhibited") : _T("enabled"));
    write_log(_T("TT: read-modify-write "));
    if (TT&TT_RWM) {
        write_log(_T("enabled\n"));
    } else {
        write_log(_T("disabled (%s only)\n"), (TT&TT_RW) ? _T("read") : _T("write"));
    }
    write_log(_T("\n"));
    write_log(_T("TT: function code base: %08X\n"), ret.fc_base);
    write_log(_T("TT: function code mask: %08X\n"), ret.fc_mask);
    write_log(_T("\n"));
    write_log(_T("TT: address base: %08X\n"), ret.addr_base);
    write_log(_T("TT: address mask: %08X\n"), ret.addr_mask);
    write_log(_T("\n"));
#endif
    
    return ret;
}

/* This function compares the address with both transparent
 * translation registers and returns the result */
int mmu030_match_ttr(uaecptr addr, uae_u32 fc, bool write)
{
    int tt0, tt1;

    bool cache_inhibit = false; /* TODO: pass to memory access function */
    
    tt0 = mmu030_do_match_ttr(tt0_030, mmu030.transparent.tt0, addr, fc, write);
    if (tt0&TT_OK_MATCH) {
        cache_inhibit = (tt0_030&TT_CI) ? true : false;
    }
    tt1 = mmu030_do_match_ttr(tt1_030, mmu030.transparent.tt1, addr, fc, write);
    if (tt1&TT_OK_MATCH) {
        if (!cache_inhibit) {
            cache_inhibit = (tt1_030&TT_CI) ? true : false;
        }
    }
    
    return (tt0|tt1);
}
int mmu030_match_ttr_access(uaecptr addr, uae_u32 fc, bool write)
{
    int tt0, tt1;
	if (!tt_enabled)
		return 0;
    tt0 = mmu030_do_match_ttr(tt0_030, mmu030.transparent.tt0, addr, fc, write);
    tt1 = mmu030_do_match_ttr(tt1_030, mmu030.transparent.tt1, addr, fc, write);
    return (tt0|tt1) & TT_OK_MATCH;
}

/* Locked Read-Modify-Write */
int mmu030_match_lrmw_ttr_access(uaecptr addr, uae_u32 fc)
{
    int tt0, tt1;

 	if (!tt_enabled)
		return 0;
    tt0 = mmu030_do_match_lrmw_ttr(tt0_030, mmu030.transparent.tt0, addr, fc);
    tt1 = mmu030_do_match_lrmw_ttr(tt1_030, mmu030.transparent.tt1, addr, fc);
    return (tt0|tt1) & TT_OK_MATCH;
}

/* This function checks if an address matches a transparent
 * translation register */

/* FIXME:
 * If !(tt&TT_RMW) neither the read nor the write portion
 * of a read-modify-write cycle is transparently translated! */

int mmu030_do_match_ttr(uae_u32 tt, TT_info comp, uaecptr addr, uae_u32 fc, bool write)
{
	if (tt & TT_ENABLE)	{	/* transparent translation enabled */
        
        /* Compare actual function code with function code base using mask */
        if ((comp.fc_base&comp.fc_mask)==(fc&comp.fc_mask)) {
            
            /* Compare actual address with address base using mask */
            if ((comp.addr_base&comp.addr_mask)==(addr&comp.addr_mask)) {
                
                if (tt&TT_RWM) {  /* r/w field disabled */
                    return TT_OK_MATCH;
                } else {
                    if (tt&TT_RW) { /* read access transparent */
                        return write ? TT_NO_WRITE : TT_OK_MATCH;
                    } else {        /* write access transparent */
                        return write ? TT_OK_MATCH : TT_NO_READ; /* TODO: check this! */
                    }
                }
            }
		}
	}
	return TT_NO_MATCH;
}

int mmu030_do_match_lrmw_ttr(uae_u32 tt, TT_info comp, uaecptr addr, uae_u32 fc)
{
	if (tt & TT_ENABLE)	{	/* transparent translation enabled */
        
        /* Compare actual function code with function code base using mask */
        if ((comp.fc_base&comp.fc_mask)==(fc&comp.fc_mask)) {
            
            /* Compare actual address with address base using mask */
            if ((comp.addr_base&comp.addr_mask)==(addr&comp.addr_mask)) {
                
                if (tt&TT_RWM) {  /* r/w field disabled */
                    return TT_OK_MATCH;
                }
            }
		}
	}
	return TT_NO_MATCH;
}



/* Translation Control Register:
 *
 * x--- ---- ---- ---- ---- ---- ---- ----
 * translation: 1 = enable, 0 = disable
 *
 * ---- --x- ---- ---- ---- ---- ---- ----
 * supervisor root: 1 = enable, 0 = disable
 *
 * ---- ---x ---- ---- ---- ---- ---- ----
 * function code lookup: 1 = enable, 0 = disable
 *
 * ---- ---- xxxx ---- ---- ---- ---- ----
 * page size:
 * 1000 = 256 bytes
 * 1001 = 512 bytes
 * 1010 =  1 kB
 * 1011 =  2 kB
 * 1100 =  4 kB
 * 1101 =  8 kB
 * 1110 = 16 kB
 * 1111 = 32 kB
 *
 * ---- ---- ---- xxxx ---- ---- ---- ----
 * initial shift
 *
 * ---- ---- ---- ---- xxxx ---- ---- ----
 * number of bits for table index A
 *
 * ---- ---- ---- ---- ---- xxxx ---- ----
 * number of bits for table index B
 *
 * ---- ---- ---- ---- ---- ---- xxxx ----
 * number of bits for table index C
 *
 * ---- ---- ---- ---- ---- ----- ---- xxxx
 * number of bits for table index D
 *
 */


#define TC_ENABLE_TRANSLATION   0x80000000
#define TC_ENABLE_SUPERVISOR    0x02000000
#define TC_ENABLE_FCL           0x01000000

#define TC_PS_MASK              0x00F00000
#define TC_IS_MASK              0x000F0000

#define TC_TIA_MASK             0x0000F000
#define TC_TIB_MASK             0x00000F00
#define TC_TIC_MASK             0x000000F0
#define TC_TID_MASK             0x0000000F


void mmu030_decode_tc(uae_u32 TC) {
        
    /* Set MMU condition */    
    if (TC & TC_ENABLE_TRANSLATION) {
        mmu030.enabled = true;
    } else {
		if (mmu030.enabled)
			write_log(_T("MMU disabled\n"));
        mmu030.enabled = false;
        return;
    }
    
    /* Note: 0 = Table A, 1 = Table B, 2 = Table C, 3 = Table D */
    int i, j;
    uae_u8 TI_bits[4] = {0,0,0,0};

    /* Reset variables before extracting new values from TC */
    for (i = 0; i < 4; i++) {
        mmu030.translation.table[i].mask = 0;
        mmu030.translation.table[i].shift = 0;
    }
    
    
    /* Extract initial shift and page size values from TC register */
    mmu030.translation.page.size = (TC & TC_PS_MASK) >> 20;
    mmu030.translation.init_shift = (TC & TC_IS_MASK) >> 16;
	regs.mmu_page_size = 1 << mmu030.translation.page.size;

    
	write_log(_T("68030 MMU enabled. Page size = %d\n"), regs.mmu_page_size);

	if (mmu030.translation.page.size<8) {
        write_log(_T("MMU Configuration Exception: Bad value in TC register! (bad page size: %i byte)\n"),
                  1<<mmu030.translation.page.size);
        Exception(56); /* MMU Configuration Exception */
        return;
    }
	mmu030.translation.page.mask = regs.mmu_page_size - 1;
	mmu030.translation.page.imask = ~mmu030.translation.page.mask;
    
    /* Calculate masks and shifts for later extracting table indices
     * from logical addresses using: index = (addr&mask)>>shift */
    
    /* Get number of bits for each table index */
    for (i = 0; i < 4; i++) {
        j = (3-i)*4;
        TI_bits[i] = (TC >> j) & 0xF;
    }

    /* Calculate masks and shifts for each table */
    mmu030.translation.last_table = 0;
    uae_u8 shift = 32 - mmu030.translation.init_shift;
    for (i = 0; (i < 4) && TI_bits[i]; i++) {
        /* Get the shift */
        shift -= TI_bits[i];
        mmu030.translation.table[i].shift = shift;
        /* Build the mask */
        for (j = 0; j < TI_bits[i]; j++) {
            mmu030.translation.table[i].mask |= (1<<(mmu030.translation.table[i].shift + j));
        }
        /* Update until reaching the last table */
        mmu030.translation.last_table = i;
    }
    
#if MMU030_REG_DBG_MSG
    /* At least one table has to be defined using at least
     * 1 bit for the index. At least 2 bits are necessary 
     * if there is no second table. If these conditions are
     * not met, it will automatically lead to a sum <32
     * and cause an exception (see below). */
    if (!TI_bits[0]) {
        write_log(_T("MMU Configuration Exception: Bad value in TC register! (no first table index defined)\n"));
    } else if ((TI_bits[0]<2) && !TI_bits[1]) {
        write_log(_T("MMU Configuration Exception: Bad value in TC register! (no second table index defined and)\n"));
        write_log(_T("MMU Configuration Exception: Bad value in TC register! (only 1 bit for first table index)\n"));
    }
#endif
    
    /* TI fields are summed up until a zero field is reached (see above
     * loop). The sum of all TI field values plus page size and initial
     * shift has to be 32: IS + PS + TIA + TIB + TIC + TID = 32 */
    if ((shift-mmu030.translation.page.size)!=0) {
        write_log(_T("MMU Configuration Exception: Bad value in TC register! (bad sum)\n"));
        Exception(56); /* MMU Configuration Exception */
        return;
    }
    
#if MMU030_REG_DBG_MSG /* enable or disable debugging output */
    write_log(_T("\n"));
    write_log(_T("TRANSLATION CONTROL: %08X\n"), TC);
    write_log(_T("\n"));
    write_log(_T("TC: translation %s\n"), (TC&TC_ENABLE_TRANSLATION ? _T("enabled") : _T("disabled")));
    write_log(_T("TC: supervisor root pointer %s\n"), (TC&TC_ENABLE_SUPERVISOR ? _T("enabled") : _T("disabled")));
    write_log(_T("TC: function code lookup %s\n"), (TC&TC_ENABLE_FCL ? _T("enabled") : _T("disabled")));
    write_log(_T("\n"));
    
    write_log(_T("TC: Initial Shift: %i\n"), mmu030.translation.init_shift);
    write_log(_T("TC: Page Size: %i byte\n"), (1<<mmu030.translation.page.size));
    write_log(_T("\n"));
    
    for (i = 0; i <= mmu030.translation.last_table; i++) {
        write_log(_T("TC: Table %c: mask = %08X, shift = %i\n"), table_letter[i], mmu030.translation.table[i].mask, mmu030.translation.table[i].shift);
    }

    write_log(_T("TC: Page:    mask = %08X\n"), mmu030.translation.page.mask);
    write_log(_T("\n"));

    write_log(_T("TC: Last Table: %c\n"), table_letter[mmu030.translation.last_table]);
    write_log(_T("\n"));
#endif
}



/* Root Pointer Registers (SRP and CRP)
 *
 * ---- ---- ---- ---- xxxx xxxx xxxx xx-- ---- ---- ---- ---- ---- ---- ---- xxxx
 * reserved, must be 0
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table A address
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx ---- ---- ---- ---- ---- ---- ---- ----
 * descriptor type
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
 * limit
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit
 *
 */


#define RP_ADDR_MASK    (UVAL64(0x00000000FFFFFFF0))
#define RP_DESCR_MASK   (UVAL64(0x0000000300000000))
#define RP_LIMIT_MASK   (UVAL64(0x7FFF000000000000))
#define RP_LOWER_MASK   (UVAL64(0x8000000000000000))

#define RP_ZERO_BITS 0x0000FFFC /* These bits in upper longword of RP must be 0 */

void mmu030_decode_rp(uae_u64 RP) {
    
    uae_u8 descriptor_type = (RP & RP_DESCR_MASK) >> 32;
    if (!descriptor_type) { /* If descriptor type is invalid */
        write_log(_T("MMU Configuration Exception: Root Pointer is invalid!\n"));
        Exception(56); /* MMU Configuration Exception */
    }

#if MMU030_REG_DBG_MSG /* enable or disable debugging output */
    uae_u32 table_limit = (RP & RP_LIMIT_MASK) >> 48;
    uae_u32 first_addr = (RP & RP_ADDR_MASK);
    
    write_log(_T("\n"));
    write_log(_T("ROOT POINTER: %08X%08X\n"), (uae_u32)(RP>>32)&0xFFFFFFFF, (uae_u32)(RP&0xFFFFFFFF));
    write_log(_T("\n"));
    
    write_log(_T("RP: descriptor type = %i "), descriptor_type);
    switch (descriptor_type) {
        case 0:
            write_log(_T("(invalid descriptor)\n"));
            break;
        case 1:
            write_log(_T("(early termination page descriptor)\n"));
            break;
        case 2:
            write_log(_T("(valid 4 byte descriptor)\n"));
            break;
        case 3:
            write_log(_T("(valid 8 byte descriptor)\n"));
            break;
    }
    
    write_log(_T("RP: %s limit = %i\n"), (RP&RP_LOWER_MASK) ? _T("lower") : _T("upper"), table_limit);
    
    write_log(_T("RP: first table address = %08X\n"), first_addr);
    write_log(_T("\n"));
#endif
}



/* Descriptors */

#define DESCR_TYPE_MASK         0x00000003

#define DESCR_TYPE_INVALID      0 /* all tables */

#define DESCR_TYPE_EARLY_TERM   1 /* all but lowest level table */
#define DESCR_TYPE_PAGE         1 /* only lowest level table */
#define DESCR_TYPE_VALID4       2 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT4    2 /* only lowest level table */
#define DESCR_TYPE_VALID8       3 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT8    3 /* only lowest level table */

#define DESCR_TYPE_VALID_MASK       0x2 /* all but lowest level table */
#define DESCR_TYPE_INDIRECT_MASK    0x2 /* only lowest level table */


/* Short format (4 byte):
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx
 * descriptor type:
 * 0 = invalid
 * 1 = page descriptor (early termination)
 * 2 = valid (4 byte)
 * 3 = valid (8 byte)
 *
 *
 * table descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x--
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x---
 * update
 *
 * xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table address
 *
 *
 * (early termination) page descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x--
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x---
 * update
 *
 * ---- ---- ---- ---- ---- ---- ---x ----
 * modified
 *
 * ---- ---- ---- ---- ---- ---- -x-- ----
 * cache inhibit
 *
 * ---- ---- ---- ---- ---- ---- x-x- ----
 * reserved (must be 0)
 *
 * xxxx xxxx xxxx xxxx xxxx xxxx ---- ----
 * page address
 *
 *
 * indirect descriptor:
 * xxxx xxxx xxxx xxxx xxxx xxxx xxxx xx--
 * descriptor address
 *
 */

#define DESCR_WP       0x00000004
#define DESCR_U        0x00000008
#define DESCR_M        0x00000010 /* only last level table */
#define DESCR_CI       0x00000040 /* only last level table */

#define DESCR_TD_ADDR_MASK 0xFFFFFFF0
#define DESCR_PD_ADDR_MASK 0xFFFFFF00
#define DESCR_ID_ADDR_MASK 0xFFFFFFFC


/* Long format (8 byte):
 *
 * ---- ---- ---- ---- ---- ---- ---- --xx | ---- ---- ---- ---- ---- ---- ---- ----
 * descriptor type:
 * 0 = invalid
 * 1 = page descriptor (early termination)
 * 2 = valid (4 byte)
 * 3 = valid (8 byte)
 *
 *
 * table desctriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x-- | ---- ---- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x--- | ---- ---- ---- ---- ---- ---- ---- ----
 * update
 *
 * ---- ---- ---- ---- ---- ---- xxxx ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 0)
 *
 * ---- ---- ---- ---- ---- ---x ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * supervisor
 *
 * ---- ---- ---- ---- xxxx xxx- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 1111 110)
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * limit
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx xxxx ----
 * table address
 *
 *
 * (early termination) page descriptor:
 * ---- ---- ---- ---- ---- ---- ---- -x-- | ---- ---- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * ---- ---- ---- ---- ---- ---- ---- x--- | ---- ---- ---- ---- ---- ---- ---- ----
 * update
 *
 * ---- ---- ---- ---- ---- ---- ---x ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * modified
 *
 * ---- ---- ---- ---- ---- ---- -x-- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * cache inhibit
 *
 * ---- ---- ---- ---- ---- ---x ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * supervisor
 *
 * ---- ---- ---- ---- ---- ---- x-x- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 0)
 *
 * ---- ---- ---- ---- xxxx xxx- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * reserved (must be 1111 110)
 *
 * -xxx xxxx xxxx xxxx ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * limit (only used with early termination page descriptor)
 *
 * x--- ---- ---- ---- ---- ---- ---- ---- | ---- ---- ---- ---- ---- ---- ---- ----
 * 0 = upper limit, 1 = lower limit (only used with early termination page descriptor)
 *
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx ---- ----
 * page address
 *
 *
 * indirect descriptor:
 * ---- ---- ---- ---- ---- ---- ---- ---- | xxxx xxxx xxxx xxxx xxxx xxxx xxxx xx--
 * descriptor address
 *
 */

/* only for long descriptors */
#define DESCR_S        0x00000100

#define DESCR_LIMIT_MASK   0x7FFF0000
#define DESCR_LOWER_MASK   0x80000000



/* This functions searches through the translation tables. It can be used 
 * for PTEST (levels 1 to 7). Using level 0 creates an ATC entry. */

uae_u32 mmu030_table_search(uaecptr addr, uae_u32 fc, bool write, int level) {
        /* During table walk up to 7 different descriptors are used:
         * root pointer, descriptors fetched from function code lookup table,
         * tables A, B, C and D and one indirect descriptor */
        uae_u32 descr[2];
        uae_u32 descr_type;
        uaecptr descr_addr[7];
        uaecptr table_addr = 0;
        uaecptr page_addr = 0;
        uaecptr indirect_addr = 0;
        uae_u32 table_index = 0;
        uae_u32 limit = 0;
        uae_u32 unused_fields_mask = 0;
        bool super = (fc&4) ? true : false;
        bool super_violation = false;
        bool write_protected = false;
        bool cache_inhibit = false;
        bool descr_modified = false;
        
        mmu030.status = 0; /* Reset status */
        
        /* Initial values for condition variables.
         * Note: Root pointer is long descriptor. */
        int t = 0;
        int addr_position = 1;
        int next_size = 0;
        int descr_size = 8;
        int descr_num = 0;
        bool early_termination = false;
        
        int i;
    
    TRY(prb) {
        /* Use super user root pointer if enabled in TC register and access is in
         * super user mode, else use cpu root pointer. */
        if ((tc_030&TC_ENABLE_SUPERVISOR) && super) {
            descr[0] = (srp_030>>32)&0xFFFFFFFF;
            descr[1] = srp_030&0xFFFFFFFF;
#if MMU030_REG_DBG_MSG
            write_log(_T("Supervisor Root Pointer: %08X%08X\n"),descr[0],descr[1]);
#endif // MMU030_REG_DBG_MSG
        } else {
            descr[0] = (crp_030>>32)&0xFFFFFFFF;
            descr[1] = crp_030&0xFFFFFFFF;
#if MMU030_REG_DBG_MSG
            write_log(_T("CPU Root Pointer: %08X%08X\n"),descr[0],descr[1]);
#endif
		}
                
        if (descr[0]&RP_ZERO_BITS) {
#if MMU030_REG_DBG_MSG
			write_log(_T("MMU Warning: Root pointer reserved bits are non-zero! %08X\n"), descr[0]);
#endif
			descr[0] &= (~RP_ZERO_BITS);
        }
        
        /* Check descriptor type of root pointer */
        descr_type = descr[0]&DESCR_TYPE_MASK;
        switch (descr_type) {
            case DESCR_TYPE_INVALID:
                write_log(_T("Fatal error: Root pointer is invalid descriptor!\n"));
                mmu030.status |= MMUSR_INVALID;
                goto stop_search;
            case DESCR_TYPE_EARLY_TERM:
                write_log(_T("Root pointer is early termination page descriptor.\n"));
                early_termination = true;
                goto handle_page_descriptor;
            case DESCR_TYPE_VALID4:
                next_size = 4;
                break;
            case DESCR_TYPE_VALID8:
                next_size = 8;
                break;
        }
        
        /* If function code lookup is enabled in TC register use function code as
         * index for top level table, limit check not required */
        
        if (tc_030&TC_ENABLE_FCL) {
            write_log(_T("Function code lookup enabled, FC = %i\n"), fc);
            
            addr_position = (descr_size==4) ? 0 : 1;
            table_addr = descr[addr_position]&DESCR_TD_ADDR_MASK;
            table_index = fc; /* table index is function code */
            write_log(_T("Table FCL at %08X: index = %i, "),table_addr,table_index);
            
            /* Fetch next descriptor */
            descr_num++;
            descr_addr[descr_num] = table_addr+(table_index*next_size);
            
            if (next_size==4) {
                descr[0] = phys_get_long(descr_addr[descr_num]);
#if MMU030_REG_DBG_MSG
                write_log(_T("Next descriptor: %08X\n"),descr[0]);
#endif
            } else {
                descr[0] = phys_get_long(descr_addr[descr_num]);
                descr[1] = phys_get_long(descr_addr[descr_num]+4);
#if MMU030_REG_DBG_MSG
                write_log(_T("Next descriptor: %08X%08X\n"),descr[0],descr[1]);
#endif
            }
            
            descr_size = next_size;
            
            /* Check descriptor type */
            descr_type = descr[0]&DESCR_TYPE_MASK;
            switch (descr_type) {
                case DESCR_TYPE_INVALID:
                    write_log(_T("Invalid descriptor!\n"));
                    /* stop table walk */
                    mmu030.status |= MMUSR_INVALID;
                    goto stop_search;
                case DESCR_TYPE_EARLY_TERM:
#if MMU030_REG_DBG_MSG
                    write_log(_T("Early termination page descriptor!\n"));
#endif
					early_termination = true;
                    goto handle_page_descriptor;
                case DESCR_TYPE_VALID4:
                    next_size = 4;
                    break;
                case DESCR_TYPE_VALID8:
                    next_size = 8;
                    break;
            }
        }
        
        
        /* Upper level tables */
        do {
            if (descr_num) { /* if not root pointer */
                /* Check protection */
                if ((descr_size==8) && (descr[0]&DESCR_S) && !super) {
                    super_violation = true;
                }
                if (descr[0]&DESCR_WP) {
                    write_protected = true;
                }
                
                /* Set the updated bit */
                if (!level && !(descr[0]&DESCR_U) && !super_violation) {
                    descr[0] |= DESCR_U;
                    phys_put_long(descr_addr[descr_num], descr[0]);
                }
                
                /* Update status bits */
                mmu030.status |= super_violation ? MMUSR_SUPER_VIOLATION : 0;
                mmu030.status |= write_protected ? MMUSR_WRITE_PROTECTED : 0;
                
                /* Check if ptest level is reached */
                if (level && (level==descr_num)) {
                    goto stop_search;
                }
            }
            
            addr_position = (descr_size==4) ? 0 : 1;
            table_addr = descr[addr_position]&DESCR_TD_ADDR_MASK;
            table_index = (addr&mmu030.translation.table[t].mask)>>mmu030.translation.table[t].shift;
#if MMU030_REG_DBG_MSG
            write_log(_T("Table %c at %08X: index = %i, "),table_letter[t],table_addr,table_index);
#endif // MMU030_REG_DBG_MSG
            t++; /* Proceed to the next table */
            
            /* Perform limit check */
            if (descr_size==8) {
                limit = (descr[0]&DESCR_LIMIT_MASK)>>16;
                if ((descr[0]&DESCR_LOWER_MASK) && (table_index<limit)) {
                    mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
#if MMU030_REG_DBG_MSG
                    write_log(_T("limit violation (lower limit %i)\n"),limit);
#endif
                    goto stop_search;
                }
                if (!(descr[0]&DESCR_LOWER_MASK) && (table_index>limit)) {
                    mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
#if MMU030_REG_DBG_MSG
                    write_log(_T("limit violation (upper limit %i)\n"),limit);
#endif
                    goto stop_search;
                }
            }
            
            /* Fetch next descriptor */
            descr_num++;
            descr_addr[descr_num] = table_addr+(table_index*next_size);
            
            if (next_size==4) {
                descr[0] = phys_get_long(descr_addr[descr_num]);
#if MMU030_REG_DBG_MSG
                write_log(_T("Next descriptor: %08X\n"),descr[0]);
#endif
            } else {
                descr[0] = phys_get_long(descr_addr[descr_num]);
                descr[1] = phys_get_long(descr_addr[descr_num]+4);
#if MMU030_REG_DBG_MSG
                write_log(_T("Next descriptor: %08X%08X\n"),descr[0],descr[1]);
#endif
            }
            
            descr_size = next_size;
            
            /* Check descriptor type */
            descr_type = descr[0]&DESCR_TYPE_MASK;
            switch (descr_type) {
                case DESCR_TYPE_INVALID:
#if MMU030_REG_DBG_MSG
                    write_log(_T("Invalid descriptor!\n"));
#endif
					/* stop table walk */
                    mmu030.status |= MMUSR_INVALID;
                    goto stop_search;
                case DESCR_TYPE_EARLY_TERM:
                    /* go to last level table handling code */
                    if (t<=mmu030.translation.last_table) {
#if MMU030_REG_DBG_MSG
                        write_log(_T("Early termination page descriptor!\n"));
#endif
						early_termination = true;
                    }
                    goto handle_page_descriptor;
                case DESCR_TYPE_VALID4:
                    next_size = 4;
                    break;
                case DESCR_TYPE_VALID8:
                    next_size = 8;
                    break;
            }
        } while (t<=mmu030.translation.last_table);
        
        
        /* Handle indirect descriptor */
        
        /* Check if ptest level is reached */
        if (level && (level==descr_num)) {
            goto stop_search;
        }
        
        addr_position = (descr_size==4) ? 0 : 1;
        indirect_addr = descr[addr_position]&DESCR_ID_ADDR_MASK;
#if MMU030_REG_DBG_MSG
        write_log(_T("Page indirect descriptor at %08X: "),indirect_addr);
#endif

        /* Fetch indirect descriptor */
        descr_num++;
        descr_addr[descr_num] = indirect_addr;
        
        if (next_size==4) {
            descr[0] = phys_get_long(descr_addr[descr_num]);
#if MMU030_REG_DBG_MSG
            write_log(_T("descr = %08X\n"),descr[0]);
#endif
		} else {
            descr[0] = phys_get_long(descr_addr[descr_num]);
            descr[1] = phys_get_long(descr_addr[descr_num]+4);
#if MMU030_REG_DBG_MSG
            write_log(_T("descr = %08X%08X"),descr[0],descr[1]);
#endif
		}
        
        descr_size = next_size;
        
        /* Check descriptor type, only page descriptor is valid */
        descr_type = descr[0]&DESCR_TYPE_MASK;
        if (descr_type!=DESCR_TYPE_PAGE) {
            mmu030.status |= MMUSR_INVALID;
            goto stop_search;
        }
        
    handle_page_descriptor:
        
        if (descr_num) { /* if not root pointer */
            /* check protection */
            if ((descr_size==8) && (descr[0]&DESCR_S) && !super) {
                super_violation = true;
            }
            if (descr[0]&DESCR_WP) {
                write_protected = true;
            }

            if (!level && !super_violation) {
                /* set modified bit */
                if (!(descr[0]&DESCR_M) && write && !write_protected) {
                    descr[0] |= DESCR_M;
                    descr_modified = true;
                }
                /* set updated bit */
                if (!(descr[0]&DESCR_U)) {
                    descr[0] |= DESCR_U;
                    descr_modified = true;
                }
                /* write modified descriptor if neccessary */
                if (descr_modified) {
                    phys_put_long(descr_addr[descr_num], descr[0]);
                }
            }
            
            /* update status bits */
            mmu030.status |= super_violation ? MMUSR_SUPER_VIOLATION : 0;
            mmu030.status |= write_protected ? MMUSR_WRITE_PROTECTED : 0;
            
            /* check if caching is inhibited */
            cache_inhibit = descr[0]&DESCR_CI ? true : false;
            
            /* check for the modified bit and set it in the status register */
            mmu030.status |= (descr[0]&DESCR_M) ? MMUSR_MODIFIED : 0;
        }
        
        /* Check limit using next index field of logical address.
         * Limit is only checked on early termination. If we are
         * still at root pointer level, only check limit, if FCL
         * is disabled. */
        if (early_termination) {
            if (descr_num || !(tc_030&TC_ENABLE_FCL)) {
                if (descr_size==8) {
                    table_index = (addr&mmu030.translation.table[t].mask)>>mmu030.translation.table[t].shift;
                    limit = (descr[0]&DESCR_LIMIT_MASK)>>16;
                    if ((descr[0]&DESCR_LOWER_MASK) && (table_index<limit)) {
                        mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
#if MMU030_REG_DBG_MSG
                        write_log(_T("Limit violation (lower limit %i)\n"),limit);
#endif
                        goto stop_search;
                    }
                    if (!(descr[0]&DESCR_LOWER_MASK) && (table_index>limit)) {
                        mmu030.status |= (MMUSR_LIMIT_VIOLATION|MMUSR_INVALID);
#if MMU030_REG_DBG_MSG
                        write_log(_T("Limit violation (upper limit %i)\n"),limit);
#endif
                        goto stop_search;
                    }
                }
            }
            /* Get all unused bits of the logical address table index field.
             * they are added to the page address */
            /* TODO: They should be added via "unsigned addition". How to? */
            do {
                unused_fields_mask |= mmu030.translation.table[t].mask;
                t++;
            } while (t<=mmu030.translation.last_table);
            page_addr = addr&unused_fields_mask;
#if MMU030_REG_DBG_MSG
            write_log(_T("Logical address unused bits: %08X (mask = %08X)\n"),
                      page_addr,unused_fields_mask);
#endif
		}

        /* Get page address */
        addr_position = (descr_size==4) ? 0 : 1;
        page_addr += (descr[addr_position]&DESCR_PD_ADDR_MASK);
#if MMU030_REG_DBG_MSG
        write_log(_T("Page at %08X\n"),page_addr);
#endif // MMU030_REG_DBG_MSG
        
    stop_search:
        ; /* Make compiler happy */
    } CATCH(prb) {
        /* We jump to this place, if a bus error occured during table search.
         * bBusErrorReadWrite is set in m68000.c, M68000_BusError: read = 1 */
        if (bBusErrorReadWrite) {
            descr_num--;
        }
        mmu030.status |= (MMUSR_BUS_ERROR|MMUSR_INVALID);
        write_log(_T("MMU: Bus error while %s descriptor!\n"),
                  bBusErrorReadWrite?_T("reading"):_T("writing"));
    } ENDTRY
    
    /* check if we have to handle ptest */
    if (level) {
        /* Note: wp, m and sv bits are undefined if the invalid bit is set */
        mmu030.status = (mmu030.status&~MMUSR_NUM_LEVELS_MASK) | descr_num;

        /* If root pointer is page descriptor (descr_num 0), return 0 */
        return descr_num ? descr_addr[descr_num] : 0;
    }
    
    /* Find an ATC entry to replace */
    /* Search for invalid entry */
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        if (!mmu030.atc[i].logical.valid) {
            break;
        }
    }
    /* If there are no invalid entries, replace first entry
     * with history bit not set */
    if (i == ATC030_NUM_ENTRIES) {
        for (i=0; i<ATC030_NUM_ENTRIES; i++) {
            if (!mmu030.atc[i].mru) {
                break;
            }
        }
#if MMU030_REG_DBG_MSG
        write_log(_T("ATC is full. Replacing entry %i\n"), i);
#endif
	}
	if (i >= ATC030_NUM_ENTRIES) {
		i = 0;
		write_log (_T("ATC entry not found!!!\n"));
	}

    mmu030_atc_handle_history_bit(i);
    
    /* Create ATC entry */
    mmu030.atc[i].logical.addr = addr & mmu030.translation.page.imask; /* delete page index bits */
    mmu030.atc[i].logical.fc = fc;
    mmu030.atc[i].logical.valid = true;
    mmu030.atc[i].physical.addr = page_addr & mmu030.translation.page.imask; /* delete page index bits */
    if ((mmu030.status&MMUSR_INVALID) || (mmu030.status&MMUSR_SUPER_VIOLATION)) {
        mmu030.atc[i].physical.bus_error = true;
    } else {
        mmu030.atc[i].physical.bus_error = false;
    }
    mmu030.atc[i].physical.cache_inhibit = cache_inhibit;
    mmu030.atc[i].physical.modified = (mmu030.status&MMUSR_MODIFIED) ? true : false;
    mmu030.atc[i].physical.write_protect = (mmu030.status&MMUSR_WRITE_PROTECTED) ? true : false;
    
#if MMU030_ATC_DBG_MSG    
    write_log(_T("ATC create entry(%i): logical = %08X, physical = %08X, FC = %i\n"), i,
              mmu030.atc[i].logical.addr, mmu030.atc[i].physical.addr,
              mmu030.atc[i].logical.fc);
    write_log(_T("ATC create entry(%i): B = %i, CI = %i, WP = %i, M = %i\n"), i,
              mmu030.atc[i].physical.bus_error?1:0,
              mmu030.atc[i].physical.cache_inhibit?1:0,
              mmu030.atc[i].physical.write_protect?1:0,
              mmu030.atc[i].physical.modified?1:0);
#endif // MMU030_ATC_DBG_MSG
    
    return 0;
}

/* This function is used for PTEST level 0. */
void mmu030_ptest_atc_search(uaecptr logical_addr, uae_u32 fc, bool write) {
    int i;
    mmu030.status = 0;
        
    if (mmu030_match_ttr(logical_addr, fc, write)&TT_OK_MATCH) {
        mmu030.status |= MMUSR_TRANSP_ACCESS;
        return;
    }
    
    for (i = 0; i < ATC030_NUM_ENTRIES; i++) {
        if ((mmu030.atc[i].logical.fc == fc) &&
            (mmu030.atc[i].logical.addr == logical_addr) &&
            mmu030.atc[i].logical.valid) {
            break;
        }
    }
    
    if (i==ATC030_NUM_ENTRIES) {
        mmu030.status |= MMUSR_INVALID;
        return;
    }
    
    mmu030.status |= mmu030.atc[i].physical.bus_error ? (MMUSR_BUS_ERROR|MMUSR_INVALID) : 0;
    /* Note: write protect and modified bits are undefined if the invalid bit is set */
    mmu030.status |= mmu030.atc[i].physical.write_protect ? MMUSR_WRITE_PROTECTED : 0;
    mmu030.status |= mmu030.atc[i].physical.modified ? MMUSR_MODIFIED : 0;
}

/* This function is used for PTEST level 1 - 7. */
uae_u32 mmu030_ptest_table_search(uaecptr logical_addr, uae_u32 fc, bool write, int level) {
    if (mmu030_match_ttr(logical_addr, fc, write)&TT_OK_MATCH) {
        return 0;
    } else {
        return mmu030_table_search(logical_addr, fc, write, level);
    }
}


/* Address Translation Cache
 *
 * The ATC uses a pseudo-least-recently-used algorithm to keep track of
 * least recently used entries. They are replaced if the cache is full.
 * An internal history-bit (MRU-bit) is used to identify these entries.
 * If an entry is accessed, its history-bit is set to 1. If after that
 * there are no more entries with zero-bits, all other history-bits are
 * set to 0. When no more invalid entries are in the ATC, the first entry
 * with a zero-bit is replaced.
 *
 *
 * Logical Portion (28 bit):
 * oooo ---- xxxx xxxx xxxx xxxx xxxx xxxx
 * logical address (most significant 24 bit)
 *
 * oooo -xxx ---- ---- ---- ---- ---- ----
 * function code
 *
 * oooo x--- ---- ---- ---- ---- ---- ----
 * valid
 *
 *
 * Physical Portion (28 bit):
 * oooo ---- xxxx xxxx xxxx xxxx xxxx xxxx
 * physical address
 *
 * oooo ---x ---- ---- ---- ---- ---- ----
 * modified
 *
 * oooo --x- ---- ---- ---- ---- ---- ----
 * write protect
 *
 * oooo -x-- ---- ---- ---- ---- ---- ----
 * cache inhibit
 *
 * oooo x--- ---- ---- ---- ---- ---- ----
 * bus error
 *
 */

#define ATC030_MASK         0x0FFFFFFF
#define ATC030_ADDR_MASK    0x00FFFFFF /* after masking shift 8 (<< 8) */

#define ATC030_LOG_FC   0x07000000
#define ATC030_LOG_V    0x08000000

#define ATC030_PHYS_M   0x01000000
#define ATC030_PHYS_WP  0x02000000
#define ATC030_PHYS_CI  0x04000000
#define ATC030_PHYS_BE  0x08000000

void mmu030_page_fault(uaecptr addr, bool read, int flags, uae_u32 fc) {
	regs.mmu_fault_addr = addr;
	regs.mmu_ssw = (fc & 1) ? MMU030_SSW_DF | (MMU030_SSW_DF << 1) : (MMU030_SSW_FB | MMU030_SSW_RB);
	regs.mmu_ssw |= read ? MMU030_SSW_RW : 0;
	regs.mmu_ssw |= flags;
	regs.mmu_ssw |= fc;
    bBusErrorReadWrite = read; 
	mm030_stageb_address = addr;
#if MMUDEBUG
	write_log(_T("MMU: page fault (logical addr=%08X SSW=%04x read=%d size=%d fc=%d pc=%08x ob=%08x ins=%04X)\n"),
		addr, regs.mmu_ssw, read, (flags & MMU030_SSW_SIZE_B) ? 1 : (flags & MMU030_SSW_SIZE_W) ? 2 : 4, fc,
		regs.instruction_pc, (mmu030_state[1] & MMU030_STATEFLAG1_MOVEM1) ? mmu030_data_buffer : mmu030_ad[mmu030_idx].val, mmu030_opcode & 0xffff);
#endif
	
//	extern void activate_debugger(void);
//	activate_debugger ();

	THROW(2);
}

void mmu030_put_long_atc(uaecptr addr, uae_u32 val, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (lput %08X)\n"),
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        mmu030_page_fault(addr, false, MMU030_SSW_SIZE_L, fc);
        return;
    }

    phys_put_long(physical_addr, val);
}

void mmu030_put_word_atc(uaecptr addr, uae_u16 val, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (wput %04X)\n"),
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        mmu030_page_fault(addr, false, MMU030_SSW_SIZE_W, fc);
        return;
    }

    phys_put_word(physical_addr, val);
}

void mmu030_put_byte_atc(uaecptr addr, uae_u8 val, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (bput %02X)\n"),
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error || mmu030.atc[l].physical.write_protect) {
        mmu030_page_fault(addr, false, MMU030_SSW_SIZE_B, fc);
        return;
    }

    phys_put_byte(physical_addr, val);
}

uae_u32 mmu030_get_long_atc(uaecptr addr, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (lget %08X)\n"), l,
              physical_addr, page_index, phys_get_long(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        mmu030_page_fault(addr, true, MMU030_SSW_SIZE_L, fc);
        return 0;
    }

    return phys_get_long(physical_addr);
}

uae_u16 mmu030_get_word_atc(uaecptr addr, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (wget %04X)\n"), l,
              physical_addr, page_index, phys_get_word(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        mmu030_page_fault(addr, true, MMU030_SSW_SIZE_W, fc);
        return 0;
    }
    
    return phys_get_word(physical_addr);
}

uae_u8 mmu030_get_byte_atc(uaecptr addr, int l, uae_u32 fc) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (bget %02X)\n"), l,
              physical_addr, page_index, phys_get_byte(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.bus_error) {
        mmu030_page_fault(addr, true, MMU030_SSW_SIZE_B, fc);
        return 0;
    }

    return phys_get_byte(physical_addr);
}

/* Generic versions of above */
void mmu030_put_atc_generic(uaecptr addr, uae_u32 val, int l, uae_u32 fc, int size, int flags) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr & addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (bput %02X)\n"),
              l, physical_addr, page_index, val);
#endif
    physical_addr += page_index;
    
    if (mmu030.atc[l].physical.write_protect || mmu030.atc[l].physical.bus_error) {
		mmu030_page_fault(addr, false, flags, fc);
        return;
    }
	if (size == sz_byte)
	    phys_put_byte(physical_addr, val);
	else if (size == sz_word)
	    phys_put_word(physical_addr, val);
	else
	    phys_put_long(physical_addr, val);

}
uae_u32 mmu030_get_atc_generic(uaecptr addr, int l, uae_u32 fc, int size, int flags, bool checkwrite) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr & addr_mask;
#if MMU030_ATC_DBG_MSG
    write_log(_T("ATC match(%i): page addr = %08X, index = %08X (bget %02X)\n"), l,
              physical_addr, page_index, phys_get_byte(physical_addr+page_index));
#endif
    physical_addr += page_index;
    
	if (mmu030.atc[l].physical.bus_error || (checkwrite && mmu030.atc[l].physical.write_protect)) {
        mmu030_page_fault(addr, true, flags, fc);
        return 0;
    }
	if (size == sz_byte)
		return phys_get_byte(physical_addr);
	else if (size == sz_word)
		return phys_get_word(physical_addr);
	return phys_get_long(physical_addr);
}


/* This function checks if a certain logical address is in the ATC 
 * by comparing the logical address and function code to the values
 * stored in the ATC entries. If a matching entry is found it sets
 * the history bit and returns the cache index of the entry. */
int mmu030_logical_is_in_atc(uaecptr addr, uae_u32 fc, bool write) {
    uaecptr logical_addr = 0;
    uae_u32 addr_mask = mmu030.translation.page.imask;
	uae_u32 maddr = addr & addr_mask;
    int offset = (maddr >> mmu030.translation.page.size) & 0x1f;
    
    int i, index;
	index = atcindextable[offset];
    for (i=0; i<ATC030_NUM_ENTRIES; i++) {
        logical_addr = mmu030.atc[index].logical.addr;
        /* If actual address matches address in ATC */
        if (maddr==(logical_addr&addr_mask) &&
            (mmu030.atc[index].logical.fc==fc) &&
            mmu030.atc[index].logical.valid) {
            /* If access is valid write and M bit is not set, invalidate entry
             * else return index */
            if (!write || mmu030.atc[index].physical.modified ||
                mmu030.atc[index].physical.write_protect ||
                mmu030.atc[index].physical.bus_error) {
                /* Maintain history bit */
                mmu030_atc_handle_history_bit(index);
                atcindextable[offset] = index;
                return index;
            } else {
                mmu030.atc[index].logical.valid = false;
            }
		}
		index++;
		if (index >= ATC030_NUM_ENTRIES)
			index = 0;
    }
    return -1;
}

void mmu030_atc_handle_history_bit(int entry_num) {
    int j;
    mmu030.atc[entry_num].mru = 1;
    for (j=0; j<ATC030_NUM_ENTRIES; j++) {
        if (!mmu030.atc[j].mru)
            break;
    }
    /* If there are no more zero-bits, reset all */
    if (j==ATC030_NUM_ENTRIES) {
        for (j=0; j<ATC030_NUM_ENTRIES; j++) {
            mmu030.atc[j].mru = 0;
        }
        mmu030.atc[entry_num].mru = 1;
#if MMU030_ATC_DBG_MSG
        write_log(_T("ATC: No more history zero-bits. Reset all.\n"));
#endif
	}
}


/* Memory access functions:
 * If the address matches one of the transparent translation registers
 * use it directly as physical address, else check ATC for the
 * logical address. If the logical address is not resident in the ATC
 * create a new ATC entry and then look up the physical address. 
 */

void mmu030_put_long(uaecptr addr, uae_u32 val, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,true)) || (fc==7)) {
		phys_put_long(addr,val);
		return;
    }

    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);

    if (atc_line_num>=0) {
        mmu030_put_long_atc(addr, val, atc_line_num, fc);
    } else {
        mmu030_table_search(addr,fc,true,0);
        mmu030_put_long_atc(addr, val, mmu030_logical_is_in_atc(addr,fc,true), fc);
    }
}

void mmu030_put_word(uaecptr addr, uae_u16 val, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,true)) || (fc==7)) {
		phys_put_word(addr,val);
		return;
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);
    
    if (atc_line_num>=0) {
        mmu030_put_word_atc(addr, val, atc_line_num, fc);
    } else {
        mmu030_table_search(addr, fc, true, 0);
        mmu030_put_word_atc(addr, val, mmu030_logical_is_in_atc(addr,fc,true), fc);
    }
}

void mmu030_put_byte(uaecptr addr, uae_u8 val, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr, fc, true)) || (fc==7)) {
		phys_put_byte(addr,val);
		return;
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);

    if (atc_line_num>=0) {
        mmu030_put_byte_atc(addr, val, atc_line_num, fc);
    } else {
        mmu030_table_search(addr, fc, true, 0);
        mmu030_put_byte_atc(addr, val, mmu030_logical_is_in_atc(addr,fc,true), fc);
    }
}

uae_u32 mmu030_get_long(uaecptr addr, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,false)) || (fc==7)) {
		return phys_get_long(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, false);

    if (atc_line_num>=0) {
        return mmu030_get_long_atc(addr, atc_line_num, fc);
    } else {
        mmu030_table_search(addr, fc, false, 0);
        return mmu030_get_long_atc(addr, mmu030_logical_is_in_atc(addr,fc,false), fc);
    }
}

uae_u16 mmu030_get_word(uaecptr addr, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,false)) || (fc==7)) {
		return phys_get_word(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, false);

    if (atc_line_num>=0) {
        return mmu030_get_word_atc(addr, atc_line_num, fc);
    } else {
        mmu030_table_search(addr, fc, false, 0);
        return mmu030_get_word_atc(addr, mmu030_logical_is_in_atc(addr,fc,false), fc);
    }
}

uae_u8 mmu030_get_byte(uaecptr addr, uae_u32 fc) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,false)) || (fc==7)) {
		return phys_get_byte(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, false);

    if (atc_line_num>=0) {
        return mmu030_get_byte_atc(addr, atc_line_num, fc);
    } else {
        mmu030_table_search(addr, fc, false, 0);
        return mmu030_get_byte_atc(addr, mmu030_logical_is_in_atc(addr,fc,false), fc);
    }
}


/* Not commonly used access function */
void mmu030_put_generic(uaecptr addr, uae_u32 val, uae_u32 fc, int size, int accesssize, int flags) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr, fc, true)) || (fc==7)) {
		if (size == sz_byte)
			phys_put_byte(addr, val);
		else if (size == sz_word)
			phys_put_word(addr, val);
		else
			phys_put_long(addr, val);
		return;
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);
    if (atc_line_num>=0) {
        mmu030_put_atc_generic(addr, val, atc_line_num, fc, size, flags);
    } else {
        mmu030_table_search(addr, fc, true, 0);
		atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);
		if (accesssize == sz_byte)
			flags |= MMU030_SSW_SIZE_B;
		else if (accesssize == sz_word)
			flags |= MMU030_SSW_SIZE_W;
        mmu030_put_atc_generic(addr, val, atc_line_num, fc, size, flags);
    }
}
static uae_u32 mmu030_get_generic_lrmw(uaecptr addr, uae_u32 fc, int size, int accesssize, int flags) {
    
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_lrmw_ttr_access(addr,fc)) || (fc==7)) {
		if (size == sz_byte)
			return phys_get_byte(addr);
		else if (size == sz_word)
			return phys_get_word(addr);
		return phys_get_long(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);
    if (atc_line_num>=0) {
        return mmu030_get_atc_generic(addr, atc_line_num, fc, size, flags, true);
    } else {
        mmu030_table_search(addr, fc, true, 0);
		atc_line_num = mmu030_logical_is_in_atc(addr, fc, true);
		if (accesssize == sz_byte)
			flags |= MMU030_SSW_SIZE_B;
		else if (accesssize == sz_word)
			flags |= MMU030_SSW_SIZE_W;
        return mmu030_get_atc_generic(addr, atc_line_num, fc, size, flags, true);
    }
}
uae_u32 mmu030_get_generic(uaecptr addr, uae_u32 fc, int size, int accesssize, int flags) {
	if (flags & MMU030_SSW_RM) {
		return mmu030_get_generic_lrmw(addr, fc, size, accesssize, flags);
	}
	//                                        addr,super,write
	if ((!mmu030.enabled) || (mmu030_match_ttr_access(addr,fc,false)) || (fc==7)) {
		if (size == sz_byte)
			return phys_get_byte(addr);
		else if (size == sz_word)
			return phys_get_word(addr);
		return phys_get_long(addr);
    }
    
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, false);
    if (atc_line_num>=0) {
        return mmu030_get_atc_generic(addr, atc_line_num, fc, size, flags, false);
    } else {
        mmu030_table_search(addr, fc, false, 0);
		atc_line_num = mmu030_logical_is_in_atc(addr, fc, false);
		if (accesssize == sz_byte)
			flags |= MMU030_SSW_SIZE_B;
		else if (accesssize == sz_word)
			flags |= MMU030_SSW_SIZE_W;
        return mmu030_get_atc_generic(addr, atc_line_num, fc, size, flags, false);
    }
}


/* Locked RMW is rarely used */
uae_u32 uae_mmu030_get_lrmw(uaecptr addr, int size)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;
	if (size == sz_byte) {
		return mmu030_get_generic(addr, fc, size, size, MMU030_SSW_RM);
	} else if (size == sz_word) {
		if (unlikely(is_unaligned(addr, 2)))
			return mmu030_get_word_unaligned(addr, fc, MMU030_SSW_RM);
		else
			return mmu030_get_generic(addr, fc, size, size, MMU030_SSW_RM);
	} else {
		if (unlikely(is_unaligned(addr, 4)))
			return mmu030_get_long_unaligned(addr, fc, MMU030_SSW_RM);
		else
			return mmu030_get_generic(addr, fc, size, size, MMU030_SSW_RM);
	}
}
void uae_mmu030_put_lrmw(uaecptr addr, uae_u32 val, int size)
{
    uae_u32 fc = (regs.s ? 4 : 0) | 1;
	if (size == sz_byte) {
		mmu030_put_generic(addr, val, fc, size, size, MMU030_SSW_RM);
	} else if (size == sz_word) {
		if (unlikely(is_unaligned(addr, 2)))
			mmu030_put_word_unaligned(addr, val, fc, MMU030_SSW_RM);
		else
			mmu030_put_generic(addr, val, fc, size, size, MMU030_SSW_RM);
	} else {
		if (unlikely(is_unaligned(addr, 4)))
			mmu030_put_long_unaligned(addr, val, fc, MMU030_SSW_RM);
		else
			mmu030_put_generic(addr, val, fc, size, size, MMU030_SSW_RM);
	}
}
uae_u16 REGPARAM2 mmu030_get_word_unaligned(uaecptr addr, uae_u32 fc, int flags)
{
	uae_u16 res;
    
	res = (uae_u16)mmu030_get_generic(addr, fc, sz_byte, sz_word, flags) << 8;
	SAVE_EXCEPTION;
	TRY(prb) {
		res |= mmu030_get_generic(addr + 1, fc, sz_byte, sz_word, flags);
		RESTORE_EXCEPTION;
	}
	CATCH(prb) {
		RESTORE_EXCEPTION;
		THROW_AGAIN(prb);
	} ENDTRY
	return res;
}

uae_u32 REGPARAM2 mmu030_get_long_unaligned(uaecptr addr, uae_u32 fc, int flags)
{
	uae_u32 res;
    
	if (likely(!(addr & 1))) {
		res = (uae_u32)mmu030_get_generic(addr, fc, sz_word, sz_long, flags) << 16;
		SAVE_EXCEPTION;
		TRY(prb) {
			res |= mmu030_get_generic(addr + 2, fc, sz_word, sz_long, flags);
			RESTORE_EXCEPTION;
		}
		CATCH(prb) {
			RESTORE_EXCEPTION;
			THROW_AGAIN(prb);
		} ENDTRY
	} else {
		res = (uae_u32)mmu030_get_generic(addr, fc, sz_byte, sz_long, flags) << 8;
		SAVE_EXCEPTION;
		TRY(prb) {
			res = (res | mmu030_get_generic(addr + 1, fc, sz_byte, sz_long, flags)) << 8;
			res = (res | mmu030_get_generic(addr + 2, fc, sz_byte, sz_long, flags)) << 8;
			res |= mmu030_get_generic(addr + 3, fc, sz_byte, sz_long, flags);
			RESTORE_EXCEPTION;
		}
		CATCH(prb) {
			RESTORE_EXCEPTION;
			THROW_AGAIN(prb);
		} ENDTRY
	}
	return res;
}


void REGPARAM2 mmu030_put_long_unaligned(uaecptr addr, uae_u32 val, uae_u32 fc, int flags)
{
	SAVE_EXCEPTION;
	TRY(prb) {
		if (likely(!(addr & 1))) {
			mmu030_put_generic(addr, val >> 16, fc, sz_word, sz_long, flags);
			mmu030_put_generic(addr + 2, val, fc, sz_word, sz_long, flags);
		} else {
			mmu030_put_generic(addr, val >> 24, fc, sz_byte, sz_long, flags);
			mmu030_put_generic(addr + 1, val >> 16, fc, sz_byte, sz_long, flags);
			mmu030_put_generic(addr + 2, val >> 8, fc, sz_byte, sz_long, flags);
			mmu030_put_generic(addr + 3, val, fc, sz_byte, sz_long, flags);
		}
		RESTORE_EXCEPTION;
	}
	CATCH(prb) {
		RESTORE_EXCEPTION;
		regs.wb3_data = val;
		THROW_AGAIN(prb);
	} ENDTRY
}

void REGPARAM2 mmu030_put_word_unaligned(uaecptr addr, uae_u16 val, uae_u32 fc, int flags)
{
	SAVE_EXCEPTION;
	TRY(prb) {
		mmu030_put_generic(addr, val >> 8, fc, sz_byte, sz_word, flags);
		mmu030_put_generic(addr + 1, val, fc, sz_byte, sz_word, flags);
		RESTORE_EXCEPTION;
	}
	CATCH(prb) {
		RESTORE_EXCEPTION;
		regs.wb3_data = val;
		THROW_AGAIN(prb);
	} ENDTRY
}


/* Used by debugger */
static uaecptr mmu030_get_addr_atc(uaecptr addr, int l, uae_u32 fc, bool write) {
    uae_u32 page_index = addr & mmu030.translation.page.mask;
    uae_u32 addr_mask = mmu030.translation.page.imask;
    
    uae_u32 physical_addr = mmu030.atc[l].physical.addr&addr_mask;
    physical_addr += page_index;
    
	if (mmu030.atc[l].physical.bus_error || (write && mmu030.atc[l].physical.write_protect)) {
        mmu030_page_fault(addr, write == 0, MMU030_SSW_SIZE_B, fc);
        return 0;
    }

    return physical_addr;
}
uaecptr mmu030_translate(uaecptr addr, bool super, bool data, bool write)
{
	int fc = (super ? 4 : 0) | (data ? 1 : 2);
	if ((!mmu030.enabled) || (mmu030_match_ttr(addr,fc,write)&TT_OK_MATCH) || (fc==7)) {
		return addr;
    }
    int atc_line_num = mmu030_logical_is_in_atc(addr, fc, write);

    if (atc_line_num>=0) {
        return mmu030_get_addr_atc(addr, atc_line_num, fc, write);
    } else {
        mmu030_table_search(addr, fc, false, 0);
        return mmu030_get_addr_atc(addr, mmu030_logical_is_in_atc(addr,fc,write), fc, write);
    }
}

/* MMU Reset */
void mmu030_reset(int hardreset)
{
    /* A CPU reset causes the E-bits of TC and TT registers to be zeroed. */
    mmu030.enabled = false;
	regs.mmu_page_size = 0;
	tc_030 &= ~TC_ENABLE_TRANSLATION;
	tt0_030 &= ~TT_ENABLE;
	tt1_030 &= ~TT_ENABLE;
	if (hardreset) {
		srp_030 = crp_030 = 0;
		tt0_030 = tt1_030 = tc_030 = 0;
        mmusr_030 = 0;
        mmu030_flush_atc_all();
	}
}


void m68k_do_rte_mmu030 (uaecptr a7)
{
	// Restore access error exception state

	uae_u16 format = get_word_mmu030 (a7 + 6);
	uae_u16 frame = format >> 12;
	uae_u16 ssw = get_word_mmu030 (a7 + 10);
    
    // Fetch last word, real CPU does it to allow OS bus handler to map
    // the page if frame crosses pages and following page is not resident.
    if (frame == 0xb)
        get_word_mmu030(a7 + 92 - 2);
    else
        get_word_mmu030(a7 + 32 - 2);
    
	// Internal register, our opcode storage area
	mmu030_opcode = get_long_mmu030 (a7 + 0x14);
	// Misc state data
	mmu030_state[0] = get_word_mmu030 (a7 + 0x30);
	mmu030_state[1] = get_word_mmu030 (a7 + 0x32);
	mmu030_state[2] = get_word_mmu030 (a7 + 0x34);
	mmu030_disp_store[0] = get_long_mmu030 (a7 + 0x1c);
	mmu030_disp_store[1] = get_long_mmu030 (a7 + 0x1c + 4);
    if (mmu030_state[1] & MMU030_STATEFLAG1_FMOVEM) {
        mmu030_fmovem_store[0] = get_long_mmu030 (a7 + 0x5c - (7 + 1) * 4);
        mmu030_fmovem_store[1] = get_long_mmu030 (a7 + 0x5c - (8 + 1) * 4);
    }
	// Rerun "mmu030_opcode" using restored state.
	mmu030_retry = true;

	if (frame == 0xb) {
		uae_u16 idxsize = get_word_mmu030 (a7 + 0x36);
		for (int i = 0; i < idxsize + 1; i++) {
			mmu030_ad[i].done = i < idxsize;
			mmu030_ad[i].val = get_long_mmu030 (a7 + 0x5c - (i + 1) * 4);
		}
		mmu030_ad[idxsize + 1].done = false;
		// did we have data fault but DF bit cleared?
		if (ssw & (MMU030_SSW_DF << 1) && !(ssw & MMU030_SSW_DF)) {
			// DF not set: mark access as done
			if (ssw & MMU030_SSW_RM) {
				// Read-Modify-Write: whole instruction is considered done
				write_log (_T("Read-Modify-Write and DF bit cleared! PC=%08x\n"), regs.instruction_pc);
				mmu030_retry = false;
			} else if (mmu030_state[1] & MMU030_STATEFLAG1_MOVEM1) {
				// if movem, skip next move
				mmu030_data_buffer = get_long_mmu030 (a7 + 0x2c);
				mmu030_state[1] |= MMU030_STATEFLAG1_MOVEM2;
			} else {
				mmu030_ad[idxsize].done = true;
				if (ssw & MMU030_SSW_RW) {
					// Read and no DF: use value in data input buffer
					mmu030_data_buffer = get_long_mmu030 (a7 + 0x2c);
					mmu030_ad[idxsize].val = mmu030_data_buffer;
				}
			}
		}
		// did we have ins fault and RB bit cleared?
		if ((ssw & MMU030_SSW_FB) && !(ssw & MMU030_SSW_RB)) {
			uae_u16 stageb = get_word_mmu030 (a7 + 0x0e);
			if (mmu030_opcode == -1) {
				mmu030_opcode_stageb = stageb;
				write_log (_T("Software fixed stage B! opcode = %04x\n"), stageb);
			} else {
				mmu030_ad[idxsize].done = true;
				mmu030_ad[idxsize].val = stageb;
				write_log (_T("Software fixed stage B! opcode = %04X, opword = %04x\n"), mmu030_opcode, stageb);
			}
		}
		m68k_areg (regs, 7) += 92;
	} else {
		m68k_areg (regs, 7) += 32;
	}
}

void flush_mmu030 (uaecptr addr, int n)
{
}

void m68k_do_rts_mmu030 (void)
{
	m68k_setpc (get_long_mmu030_state (m68k_areg (regs, 7)));
	m68k_areg (regs, 7) += 4;
}

void m68k_do_bsr_mmu030 (uaecptr oldpc, uae_s32 offset)
{
	put_long_mmu030_state (m68k_areg (regs, 7) - 4, oldpc);
	m68k_areg (regs, 7) -= 4;
	m68k_incpci (offset);
}

uae_u32 REGPARAM2 get_disp_ea_020_mmu030 (uae_u32 base, int idx)
{
	uae_u16 dp;
	int reg;
	uae_u32 v;
	int oldidx;
	int pcadd = 0;

	// we need to do this hack here because in worst case we don't have enough
	// stack frame space to store two very large 020 addressing mode access state
	// + whatever the instruction itself does.

	if (mmu030_state[1] & (1 << idx)) {
		m68k_incpci (((mmu030_state[2] >> (idx * 4)) & 15) * 2);
		return mmu030_disp_store[idx];
	}

	oldidx = mmu030_idx;
	dp = next_iword_mmu030_state ();
	pcadd += 1;
	
	reg = (dp >> 12) & 15;
	uae_s32 regd = regs.regs[reg];
	if ((dp & 0x800) == 0)
		regd = (uae_s32)(uae_s16)regd;
	regd <<= (dp >> 9) & 3;
	if (dp & 0x100) {
		uae_s32 outer = 0;
		if (dp & 0x80)
			base = 0;
		if (dp & 0x40)
			regd = 0;

		if ((dp & 0x30) == 0x20) {
			base += (uae_s32)(uae_s16) next_iword_mmu030_state ();
			pcadd += 1;
		}
		if ((dp & 0x30) == 0x30) {
			base += next_ilong_mmu030_state ();
			pcadd += 2;
		}

		if ((dp & 0x3) == 0x2) {
			outer = (uae_s32)(uae_s16) next_iword_mmu030_state ();
			pcadd += 1;
		}
		if ((dp & 0x3) == 0x3) {
			outer = next_ilong_mmu030_state ();
			pcadd += 2;
		}

		if ((dp & 0x4) == 0) {
			base += regd;
		}
		if (dp & 0x3) {
			base = get_long_mmu030_state (base);
		}
		if (dp & 0x4) {
			base += regd;
		}
		v = base + outer;
	} else {
		v = base + (uae_s32)((uae_s8)dp) + regd;
	}

	mmu030_state[1] |= 1 << idx;
	mmu030_state[2] |= pcadd << (idx * 4);
	mmu030_disp_store[idx] = v;
	mmu030_idx = oldidx;
	mmu030_ad[mmu030_idx].done = false;

	return v;
}
