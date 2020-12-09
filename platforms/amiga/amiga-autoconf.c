#include "../platforms.h"
#include "amiga-autoconf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned char ac_fast_ram_rom[] = {
    0xe, AC_MEM_SIZE_8MB,                   // 00/02, link into memory free list, 8 MB
    0x6, 0x9,                               // 04/06, product id
    0x8, 0x0,                               // 08/0a, preference to 8 MB space
    0x0, 0x0,                               // 0c/0e, reserved
    0x0, 0x7, 0xd, 0xb,                     // 10/12/14/16, mfg id
    0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x2, 0x0  // 18/.../26, serial
};

static unsigned char ac_a314_rom[] = {
    0xc, AC_MEM_SIZE_64KB,                  // 00/02, 64 kB
    0xa, 0x3,                               // 04/06, product id
    0x0, 0x0,                               // 08/0a, any space okay
    0x0, 0x0,                               // 0c/0e, reserved
    0x0, 0x7, 0xd, 0xb,                     // 10/12/14/16, mfg id
    0xa, 0x3, 0x1, 0x4, 0x0, 0x0, 0x0, 0x0  // 18/.../26, serial
};

int ac_z2_current_pic = 0;
int ac_z2_pic_count = 0;
int ac_z2_done = 0;
int ac_z2_type[AC_PIC_LIMIT];
int ac_z2_index[AC_PIC_LIMIT];
unsigned int ac_base[AC_PIC_LIMIT];

int ac_z3_current_pic = 0;
int ac_z3_pic_count = 0;
int ac_z3_done = 0;
int ac_z3_type[AC_PIC_LIMIT];
int ac_z3_index[AC_PIC_LIMIT];


unsigned char get_autoconf_size(int size) {
  if (size == 8 * SIZE_MEGA)
    return AC_MEM_SIZE_8MB;
  if (size == 4 * SIZE_MEGA)
    return AC_MEM_SIZE_4MB;
  if (size == 2 * SIZE_MEGA)
    return AC_MEM_SIZE_2MB;
  else
    return AC_MEM_SIZE_64KB;
}

