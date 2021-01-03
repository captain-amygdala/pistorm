//
//  Gayle.c
//  Omega
//
//  Created by Matt Parsons on 06/03/2019.
//  Copyright © 2019 Matt Parsons. All rights reserved.
//

// Write Byte to Gayle Space 0xda9000 (0x0000c3)
// Read Byte From Gayle Space 0xda9000
// Read Byte From Gayle Space 0xdaa000

#include "Gayle.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "ide.h"
#include "platforms/amiga/amiga-registers.h"


//#define GSTATUS 0xda201c
//#define GCLOW   0xda2010
//#define GDH	0xda2018

// Gayle Addresses
//#define GAYLE_IDE_BASE_A1200 0xDA0000 //8 bit base
#define GAYLE_IDE_BASE_A1200 0xDA2000 //16bit base
#define GAYLE_IDE_BASE_A4000 0xDD2020

// Gayle IDE Reads
#define GERROR 0xda2004   // Error
#define GSTATUS 0xda201C  // Status
#define GERROR_A4000 GAYLE_IDE_BASE_A4000 + 0x06   // Error
#define GSTATUS_A4000 GAYLE_IDE_BASE_A4000 + 0x1E  // Status

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

// Gayle Ident
#define GIDENT 0xDE1000

// Gayle IRQ/CC
#define GCS 0xDA8000   // Card Control
#define GIRQ 0xDA9000  // IRQ
#define GINT 0xDAA000  // Int enable
#define GCONF 0xDAB000  // Gayle Config

// For A4000 there's no need to populate other areas, just GIRQ
#define GIRQ_A4000 GAYLE_IDE_BASE_A4000 + 0x1000  // IRQ  0xDD3020

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

#define GAYLE_MAX_HARDFILES 8

int counter;
static uint8_t gayle_irq, gayle_int, gayle_cs, gayle_cs_mask, gayle_cfg;
static struct ide_controller *ide0;
int fd;

uint8_t gary_cfg0 = 0x00;
uint8_t gary_cfg1 = 0x00;
uint8_t gary_cfg2 = 0x00;
uint8_t gary_cfg3 = 0x00;
uint8_t gary_cfg4 = 0x00;
uint8_t gary_cfg5 = 0x00;

uint8_t gayle_a4k = 0xA0;
uint16_t gayle_a4k_irq;
uint8_t ramsey_cfg = 0x08;
static uint8_t ramsey_id = RAMSEY_REV7;

char *hdd_image_file[GAYLE_MAX_HARDFILES];

void set_hard_drive_image_file_amiga(uint8_t index, char *filename) {
  if (hdd_image_file[index] != NULL)
    free(hdd_image_file[index]);
  hdd_image_file[index] = calloc(1, strlen(filename) + 1);
  strcpy(hdd_image_file[index], filename);
}

void InitGayle(void) {
  if (!hdd_image_file[0]) {
    hdd_image_file[0] = calloc(1, 64);
    sprintf(hdd_image_file[0], "hd0.img");
  }

  ide0 = ide_allocate("cf");
  fd = open(hdd_image_file[0], O_RDWR);
  if (fd == -1) {
    printf("HDD Image %s failed open\n", hdd_image_file[0]);
  } else {
    ide_attach(ide0, 0, fd);
    ide_reset_begin(ide0);
    printf("HDD Image %s attached\n", hdd_image_file[0]);
  }
}

uint8_t CheckIrq(void) {
  uint8_t irq;
  /* skipping gayle_int check makes A4000 ROMs IDE work at decent speed  */
  irq = ide0->drive->intrq;
  return irq;
}

