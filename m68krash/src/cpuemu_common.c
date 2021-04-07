

#include "sysconfig.h"
#include "sysdeps.h"

#define MOVEC_DEBUG 0

#include "options_cpu.h"
#include "memory.h"
#include "newcpu.h"
#include "cpummu.h"
#include "cpummu030.h"
#include "cpu_prefetch.h"

#include "log.h"

void val_move2c2 (int regno, uae_u32 val)
{
	switch (regno) {
	case 0: regs.sfc = val; break;
	case 1: regs.dfc = val; break;
	case 2: regs.cacr = val; break;
	case 3: regs.tcr = val; break;
	case 4: regs.itt0 = val; break;
	case 5: regs.itt1 = val; break;
	case 6: regs.dtt0 = val; break;
	case 7: regs.dtt1 = val; break;
	case 8: regs.buscr = val; break;
	case 0x800: regs.usp = val; break;
	case 0x801: regs.vbr = val; break;
	case 0x802: regs.caar = val; break;
	case 0x803: regs.msp = val; break;
	case 0x804: regs.isp = val; break;
	case 0x805: regs.mmusr = val; break;
	case 0x806: regs.urp = val; break;
	case 0x807: regs.srp = val; break;
	case 0x808: regs.pcr = val; break;
	}
}

uae_u32 val_move2c (int regno)
{
	switch (regno) {
	case 0: return regs.sfc;
	case 1: return regs.dfc;
	case 2: return regs.cacr;
	case 3: return regs.tcr;
	case 4: return regs.itt0;
	case 5: return regs.itt1;
	case 6: return regs.dtt0;
	case 7: return regs.dtt1;
	case 8: return regs.buscr;
	case 0x800: return regs.usp;
	case 0x801: return regs.vbr;
	case 0x802: return regs.caar;
	case 0x803: return regs.msp;
	case 0x804: return regs.isp;
	case 0x805: return regs.mmusr;
	case 0x806: return regs.urp;
	case 0x807: return regs.srp;
	case 0x808: return regs.pcr;
	default: return 0;
	}
}


#ifndef CPUEMU_68000_ONLY

int movec_illg (int regno)
{
	int regno2 = regno & 0x7ff;

	if (currprefs.cpu_model == 68060) {
		if (regno <= 8)
			return 0;
		if (regno == 0x800 || regno == 0x801 ||
			regno == 0x806 || regno == 0x807 || regno == 0x808)
			return 0;
		return 1;
	} else if (currprefs.cpu_model == 68010) {
		if (regno2 < 2)
			return 0;
		return 1;
	} else if (currprefs.cpu_model == 68020) {
		if (regno == 3)
			return 1; /* 68040/060 only */
		/* 4 is >=68040, but 0x804 is in 68020 */
		if (regno2 < 4 || regno == 0x804)
			return 0;
		return 1;
	} else if (currprefs.cpu_model == 68030) {
		if (regno2 <= 2)
			return 0;
		if (regno == 0x803 || regno == 0x804)
			return 0;
		return 1;
	} else if (currprefs.cpu_model == 68040) {
		if (regno == 0x802)
			return 1; /* 68020/030 only */
		if (regno2 < 8) return 0;
		return 1;
	}
	return 1;
}

