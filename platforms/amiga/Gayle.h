//
//  Gayle.h
//  Omega
//
//  Created by Matt Parsons on 06/03/2019.
//  Copyright © 2019 Matt Parsons. All rights reserved.
//

#ifndef Gayle_h
#define Gayle_h

#define GAYLE_MAX_HARDFILES 8

#include <stdio.h>
#include <stdint.h>

void InitGayle(void);
void writeGayleB(unsigned int address, unsigned value);
void writeGayle(unsigned int address, unsigned value);
void writeGayleL(unsigned int address, unsigned value);
uint8_t readGayleB(unsigned int address);
uint16_t readGayle(unsigned int address);
uint32_t readGayleL(unsigned int address);

// Gayle Addresses
#define GAYLE_IDE_BASE_A1200 0xDA2000 //16bit base
#define GAYLE_IDE_BASE_A4000 0xDD2020

// Gayle IDE Reads
#define GERROR 0xda2004   // Error
#define GSTATUS 0xda201C  // Status
#define GERROR_A4000 GAYLE_IDE_BASE_A4000 + 0x06   // Error
#define GSTATUS_A4000 GAYLE_IDE_BASE_A4000 + 0x1E  // Status

// Gayle IDE read offsets
#define GERROR_OFFSET 0x04
#define GSTATUS_OFFSET 0x1C
// Gayle IDE write offsets
#define GFEAT_OFFSET 0x04
#define GCMD_OFFSET 0x1C
// Gayle IDE RW offsets
#define GDATA_OFFSET 0x00
#define GSECTCOUNT_OFFSET 0x08
#define GSECTNUM_OFFSET 0x0C
#define GCYLLOW_OFFSET 0x10
#define GCYLHIGH_OFFSET 0x14
#define GDEVHEAD_OFFSET 0x18
#define GCTRL_OFFSET 0x1018
#define GIRQ_OFFSET 0x7000
#define GIRQ_4000_OFFSET 0x1002

// Gayle IDE Writes
#define GFEAT 0xda2004  // Write : Feature
#define GCMD 0xda201c   // Write : Command
#define GFEAT_A4000 GAYLE_IDE_BASE_A4000 + 0x06  // Write : Feature
#define GCMD_A4000 GAYLE_IDE_BASE_A4000 + 0x1E   // Write : Command
#define GMODEREG0_A4000 0x0DD1020   // D31, PIO modes (00,01,10)
#define GMODEREG1_A4000 0x0DD1022   // D31, (MSB)

// Gayle IDE RW
#define GDATA 0xda2000     // Data - 16 bit
#define GSECTCNT 0xda2008  // SectorCount
#define GSECTNUM 0xda200c  // SectorNumber
#define GCYLLOW 0xda2010   // CylinderLow
#define GCYLHIGH 0xda2014  // CylinderHigh
#define GDEVHEAD 0xda2018  // Device/Head
#define GCTRL 0xda3018     // Control

#define GDATA_A4000 GAYLE_IDE_BASE_A4000     // Data
#define GSECTCNT_A4000 GAYLE_IDE_BASE_A4000 + 0x0a  // SectorCount
#define GSECTNUM_A4000 GAYLE_IDE_BASE_A4000 + 0x0e  // SectorNumber
#define GCYLLOW_A4000 GAYLE_IDE_BASE_A4000 + 0x12   // CylinderLow
#define GCYLHIGH_A4000 GAYLE_IDE_BASE_A4000 + 0x16  // CylinderHigh
#define GDEVHEAD_A4000 GAYLE_IDE_BASE_A4000 + 0x1a  // Device/Head
#define GCTRL_A4000 GAYLE_IDE_BASE_A4000 + 0x101a     // Control

// For A4000 there's no need to populate other areas, just GIRQ
#define GIRQ_A4000 GAYLE_IDE_BASE_A4000 + 0x1000  // IRQ  0xDD3020

// Gayle Ident
#define GIDENT 0xDE1000

// Gayle IRQ/CC
#define GCS 0xDA8000   // Card Control
#define GIRQ 0xDA9000  // IRQ
#define GINT 0xDAA000  // Int enable
#define GCONF 0xDAB000  // Gayle Config

/* DA8000 */
#define GAYLE_CS_IDE 0x80   /* IDE int status */
#define GAYLE_CS_CCDET 0x40 /* credit card detect */
#define GAYLE_CS_BVD1 0x20  /* battery voltage detect 1 */
#define GAYLE_CS_SC 0x20    /* credit card status change */
#define GAYLE_CS_BVD2 0x10  /* battery voltage detect 2 */
#define GAYLE_CS_DA 0x10    /* digital audio */
#define GAYLE_CS_WR 0x08    /* write enable (1 == enabled) */
#define GAYLE_CS_BSY 0x04   /* credit card busy */
#define GAYLE_CS_IRQ 0x04   /* interrupt request */
#define GAYLE_CS_DAEN 0x02  /* enable digital audio */
#define GAYLE_CS_DIS 0x01   /* disable PCMCIA slot */

/* DA9000 */
#define GAYLE_IRQ_IDE 0x80
#define GAYLE_IRQ_CCDET 0x40 /* credit card detect */
#define GAYLE_IRQ_BVD1 0x20  /* battery voltage detect 1 */
#define GAYLE_IRQ_SC 0x20    /* credit card status change */
#define GAYLE_IRQ_BVD2 0x10  /* battery voltage detect 2 */
#define GAYLE_IRQ_DA 0x10    /* digital audio */
#define GAYLE_IRQ_WR 0x08    /* write enable (1 == enabled) */
#define GAYLE_IRQ_BSY 0x04   /* credit card busy */
#define GAYLE_IRQ_IRQ 0x04   /* interrupt request */
#define GAYLE_IRQ_RESET 0x02 /* reset machine after CCDET change */
#define GAYLE_IRQ_BERR 0x01  /* generate bus error after CCDET change */

/* DAA000 */
#define GAYLE_INT_IDE 0x80     /* IDE interrupt enable */
#define GAYLE_INT_CCDET 0x40   /* credit card detect change enable */
#define GAYLE_INT_BVD1 0x20    /* battery voltage detect 1 change enable */
#define GAYLE_INT_SC 0x20      /* credit card status change enable */
#define GAYLE_INT_BVD2 0x10    /* battery voltage detect 2 change enable */
#define GAYLE_INT_DA 0x10      /* digital audio change enable */
#define GAYLE_INT_WR 0x08      /* write enable change enabled */
#define GAYLE_INT_BSY 0x04     /* credit card busy */
#define GAYLE_INT_IRQ 0x04     /* credit card interrupt request */
#define GAYLE_INT_BVD_LEV 0x02 /* BVD int level, 0=lev2,1=lev6 */
#define GAYLE_INT_BSY_LEV 0x01 /* BSY int level, 0=lev2,1=lev6 */

#endif /* Gayle_h */
