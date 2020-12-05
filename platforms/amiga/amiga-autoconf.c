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

int ac_current_pic = 0;
int ac_pic_count = 0;
int ac_done = 0;
int ac_type[AC_PIC_LIMIT];
int ac_index[AC_PIC_LIMIT];
unsigned int ac_base[AC_PIC_LIMIT];

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

unsigned int autoconfig_read_memory_8(struct emulator_config *cfg, unsigned int address_) {
  unsigned char *rom = NULL;
  int address = address_ - AC_BASE;
  unsigned char val = 0;

  switch(ac_type[ac_current_pic]) {
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
    if (ac_type[ac_current_pic] == ACTYPE_MAPFAST_Z2 && address / 2 == 1)
      val = get_autoconf_size(cfg->map_size[ac_index[ac_current_pic]]);
    else
      val = rom[address / 2];
    //printf("Read byte %d from autoconf for PIC %d (%.2X).\n", address/2, ac_current_pic, val);
  }
  val <<= 4;
  if (address != 0 && address != 2 && address != 40 && address != 42)
    val ^= 0xf0;
  
  return (unsigned int)val;
}

void autoconfig_write_memory_8(struct emulator_config *cfg, unsigned int address_, unsigned int value) {
  int address = address_ - AC_BASE;
  int done = 0;

  unsigned int *base = NULL;

  switch(ac_type[ac_current_pic]) {
    case ACTYPE_MAPFAST_Z2:
      base = &ac_base[ac_current_pic];
      break;
    case ACTYPE_A314:
      //base = &a314_base;
      break;
    default:
      break;
  }

  if (!base) {
    printf("Failed to set up the base for autoconfig PIC %d.\n", ac_current_pic);
    done = 1;
  }
  else {
    if (address == 0x4a) {  // base[19:16]
      *base = (value & 0xf0) << (16 - 4);
    } else if (address == 0x48) {  // base[23:20]
      *base &= 0xff0fffff;
      *base |= (value & 0xf0) << (20 - 4);

      if (ac_type[ac_current_pic] == ACTYPE_MAPFAST_Z2) { // fast ram
        //a314_set_mem_base_size(*base, cfg->map_size[ac_index[ac_current_pic]]);
      }
      done = 1;
    } else if (address == 0x4c) {  // shut up
      done = 1;
    }
  }

  if (done) {
    //printf("Address of Z2 autoconf RAM changed to %.8x\n", ac_base[ac_current_pic]);
    cfg->map_offset[ac_index[ac_current_pic]] = ac_base[ac_current_pic];
    ac_current_pic++;
    if (ac_current_pic == ac_pic_count)
      ac_done = 1;
  }
}