int m68k_move2c (int regno, uae_u32 *regp)
{
#if MOVEC_DEBUG > 0
	write_log (_T("move2c %04X <- %08X PC=%x\n"), regno, *regp, M68K_GETPC);
#endif
	if (movec_illg (regno)) {
		op_illg (0x4E7B);
		return 0;
	} else {
		switch (regno) {
		case 0: regs.sfc = *regp & 7; break;
		case 1: regs.dfc = *regp & 7; break;
		case 2:
			{
				uae_u32 cacr_mask = 0;
				if (currprefs.cpu_model == 68020)
					cacr_mask = 0x0000000f;
				else if (currprefs.cpu_model == 68030)
					cacr_mask = 0x00003f1f;
				else if (currprefs.cpu_model == 68040)
					cacr_mask = 0x80008000;
				else if (currprefs.cpu_model == 68060)
					cacr_mask = 0xf8e0e000;
				regs.cacr = *regp & cacr_mask;
				set_cpu_caches (false);
			}
			break;
			/* 68040/060 only */
		case 3:
			regs.tcr = *regp & (currprefs.cpu_model == 68060 ? 0xfffe : 0xc000);
			if (currprefs.mmu_model)
				mmu_set_tc (regs.tcr);
			break;

			/* no differences between 68040 and 68060 */
		case 4: regs.itt0 = *regp & 0xffffe364; mmu_tt_modified (); break;
		case 5: regs.itt1 = *regp & 0xffffe364; mmu_tt_modified (); break;
		case 6: regs.dtt0 = *regp & 0xffffe364; mmu_tt_modified (); break;
		case 7: regs.dtt1 = *regp & 0xffffe364; mmu_tt_modified (); break;
			/* 68060 only */
		case 8: regs.buscr = *regp & 0xf0000000; break;

		case 0x800: regs.usp = *regp; break;
		case 0x801: regs.vbr = *regp; break;
		case 0x802: regs.caar = *regp; break;
		case 0x803: regs.msp = *regp; if (regs.m == 1) m68k_areg (regs, 7) = regs.msp; break;
		case 0x804: regs.isp = *regp; if (regs.m == 0) m68k_areg (regs, 7) = regs.isp; break;
			/* 68040 only */
		case 0x805: regs.mmusr = *regp; break;
			/* 68040/060 */
		case 0x806: regs.urp = *regp & 0xfffffe00; break;
		case 0x807: regs.srp = *regp & 0xfffffe00; break;
			/* 68060 only */
		case 0x808:
			{
				uae_u32 opcr = regs.pcr;
				regs.pcr &= ~(0x40 | 2 | 1);
				regs.pcr |= (*regp) & (0x40 | 2 | 1);
				if (currprefs.fpu_model <= 0)
					regs.pcr |= 2;
				if (((opcr ^ regs.pcr) & 2) == 2) {
					//write_log (_T("68060 FPU state: %s\n"), regs.pcr & 2 ? _T("disabled") : _T("enabled"));
					/* flush possible already translated FPU instructions */
					flush_icache (0, 3);
				}
			}
			break;
		default:
			op_illg (0x4E7B);
			return 0;
		}
	}
	return 1;
}

int m68k_movec2 (int regno, uae_u32 *regp)
{
#if MOVEC_DEBUG > 0
	write_log (_T("movec2 %04X PC=%x\n"), regno, M68K_GETPC);
#endif
	if (movec_illg (regno)) {
		op_illg (0x4E7A);
		return 0;
	} else {
		switch (regno) {
		case 0: *regp = regs.sfc; break;
		case 1: *regp = regs.dfc; break;
		case 2:
			{
				uae_u32 v = regs.cacr;
				uae_u32 cacr_mask = 0;
				if (currprefs.cpu_model == 68020)
					cacr_mask = 0x00000003;
				else if (currprefs.cpu_model == 68030)
					cacr_mask = 0x00003313;
				else if (currprefs.cpu_model == 68040)
					cacr_mask = 0x80008000;
				else if (currprefs.cpu_model == 68060)
					cacr_mask = 0xf880e000;
				*regp = v & cacr_mask;
			}
			break;
		case 3: *regp = regs.tcr; break;
		case 4: *regp = regs.itt0; break;
		case 5: *regp = regs.itt1; break;
		case 6: *regp = regs.dtt0; break;
		case 7: *regp = regs.dtt1; break;
		case 8: *regp = regs.buscr; break;

		case 0x800: *regp = regs.usp; break;
		case 0x801: *regp = regs.vbr; break;
		case 0x802: *regp = regs.caar; break;
		case 0x803: *regp = regs.m == 1 ? m68k_areg (regs, 7) : regs.msp; break;
		case 0x804: *regp = regs.m == 0 ? m68k_areg (regs, 7) : regs.isp; break;
		case 0x805: *regp = regs.mmusr; break;
		case 0x806: *regp = regs.urp; break;
		case 0x807: *regp = regs.srp; break;
		case 0x808: *regp = regs.pcr; break;

		default:
			op_illg (0x4E7A);
			return 0;
		}
	}
#if MOVEC_DEBUG > 0
	write_log (_T("-> %08X\n"), *regp);
#endif
	return 1;
}