void writeGayleB(unsigned int address, unsigned int value) {
  if (address == GFEAT || address == GFEAT_A4000) {
    ide_write8(ide0, ide_feature_w, value);
    return;
  }
  if (address == GCMD || address == GCMD_A4000) {
    ide_write8(ide0, ide_command_w, value);
    return;
  }
  if (address == GSECTCNT || address == GSECTCNT_A4000) {
    ide_write8(ide0, ide_sec_count, value);
    return;
  }
  if (address == GSECTNUM || address == GSECTNUM_A4000) {
    ide_write8(ide0, ide_sec_num, value);
    return;
  }
  if (address == GCYLLOW || address == GCYLLOW_A4000) {
    ide_write8(ide0, ide_cyl_low, value);
    return;
  }
  if (address == GCYLHIGH || address == GCYLHIGH_A4000) {
    ide_write8(ide0, ide_cyl_hi, value);
    return;
  }
  if (address == GDEVHEAD || address == GDEVHEAD_A4000) {
    ide_write8(ide0, ide_dev_head, value);
    return;
  }
  if (address == GCTRL || address == GCTRL_A4000) {
    ide_write8(ide0, ide_devctrl_w, value);
    return;
  }

  if (address == GIDENT) {
    counter = 0;
    printf("Write Byte to Gayle Ident 0x%06x (0x%06x)\n",address,value);
    return;
  }

  if (address == 0xDD203A) {
    printf("Write Byte to 0xDD203A Gayle 0x%06x (0x%06x)\n",address,value);
    gayle_a4k = value;
     return;
   }

  if (address == GIRQ || address == GIRQ_A4000) {
    //printf("Write Byte to Gayle GIRQ 0x%06x (0x%06x)\n",address,value);
    gayle_irq = (gayle_irq & value) & (value & (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR));
    return;
  }

  if (address == GCS) {
    printf("Write Byte to Gayle GCS 0x%06x (0x%06x)\n", address, value);
    gayle_cs_mask = value & ~3;
    gayle_cs &= ~3;
    gayle_cs |= value & 3;
    return;
  }

  if (address == GINT) {
    printf("Write Byte to Gayle GINT 0x%06x (0x%06x)\n", address, value);
    gayle_int = value;
    return;
  }

  if (address == GCONF) {
    printf("Write Byte to Gayle GCONF 0x%06x (0x%06x)\n", address, value);
    gayle_cfg = value;
    return;
  }

  if (address == RAMSEY_REG) {
    printf("Write Byte to RAMSEY_REG Space 0x%06x (0x%06x)\n", address, value);
    ramsey_cfg = value & 0x0f;
    return;
  }

  printf("Write Byte to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

void writeGayle(unsigned int address, unsigned int value) {
  if (address == GDATA || address == GDATA_A4000) {
    ide_write16(ide0, ide_data, value);
    return;
  }

  if (address == GIRQ_A4000) {
    gayle_a4k_irq = value;
    return;
  }

  printf("Write Word to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

void writeGayleL(unsigned int address, unsigned int value) {
  printf("Write Long to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

uint8_t readGayleB(unsigned int address) {
  if (address == GERROR || address == GERROR_A4000) {
    return ide_read8(ide0, ide_error_r);
  }
  if (address == GSTATUS || address == GSTATUS_A4000) {
    return ide_read8(ide0, ide_status_r);
  }

  if (address == GSECTCNT || address == GSECTCNT_A4000) {
    return ide_read8(ide0, ide_sec_count);
  }

  if (address == GSECTNUM || address == GSECTNUM_A4000) {
    return ide_read8(ide0, ide_sec_num);
  }

  if (address == GCYLLOW || address == GCYLLOW_A4000) {
    return ide_read8(ide0, ide_cyl_low);
  }

  if (address == GCYLHIGH || address == GCYLHIGH_A4000) {
    return ide_read8(ide0, ide_cyl_hi);
  }

  if (address == GDEVHEAD || address == GDEVHEAD_A4000) {
    return ide_read8(ide0, ide_dev_head);
  }

  if (address == GCTRL || address == GCTRL_A4000) {
    return ide_read8(ide0, ide_altst_r);
  }

  if (address == 0xDD203A) {
    printf("Write Byte to 0xDD203A Gayle 0x%06x (0x%06x)\n",address, gayle_a4k);
    return gayle_a4k;
  }

  if (address == GIDENT) {
    uint8_t val;
    // printf("Read Byte from Gayle Ident 0x%06x (0x%06x)\n",address,counter);
    if (counter == 0 || counter == 1 || counter == 3) {
      val = 0x80;  // 80; to enable gayle
    } else {
      val = 0x00;
    }
    counter++;
    return val;
  }

  if (address == GARY_REG5) {
    uint8_t val;
    printf("Read Byte from GARY Ident 0x%06x (0x%06x)\n",address,counter);
    if (counter == 0 || counter == 1 || counter == 3) {
      val = 0x80;  // 80; to enable GARY
    } else {
      val = 0x00;
    }
    counter++;
    return val;
  }


  if (address == GIRQ || address == GIRQ_A4000) {
    //printf("Read Byte From GIRQ Space 0x%06x\n",gayle_irq);
    return 0x80;//gayle_irq;
/*
    uint8_t irq;
    irq = ide0->drive->intrq;

    if (irq == 1) {
      // printf("IDE IRQ: %x\n",irq);
      return 0x80;  // gayle_irq;
    }

    return 0;
*/ 
 }

  if (address == GCS) {
    printf("Read Byte From GCS Space 0x%06x\n", 0x1234);
    uint8_t v;
    v = gayle_cs_mask | gayle_cs;
    return v;
  }

  if (address == GINT) {
    printf("Read Byte From GINT Space 0x%06x\n",gayle_int);
    return gayle_int;
  }

  if (address == GCONF) {
    printf("Read Byte From GCONF Space 0x%06x\n", gayle_cfg & 0x0f);
    return gayle_cfg & 0x0f;
  }

  if (address == GARY_REG0) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg0 & 0x80);
    return gary_cfg0;
  }
  if (address == GARY_REG1) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg1 & 0x80);
    return gary_cfg1;
  }
  if (address == GARY_REG2) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg2 & 0x80);
    return gary_cfg2;
  }
  if (address == GARY_REG3) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg3 & 0x80);
    return gary_cfg3;
  }
  if (address == GARY_REG4) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg4 & 0x80);
    return gary_cfg4;
  }
  if (address == GARY_REG5) {
    printf("Read Byte From GARY_REG Space 0x%06x (0x%06x)\n", address, gary_cfg5 & 0x80);
    return gary_cfg5;
  }

  if (address == RAMSEY_REG) {
    printf("Read Byte From RAMSEY_REG Space 0x%06x (0x%06x)\n", address, ramsey_cfg & 0x0f);
    return ramsey_cfg;
  }

  if (address == RAMSEY_ID) {
    printf("Read Byte From RAMSEY_ID Space 0x%06x (0x%06x)\n", address, ramsey_id & 0x0f);
    return ramsey_id;
  }

  printf("Read Byte From Gayle Space 0x%06x\n", address);
  return 0xFF;
}

uint16_t readGayle(unsigned int address) {
  if (address == GDATA || address == GDATA_A4000) {
    uint16_t value;
    value = ide_read16(ide0, ide_data);
    //	value = (value << 8) | (value >> 8);
    return value;
  }
  if (address == GIRQ_A4000) {
    //printf("Read Word From GIRQ_A4000 Space 0x%06x\n", address);
    gayle_a4k_irq = 0x8000;
    return 0x80FF;
  }
  printf("Read Word From Gayle Space 0x%06x\n", address);
  return 0x8000;
}

uint32_t readGayleL(unsigned int address) {
  printf("Read Long From Gayle Space 0x%06x\n", address);
  return 0x8000;
}
