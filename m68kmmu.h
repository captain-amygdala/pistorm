/*
    m68kmmu.h - PMMU implementation for 68851/68030/68040

    By R. Belmont

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.
*/

// MMU status register bit definitions



struct m68ki_cpu_core;
#if 0
#define MMULOG(A) printf A
#else
#define MMULOG(...)
#endif
#if 1
#define logerror printf
#else
#define logerror(...)
#endif

// MMU SR register fields
#define M68K_MMU_SR_BUS_ERROR        0x8000
#define M68K_MMU_SR_SUPERVISOR_ONLY  0x2000
#define M68K_MMU_SR_WRITE_PROTECT    0x0800
#define M68K_MMU_SR_INVALID          0x0400
#define M68K_MMU_SR_MODIFIED         0x0200
#define M68K_MMU_SR_TRANSPARENT      0x0040

// MMU translation table descriptor field definitions
#define M68K_MMU_DF_DT               0x00000003
#define M68K_MMU_DF_DT_INVALID       0x00000000
#define M68K_MMU_DF_DT_PAGE          0x00000001
#define M68K_MMU_DF_DT_TABLE_4BYTE   0x00000002
#define M68K_MMU_DF_DT_TABLE_8BYTE   0x00000003
#define M68K_MMU_DF_WP               0x00000004
#define M68K_MMU_DF_USED             0x00000008
#define M68K_MMU_DF_MODIFIED         0x00000010
#define M68K_MMU_DF_CI               0x00000040
#define M68K_MMU_DF_SUPERVISOR       0x00000100
#define M68K_MMU_DF_ADDR_MASK        0xfffffff0
#define M68K_MMU_DF_IND_ADDR_MASK    0xfffffffc

// MMU ATC Fields
#define M68K_MMU_ATC_BUSERROR        0x08000000
#define M68K_MMU_ATC_CACHE_IN        0x04000000
#define M68K_MMU_ATC_WRITE_PR        0x02000000
#define M68K_MMU_ATC_MODIFIED        0x01000000
#define M68K_MMU_ATC_MASK            0x00ffffff
#define M68K_MMU_ATC_SHIFT           8
#define M68K_MMU_ATC_VALID           0x08000000

// MMU Translation Control register
#define M68K_MMU_TC_SRE              0x02000000
#define M68K_MMU_TC_FCL              0x01000000

// TT register
#define M68K_MMU_TT_ENABLE           0x8000

#define m_side_effects_disabled 0

/* decodes the effective address */
uint32 DECODE_EA_32(m68ki_cpu_core *state, int ea)
{
	int mode = (ea >> 3) & 0x7;
	int reg = (ea & 0x7);

	switch (mode)
	{
		case 2:     // (An)
		{
			return REG_A[reg];
		}
		case 3:     // (An)+
		{
			uint32 ea = EA_AY_PI_32();
			return ea;
		}
		case 5:     // (d16, An)
		{
			uint32 ea = EA_AY_DI_32();
			return ea;
		}
		case 6:     // (An) + (Xn) + d8
		{
			uint32 ea = EA_AY_IX_32();
			return ea;
		}
		case 7:
		{
			switch (reg)
			{
				case 0:     // (xxx).W
				{
					uint32 ea = OPER_I_16(state);
					return ea;
				}
				case 1:     // (xxx).L
				{
					uint32 d1 = OPER_I_16(state);
					uint32 d2 = OPER_I_16(state);
					uint32 ea = (d1 << 16) | d2;
					return ea;
				}
				case 2:     // (d16, PC)
				{
					uint32 ea = EA_PCDI_32();
					return ea;
				}
				default:    fatalerror("m68k: DECODE_EA_32: unhandled mode %d, reg %d at %08X\n", mode, reg, REG_PC);
			}
			break;
		}
		default:    fatalerror("m68k: DECODE_EA_32: unhandled mode %d, reg %d at %08X\n", mode, reg, REG_PC);
	}
	return 0;
}

void pmmu_set_buserror(m68ki_cpu_core *state, uint32 addr_in)
{
	if (!m_side_effects_disabled && ++state->mmu_tmp_buserror_occurred == 1)
	{
		state->mmu_tmp_buserror_address = addr_in;
		state->mmu_tmp_buserror_rw = state->mmu_tmp_rw;
		state->mmu_tmp_buserror_fc = state->mmu_tmp_fc;
		state->mmu_tmp_buserror_sz = state->mmu_tmp_sz;
	}
}


// pmmu_atc_add: adds this address to the ATC
void pmmu_atc_add(m68ki_cpu_core *state, uint32 logical, uint32 physical, int fc, int rw)
{
	// get page size (i.e. # of bits to ignore); is 10 for Apollo
	int ps = (state->mmu_tc >> 20) & 0xf;
	uint32 atc_tag = M68K_MMU_ATC_VALID | ((fc & 7) << 24) | ((logical >> ps) << (ps - 8));
	uint32 atc_data = (physical >> ps) << (ps - 8);

	if (state->mmu_tmp_sr & (M68K_MMU_SR_BUS_ERROR|M68K_MMU_SR_INVALID|M68K_MMU_SR_SUPERVISOR_ONLY))
	{
		atc_data |= M68K_MMU_ATC_BUSERROR;
	}

	if (state->mmu_tmp_sr & M68K_MMU_SR_WRITE_PROTECT)
	{
		atc_data |= M68K_MMU_ATC_WRITE_PR;
	}

	if (!rw && !(state->mmu_tmp_sr & M68K_MMU_SR_WRITE_PROTECT))
	{
		atc_data |= M68K_MMU_ATC_MODIFIED;
	}

	// first see if this is already in the cache
	for (int i = 0; i < MMU_ATC_ENTRIES; i++)
	{
		// if tag bits and function code match, don't add
		if (state->mmu_atc_tag[i] == atc_tag)
		{
			MMULOG(("%s: hit, old %08x new %08x\n", __func__, state->mmu_atc_data[i], atc_data));
			state->mmu_atc_data[i] = atc_data;
			return;
		}
	}

	// find an open entry
	int found = -1;
	for (int i = 0; i < MMU_ATC_ENTRIES; i++)
	{
		if (!(state->mmu_atc_tag[i] & M68K_MMU_ATC_VALID))
		{
			found = i;
			break;
		}
	}

	// did we find an entry?  steal one by round-robin then
	if (found == -1)
	{
		found = state->mmu_atc_rr++;

		if (state->mmu_atc_rr >= MMU_ATC_ENTRIES)
		{
			state->mmu_atc_rr = 0;
		}
	}

	// add the entry
	MMULOG(("ATC[%2d] add: log %08x -> phys %08x (fc=%d) data=%08x\n",
			found, (logical >> ps) << ps, (physical >> ps) << ps, fc, atc_data));
	state->mmu_atc_tag[found] = atc_tag;
	state->mmu_atc_data[found] = atc_data;
}