#endif

/*
* extract bitfield data from memory and return it in the MSBs
* bdata caches the unmodified data for put_bitfield()
*/
uae_u32 REGPARAM2 get_bitfield (uae_u32 src, uae_u32 bdata[2], uae_s32 offset, int width)
{
	uae_u32 tmp, res, mask;

	offset &= 7;
	mask = 0xffffffffu << (32 - width);
	switch ((offset + width + 7) >> 3) {
	case 1:
		tmp = get_byte (src);
		res = tmp << (24 + offset);
		bdata[0] = tmp & ~(mask >> (24 + offset));
		break;
	case 2:
		tmp = get_word (src);
		res = tmp << (16 + offset);
		bdata[0] = tmp & ~(mask >> (16 + offset));
		break;
	case 3:
		tmp = get_word (src);
		res = tmp << (16 + offset);
		bdata[0] = tmp & ~(mask >> (16 + offset));
		tmp = get_byte (src + 2);
		res |= tmp << (8 + offset);
		bdata[1] = tmp & ~(mask >> (8 + offset));
		break;
	case 4:
		tmp = get_long (src);
		res = tmp << offset;
		bdata[0] = tmp & ~(mask >> offset);
		break;
	case 5:
		tmp = get_long (src);
		res = tmp << offset;
		bdata[0] = tmp & ~(mask >> offset);
		tmp = get_byte (src + 4);
		res |= tmp >> (8 - offset);
		bdata[1] = tmp & ~(mask << (8 - offset));
		break;
	default:
		/* Panic? */
		//write_log (_T("get_bitfield() can't happen %d\n"), (offset + width + 7) >> 3);
		res = 0;
		break;
	}
	return res;
}
/*
* write bitfield data (in the LSBs) back to memory, upper bits
* must be cleared already.
*/
void REGPARAM2 put_bitfield (uae_u32 dst, uae_u32 bdata[2], uae_u32 val, uae_s32 offset, int width)
{
	offset = (offset & 7) + width;
	switch ((offset + 7) >> 3) {
	case 1:
		put_byte (dst, bdata[0] | (val << (8 - offset)));
		break;
	case 2:
		put_word (dst, bdata[0] | (val << (16 - offset)));
		break;
	case 3:
		put_word (dst, bdata[0] | (val >> (offset - 16)));
		put_byte (dst + 2, bdata[1] | (val << (24 - offset)));
		break;
	case 4:
		put_long (dst, bdata[0] | (val << (32 - offset)));
		break;
	case 5:
		put_long (dst, bdata[0] | (val >> (offset - 32)));
		put_byte (dst + 4, bdata[1] | (val << (40 - offset)));
		break;
	default:
		//write_log (_T("put_bitfield() can't happen %d\n"), (offset + 7) >> 3);
		break;
	}
}

