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
#include <time.h>
#include <endian.h>

#include "../shared/rtc.h"
#include "../../config_file/config_file.h"

#include "gayle-ide/ide.h"
#include "amiga-registers.h"

uint8_t gary_cfg[8];

uint8_t ramsey_cfg = 0x08;
static uint8_t ramsey_id = RAMSEY_REV7;

int counter;
static uint8_t gayle_irq, gayle_cs, gayle_cs_mask, gayle_cfg;
static struct ide_controller *ide0;
int fd;

uint8_t rtc_type = RTC_TYPE_RICOH;

char *hdd_image_file[GAYLE_MAX_HARDFILES];

uint8_t cdtv_mode = 0;
unsigned char cdtv_sram[32 * SIZE_KILO];

uint8_t gayle_a4k = 0xA0;
uint16_t gayle_a4k_irq = 0;
uint8_t gayle_a4k_int = 0;
uint8_t gayle_int = 0;

uint32_t gayle_ide_mask = ~GDATA;
uint32_t gayle_ide_base = GDATA;
uint8_t gayle_ide_adj = 0;

struct ide_controller *get_ide(int index) {
  //if (index) {}
  return ide0;
}

void adjust_gayle_4000() {
  gayle_ide_base = GAYLE_IDE_BASE_A4000;
  gayle_ide_adj = 2;
  gayle_a4k_int = 1;
}

void adjust_gayle_1200() {

}

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

  if (gayle_int & (1 << 7)) {
    irq = ide0->drive->intrq;
    //	if (irq==0)
    //	printf("IDE IRQ: %x\n",irq);
    return irq;
  };
  return 0;
}

static uint8_t ide_action = 0;

void writeGayleB(unsigned int address, unsigned int value) {
  if (address >= gayle_ide_base) {
    switch ((address - gayle_ide_base) - gayle_ide_adj) {
      case GFEAT_OFFSET:
        ide_action = ide_feature_w;
        goto idewrite8;
      case GCMD_OFFSET:
        ide_action = ide_command_w;
        goto idewrite8;
      case GSECTCOUNT_OFFSET:
        ide_action = ide_sec_count;
        goto idewrite8;
      case GSECTNUM_OFFSET:
        ide_action = ide_sec_num;
        goto idewrite8;
      case GCYLLOW_OFFSET:
        ide_action = ide_cyl_low;
        goto idewrite8;
      case GCYLHIGH_OFFSET:
        ide_action = ide_cyl_hi;
        goto idewrite8;
      case GDEVHEAD_OFFSET:
        ide_action = ide_dev_head;
        goto idewrite8;
      case GCTRL_OFFSET:
        ide_action = ide_devctrl_w;
        goto idewrite8;
      case GIRQ_4000_OFFSET:
        gayle_a4k_irq = value;
      case GIRQ_OFFSET:
        gayle_irq = (gayle_irq & value) | (value & (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR));
        return;
    }
    goto skip_idewrite8;
idewrite8:;
    ide_write8(ide0, ide_action, value);
    return;
skip_idewrite8:;
  }

  switch (address) {
    /*case 0xDD203A:
      printf("Write bye to A4000 Gayle: %.2X\n", value);
      gayle_a4k = value;
      return;*/
    case GIDENT:
      counter = 0;
      return;
    case GCONF:
      gayle_cfg = value;
      return;
    case RAMSEY_REG:
      ramsey_cfg = value & 0x0F;
      return;
    case GINT:
      gayle_int = value;
      return;
    case GCS:
      gayle_cs_mask = value & ~3;
      gayle_cs &= ~3;
      gayle_cs |= value & 3;
      return;
  }

  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] BYTE write to SRAM @%.8X (%.8X): %.2X\n", (address & CLOCKMASK) - 0x8000, address, value);
        cdtv_sram[(address & CLOCKMASK) - 0x8000] = value;
      }
      return;
    }
    //printf("Byte write to RTC.\n");
    put_rtc_byte(address, value, rtc_type);
    return;
  }

  printf("Write Byte to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

void writeGayle(unsigned int address, unsigned int value) {
  if (address - gayle_ide_base == GDATA_OFFSET) {
    ide_write16(ide0, ide_data, value);
    return;
  }

  if (address == GIRQ_A4000) {
    gayle_a4k_irq = value;
    return;
  }

  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] WORD write to SRAM @%.8X (%.8X): %.4X\n", (address & CLOCKMASK) - 0x8000, address, htobe16(value));
        ((short *) ((size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000)))[0] = htobe16(value);
      }
      return;
    }
    //printf("Word write to RTC.\n");
    put_rtc_byte(address + 1, (value & 0xFF), rtc_type);
    put_rtc_byte(address, (value >> 8), rtc_type);
    return;
  }

  printf("Write Word to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

void writeGayleL(unsigned int address, unsigned int value) {
  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] LONGWORD write to SRAM @%.8X (%.8X): %.8X\n", (address & CLOCKMASK) - 0x8000, address, htobe32(value));
        ((int *) (size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000))[0] = htobe32(value);
      }
      return;
    }
    //printf("Longword write to RTC.\n");
    put_rtc_byte(address + 3, (value & 0xFF), rtc_type);
    put_rtc_byte(address + 2, ((value & 0x0000FF00) >> 8), rtc_type);
    put_rtc_byte(address + 1, ((value & 0x00FF0000) >> 16), rtc_type);
    put_rtc_byte(address, (value >> 24), rtc_type);
    return;
  }

  printf("Write Long to Gayle Space 0x%06x (0x%06x)\n", address, value);
}

