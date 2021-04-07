 /*
  * UAE - The Un*x Amiga Emulator
  *
  * custom chip support
  *
  * (c) 1995 Bernd Schmidt
  */

#ifndef WINUAE_CUSTOM_H
#define WINUAE_CUSTOM_H

#include "rpt.h"

/* These are the masks that are ORed together in the chipset_mask option.
 * If CSMASK_AGA is set, the ECS bits are guaranteed to be set as well.  */
#define CSMASK_ECS_AGNUS 1
#define CSMASK_ECS_DENISE 2
#define CSMASK_AGA 4
#define CSMASK_MASK (CSMASK_ECS_AGNUS | CSMASK_ECS_DENISE | CSMASK_AGA)

uae_u32 get_copper_address (int copno);

extern int custom_init (void);
extern void customreset (void);
extern int intlev (void);
extern void dumpcustom (void);
extern void uae_reset (int hardreset);

extern void do_disk (void);
extern void do_copper (void);

extern void notice_new_xcolors (void);
extern void notice_screen_contents_lost (void);
extern void init_row_map (void);
extern void init_hz (void);
extern void init_custom (void);

extern bool picasso_requested_on;
extern bool picasso_on;
extern void set_picasso_hack_rate (int hz);

/* Set to 1 to leave out the current frame in average frame time calculation.
 * Useful if the debugger was active.  */
extern int bogusframe;
extern unsigned long int hsync_counter;

extern uae_u16 dmacon;
extern uae_u16 intena, intreq, intreqr;

extern int current_hpos (void);
extern int vpos;

extern int find_copper_record (uaecptr, int *, int *);

extern int n_frames;

STATIC_INLINE int dmaen (unsigned int dmamask)
{
    return (dmamask & dmacon) && (dmacon & 0x200);
}


#define SPCFLAG_STOP 2
#define SPCFLAG_COPPER 4
#define SPCFLAG_INT 8
//#define SPCFLAG_BRK 16
//#define SPCFLAG_EXTRA_CYCLES 32
//#define SPCFLAG_TRACE 64
//#define SPCFLAG_DOTRACE 128
//#define SPCFLAG_DOINT 256 /* arg, JIT fails without this.. */
#define SPCFLAG_BLTNASTY 512
//#define SPCFLAG_EXEC 1024
#define SPCFLAG_ACTION_REPLAY 2048
#define SPCFLAG_TRAP 4096 /* enforcer-hack */
//#define SPCFLAG_MODE_CHANGE 8192
#ifdef JIT
#define SPCFLAG_END_COMPILE 16384
#endif

extern uae_u16 adkcon;

extern unsigned int joy0dir, joy1dir;
extern int joy0button, joy1button;

extern void INTREQ (uae_u16);
extern void INTREQ_0 (uae_u16);
extern void INTREQ_f (uae_u16);
extern void send_interrupt (int num, int delay);
extern uae_u16 INTREQR (void);

/* maximums for statically allocated tables */
#ifdef UAE_MINI
/* absolute minimums for basic A500/A1200-emulation */
#define MAXHPOS 227
#define MAXVPOS 312
#else
#define MAXHPOS 256
#define MAXVPOS 592
#endif

/* PAL/NTSC values */

#define MAXHPOS_PAL 227
#define MAXHPOS_NTSC 227
#define MAXVPOS_PAL 312
#define MAXVPOS_NTSC 262
#define VBLANK_ENDLINE_PAL 26
#define VBLANK_ENDLINE_NTSC 21
#define VBLANK_SPRITE_PAL 25
#define VBLANK_SPRITE_NTSC 20
#define VBLANK_HZ_PAL 50
#define VBLANK_HZ_NTSC 60
#define EQU_ENDLINE_PAL 9
#define EQU_ENDLINE_NTSC 10

extern int maxhpos, maxhpos_short;
extern int maxvpos, maxvpos_nom;
extern int hsyncstartpos;
extern int minfirstline, vblank_endline, numscrlines;
extern int vblank_hz, fake_vblank_hz, vblank_skip, doublescan;
extern frame_time_t syncbase;
#define NUMSCRLINES (maxvpos + 1 - minfirstline + 1)