uae_u32 REGPARAM2 x_get_bitfield (uae_u32 src, uae_u32 bdata[2], uae_s32 offset, int width)
{
	uae_u32 tmp1, tmp2, res, mask;

	offset &= 7;
	mask = 0xffffffffu << (32 - width);
	switch ((offset + width + 7) >> 3) {
	case 1:
		tmp1 = x_cp_get_byte (src);
		res = tmp1 << (24 + offset);
		bdata[0] = tmp1 & ~(mask >> (24 + offset));
		break;
	case 2:
		tmp1 = x_cp_get_word (src);
		res = tmp1 << (16 + offset);
		bdata[0] = tmp1 & ~(mask >> (16 + offset));
		break;
	case 3:
		tmp1 = x_cp_get_word (src);
		tmp2 = x_cp_get_byte (src + 2);
		res = tmp1 << (16 + offset);
		bdata[0] = tmp1 & ~(mask >> (16 + offset));
		res |= tmp2 << (8 + offset);
		bdata[1] = tmp2 & ~(mask >> (8 + offset));
		break;
	case 4:
		tmp1 = x_cp_get_long (src);
		res = tmp1 << offset;
		bdata[0] = tmp1 & ~(mask >> offset);
		break;
	case 5:
		tmp1 = x_cp_get_long (src);
		tmp2 = x_cp_get_byte (src + 4);
		res = tmp1 << offset;
		bdata[0] = tmp1 & ~(mask >> offset);
		res |= tmp2 >> (8 - offset);
		bdata[1] = tmp2 & ~(mask << (8 - offset));
		break;
	default:
		/* Panic? */
		//write_log (_T("x_get_bitfield() can't happen %d\n"), (offset + width + 7) >> 3);
		res = 0;
		break;
	}
	return res;
}

void REGPARAM2 x_put_bitfield (uae_u32 dst, uae_u32 bdata[2], uae_u32 val, uae_s32 offset, int width)
{
	offset = (offset & 7) + width;
	switch ((offset + 7) >> 3) {
	case 1:
		x_cp_put_byte (dst, bdata[0] | (val << (8 - offset)));
		break;
	case 2:
		x_cp_put_word (dst, bdata[0] | (val << (16 - offset)));
		break;
	case 3:
		x_cp_put_word (dst, bdata[0] | (val >> (offset - 16)));
		x_cp_put_byte (dst + 2, bdata[1] | (val << (24 - offset)));
		break;
	case 4:
		x_cp_put_long (dst, bdata[0] | (val << (32 - offset)));
		break;
	case 5:
		x_cp_put_long (dst, bdata[0] | (val >> (offset - 32)));
		x_cp_put_byte (dst + 4, bdata[1] | (val << (40 - offset)));
		break;
	default:
		//write_log (_T("x_put_bitfield() can't happen %d\n"), (offset + 7) >> 3);
		break;
	}
}

uae_u32 REGPARAM2 get_disp_ea_020 (uae_u32 base, int idx)
{
	uae_u16 dp = next_iword ();
	int reg = (dp >> 12) & 15;
	uae_s32 regd = regs.regs[reg];
	if ((dp & 0x800) == 0)
		regd = (uae_s32)(uae_s16)regd;
	regd <<= (dp >> 9) & 3;
	if (dp & 0x100) {
		uae_s32 outer = 0;
		if (dp & 0x80) base = 0;
		if (dp & 0x40) regd = 0;

		if ((dp & 0x30) == 0x20)
			base += (uae_s32)(uae_s16) next_iword ();
		if ((dp & 0x30) == 0x30)
			base += next_ilong ();

		if ((dp & 0x3) == 0x2)
			outer = (uae_s32)(uae_s16) next_iword ();
		if ((dp & 0x3) == 0x3)
			outer = next_ilong ();

		if ((dp & 0x4) == 0)
			base += regd;
		if (dp & 0x3)
			base = get_long (base);
		if (dp & 0x4)
			base += regd;

		return base + outer;
	} else {
		return base + (uae_s32)((uae_s8)dp) + regd;
	}
}

uae_u32 REGPARAM2 x_get_disp_ea_020 (uae_u32 base, int idx)
{
	uae_u16 dp = x_next_iword ();
	int reg = (dp >> 12) & 15;
	int cycles = 0;
	uae_u32 v;

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
			base += (uae_s32)(uae_s16) x_next_iword ();
			cycles++;
		}
		if ((dp & 0x30) == 0x30) {
			base += x_next_ilong ();
			cycles++;
		}

		if ((dp & 0x3) == 0x2) {
			outer = (uae_s32)(uae_s16) x_next_iword ();
			cycles++;
		}
		if ((dp & 0x3) == 0x3) {
			outer = x_next_ilong ();
			cycles++;
		}

		if ((dp & 0x4) == 0) {
			base += regd;
			cycles++;
		}
		if (dp & 0x3) {
			base = x_get_long (base);
			cycles++;
		}
		if (dp & 0x4) {
			base += regd;
			cycles++;
		}
		v = base + outer;
	} else {
		v = base + (uae_s32)((uae_s8)dp) + regd;
	}