uint8_t readGayleB(unsigned int address) {
  uint8_t ide_action = 0;

  if (address >= gayle_ide_base) {
    switch ((address - gayle_ide_base) - gayle_ide_adj) {
      case GERROR_OFFSET:
        ide_action = ide_error_r;
        goto ideread8;
      case GSTATUS_OFFSET:
        ide_action = ide_status_r;
        goto ideread8;
      case GSECTCOUNT_OFFSET:
        ide_action = ide_sec_count;
        goto ideread8;
      case GSECTNUM_OFFSET:
        ide_action = ide_sec_num;
        goto ideread8;
      case GCYLLOW_OFFSET:
        ide_action = ide_cyl_low;
        goto ideread8;
      case GCYLHIGH_OFFSET:
        ide_action = ide_cyl_hi;
        goto ideread8;
      case GDEVHEAD_OFFSET:
        ide_action = ide_dev_head;
        goto ideread8;
      case GCTRL_OFFSET:
        ide_action = ide_altst_r;
        goto ideread8;
      case GIRQ_4000_OFFSET:
      case GIRQ_OFFSET:
        return 0x80;
        //gayle_irq = (gayle_irq & value) | (value & (GAYLE_IRQ_RESET | GAYLE_IRQ_BERR));
    }
    goto skip_ideread8;
ideread8:;
    return ide_read8(ide0, ide_action);
skip_ideread8:;
  }

  switch (address) {
    case GIDENT: {
      uint8_t val;
      if (counter == 0 || counter == 1 || counter == 3) {
        val = 0x80;  // 80; to enable gayle
      } else {
        val = 0x00;
      }
      counter++;
      return val;
    }
    case GINT:
      return gayle_int;
    case GCONF:
      return gayle_cfg & 0x0f;
    case GCS: {
      uint8_t v;
      v = gayle_cs_mask | gayle_cs;
      return v;
    }
    // This seems incorrect, GARY_REG3 is the same as GIDENT, and the A4000
    // service manual says that Gary is accessible in the address range $DFC000 to $DFFFFF.
    case GARY_REG0:
    case GARY_REG1:
    case GARY_REG2:
      return gary_cfg[address - GARY_REG0];
      break;
    //case GARY_REG3:
    case GARY_REG4:
    //case GARY_REG5:
      return gary_cfg[address - GARY_REG4];
    case RAMSEY_ID:
      return ramsey_id;
    case RAMSEY_REG:
      return ramsey_cfg;
    case GARY_REG5: { // This makes no sense.
      uint8_t val;
      if (counter == 0 || counter == 1 || counter == 3) {
        val = 0x80;  // 80; to enable GARY
      } else {
        val = 0x00;
      }
      counter++;
      return val;
    }
    //case 0xDD203A:
      // This can't be correct, as this is the same address as GDEVHEAD on the A4000 Gayle.
      //printf("Read Byte from Gayle A4k: %.2X\n", gayle_a4k);
      //return gayle_a4k;
  }

  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] BYTE read from SRAM @%.8X (%.8X): %.2X\n", (address & CLOCKMASK) - 0x8000, address, cdtv_sram[(address & CLOCKMASK) - 0x8000]);
        return cdtv_sram[(address & CLOCKMASK) - 0x8000];
      }
      return 0;
    }
    //printf("Byte read from RTC.\n");
    return get_rtc_byte(address, rtc_type);
  }

  printf("Read Byte From Gayle Space 0x%06x\n", address);
  return 0xFF;
}

uint16_t readGayle(unsigned int address) {
  if (address - gayle_ide_base == GDATA_OFFSET) {
    uint16_t value;
    value = ide_read16(ide0, ide_data);
    //	value = (value << 8) | (value >> 8);
    return value;
  }

  if (address == GIRQ_A4000) {
    gayle_a4k_irq = 0x8000;
    return 0x8000;
  }

  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] WORD read from SRAM @%.8X (%.8X): %.4X\n", (address & CLOCKMASK) - 0x8000, address, be16toh( (( unsigned short *) (size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000))[0]));
        return be16toh( (( unsigned short *) (size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000))[0]);
      }
      return 0;
    }
    //printf("Word read from RTC.\n");
    return ((get_rtc_byte(address, rtc_type) << 8) | (get_rtc_byte(address + 1, rtc_type)));
  }

  printf("Read Word From Gayle Space 0x%06x\n", address);
  return 0x8000;
}

uint32_t readGayleL(unsigned int address) {
  if ((address & GAYLEMASK) == CLOCKBASE) {
    if ((address & CLOCKMASK) >= 0x8000) {
      if (cdtv_mode) {
        //printf("[CDTV] LONGWORD read from SRAM @%.8X (%.8X): %.8X\n", (address & CLOCKMASK) - 0x8000, address, be32toh( (( unsigned short *) (size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000))[0]));
        return be32toh( (( unsigned int *) (size_t)(cdtv_sram + (address & CLOCKMASK) - 0x8000))[0]);
      }
      return 0;
    }
    //printf("Longword read from RTC.\n");
    return ((get_rtc_byte(address, rtc_type) << 24) | (get_rtc_byte(address + 1, rtc_type) << 16) | (get_rtc_byte(address + 2, rtc_type) << 8) | (get_rtc_byte(address + 3, rtc_type)));
  }

  printf("Read Long From Gayle Space 0x%06x\n", address);
  return 0x8000;
}