// pmmu_atc_flush: flush entire ATC
// 7fff0003 001ffd10 80f05750 is what should load
void pmmu_atc_flush(m68ki_cpu_core *state)
{
	MMULOG(("ATC flush: pc=%08x\n", state->ppc));
//	std::fill(std::begin(state->mmu_atc_tag), std::end(state->mmu_atc_tag), 0);
	for(int i=0;i<MMU_ATC_ENTRIES;i++)
		state->mmu_atc_tag[i]=0;
	state->mmu_atc_rr = 0;
}

int fc_from_modes(m68ki_cpu_core *state, uint16 modes);

void pmmu_atc_flush_fc_ea(m68ki_cpu_core *state, uint16 modes)
{
	unsigned int fcmask = (modes >> 5) & 7;
	unsigned int fc = fc_from_modes(state, modes) & fcmask;
	unsigned int ps = (state->mmu_tc >> 20) & 0xf;
	unsigned int mode = (modes >> 10) & 7;
	uint32 ea;

	switch (mode)
	{
	case 1: // PFLUSHA
		MMULOG(("PFLUSHA: mode %d\n", mode));
			pmmu_atc_flush(state);
		break;

	case 4: // flush by fc
		MMULOG(("flush by fc: %d, mask %d\n", fc, fcmask));
		for(int i=0,e;i<MMU_ATC_ENTRIES;i++)
		{
			e=state->mmu_atc_tag[i];
			if ((e & M68K_MMU_ATC_VALID) && ((e >> 24) & fcmask) == fc)
			{
				MMULOG(("flushing entry %08x\n", e));
				state->mmu_atc_tag[i] = 0;
			}
		}
		break;

	case 6: // flush by fc + ea

		ea = DECODE_EA_32(state, state->ir);
		MMULOG(("flush by fc/ea: fc %d, mask %d, ea %08x\n", fc, fcmask, ea));
		for(unsigned int i=0,e;i<MMU_ATC_ENTRIES;i++)
		{
			e=state->mmu_atc_tag[i];
			if ((e & M68K_MMU_ATC_VALID) &&
				(((e >> 24) & fcmask) == fc) &&
//              (((e >> ps) << (ps - 8)) == ((ea >> ps) << (ps - 8))))
				( (e << ps) == (ea >> 8 << ps) ))
			{
				MMULOG(("flushing entry %08x\n", e));
				state->mmu_atc_tag[i] = 0;
			}
		}
		break;

	default:
		logerror("PFLUSH mode %d not supported\n", mode);
		break;
	}
}

//template<bool ptest>
uint16 pmmu_atc_lookup(m68ki_cpu_core *state, uint32 addr_in, int fc, uint16 rw, uint32 *addr_out, int ptest)
{
	MMULOG(("%s: LOOKUP addr_in=%08x, fc=%d, ptest=%d, rw=%d\n", __func__, addr_in, fc, ptest,rw));
	unsigned int ps = (state->mmu_tc >> 20) & 0xf;
	uint32 atc_tag = M68K_MMU_ATC_VALID | ((fc & 7) << 24) | ((addr_in >> ps) << (ps - 8));

	for (int i = 0; i < MMU_ATC_ENTRIES; i++)
	{

		if (state->mmu_atc_tag[i] != atc_tag)
		{
			continue;
		}
		
		uint32 atc_data = state->mmu_atc_data[i];

		if (!ptest && !rw)
		{
			// According to MC86030UM:
			// "If the M bit is clear and a write access to this logical
			// address is attempted, the MC68030 aborts the access and initiates a table
			// search, setting the M bit in the page descriptor, invalidating the old ATC
			// entry, and creating a new entry with the M bit set.
			if (!(atc_data & M68K_MMU_ATC_MODIFIED))
			{
				state->mmu_atc_tag[i] = 0;
				continue;
			}
		}

		state->mmu_tmp_sr = 0;
		if (atc_data & M68K_MMU_ATC_MODIFIED)
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_MODIFIED;
		}

		if (atc_data & M68K_MMU_ATC_WRITE_PR)
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_WRITE_PROTECT;
		}

		if (atc_data & M68K_MMU_ATC_BUSERROR)
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_BUS_ERROR|M68K_MMU_SR_INVALID;
		}
		*addr_out = (atc_data << 8) | (addr_in & ~(((uint32)~0) << ps));
		MMULOG(("%s: addr_in=%08x, addr_out=%08x, MMU SR %04x\n",
				__func__, addr_in, *addr_out, state->mmu_tmp_sr));
		return 1;
	}
	MMULOG(("%s: lookup failed\n", __func__));
	if (ptest)
	{
		state->mmu_tmp_sr = M68K_MMU_SR_INVALID;
	}
	return 0;
}