//	if (cycles && currprefs.cpu_cycle_exact)
//		x_do_cycles (cycles * cpucycleunit);
	return v;
}

uae_u32 REGPARAM2 x_get_disp_ea_ce030 (uae_u32 base, int idx)
{
	uae_u16 dp = next_iword_030ce ();
	int reg = (dp >> 12) & 15;
	uae_u32 v;

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
			base += (uae_s32)(uae_s16) next_iword_030ce ();
		}
		if ((dp & 0x30) == 0x30) {
			base += next_ilong_030ce ();
		}

		if ((dp & 0x3) == 0x2) {
			outer = (uae_s32)(uae_s16) next_iword_030ce ();
		}
		if ((dp & 0x3) == 0x3) {
			outer = next_ilong_030ce ();
		}

		if ((dp & 0x4) == 0) {
			base += regd;
		}
		if (dp & 0x3) {
			base = x_get_long (base);
		}
		if (dp & 0x4) {
			base += regd;
		}
		v = base + outer;
	} else {
		v = base + (uae_s32)((uae_s8)dp) + regd;
	}
	return v;
}

#if 0
uae_u32 REGPARAM2 x_get_disp_ea_ce020 (uae_u32 base, int idx)
{
	uae_u16 dp = next_iword_020ce ();
	int reg = (dp >> 12) & 15;
	uae_u32 v;

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
			base += (uae_s32)(uae_s16) next_iword_020ce ();
		}
		if ((dp & 0x30) == 0x30) {
			base += next_ilong_020ce ();
		}

		if ((dp & 0x3) == 0x2) {
			outer = (uae_s32)(uae_s16) next_iword_020ce ();
		}
		if ((dp & 0x3) == 0x3) {
			outer = next_ilong_020ce ();
		}

		if ((dp & 0x4) == 0) {
			base += regd;
		}
		if (dp & 0x3) {
			base = x_get_long (base);
		}
		if (dp & 0x4) {
			base += regd;
		}
		v = base + outer;
	} else {
		v = base + (uae_s32)((uae_s8)dp) + regd;
	}
	return v;
}

#endif

/*
* Compute exact number of CPU cycles taken
* by DIVU and DIVS on a 68000 processor.
*
* Copyright (c) 2005 by Jorge Cwik, pasti@fxatari.com
*
* This is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This software is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this software; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
*/


/*

The routines below take dividend and divisor as parameters.
They return 0 if division by zero, or exact number of cycles otherwise.

The number of cycles returned assumes a register operand.
Effective address time must be added if memory operand.

For 68000 only (not 68010, 68012, 68020, etc).
Probably valid for 68008 after adding the extra prefetch cycle.


Best and worst cases for register operand:
(Note the difference with the documented range.)


DIVU:

Overflow (always): 10 cycles.
Worst case: 136 cycles.
Best case: 76 cycles.


DIVS:

Absolute overflow: 16-18 cycles.
Signed overflow is not detected prematurely.

Worst case: 156 cycles.
Best case without signed overflow: 122 cycles.
Best case with signed overflow: 120 cycles


*/