unsigned char get_autoconf_size_ext(int size) {
  if (size == 16 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_16MB;
  if (size == 32 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_32MB;
  if (size == 64 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_64MB;
  if (size == 128 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_128MB;
  if (size == 256 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_256MB;
  if (size == 512 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_512MB;
  if (size == 1024 * SIZE_MEGA)
    return AC_MEM_SIZE_EXT_1024MB;
  else
    return AC_MEM_SIZE_EXT_64MB;
}

unsigned int autoconfig_read_memory_z3_8(struct emulator_config *cfg, unsigned int address_) {
  int address = address_ - AC_Z3_BASE;
  int index = ac_z3_index[ac_z3_current_pic];
  unsigned char val = 0;

  if ((address & 0xFF) >= AC_Z3_REG_RES50 && (address & 0xFF) <= AC_Z3_REG_RES7C)
    val = 0;
  else {
    switch(address & 0xFF) {
      case AC_Z3_REG_ER_TYPE:
        val |= BOARDTYPE_Z3;
        if (cfg->map_type[index] == MAPTYPE_RAM)
          val |= BOARDTYPE_FREEMEM;
        if (cfg->map_size[index] > 8 * SIZE_MEGA)
          val |= get_autoconf_size_ext(cfg->map_size[index]);
        else
          val |= get_autoconf_size(cfg->map_size[index]);
        if (ac_z3_current_pic + 1 < ac_z3_pic_count)
          val |= BOARDTYPE_LINKED;
        // Pre-invert this value, since it's the only value not physically complemented
        // for Zorro III.
        val ^= 0xFF;
        break;
      case AC_Z3_REG_ER_PRODUCT:
        // 1.1... maybe...
        val = 0x11;
        break;
      case AC_Z3_REG_ER_FLAGS:
        if (cfg->map_type[index] == MAPTYPE_RAM)
          val |= Z3_FLAGS_MEMORY;
        if (cfg->map_size[index] > 8 * SIZE_MEGA)
          val |= Z3_FLAGS_EXTENSION;
        val |= Z3_FLAGS_RESERVED;
        // Bottom four bits are zero, useless unles you want really odd RAM sizes.
        break;
      // Manufacturer ID low/high bytes.
      case AC_Z3_REG_MAN_LO:
        val = PISTORM_MANUF_ID & 0x00FF;
        break;
      case AC_Z3_REG_MAN_HI:
        val = (PISTORM_MANUF_ID >> 8);
        break;
      case AC_Z3_REG_SER_BYTE0:
      case AC_Z3_REG_SER_BYTE1:
      case AC_Z3_REG_SER_BYTE2:
      case AC_Z3_REG_SER_BYTE3:
        // Expansion board serial assigned by manufacturer.
        val = 0;
        break;
      case AC_Z3_REG_INIT_DIAG_VEC_LO:
      case AC_Z3_REG_INIT_DIAG_VEC_HI:
        // 16-bit offset to boot ROM in assigned memory range.
        val = 0;
        break;
      // Additional reserved/unused registers.
      case AC_Z3_REG_ER_RES03:
      case AC_Z3_REG_ER_RES0D:
      case AC_Z3_REG_ER_RES0E:
      case AC_Z3_REG_ER_RES0F:
      case AC_Z3_REG_ER_Z2_INT:
      default:
        val = 0;
        break;
    }
  }
  //printf("Read byte %d from Z3 autoconf for PIC %d (%.2X).\n", address, ac_z3_current_pic, val);
  return (address & 0x100) ? (val << 4) ^ 0xFF : (val & 0xF0) ^ 0xFF;
}

int nib_latch = 0;

void autoconfig_write_memory_z3_8(struct emulator_config *cfg, unsigned int address_, unsigned int value) {
  int address = address_ - AC_Z3_BASE;
  int index = ac_z3_index[ac_z3_current_pic];
  unsigned char val = (unsigned char)value;
  int done = 0;

  switch(address & 0xFF) {
    case AC_Z3_REG_WR_ADDR_LO:
      if (nib_latch) {
        ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0xFF0F0000) | ((val & 0xF0) << 16);
        nib_latch = 0;
      }
      else
        ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0xFF000000) | (val << 16);
      break;
    case AC_Z3_REG_WR_ADDR_HI:
      if (nib_latch) {
        ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0x0FFF0000) | ((val & 0xF0) << 24);
        nib_latch = 0;
      }
      ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0x00FF0000) | (val << 24);
      done = 1;
      break;
    case AC_Z3_REG_WR_ADDR_NIB_LO:
      ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0xFFF00000) | ((val & 0xF0) << 12);
      nib_latch = 1;
      break;
    case AC_Z3_REG_WR_ADDR_NIB_HI:
      ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0xF0FF0000) | ((val & 0xF0) << 20);
      nib_latch = 1;
      break;
    case AC_Z3_REG_SHUTUP:
      //printf("Write to Z3 shutup register for PIC %d.\n", ac_z3_current_pic);
      done = 1;
      break;
    default:
      break;
  }

  if (done) {
    nib_latch = 0;
    printf("Address of Z3 autoconf RAM assigned to $%.8x\n", ac_base[ac_z3_current_pic]);
    cfg->map_offset[index] = ac_base[ac_z3_current_pic];
    ac_z3_current_pic++;
    if (ac_z3_current_pic == ac_z3_pic_count)
      ac_z3_done = 1;
  }

  return;
}

