 /*
  * UAE - The Un*x Amiga Emulator - CPU core
  *
  * Big endian memory access functions.
  *
  * Copyright 1996 Bernd Schmidt
  *
  * Adaptation to Hatari by Thomas Huth, Eero Tamminen
  *
  * This file is distributed under the GNU Public License, version 2 or at
  * your option any later version. Read the file gpl.txt for details.
  */

#ifndef UAE_MACCESS_H
#define UAE_MACCESS_H


/* Can the actual CPU access unaligned memory? */
#ifndef CPU_CAN_ACCESS_UNALIGNED
# if defined(__i386__) || defined(powerpc) || defined(__mc68020__)
#  define CPU_CAN_ACCESS_UNALIGNED 1
# else
#  define CPU_CAN_ACCESS_UNALIGNED 0
# endif
#endif


/* If the CPU can access unaligned memory, use these accelerated functions: */
#if CPU_CAN_ACCESS_UNALIGNED

#include <SDL_endian.h>


static inline uae_u32 do_get_mem_long(void *a)
{
	return SDL_SwapBE32(*(uae_u32 *)a);
}

static inline uae_u16 do_get_mem_word(void *a)
{
	return SDL_SwapBE16(*(uae_u16 *)a);
}


static inline void do_put_mem_long(void *a, uae_u32 v)
{
	*(uae_u32 *)a = SDL_SwapBE32(v);
}

static inline void do_put_mem_word(void *a, uae_u16 v)
{
	*(uae_u16 *)a = SDL_SwapBE16(v);
}


#else  /* Cpu can not access unaligned memory: */


static inline uae_u32 do_get_mem_long(void *a)
{
	uae_u8 *b = (uae_u8 *)a;

	return (b[0] << 24) | (b[1] << 16) | (b[2] << 8) | b[3];
}

static inline uae_u16 do_get_mem_word(void *a)
{
	uae_u8 *b = (uae_u8 *)a;

	return (b[0] << 8) | b[1];
}


static inline void do_put_mem_long(void *a, uae_u32 v)
{
	uae_u8 *b = (uae_u8 *)a;

	b[0] = v >> 24;
	b[1] = v >> 16;    
	b[2] = v >> 8;
	b[3] = v;
}

static inline void do_put_mem_word(void *a, uae_u16 v)
{
	uae_u8 *b = (uae_u8 *)a;

	b[0] = v >> 8;
	b[1] = v;
}


#endif  /* CPU_CAN_ACCESS_UNALIGNED */


/* These are same for all architectures: */

static inline uae_u8 do_get_mem_byte(uae_u8 *a)
{
	return *a;
}

static inline void do_put_mem_byte(uae_u8 *a, uae_u8 v)
{
	*a = v;
}


#endif /* UAE_MACCESS_H */