int getDivu68kCycles (uae_u32 dividend, uae_u16 divisor)
{
	int mcycles;
	uae_u32 hdivisor;
	int i;

	if (divisor == 0)
		return 0;

	// Overflow
	if ((dividend >> 16) >= divisor)
		return (mcycles = 5) * 2;

	mcycles = 38;
	hdivisor = divisor << 16;

	for (i = 0; i < 15; i++) {
		uae_u32 temp;
		temp = dividend;

		dividend <<= 1;

		// If carry from shift
		if ((uae_s32)temp < 0)
			dividend -= hdivisor;
		else {
			mcycles += 2;
			if (dividend >= hdivisor) {
				dividend -= hdivisor;
				mcycles--;
			}
		}
	}
	return mcycles * 2;
}

int getDivs68kCycles (uae_s32 dividend, uae_s16 divisor)
{
	int mcycles;
	uae_u32 aquot;
	int i;

	if (divisor == 0)
		return 0;

	mcycles = 6;

	if (dividend < 0)
		mcycles++;

	// Check for absolute overflow
	if (((uae_u32)abs (dividend) >> 16) >= (uae_u16)abs (divisor))
		return (mcycles + 2) * 2;

	// Absolute quotient
	aquot = (uae_u32) abs (dividend) / (uae_u16)abs (divisor);

	mcycles += 55;

	if (divisor >= 0) {
		if (dividend >= 0)
			mcycles--;
		else
			mcycles++;
	}

	// Count 15 msbits in absolute of quotient

	for (i = 0; i < 15; i++) {
		if ((uae_s16)aquot >= 0)
			mcycles++;
		aquot <<= 1;
	}

	return mcycles * 2;
}

/* 68000 Z=1. NVC=0
 * 68020 and 68030: Signed: Z=1 NVC=0. Unsigned: V=1, N<dst, Z=!N, C=0.
 * 68040/68060 C=0.
 */
void divbyzero_special (bool issigned, uae_s32 dst)
{
	if (currprefs.cpu_model == 68020 || currprefs.cpu_model == 68030) {
		CLEAR_CZNV ();
		if (issigned == false) {
			if (dst < 0) 
				SET_NFLG (1);
			SET_ZFLG (!GET_NFLG ());
			SET_VFLG (1);
		} else {
			SET_ZFLG (1);
		}
	} else if (currprefs.cpu_model >= 68040) {
		SET_CFLG (0);
	} else {
		// 68000/010
		CLEAR_CZNV ();
	}
}

#ifndef CPUEMU_68000_ONLY

STATIC_INLINE int div_unsigned (uae_u32 src_hi, uae_u32 src_lo, uae_u32 div, uae_u32 *quot, uae_u32 *rem)
{
	uae_u32 q = 0, cbit = 0;
	int i;

	if (div <= src_hi) {
		return 1;
	}
	for (i = 0 ; i < 32 ; i++) {
		cbit = src_hi & 0x80000000ul;
		src_hi <<= 1;
		if (src_lo & 0x80000000ul) src_hi++;
		src_lo <<= 1;
		q = q << 1;
		if (cbit || div <= src_hi) {
			q |= 1;
			src_hi -= div;
		}
	}
	*quot = q;
	*rem = src_hi;
	return 0;
}