void autoconfig_write_memory_z3_16(struct emulator_config *cfg, unsigned int address_, unsigned int value) {
  int address = address_ - AC_Z3_BASE;
  int index = ac_z3_index[ac_z3_current_pic];
  unsigned short val = (unsigned short)value;
  int done = 0;

  switch(address & 0xFF) {
    case AC_Z3_REG_WR_ADDR_HI:
      // This is, as far as I know, the only regiter it should write a 16-bit value to.
      ac_base[ac_z3_current_pic] = (ac_base[ac_z3_current_pic] & 0x00000000) | (val << 16);
      done = 1;
      break;
    default:
      printf("Unknown WORD write to Z3 autoconf address $%.2X", address & 0xFF);
      //stop_cpu_emulation();
      break;
  }

  if (done) {
    printf("Address of Z3 autoconf RAM assigned to $%.8x\n", ac_base[ac_z3_current_pic]);
    cfg->map_offset[index] = ac_base[ac_z3_current_pic];
    ac_z3_current_pic++;
    if (ac_z3_current_pic == ac_z3_pic_count)
      ac_z3_done = 1;
  }

  return;
}

unsigned int autoconfig_read_memory_8(struct emulator_config *cfg, unsigned int address_) {
  unsigned char *rom = NULL;
  int address = address_ - AC_Z2_BASE;
  unsigned char val = 0;

  switch(ac_z2_type[ac_z2_current_pic]) {
    case ACTYPE_MAPFAST_Z2:
      rom = ac_fast_ram_rom;
      break;
    case ACTYPE_A314:
      rom = ac_a314_rom;
      break;
    default:
      return 0;
      break;
  }

  
  if ((address & 1) == 0 && (address / 2) < (int)sizeof(ac_fast_ram_rom)) {
    if (ac_z2_type[ac_z2_current_pic] == ACTYPE_MAPFAST_Z2 && address / 2 == 1) {
      val = get_autoconf_size(cfg->map_size[ac_z2_index[ac_z2_current_pic]]);
      if (ac_z2_current_pic + 1 < ac_z2_pic_count)
        val |= BOARDTYPE_LINKED;
    }
    else
      val = rom[address / 2];
    //printf("Read byte %d from Z2 autoconf for PIC %d (%.2X).\n", address/2, ac_z2_current_pic, val);
  }
  val <<= 4;
  if (address != 0 && address != 2 && address != 40 && address != 42)
    val ^= 0xff;
  
  return (unsigned int)val;
}

void autoconfig_write_memory_8(struct emulator_config *cfg, unsigned int address_, unsigned int value) {
  int address = address_ - AC_Z2_BASE;
  int done = 0;

  unsigned int *base = NULL;

  switch(ac_z2_type[ac_z2_current_pic]) {
    case ACTYPE_MAPFAST_Z2:
      base = &ac_base[ac_z2_current_pic];
      break;
    case ACTYPE_A314:
      //base = &a314_base;
      break;
    default:
      break;
  }

  if (!base) {
    //printf("Failed to set up the base for autoconfig PIC %d.\n", ac_z2_current_pic);
    done = 1;
  }
  else {
    if (address == 0x4a) {  // base[19:16]
      *base = (value & 0xf0) << (16 - 4);
    } else if (address == 0x48) {  // base[23:20]
      *base &= 0xff0fffff;
      *base |= (value & 0xf0) << (20 - 4);

      if (ac_z2_type[ac_z2_current_pic] == ACTYPE_MAPFAST_Z2) { // fast ram
        //a314_set_mem_base_size(*base, cfg->map_size[ac_index[ac_z2_current_pic]]);
      }
      done = 1;
    } else if (address == 0x4c) {  // shut up
      //printf("Write to Z2 shutup register for PIC %d.\n", ac_z2_current_pic);
      done = 1;
    }
  }

  if (done) {
    printf("Address of Z2 autoconf RAM assigned to $%.8x\n", ac_base[ac_z2_current_pic]);
    cfg->map_offset[ac_z2_index[ac_z2_current_pic]] = ac_base[ac_z2_current_pic];
    ac_z2_current_pic++;
    if (ac_z2_current_pic == ac_z2_pic_count)
      ac_z2_done = 1;
  }
}