uint16 pmmu_match_tt(m68ki_cpu_core *state, uint32 addr_in, int fc, uint32 tt, uint16 rw)
{
	if (!(tt & M68K_MMU_TT_ENABLE))
	{
		return 0;
	}

	// transparent translation enabled
	uint32 address_base = tt & 0xff000000;
	uint32 address_mask = ((tt << 8) & 0xff000000) ^ 0xff000000;
	uint32 fcmask = (~tt) & 7;
	uint32 fcbits = (tt >> 4) & 7;
	uint16 rwmask = !!(~tt & 0x100);
	uint16 rwbit = !!(tt & 0x200);

	if ((addr_in & address_mask) != (address_base & address_mask))
	{
		return 0;
	}

	if ((fc & fcmask) != (fcbits & fcmask))
	{
		return 0;
	}

	if ((rw & rwmask) != (rwbit & rwmask))
	{
		return 0;
	}

	state->mmu_tmp_sr |= M68K_MMU_SR_TRANSPARENT;
	return 1;
}

void update_descriptor(m68ki_cpu_core *state, uint32 tptr, int type, uint32 entry, int16 rw)
{
	// FIXME: Silence unused variable warning
	if (state) {}

	if (type == M68K_MMU_DF_DT_PAGE && !rw &&
			!(entry & M68K_MMU_DF_MODIFIED) &&
			!(entry & M68K_MMU_DF_WP))
	{
		MMULOG(("%s: set M+U at %08x\n", __func__, tptr));
		m68k_write_memory_32(tptr, entry | M68K_MMU_DF_USED | M68K_MMU_DF_MODIFIED);
	}
	else if (type != M68K_MMU_DF_DT_INVALID && !(entry & M68K_MMU_DF_USED))
	{
		MMULOG(("%s: set U at %08x\n", __func__, tptr));
		m68k_write_memory_32(tptr, entry | M68K_MMU_DF_USED);
	}
}


//template<bool _long>
void update_sr(m68ki_cpu_core *state, int type, uint32 tbl_entry, int fc, uint16 _long)
{
	if (m_side_effects_disabled)
	{
		return;
	}

	switch(type)
	{
	case M68K_MMU_DF_DT_INVALID:
		// Invalid has no flags
		break;

	case M68K_MMU_DF_DT_PAGE:
		if (tbl_entry & M68K_MMU_DF_MODIFIED)
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_MODIFIED;
		}
		/* FALLTHROUGH */

	case M68K_MMU_DF_DT_TABLE_4BYTE:
		/* FALLTHROUGH */

	case M68K_MMU_DF_DT_TABLE_8BYTE:

		if (tbl_entry & M68K_MMU_DF_WP)
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_WRITE_PROTECT;
		}

		if (_long && !(fc & 4) && (tbl_entry & M68K_MMU_DF_SUPERVISOR))
		{
			state->mmu_tmp_sr |= M68K_MMU_SR_SUPERVISOR_ONLY;
		}
		break;
	default:
		break;
	}
}

//template<bool ptest>
uint16 pmmu_walk_tables(m68ki_cpu_core *state, uint32 addr_in, int type, uint32 table, uint8 fc, int limit, uint16 rw,
						uint32 *addr_out, int ptest)
{
	int level = 0;
	uint32 bits = state->mmu_tc & 0xffff;
	int pagesize = (state->mmu_tc >> 20) & 0xf;
	int is = (state->mmu_tc >> 16) & 0xf;
	int bitpos = 12;
	int resolved = 0;
	int pageshift = is;

	addr_in <<= is;

	state->mmu_tablewalk = 1;

	if (state->mmu_tc & M68K_MMU_TC_FCL)
	{
		bitpos = 16;
	}

	do
	{
		int indexbits = (bits >> bitpos) & 0xf;
		int table_index  = (bitpos == 16) ? fc : (addr_in >> (32 - indexbits));
		bitpos -= 4;
		uint16 indirect = (!bitpos || !(bits >> bitpos)) && indexbits;
		uint32 tbl_entry, tbl_entry2;

		MMULOG(("%s: type %d, table %08x, addr_in %08x, indexbits %d, pageshift %d, indirect %d table_index %08x, rw=%d fc=%d\n",
				__func__, type, table, addr_in, indexbits, pageshift, indirect, table_index, rw, fc));

		switch(type)
		{
			case M68K_MMU_DF_DT_INVALID:   // invalid, will cause MMU exception
				state->mmu_tmp_sr = M68K_MMU_SR_INVALID;
				MMULOG(("PMMU: DT0 PC=%x (addr_in %08x -> %08x)\n", state->ppc, addr_in, *addr_out));
				resolved = 1;
				break;

			case M68K_MMU_DF_DT_PAGE:   // page descriptor, will cause direct mapping
				if (!ptest)
				{
					table &= ((uint32)~0) << pagesize;
					*addr_out = table + (addr_in >> pageshift);
				}
				resolved = 1;
				break;

			case M68K_MMU_DF_DT_TABLE_4BYTE:   // valid 4 byte descriptors
				level++;
				*addr_out = table + (table_index << 2);
				tbl_entry = m68k_read_memory_32(*addr_out);
				type = tbl_entry & M68K_MMU_DF_DT;

				if (indirect && (type == 2 || type == 3))
				{
					level++;
					MMULOG(("SHORT INDIRECT DESC: %08x\n", tbl_entry));
					*addr_out = tbl_entry & M68K_MMU_DF_IND_ADDR_MASK;
					tbl_entry = m68k_read_memory_32(*addr_out);
					type = tbl_entry & M68K_MMU_DF_DT;
				}

				MMULOG(("SHORT DESC: %08x\n", tbl_entry));
				table = tbl_entry & M68K_MMU_DF_ADDR_MASK;
				if (!m_side_effects_disabled)
				{
					update_sr(state, type, tbl_entry, fc, 0);
					if (!ptest)
					{
						update_descriptor(state, *addr_out, type, tbl_entry, rw);
					}
				}
				break;

			case M68K_MMU_DF_DT_TABLE_8BYTE:   // valid 8 byte descriptors
				level++;
				*addr_out = table + (table_index << 3);
				tbl_entry = m68k_read_memory_32(*addr_out);
				tbl_entry2 = m68k_read_memory_32((*addr_out) + 4);
				type = tbl_entry & M68K_MMU_DF_DT;

				if (indirect && (type == 2 || type == 3))
				{
					level++;
					MMULOG(("LONG INDIRECT DESC: %08x%08x\n", tbl_entry, tbl_entry2));
					*addr_out = tbl_entry2 & M68K_MMU_DF_IND_ADDR_MASK;
					tbl_entry = m68k_read_memory_32(*addr_out);
					tbl_entry2 = m68k_read_memory_32(*addr_out);
					type = tbl_entry & M68K_MMU_DF_DT;
				}

				MMULOG(("LONG DESC: %08x %08x\n", tbl_entry, tbl_entry2));
				table = tbl_entry2 & M68K_MMU_DF_ADDR_MASK;
				if (!m_side_effects_disabled)
				{
					update_sr(state, type, tbl_entry, fc, 1);
					if (!ptest)
					{
						update_descriptor(state, *addr_out, type, tbl_entry, rw);
					}
				}
				break;
		}

		if (state->mmu_tmp_sr & M68K_MMU_SR_BUS_ERROR)
		{
			// Bus error during page table walking is always fatal
			resolved = 1;
			break;
		}

		if (!ptest && !m_side_effects_disabled)
		{
			if (!rw && (state->mmu_tmp_sr & M68K_MMU_SR_WRITE_PROTECT))
			{
				resolved = 1;
				break;
			}

			if (!(fc & 4) && (state->mmu_tmp_sr & M68K_MMU_SR_SUPERVISOR_ONLY))
			{
				resolved = 1;
				break;
			}

		}
		addr_in <<= indexbits;
		pageshift += indexbits;
	} while(level < limit && !resolved);


	state->mmu_tmp_sr &= 0xfff0;
	state->mmu_tmp_sr |= level;
	MMULOG(("MMU SR after walk: %04X\n", state->mmu_tmp_sr));
	state->mmu_tablewalk = 0;
	return resolved;
}