bool m68k_divl (uae_u32 opcode, uae_u32 src, uae_u16 extra)
{
	if ((extra & 0x400) && currprefs.int_no_unimplemented && currprefs.cpu_model == 68060) {
		op_unimpl (opcode);
		return false;
	}
	if (src == 0) {
		Exception (5);
		return false;
	}
#if defined (uae_s64)
	if (extra & 0x800) {
		/* signed variant */
		uae_s64 a = (uae_s64)(uae_s32)m68k_dreg (regs, (extra >> 12) & 7);
		uae_s64 quot, rem;

		if (extra & 0x400) {
			a &= 0xffffffffu;
			a |= (uae_s64)m68k_dreg (regs, extra & 7) << 32;
		}

		if (a == 0x8000000000000000 && src == -1) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			rem = a % (uae_s64)(uae_s32)src;
			quot = a / (uae_s64)(uae_s32)src;
			if ((quot & UVAL64 (0xffffffff80000000)) != 0
				&& (quot & UVAL64 (0xffffffff80000000)) != UVAL64 (0xffffffff80000000))
			{
				SET_VFLG (1);
				SET_NFLG (1);
				SET_CFLG (0);
			} else {
				if (((uae_s32)rem < 0) != ((uae_s64)a < 0)) rem = -rem;
				SET_VFLG (0);
				SET_CFLG (0);
				SET_ZFLG (((uae_s32)quot) == 0);
				SET_NFLG (((uae_s32)quot) < 0);
				m68k_dreg (regs, extra & 7) = (uae_u32)rem;
				m68k_dreg (regs, (extra >> 12) & 7) = (uae_u32)quot;
			}
		}
	} else {
		/* unsigned */
		uae_u64 a = (uae_u64)(uae_u32)m68k_dreg (regs, (extra >> 12) & 7);
		uae_u64 quot, rem;

		if (extra & 0x400) {
			a &= 0xffffffffu;
			a |= (uae_u64)m68k_dreg (regs, extra & 7) << 32;
		}
		rem = a % (uae_u64)src;
		quot = a / (uae_u64)src;
		if (quot > 0xffffffffu) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg (regs, extra & 7) = (uae_u32)rem;
			m68k_dreg (regs, (extra >> 12) & 7) = (uae_u32)quot;
		}
	}
#else
	if (extra & 0x800) {
		/* signed variant */
		uae_s32 lo = (uae_s32)m68k_dreg (regs, (extra >> 12) & 7);
		uae_s32 hi = lo < 0 ? -1 : 0;
		uae_s32 save_high;
		uae_u32 quot, rem;
		uae_u32 sign;

		if (extra & 0x400) {
			hi = (uae_s32)m68k_dreg (regs, extra & 7);
		}
		save_high = hi;
		sign = (hi ^ src);
		if (hi < 0) {
			hi = ~hi;
			lo = -lo;
			if (lo == 0) hi++;
		}
		if ((uae_s32)src < 0) src = -src;
		if (div_unsigned (hi, lo, src, &quot, &rem) ||
			(sign & 0x80000000) ? quot > 0x80000000 : quot > 0x7fffffff) {
				SET_VFLG (1);
				SET_NFLG (1);
				SET_CFLG (0);
		} else {
			if (sign & 0x80000000) quot = -quot;
			if (((uae_s32)rem < 0) != (save_high < 0)) rem = -rem;
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg (regs, extra & 7) = rem;
			m68k_dreg (regs, (extra >> 12) & 7) = quot;
		}
	} else {
		/* unsigned */
		uae_u32 lo = (uae_u32)m68k_dreg (regs, (extra >> 12) & 7);
		uae_u32 hi = 0;
		uae_u32 quot, rem;

		if (extra & 0x400) {
			hi = (uae_u32)m68k_dreg (regs, extra & 7);
		}
		if (div_unsigned (hi, lo, src, &quot, &rem)) {
			SET_VFLG (1);
			SET_NFLG (1);
			SET_CFLG (0);
		} else {
			SET_VFLG (0);
			SET_CFLG (0);
			SET_ZFLG (((uae_s32)quot) == 0);
			SET_NFLG (((uae_s32)quot) < 0);
			m68k_dreg (regs, extra & 7) = rem;
			m68k_dreg (regs, (extra >> 12) & 7) = quot;
		}
	}
#endif
	return true;
}

STATIC_INLINE void mul_unsigned (uae_u32 src1, uae_u32 src2, uae_u32 *dst_hi, uae_u32 *dst_lo)
{
	uae_u32 r0 = (src1 & 0xffff) * (src2 & 0xffff);
	uae_u32 r1 = ((src1 >> 16) & 0xffff) * (src2 & 0xffff);
	uae_u32 r2 = (src1 & 0xffff) * ((src2 >> 16) & 0xffff);
	uae_u32 r3 = ((src1 >> 16) & 0xffff) * ((src2 >> 16) & 0xffff);
	uae_u32 lo;

	lo = r0 + ((r1 << 16) & 0xffff0000ul);
	if (lo < r0) r3++;
	r0 = lo;
	lo = r0 + ((r2 << 16) & 0xffff0000ul);
	if (lo < r0) r3++;
	r3 += ((r1 >> 16) & 0xffff) + ((r2 >> 16) & 0xffff);
	*dst_lo = lo;
	*dst_hi = r3;
}

