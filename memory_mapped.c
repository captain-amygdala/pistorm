#include "config_file/config_file.h"
#include "m68k.h"
#include "Gayle.h"
#include <endian.h>

#define CHKRANGE(a, b, c) a >= (unsigned int)b && a < (unsigned int)(b + c)
#define CHKRANGE_ABS(a, b, c) a >= (unsigned int)b && a < (unsigned int) c

static unsigned int target;
extern int ovl;

extern const char *map_type_names[MAPTYPE_NUM];
const char *op_type_names[OP_TYPE_NUM] = {
  "BYTE",
  "WORD",
  "LONGWORD",
  "MEM",
};

inline int handle_mapped_read(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type) {
  unsigned char *read_addr = NULL;

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE)
      continue;
    else if (ovl && cfg->map_type[i] == MAPTYPE_ROM) {
      if (cfg->map_mirror[i] != -1 && CHKRANGE_ABS(addr, cfg->map_mirror[i], cfg->map_high[i])) {
        read_addr = cfg->map_data[i] + ((addr - cfg->map_mirror[i]) % cfg->rom_size[i]);
        goto read_value;
      }
    }
    else if (CHKRANGE_ABS(addr, cfg->map_offset[i], cfg->map_high[i])) {
      switch(cfg->map_type[i]) {
        case MAPTYPE_ROM:
          read_addr = cfg->map_data[i] + ((addr - cfg->map_offset[i]) % cfg->rom_size[i]);
          goto read_value;
          break;
        case MAPTYPE_RAM:
          read_addr = cfg->map_data[i] + (addr - cfg->map_offset[i]);
          goto read_value;
          break;
        case MAPTYPE_REGISTER:
          if (cfg->platform && cfg->platform->register_read) {
            if (cfg->platform->register_read(addr, type, &target) != -1) {
              *val = target;
              return 1;
            }
          }
          return -1;
          break;
      }
    }
  }

  return -1;

read_value:;
  //printf("Read value from %.8X\n", addr);
  switch(type) {
    case OP_TYPE_BYTE:
      *val = read_addr[0];
      return 1;
      break;
    case OP_TYPE_WORD:
      *val = be16toh(((unsigned short *)read_addr)[0]);
      return 1;
      break;
    case OP_TYPE_LONGWORD:
      *val = be32toh(((unsigned int *)read_addr)[0]);
      return 1;
      break;
    case OP_TYPE_MEM:
      return -1;
      break;
  }

  return 1;
}

inline int handle_mapped_write(struct emulator_config *cfg, unsigned int addr, unsigned int value, unsigned char type) {
  unsigned char *write_addr = NULL;

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE)
      continue;
    else if (CHKRANGE_ABS(addr, cfg->map_offset[i], cfg->map_high[i])) {
      switch(cfg->map_type[i]) {
        case MAPTYPE_ROM:
          return 1;
          break;
        case MAPTYPE_RAM:
          write_addr = cfg->map_data[i] + (addr - cfg->map_offset[i]);
          goto write_value;
          break;
        case MAPTYPE_REGISTER:
          if (cfg->platform && cfg->platform->register_write) {
            return cfg->platform->register_write(addr, value, type);
          }
          break;
      }
    }
  }

  return -1;

write_value:;
  //printf("Write value to %.8X\n", addr);
  switch(type) {
    case OP_TYPE_BYTE:
      write_addr[0] = (unsigned char)value;
      return 1;
      break;
    case OP_TYPE_WORD:
      ((short *)write_addr)[0] = htobe16(value);
      return 1;
      break;
    case OP_TYPE_LONGWORD:
      ((int *)write_addr)[0] = htobe32(value);
      return 1;
      break;
    case OP_TYPE_MEM:
      return -1;
      break;
  }

  return 1;
}