// pmmu_translate_addr_with_fc: perform 68851/68030-style PMMU address translation
//template<bool ptest, bool pload>
uint32 pmmu_translate_addr_with_fc(m68ki_cpu_core *state, uint32 addr_in, uint8 fc, uint16 rw, int limit, int ptest,
								   int pload)
{
	uint32 addr_out = 0;


	MMULOG(("%s: addr_in=%08x, fc=%d, ptest=%d, rw=%d, limit=%d, pload=%d\n",
			__func__, addr_in, fc, ptest, rw, limit, pload));
	state->mmu_tmp_sr = 0;

	state->mmu_last_logical_addr = addr_in;

	if (pmmu_match_tt(state, addr_in, fc, state->mmu_tt0, rw) ||
		pmmu_match_tt(state, addr_in, fc, state->mmu_tt1, rw) ||
		fc == 7)
	{
		return addr_in;
	}

	if (ptest && limit == 0)
	{
		pmmu_atc_lookup(state, addr_in, fc, rw, &addr_out, 1);
		return addr_out;
	}

	if (!ptest && !pload && pmmu_atc_lookup(state, addr_in, fc, rw, &addr_out, 0))
	{
		if ((state->mmu_tmp_sr & M68K_MMU_SR_BUS_ERROR) || (!rw && (state->mmu_tmp_sr & M68K_MMU_SR_WRITE_PROTECT)))
		{
			MMULOG(("set atc hit buserror: addr_in=%08x, addr_out=%x, rw=%x, fc=%d, sz=%d\n",
					addr_in, addr_out, state->mmu_tmp_rw, state->mmu_tmp_fc, state->mmu_tmp_sz));
			pmmu_set_buserror(state, addr_in);
		}
		return addr_out;
	}

	int type;
	uint32 tbl_addr;
	// if SRP is enabled and we're in supervisor mode, use it
	if ((state->mmu_tc & M68K_MMU_TC_SRE) && (fc & 4))
	{
		tbl_addr = state->mmu_srp_aptr & M68K_MMU_DF_ADDR_MASK;
		type = state->mmu_srp_limit & M68K_MMU_DF_DT;
	}
	else    // else use the CRP
	{
		tbl_addr = state->mmu_crp_aptr & M68K_MMU_DF_ADDR_MASK;
		type = state->mmu_crp_limit & M68K_MMU_DF_DT;
	}

	if (!pmmu_walk_tables(state, addr_in, type, tbl_addr, fc, limit, rw, &addr_out, ptest))
	{
		MMULOG(("%s: addr_in=%08x, type=%x, tbl_addr=%x, fc=%d, limit=%x, rw=%x, addr_out=%x, ptest=%d\n",
				__func__, addr_in, type, tbl_addr, fc, limit, rw, addr_out, ptest));
		fatalerror("Table walk did not resolve\n");
	}

	if (ptest)
	{
		return addr_out;
	}

	if ((state->mmu_tmp_sr & (M68K_MMU_SR_INVALID|M68K_MMU_SR_SUPERVISOR_ONLY)) ||
			((state->mmu_tmp_sr & M68K_MMU_SR_WRITE_PROTECT) && !rw))
	{

		if (!pload)
		{
			MMULOG(("%s: set buserror (SR %04X)\n", __func__, state->mmu_tmp_sr));
			pmmu_set_buserror(state, addr_in);
		}
	}

	// it seems like at least the 68030 sets the M bit in the MMU SR
	// if the root descriptor is of PAGE type, so do a logical and
	// between RW and the root type
	if (!m_side_effects_disabled)
	{
		pmmu_atc_add(state, addr_in, addr_out, fc, rw && type != 1);
	}
	MMULOG(("PMMU: [%08x] => [%08x] (SR %04x)\n", addr_in, addr_out, state->mmu_tmp_sr));
	return addr_out;
}