bool m68k_mull (uae_u32 opcode, uae_u32 src, uae_u16 extra)
{
	if ((extra & 0x400) && currprefs.int_no_unimplemented && currprefs.cpu_model == 68060) {
		op_unimpl (opcode);
		return false;
	}
#if defined (uae_s64)
	if (extra & 0x800) {
		/* signed variant */
		uae_s64 a = (uae_s64)(uae_s32)m68k_dreg (regs, (extra >> 12) & 7);

		a *= (uae_s64)(uae_s32)src;
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (a == 0);
		SET_NFLG (a < 0);
		if (extra & 0x400)
			m68k_dreg (regs, extra & 7) = (uae_u32)(a >> 32);
		else if ((a & UVAL64 (0xffffffff80000000)) != 0
			&& (a & UVAL64 (0xffffffff80000000)) != UVAL64 (0xffffffff80000000))
		{
			SET_VFLG (1);
		}
		m68k_dreg (regs, (extra >> 12) & 7) = (uae_u32)a;
	} else {
		/* unsigned */
		uae_u64 a = (uae_u64)(uae_u32)m68k_dreg (regs, (extra >> 12) & 7);

		a *= (uae_u64)src;
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (a == 0);
		SET_NFLG (((uae_s64)a) < 0);
		if (extra & 0x400)
			m68k_dreg (regs, extra & 7) = (uae_u32)(a >> 32);
		else if ((a & UVAL64 (0xffffffff00000000)) != 0) {
			SET_VFLG (1);
		}
		m68k_dreg (regs, (extra >> 12) & 7) = (uae_u32)a;
	}
#else
	if (extra & 0x800) {
		/* signed variant */
		uae_s32 src1, src2;
		uae_u32 dst_lo, dst_hi;
		uae_u32 sign;

		src1 = (uae_s32)src;
		src2 = (uae_s32)m68k_dreg (regs, (extra >> 12) & 7);
		sign = (src1 ^ src2);
		if (src1 < 0) src1 = -src1;
		if (src2 < 0) src2 = -src2;
		mul_unsigned ((uae_u32)src1, (uae_u32)src2, &dst_hi, &dst_lo);
		if (sign & 0x80000000) {
			dst_hi = ~dst_hi;
			dst_lo = -dst_lo;
			if (dst_lo == 0) dst_hi++;
		}
		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (dst_hi == 0 && dst_lo == 0);
		SET_NFLG (((uae_s32)dst_hi) < 0);
		if (extra & 0x400)
			m68k_dreg (regs, extra & 7) = dst_hi;
		else if ((dst_hi != 0 || (dst_lo & 0x80000000) != 0)
			&& ((dst_hi & 0xffffffff) != 0xffffffff
			|| (dst_lo & 0x80000000) != 0x80000000))
		{
			SET_VFLG (1);
		}
		m68k_dreg (regs, (extra >> 12) & 7) = dst_lo;
	} else {
		/* unsigned */
		uae_u32 dst_lo, dst_hi;

		mul_unsigned (src, (uae_u32)m68k_dreg (regs, (extra >> 12) & 7), &dst_hi, &dst_lo);

		SET_VFLG (0);
		SET_CFLG (0);
		SET_ZFLG (dst_hi == 0 && dst_lo == 0);
		SET_NFLG (((uae_s32)dst_hi) < 0);
		if (extra & 0x400)
			m68k_dreg (regs, extra & 7) = dst_hi;
		else if (dst_hi != 0) {
			SET_VFLG (1);
		}
		m68k_dreg (regs, (extra >> 12) & 7) = dst_lo;
	}
#endif
	return true;
}

#endif
