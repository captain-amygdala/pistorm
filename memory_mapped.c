#include "config_file/config_file.h"
#include "m68k.h"
#include "Gayle.h"
#include <endian.h>

#define CHKRANGE(a, b, c) a >= (unsigned int)b && a < (unsigned int)(b + c)

static unsigned int target;

extern const char *map_type_names[MAPTYPE_NUM];
const char *op_type_names[OP_TYPE_NUM] = {
  "BYTE",
  "WORD",
  "LONGWORD",
  "MEM",
};

int handle_mapped_read(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type, unsigned char mirror) {
  unsigned char *read_addr = NULL;
  char handle_regs = 0;

  //printf("Mapped read: %.8x\n", addr);

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE)
      continue;
    switch(cfg->map_type[i]) {
      case MAPTYPE_ROM:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          read_addr = cfg->map_data[i] + (addr - cfg->map_offset[i]);
        else if (cfg->map_mirror[i] != -1 && mirror && CHKRANGE(addr, cfg->map_mirror[i], cfg->map_size[i]))
          read_addr = cfg->map_data[i] + (addr - cfg->map_mirror[i]);
        break;
      case MAPTYPE_RAM:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          read_addr = cfg->map_data[i] + (addr - cfg->map_offset[i]);
        break;
      case MAPTYPE_REGISTER:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          handle_regs = 1;
        break;
    }

    if (!read_addr && !handle_regs)
      continue;
    
    if (handle_regs) {
      if (handle_register_read(addr, type, &target) != -1) {
        *val = target;
        return 1;
      }
      return -1;
    }
    else if (read_addr) {
      //printf("[PC: %.8X] Read %s from %s (%.8X) (%d)\n", m68k_get_reg(NULL, M68K_REG_PC), op_type_names[type], map_type_names[cfg->map_type[i]], addr, mirror);
      //printf("Readaddr: %.8lX (Base %.8lX\n", (uint64_t)(read_addr), (uint64_t)cfg->map_data[i]);
      switch(type) {
        case OP_TYPE_BYTE:
          *val = read_addr[0];
          //printf("Read val: %.8lX (%d)\n", (uint64_t)val, *val);
          return 1;
          break;
        case OP_TYPE_WORD:
          *val = be16toh(((unsigned short *)read_addr)[0]);
          //printf("Read val: %.8lX (%d)\n", (uint64_t)val, *val);
          return 1;
          break;
        case OP_TYPE_LONGWORD:
          *val = be32toh(((unsigned int *)read_addr)[0]);
          //printf("Read val: %.8lX (%d)\n", (uint64_t)val, *val);
          return 1;
          break;
        case OP_TYPE_MEM:
          return -1;
          break;
      }
    }
  }

  return -1;
}

int handle_mapped_write(struct emulator_config *cfg, unsigned int addr, unsigned int value, unsigned char type, unsigned char mirror) {
  unsigned char *write_addr = NULL;
  char handle_regs = 0;

  //printf("Mapped write: %.8x\n", addr);
  if (mirror) { }

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE)
      continue;
    switch(cfg->map_type[i]) {
      case MAPTYPE_ROM:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          return 1;
        break;
      case MAPTYPE_RAM:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          write_addr = cfg->map_data[i] + (addr - cfg->map_offset[i]);
        break;
      case MAPTYPE_REGISTER:
        if (CHKRANGE(addr, cfg->map_offset[i], cfg->map_size[i]))
          handle_regs = 1;
        break;
    }

    if (!write_addr && !handle_regs)
      continue;
    
    if (handle_regs) {
      return handle_register_write(addr, value, type);
    }
    else if (write_addr) {
      //printf("[PC: %.8X] Write %s to %s (%.8X) (%d)\n", m68k_get_reg(NULL, M68K_REG_PC), op_type_names[type], map_type_names[cfg->map_type[i]], addr, mirror);
      //printf("Writeaddr: %.8lX (Base %.8lX\n", (uint64_t)(write_addr), (uint64_t)cfg->map_data[i]);
      switch(type) {
        case OP_TYPE_BYTE:
          write_addr[0] = (unsigned char)value;
          //printf("Write val: %.8X (%d)\n", (uint32_t)value, value);
          return 1;
          break;
        case OP_TYPE_WORD:
          ((short *)write_addr)[0] = htobe16(value);
          //printf("Write val: %.8X (%d)\n", (uint32_t)value, value);
          return 1;
          break;
        case OP_TYPE_LONGWORD:
          ((int *)write_addr)[0] = htobe32(value);
          //printf("Write val: %.8X (%d)\n", (uint32_t)value, value);
          return 1;
          break;
        case OP_TYPE_MEM:
          return -1;
          break;
      }
    }
  }

  return -1;
}