// FC bits: 2 = supervisor, 1 = program, 0 = data
// the 68040 is a subset of the 68851 and 68030 PMMUs - the page table sizes are fixed, there is no early termination, etc, etc.
uint32 pmmu_translate_addr_with_fc_040(m68ki_cpu_core *state, uint32 addr_in, uint8 fc, uint8 ptest)
{
	uint32 addr_out, tt0, tt1;

	addr_out = addr_in;
	state->mmu_tmp_sr = 0;

	// transparent translation registers are always in force even if the PMMU itself is disabled
	// they don't do much in emulation because we never write out of order, but the write-protect and cache control features
	// are emulatable, and apparently transparent translation regions skip the page table lookup.
	if (fc & 1) // data, use DTT0/DTT1
	{
		tt0 = state->mmu_dtt0;
		tt1 = state->mmu_dtt1;
	}
	else if (fc & 2)    // program, use ITT0/ITT1
	{
		tt0 = state->mmu_itt0;
		tt1 = state->mmu_itt1;
	}
	else
	{
		fatalerror("68040: function code %d is neither data nor program!\n", fc & 7);
	}

	if (tt0 & M68K_MMU_TT_ENABLE)
	{
		int fcmask[4] = { 4, 4, 0, 0 };
		int fcmatch[4] = { 0, 4, 0, 0 };
		uint32 mask = (tt0 >> 16) & 0xff;
		mask ^= 0xff;
		mask <<= 24;

		if ((addr_in & mask) == (tt0 & mask) && (fc & fcmask[(tt0 >> 13) & 3]) == fcmatch[(tt0 >> 13) & 3])
		{
			MMULOG(("TT0 match on address %08x (TT0 = %08x, mask = %08x)\n", addr_in, tt0, mask));
			if ((tt0 & 4) && !state->mmu_tmp_rw && !ptest)   // write protect?
			{
				pmmu_set_buserror(state, addr_in);
			}

			return addr_in;
		}
	}

	if (tt1 & M68K_MMU_TT_ENABLE)
	{
		static int fcmask[4] = { 4, 4, 0, 0 };
		static int fcmatch[4] = { 0, 4, 0, 0 };
		uint32 mask = (tt1 >> 16) & 0xff;
		mask ^= 0xff;
		mask <<= 24;

		if ((addr_in & mask) == (tt1 & mask) && (fc & fcmask[(tt1 >> 13) & 3]) == fcmatch[(tt1 >> 13) & 3])
		{
			MMULOG(("TT1 match on address %08x (TT0 = %08x, mask = %08x)\n", addr_in, tt1, mask));
			if ((tt1 & 4) && !state->mmu_tmp_rw && !ptest)   // write protect?
			{
				pmmu_set_buserror(state, addr_in);
			}

			return addr_in;
		}
	}

	if (state->pmmu_enabled)
	{
		uint32 root_idx = (addr_in >> 25) & 0x7f;
		uint32 ptr_idx = (addr_in >> 18) & 0x7f;
		uint32 page_idx, page;
		uint32 root_ptr, pointer_ptr, page_ptr;
		uint32 root_entry, pointer_entry, page_entry;

		// select supervisor or user root pointer
		if (fc & 4)
		{
			root_ptr = state->mmu_srp_aptr + (root_idx<<2);
		}
		else
		{
			root_ptr = state->mmu_urp_aptr + (root_idx<<2);
		}

		// get the root entry
		root_entry = m68k_read_memory_32(root_ptr);

		// is UDT marked valid?
		if (root_entry & 2)
		{
			// we're accessing through this root entry, so set the U bit
			if ((!(root_entry & 0x8)) && (!ptest) && !m_side_effects_disabled)
			{
				root_entry |= 0x8;
				m68k_write_memory_32(root_ptr, root_entry);
			}

			// PTEST: any write protect bits set in the search tree will set W in SR
			if ((ptest) && (root_entry & 4))
			{
				state->mmu_tmp_sr |= 4;
			}

			pointer_ptr = (root_entry & ~0x1ff) + (ptr_idx<<2);
			pointer_entry = m68k_read_memory_32(pointer_ptr);

			// PTEST: any write protect bits set in the search tree will set W in SR
			if ((ptest) && (pointer_entry & 4))
			{
				state->mmu_tmp_sr |= 4;
			}

			// update U bit on this pointer entry too
			if ((!(pointer_entry & 0x8)) && (!ptest) && !m_side_effects_disabled)
			{
				pointer_entry |= 0x8;
				m68k_write_memory_32(pointer_ptr, pointer_entry);
			}

			MMULOG(("pointer entry = %08x\n", pointer_entry));

			// write protected by the root or pointer entries?
			if ((((root_entry & 4) && !state->mmu_tmp_rw) || ((pointer_entry & 4) && !state->mmu_tmp_rw)) && !ptest)
			{
				pmmu_set_buserror(state, addr_in);
				return addr_in;
			}

			// is UDT valid on the pointer entry?
			if (!(pointer_entry & 2) && !ptest)
			{
				logerror("Invalid pointer entry!  PC=%x, addr=%x\n", state->ppc, addr_in);
				pmmu_set_buserror(state, addr_in);
				return addr_in;
			}

			// (fall out of these ifs into the page lookup below)
		}
		else // throw an error
		{
			logerror("Invalid root entry!  PC=%x, addr=%x\n", state->ppc, addr_in);

			if (!ptest)
			{
				pmmu_set_buserror(state, addr_in);
			}

			return addr_in;
		}

		// now do the page lookup
		if (state->mmu_tc & 0x4000)  // 8k pages?
		{
			page_idx = (addr_in >> 13) & 0x1f;
			page = addr_in & 0x1fff;
			pointer_entry &= ~0x7f;
			MMULOG(("8k pages: index %x page %x\n", page_idx, page));
		}
		else    // 4k pages
		{
			page_idx = (addr_in >> 12) & 0x3f;
			page = addr_in & 0xfff;
			pointer_entry &= ~0xff;
			MMULOG(("4k pages: index %x page %x\n", page_idx, page));
		}

		page_ptr = pointer_entry + (page_idx<<2);
		page_entry = m68k_read_memory_32(page_ptr);
		state->mmu_last_page_entry_addr = page_ptr;

		MMULOG(("page_entry = %08x\n", page_entry));

		// resolve indirect page pointers
		while ((page_entry & 3) == 2)
		{
			page_entry = m68k_read_memory_32(page_entry & ~0x3);
			state->mmu_last_page_entry_addr = (page_entry & ~0x3);
		}
		state->mmu_last_page_entry = page_entry;

		// is the page write protected or supervisor protected?
		if ((((page_entry & 4) && !state->mmu_tmp_rw) || ((page_entry & 0x80) && !(fc & 4))) && !ptest)
		{
			pmmu_set_buserror(state, addr_in);
			return addr_in;
		}

		switch (page_entry & 3)
		{
			case 0: // invalid
				MMULOG(("Invalid page entry!  PC=%x, addr=%x\n", state->ppc, addr_in));
				if (!ptest)
				{
					pmmu_set_buserror(state, addr_in);
				}

				return addr_in;

			case 1:
			case 3: // normal
				if (state->mmu_tc & 0x4000)  // 8k pages?
				{
					addr_out = (page_entry & ~0x1fff) | page;
				}
				else
				{
					addr_out = (page_entry & ~0xfff) | page;
				}

				if (!(ptest))
				{
					page_entry |= 0x8;  // always set the U bit

					// if we're writing, the M bit comes into play
					if (!state->mmu_tmp_rw)
					{
						page_entry |= 0x10; // set Modified
					}

					// if these updates resulted in a change, write the entry back where we found it
					if (page_entry != state->mmu_last_page_entry && !m_side_effects_disabled)
					{
						state->mmu_last_page_entry = page_entry;
						m68k_write_memory_32(state->mmu_last_page_entry_addr, state->mmu_last_page_entry);
					}
				}
				else
				{
					// page entry: UR G U1 U0 S CM CM M U W PDT
					// SR:         B  G U1 U0 S CM CM M 0 W T R
					state->mmu_tmp_sr |= ((addr_out & ~0xfff) || (page_entry & 0x7f4));
				}
				break;

			case 2: // shouldn't happen
				fatalerror("68040: got indirect final page pointer, shouldn't be possible\n");
				break;
		}
		//      if (addr_in != addr_out) MMULOG(("040MMU: [%08x] => [%08x]\n", addr_in, addr_out));
	}

	return addr_out;
}

