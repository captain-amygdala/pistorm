/**
 * amiga custom chip registers
 *
 * transcribed from adcd 2.1 amigaguide. likely omits aa chipset registers, and gayle.
 *
 * just nine <nine@aphlor.org>
 */

#ifndef _AMIGA_REGISTERS_H
#define _AMIGA_REGISTERS_H

/**
 * three discernable banks which cover all amiga systems.
 *
 * bfd000-bfdf00 for the even cia
 * bfe001-bfef01 for the odd cia
 * dff000-dfffff for custom chip registers
 *
 * we will concern ourselves with the custom chip registers here; the cia is for additional i/o ops
 */

#define CREG_BASE   0xdff000
#define CREG_END    0xdfffff

#define BLTDDAT     CREG_BASE + 0x000   // agnus, blitter destination early read address (dummy)
#define DMACONR     CREG_BASE + 0x002   // agnus, paula dma control + blitter status read
#define VPOSR       CREG_BASE + 0x004   // ecs agnus, read vertical msb + frame flop
#define VHPOSR      CREG_BASE + 0x006   // agnus, vertical + horizonal position of beam
#define DSKDATR     CREG_BASE + 0x008   // paula, disk data early read address (dummy)
#define JOY0DAT     CREG_BASE + 0x00a   // denise, input port 0 data
#define JOY1DAT     CREG_BASE + 0x00c   // denise, input port 1 data
// clxdat
// adkconr
// pot0dat
// pot1dat
// potgor
#define SERDATR     CREG_BASE + 0x018   // paula, serial port data and status, read
// dskbytr
// internar
// intreqr
// dskpth
// dskptl
// dsklen
// refptr
// vposw
// vhposw
// copcon
#define SERDAT      CREG_BASE + 0x030   // paula, serial port data and stop bits, write
#define SERPER      CREG_BASE + 0x032   // paula, serial port speed and control
// @todo: the rest?!

#endif