#define DMA_AUD0      0x0001
#define DMA_AUD1      0x0002
#define DMA_AUD2      0x0004
#define DMA_AUD3      0x0008
#define DMA_DISK      0x0010
#define DMA_SPRITE    0x0020
#define DMA_BLITTER   0x0040
#define DMA_COPPER    0x0080
#define DMA_BITPLANE  0x0100
#define DMA_MASTER    0x0200
#define DMA_BLITPRI   0x0400

#define CYCLE_REFRESH	0x01
#define CYCLE_STROBE	0x02
#define CYCLE_MISC	0x04
#define CYCLE_SPRITE	0x08
#define CYCLE_COPPER	0x10
#define CYCLE_BLITTER	0x20
#define CYCLE_CPU	0x40
#define CYCLE_CPUNASTY	0x80

extern unsigned long frametime, timeframes;
extern int plfstrt, plfstop, plffirstline, plflastline;
extern uae_u16 htotal, vtotal, beamcon0;

/* 100 words give you 1600 horizontal pixels. Should be more than enough for
 * superhires. Don't forget to update the definition in genp2c.c as well.
 * needs to be larger for superhires support */
#ifdef CUSTOM_SIMPLE
#define MAX_WORDS_PER_LINE 50
#else
#define MAX_WORDS_PER_LINE 100
#endif

extern uae_u32 hirestab_h[256][2];
extern uae_u32 lorestab_h[256][4];

extern uae_u32 hirestab_l[256][1];
extern uae_u32 lorestab_l[256][2];

#ifdef AGA
/* AGA mode color lookup tables */
extern unsigned int xredcolors[256], xgreencolors[256], xbluecolors[256];
#endif
extern int xredcolor_s, xredcolor_b, xredcolor_m;
extern int xgreencolor_s, xgreencolor_b, xgreencolor_m;
extern int xbluecolor_s, xbluecolor_b, xbluecolor_m;

#define RES_LORES 0
#define RES_HIRES 1
#define RES_SUPERHIRES 2
#define RES_MAX 2
#define VRES_NONDOUBLE 0
#define VRES_DOUBLE 1
#define VRES_MAX 1

/* calculate shift depending on resolution (replaced "decided_hires ? 4 : 8") */
#define RES_SHIFT(res) ((res) == RES_LORES ? 8 : (res) == RES_HIRES ? 4 : 2)

/* get resolution from bplcon0 */
#if AMIGA_ONLY
STATIC_INLINE int GET_RES_DENISE (uae_u16 con0)
{
    if (!(currprefs.chipset_mask & CSMASK_ECS_DENISE))
	con0 &= ~0x40;
    return ((con0) & 0x8000) ? RES_HIRES : ((con0) & 0x40) ? RES_SUPERHIRES : RES_LORES;
}
STATIC_INLINE int GET_RES_AGNUS (uae_u16 con0)
{
    if (!(currprefs.chipset_mask & CSMASK_ECS_AGNUS))
	con0 &= ~0x40;
    return ((con0) & 0x8000) ? RES_HIRES : ((con0) & 0x40) ? RES_SUPERHIRES : RES_LORES;
}
#endif // AMIGA_ONLY

/* get sprite width from FMODE */
#define GET_SPRITEWIDTH(FMODE) ((((FMODE) >> 2) & 3) == 3 ? 64 : (((FMODE) >> 2) & 3) == 0 ? 16 : 32)
/* Compute the number of bitplanes from a value written to BPLCON0  */
STATIC_INLINE int GET_PLANES(uae_u16 bplcon0)
{
    if ((bplcon0 & 0x0010) && (bplcon0 & 0x7000))
	return 0;
    if (bplcon0 & 0x0010)
	return 8;
    return (bplcon0 >> 12) & 7;
}

extern void fpscounter_reset (void);
extern unsigned long idletime;
extern int lightpen_x, lightpen_y, lightpen_cx, lightpen_cy;

struct customhack {
    uae_u16 v;
    int vpos, hpos;
};
void customhack_put (struct customhack *ch, uae_u16 v, int hpos);
uae_u16 customhack_get (struct customhack *ch, int hpos);
extern void alloc_cycle_ext (int, int);
extern bool ispal (void);
extern int inprec_open(char *fname, int record);
extern void sleep_millis (int ms);
extern void mmu_do_hit (void);

#endif /* WINUAE_CUSTOM_H */