// pmmu_translate_addr: perform 68851/68030-style PMMU address translation
uint32 pmmu_translate_addr(m68ki_cpu_core *state, uint32 addr_in, uint16 rw)
{
	uint32 addr_out;

	if (CPU_TYPE_IS_040_PLUS(state->cpu_type))
	{
		addr_out = pmmu_translate_addr_with_fc_040(state, addr_in, state->mmu_tmp_fc, 0);
	}
	else
	{
		addr_out = pmmu_translate_addr_with_fc(state, addr_in, state->mmu_tmp_fc, rw, 7, 0, 0);
		MMULOG(("ADDRIN %08X, ADDROUT %08X\n", addr_in, addr_out));
	}
	return addr_out;
}

int fc_from_modes(m68ki_cpu_core *state, uint16 modes)
{
	if ((modes & 0x1f) == 0)
	{
		return state->sfc;
	}

	if ((modes & 0x1f) == 1)
	{
		return state->dfc;
	}

	if (state->cpu_type & CPU_TYPE_030)
	{
		// 68030 has 3 bits fc, but 68851 4 bits
		if (((modes >> 3) & 3) == 1)
		{
			return REG_D[modes & 7] & 0x7;
		}

		if (((modes >> 3) & 3) == 2)
		{
			return modes & 7;
		}
	}
	else
	{
		if (((modes >> 3) & 3) == 1)
		{
			return REG_D[modes & 7] & 0xf;
		}

		if (modes & 0x10)
		{
			return modes & 0xf;
		}
	}


	fatalerror("%s: unknown fc mode: 0x%02xn", __func__, modes & 0x1f);
	return 0;
}

void m68851_pload(m68ki_cpu_core *state, uint32 ea, uint16 modes)
{
	uint32 ltmp = DECODE_EA_32(state, ea);
	int fc = fc_from_modes(state, modes);
	uint16 rw = !!(modes & 0x200);

	MMULOG(("%s: PLOAD%c addr=%08x, fc=%d\n", __func__, rw ? 'R' : 'W', ltmp, fc));

	// MC68851 traps if MMU is not enabled, 030 not
	if (state->pmmu_enabled || (state->cpu_type & CPU_TYPE_030))
	{
		if (CPU_TYPE_IS_040_PLUS(state->cpu_type))
		{
			pmmu_translate_addr_with_fc_040(state, ltmp, fc, 0);
		}
		else
		{
			pmmu_translate_addr_with_fc(state, ltmp, fc, rw, 7, 0, 1);
		}
	}
	else
	{
		MMULOG(("PLOAD with MMU disabled on MC68851\n"));
		m68ki_exception_trap(state, 57);
		return;
	}
}

void m68851_ptest(m68ki_cpu_core *state, uint32 ea, uint16 modes)
{
	uint32 v_addr = DECODE_EA_32(state, ea);
	uint32 p_addr;

	int level = (modes >> 10) & 7;
	uint16 rw = !!(modes & 0x200);
	int fc = fc_from_modes(state, modes);

	MMULOG(("PMMU: PTEST%c (%04X) pc=%08x sp=%08x va=%08x fc=%x level=%x a=%d, areg=%d\n",
			rw ? 'R' : 'W', modes, state->ppc, REG_A[7], v_addr, fc, level,
					(modes & 0x100) ? 1 : 0, (modes >> 5) & 7));

	if (CPU_TYPE_IS_040_PLUS(state->cpu_type))
	{
		p_addr = pmmu_translate_addr_with_fc_040(state, v_addr, fc, 1);
	}
	else
	{
		p_addr = pmmu_translate_addr_with_fc(state, v_addr, fc, rw, level, 1, 0);
	}

	state->mmu_sr = state->mmu_tmp_sr;

	MMULOG(("PMMU: PTEST result: %04x pa=%08x\n", state->mmu_sr, p_addr));
	if (modes & 0x100)
	{
		int areg = (modes >> 5) & 7;
		WRITE_EA_32(state, 0x08 | areg, p_addr);
	}
}

void m68851_pmove_get(m68ki_cpu_core *state, uint32 ea, uint16 modes)
{
	switch ((modes>>10) & 0x3f)
	{
	case 0x02: // transparent translation register 0
		WRITE_EA_32(state, ea, state->mmu_tt0);
		MMULOG(("PMMU: pc=%x PMOVE from mmu_tt0=%08x\n", state->ppc, state->mmu_tt0));
		break;
	case 0x03: // transparent translation register 1
		WRITE_EA_32(state, ea, state->mmu_tt1);
		MMULOG(("PMMU: pc=%x PMOVE from mmu_tt1=%08x\n", state->ppc, state->mmu_tt1));
		break;
	case 0x10:  // translation control register
		WRITE_EA_32(state, ea, state->mmu_tc);
		MMULOG(("PMMU: pc=%x PMOVE from mmu_tc=%08x\n", state->ppc, state->mmu_tc));
		break;

	case 0x12: // supervisor root pointer
		WRITE_EA_64(state, ea, (uint64)state->mmu_srp_limit<<32 | (uint64)state->mmu_srp_aptr);
		MMULOG(("PMMU: pc=%x PMOVE from SRP limit = %08x, aptr = %08x\n", state->ppc, state->mmu_srp_limit, state->mmu_srp_aptr));
		break;

	case 0x13: // CPU root pointer
		WRITE_EA_64(state, ea, (uint64)state->mmu_crp_limit<<32 | (uint64)state->mmu_crp_aptr);
		MMULOG(("PMMU: pc=%x PMOVE from CRP limit = %08x, aptr = %08x\n", state->ppc, state->mmu_crp_limit, state->mmu_crp_aptr));
		break;

	default:
		logerror("680x0: PMOVE from unknown MMU register %x, PC %x\n", (modes>>10) & 7, state->pc);
		return;
	}

	if (!(modes & 0x100))   // flush ATC on moves to TC, SRP, CRP, TT with FD bit clear
	{
		pmmu_atc_flush(state);
	}

}

void m68851_pmove_put(m68ki_cpu_core *state, uint32 ea, uint16 modes)
{
	uint64 temp64;
	switch ((modes>>13) & 7)
	{
	case 0:
	{
		uint32 temp = READ_EA_32(state, ea);

		if (((modes >> 10) & 7) == 2)
		{
			MMULOG(("WRITE TT0 = 0x%08x\n", state->mmu_tt0));
			state->mmu_tt0 = temp;
		}
		else if (((modes >> 10) & 7) == 3)
		{
			MMULOG(("WRITE TT1 = 0x%08x\n", state->mmu_tt1));
			state->mmu_tt1 = temp;
		}
		break;

		// FIXME: unreachable
		if (!(modes & 0x100))
		{
			pmmu_atc_flush(state);
		}
	}
	/* fall through */
	/* no break */

	case 1:
		logerror("680x0: unknown PMOVE case 1, PC %x\n", state->pc);
		break;

	case 2:
		switch ((modes >> 10) & 7)
		{
		case 0: // translation control register
			state->mmu_tc = READ_EA_32(state, ea);
			MMULOG(("PMMU: TC = %08x\n", state->mmu_tc));

			if (state->mmu_tc & 0x80000000)
			{
				int bits = 0;
				for (int shift = 20; shift >= 0; shift -= 4)
				{
					bits += (state->mmu_tc >> shift) & 0x0f;
				}

				if (bits != 32 || !((state->mmu_tc >> 23) & 1))
				{
					logerror("MMU: TC invalid!\n");
					state->mmu_tc &= ~0x80000000;
					m68ki_exception_trap(state, EXCEPTION_MMU_CONFIGURATION);
				} else {
					state->pmmu_enabled = 1;
				}
				MMULOG(("PMMU enabled\n"));
			}
			else
			{
				state->pmmu_enabled = 0;
				MMULOG(("PMMU disabled\n"));
			}

			if (!(modes & 0x100))   // flush ATC on moves to TC, SRP, CRP with FD bit clear
			{
				pmmu_atc_flush(state);
			}
			break;

		case 2: // supervisor root pointer
			temp64 = READ_EA_64(state, ea);
			state->mmu_srp_limit = (temp64 >> 32) & 0xffffffff;
			state->mmu_srp_aptr = temp64 & 0xffffffff;
			MMULOG(("PMMU: SRP limit = %08x aptr = %08x\n", state->mmu_srp_limit, state->mmu_srp_aptr));
			// SRP type 0 is not allowed
			if ((state->mmu_srp_limit & 3) == 0)
			{
				m68ki_exception_trap(state, EXCEPTION_MMU_CONFIGURATION);
				return;
			}

			if (!(modes & 0x100))
			{
				pmmu_atc_flush(state);
			}
			break;

		case 3: // CPU root pointer
			temp64 = READ_EA_64(state, ea);
			state->mmu_crp_limit = (temp64 >> 32) & 0xffffffff;
			state->mmu_crp_aptr = temp64 & 0xffffffff;
			MMULOG(("PMMU: CRP limit = %08x aptr = %08x\n", state->mmu_crp_limit, state->mmu_crp_aptr));
			// CRP type 0 is not allowed
			if ((state->mmu_crp_limit & 3) == 0)
			{
				m68ki_exception_trap(state, EXCEPTION_MMU_CONFIGURATION);
				return;
			}

			if (!(modes & 0x100))
			{
				pmmu_atc_flush(state);
			}
			break;

		case 7: // MC68851 Access Control Register
			if (state->cpu_type == CPU_TYPE_020)
			{
				// DomainOS on Apollo DN3000 will only reset this to 0
				uint16 mmu_ac = READ_EA_16(state, ea);
				if (mmu_ac != 0)
				{
					MMULOG(("680x0 PMMU: pc=%x PMOVE to mmu_ac=%08x\n",
							state->ppc, mmu_ac));
				}
				break;
			}
			// fall through; unknown PMOVE mode unless MC68020 with MC68851
			/* fall through */
			/* no break */
		default:
			logerror("680x0: PMOVE to unknown MMU register %x, PC %x\n", (modes>>10) & 7, state->pc);
			break;
		}
		break;
	case 3: // MMU status
	{
		uint32 temp = READ_EA_32(state, ea);
		logerror("680x0: unsupported PMOVE %x to MMU status, PC %x\n", temp, state->pc);
	}
	break;
	}
}


void m68851_pmove(m68ki_cpu_core *state, uint32 ea, uint16 modes)
{
	switch ((modes>>13) & 0x7)
	{
	case 0:	// MC68030/040 form with FD bit
	case 2: // MC68851 form, FD never set
		if (modes & 0x200)
		{
			m68851_pmove_get(state, ea, modes);
			break;
		}
		else    // top 3 bits of modes: 010 for this, 011 for status, 000 for transparent translation regs
		{
			m68851_pmove_put(state, ea, modes);
			break;
		}
	case 3: // MC68030 to/from status reg
		if (modes & 0x200)
		{
			MMULOG(("%s: read SR = %04x\n", __func__, state->mmu_sr));
			WRITE_EA_16(state, ea, state->mmu_sr);
		}
		else
		{
			state->mmu_sr = READ_EA_16(state, ea);
			MMULOG(("%s: write SR = %04X\n", __func__, state->mmu_sr));
		}
		break;

	default:
		logerror("680x0: unknown PMOVE mode %x (modes %04x) (PC %x)\n", (modes >> 13) & 0x7, modes, state->pc);
		break;

	}

}

void m68851_mmu_ops(m68ki_cpu_core *state)
{
	uint16 modes;
	uint32 ea = state->ir & 0x3f;

	// catch the 2 "weird" encodings up front (PBcc)
	if ((state->ir & 0xffc0) == 0xf0c0)
	{
		logerror("680x0: unhandled PBcc\n");
		return;
	}
	else if ((state->ir & 0xffc0) == 0xf080)
	{
		logerror("680x0: unhandled PBcc\n");
		return;
	}
	else if ((state->ir & 0xffe0) == 0xf500)
	{
		MMULOG(("68040 pflush: pc=%08x ir=%04x opmode=%d register=%d\n", REG_PC-4, state->ir, (state->ir >> 3) & 3, state->ir & 7));
		pmmu_atc_flush(state);
	}
	else	// the rest are 1111000xxxXXXXXX where xxx is the instruction family
	{
		switch ((state->ir>>9) & 0x7)
		{
			case 0:
				modes = OPER_I_16(state);

				if ((modes & 0xfde0) == 0x2000)	// PLOAD
				{
					m68851_pload(state, ea, modes);
					return;
				}
				else if ((modes & 0xe200) == 0x2000)	// PFLUSH
				{
					pmmu_atc_flush_fc_ea(state, modes);
					return;
				}
				else if (modes == 0xa000)	// PFLUSHR
				{
					pmmu_atc_flush(state);
					return;
				}
				else if (modes == 0x2800)	// PVALID (FORMAT 1)
				{
					logerror("680x0: unhandled PVALID1\n");
					return;
				}
				else if ((modes & 0xfff8) == 0x2c00)	// PVALID (FORMAT 2)
				{
					logerror("680x0: unhandled PVALID2\n");
					return;
				}
				else if ((modes & 0xe000) == 0x8000)	// PTEST
				{
					m68851_ptest(state, ea, modes);
					return;
				}
				else
				{
					m68851_pmove(state, ea, modes);
				}
				break;

			default:
				logerror("680x0: unknown PMMU instruction group %d\n", (state->ir>>9) & 0x7);
				break;
		}
	}
}


/* Apple HMMU translation is much simpler */
/*
inline uint32 hmmu_translate_addr(uint32 addr_in)
{
	uint32 addr_out;

	addr_out = addr_in;

	// check if LC 24-bit mode is enabled - this simply blanks out A31, the V8 ignores A30-24 always
	if (state->hmmu_enabled == M68K_HMMU_ENABLE_LC)
	{
		addr_out = addr_in & 0xffffff;
	}
	else if (state->hmmu_enabled == M68K_HMMU_ENABLE_II) // the original II does a more complex translation
	{
		addr_out = addr_in & 0xffffff;

		if ((addr_out >= 0x800000) && (addr_out <= 0x8fffff))
		{
			addr_out |= 0x40000000; // ROM
		}
		else if ((addr_out >= 0x900000) && (addr_out <= 0xefffff))
		{
			addr_out = 0xf0000000;  // NuBus
			addr_out |= ((addr_in & 0xf00000)<<4);
			addr_out |= (addr_in & 0xfffff);
		}
		else if (addr_out >= 0xf00000)
		{
			addr_out |= 0x50000000; // I/O
		}

		// (RAM is at 0 and doesn't need special massaging)
	}

	return addr_out;
}

int m68851_buserror(u32& addr)
{
	if (!state->pmmu_enabled)
	{
		return false;
	}

	if (state->mmu_tablewalk)
	{
		MMULOG(("buserror during table walk\n"));
		state->mmu_tmp_sr |= M68K_MMU_SR_BUS_ERROR|M68K_MMU_SR_INVALID;
		return true;
	}

	addr = state->mmu_last_logical_addr;
	return false;
}
*/
