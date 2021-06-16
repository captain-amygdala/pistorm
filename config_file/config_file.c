// SPDX-License-Identifier: MIT

#include "platforms/platforms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rominfo.h"

#define M68K_CPU_TYPES M68K_CPU_TYPE_SCC68070

const char *cpu_types[M68K_CPU_TYPES] = {
  "68000",
  "68010",
  "68EC020",
  "68020",
  "68EC030",
  "68030",
  "68EC040",
  "68LC040",
  "68040",
  "SCC68070",
};

const char *map_type_names[MAPTYPE_NUM] = {
  "NONE",
  "rom",
  "ram",
  "register",
  "ram (no alloc)",
};

const char *config_item_names[CONFITEM_NUM] = {
  "NONE",
  "cpu",
  "map",
  "loopcycles",
  "mouse",
  "keyboard",
  "platform",
  "setvar",
  "kbfile",
};

const char *mapcmd_names[MAPCMD_NUM] = {
  "NONE",
  "type",
  "address",
  "size",
  "range",
  "file",
  "ovl",
  "id",
  "autodump_file",
  "autodump_mem",
};

int get_config_item_type(char *cmd) {
  for (int i = 0; i < CONFITEM_NUM; i++) {
    if (strcmp(cmd, config_item_names[i]) == 0) {
      return i;
    }
  }

  return CONFITEM_NONE;
}

unsigned int get_m68k_cpu_type(char *name) {
  for (int i = 0; i < M68K_CPU_TYPES; i++) {
    if (strcmp(name, cpu_types[i]) == 0) {
      printf("[CFG] Set CPU type to %s.\n", cpu_types[i]);
      return i + 1;
    }
  }

  printf("[CFG] Invalid CPU type %s specified, defaulting to 68000.\n", name);
  return M68K_CPU_TYPE_68000;
}

unsigned int get_map_cmd(char *name) {
  for (int i = 1; i < MAPCMD_NUM; i++) {
    if (strcmp(name, mapcmd_names[i]) == 0) {
      return i;
    }
  }

  return MAPCMD_UNKNOWN;
}

unsigned int get_map_type(char *name) {
  for (int i = 1; i < MAPTYPE_NUM; i++) {
    if (strcmp(name, map_type_names[i]) == 0) {
      return i;
    }
  }

  return MAPTYPE_NONE;
}

void trim_whitespace(char *str) {
  while (strlen(str) != 0 && (str[strlen(str) - 1] == ' ' || str[strlen(str) - 1] == '\t' || str[strlen(str) - 1] == 0x0A || str[strlen(str) - 1] == 0x0D)) {
    str[strlen(str) - 1] = '\0';
  }
}

unsigned int get_int(char *str) {
  if (strlen(str) == 0)
    return -1;

  int ret_int = 0;

  if (strlen(str) > 2 && str[0] == '0' && str[1] == 'x') {
    for (int i = 2; i < (int)strlen(str); i++) {
      if (str[i] >= '0' && str[i] <= '9') {
        ret_int = (str[i] - '0') | (ret_int << 4);
      }
      else {
        switch(str[i]) {
          case 'A': ret_int = 0xA | (ret_int << 4); break;
          case 'B': ret_int = 0xB | (ret_int << 4); break;
          case 'C': ret_int = 0xC | (ret_int << 4); break;
          case 'D': ret_int = 0xD | (ret_int << 4); break;
          case 'E': ret_int = 0xE | (ret_int << 4); break;
          case 'F': ret_int = 0xF | (ret_int << 4); break;
          case 'K': ret_int = ret_int * SIZE_KILO; break;
          case 'M': ret_int = ret_int * SIZE_MEGA; break;
          case 'G': ret_int = ret_int * SIZE_GIGA; break;
          default:
            printf("[CFG] Unknown character %c in hex value.\n", str[i]);
            break;
        }
      }
    }
    return ret_int;
  }
  else {
    ret_int = atoi(str);
    if (str[strlen(str) - 1] == 'K')
      ret_int = ret_int * SIZE_KILO;
    else if (str[strlen(str) - 1] == 'M')
      ret_int = ret_int * SIZE_MEGA;
    else if (str[strlen(str) - 1] == 'G')
      ret_int = ret_int * SIZE_GIGA;

    return ret_int;
  }
}

void get_next_string(char *str, char *str_out, int *strpos, char separator) {
  int str_pos = 0, out_pos = 0, startquote = 0, endstring = 0;

  if (!str_out)
    return;

  if (strpos)
    str_pos = *strpos;

  while ((str[str_pos] == ' ' || str[str_pos] == '\t') && str_pos < (int)strlen(str)) {
    str_pos++;
  }

  if (str[str_pos] == '\"') {
    str_pos++;
    startquote = 1;
  }


  for (int i = str_pos; i < (int)strlen(str); i++) {
    str_out[out_pos] = str[i];

    if (startquote) {
      if (str[i] == '\"')
        endstring = 1;
    } else {
      if ((separator == ' ' && (str[i] == ' ' || str[i] == '\t')) || str[i] == separator) {
        endstring = 1;
      }
    }

    if (endstring) {
      str_out[out_pos] = '\0';
      if (strpos) {
        *strpos = i + 1;
      }
      break;
    }

    out_pos++;
    if (i + 1 == (int)strlen(str) && strpos) {
      *strpos = i + 1;
      str_out[out_pos] = '\0';
    }
  }
}

void add_mapping(struct emulator_config *cfg, unsigned int type, unsigned int addr, unsigned int size, int mirr_addr, char *filename, char *map_id, unsigned int autodump) {
  unsigned int index = 0, file_size = 0;
  FILE *in = NULL;

  while (index < MAX_NUM_MAPPED_ITEMS) {
    if (cfg->map_type[index] == MAPTYPE_NONE)
      break;
    index++;
  }
  if (index == MAX_NUM_MAPPED_ITEMS) {
    printf("[CFG] Unable to map item, only %d items can be mapped with current binary.\n", MAX_NUM_MAPPED_ITEMS);
    return;
  }

  cfg->map_type[index] = type;
  cfg->map_offset[index] = addr;
  cfg->map_size[index] = size;
  cfg->map_high[index] = addr + size;
  cfg->map_mirror[index] = mirr_addr;
  if (strlen(map_id)) {
    cfg->map_id[index] = (char *)malloc(strlen(map_id) + 1);
    strcpy(cfg->map_id[index], map_id);
  }

  switch(type) {
    case MAPTYPE_RAM_NOALLOC:
      printf("[CFG] Adding %d byte (%d MB) RAM mapping %s...\n", size, size / 1024 / 1024, map_id);
      cfg->map_data[index] = (unsigned char *)filename;
      break;
    case MAPTYPE_RAM:
      printf("[CFG] Allocating %d bytes for RAM mapping (%d MB)...\n", size, size / 1024 / 1024);
      cfg->map_data[index] = (unsigned char *)malloc(size);
      if (!cfg->map_data[index]) {
        printf("[CFG] ERROR: Unable to allocate memory for mapped RAM!\n");
        goto mapping_failed;
      }
      memset(cfg->map_data[index], 0x00, size);
      break;
    case MAPTYPE_ROM:
      in = fopen(filename, "rb");
      if (!in) {
        if (!autodump) {
          printf("[CFG] Failed to open file %s for ROM mapping. Using onboard ROM instead, if available.\n", filename);
          goto mapping_failed;
        } else if (autodump == MAPCMD_AUTODUMP_FILE) {
          printf("[CFG] Could not open file %s for ROM mapping. Autodump flag is set, dumping to file.\n", filename);
          dump_range_to_file(cfg->map_offset[index], cfg->map_size[index], filename);
          in = fopen(filename, "rb");
          if (in == NULL) {
            printf("[CFG] Could not open dumped file for reading. Using onboard ROM instead, if available.\n");
            goto mapping_failed;
          }
        } else if (autodump == MAPCMD_AUTODUMP_MEM) {
          printf("[CFG] Could not open file %s for ROM mapping. Autodump flag is set, dumping to memory.\n", filename);
          cfg->map_data[index] = dump_range_to_memory(cfg->map_offset[index], cfg->map_size[index]);
          cfg->rom_size[index] = cfg->map_size[index];
          if (cfg->map_data[index] == NULL) {
            printf("[CFG] Could not dump range to memory. Using onboard ROM instead, if available.\n");
            goto mapping_failed;
          }
          goto skip_file_ops;
        }
      }
      fseek(in, 0, SEEK_END);
      file_size = (int)ftell(in);
      if (size == 0) {
        cfg->map_size[index] = file_size;
        cfg->map_high[index] = addr + cfg->map_size[index];
      }
      fseek(in, 0, SEEK_SET);
      cfg->map_data[index] = (unsigned char *)calloc(1, cfg->map_size[index]);
      cfg->rom_size[index] = (cfg->map_size[index] <= file_size) ? cfg->map_size[index] : file_size;
      if (!cfg->map_data[index]) {
        printf("[CFG] ERROR: Unable to allocate memory for mapped ROM!\n");
        goto mapping_failed;
      }
      memset(cfg->map_data[index], 0x00, cfg->map_size[index]);
      fread(cfg->map_data[index], cfg->rom_size[index], 1, in);
      if (in)
        fclose(in);
skip_file_ops:
      displayRomInfo(cfg->map_data[index], cfg->rom_size[index]);
      if (cfg->map_size[index] == cfg->rom_size[index])
        m68k_add_rom_range(cfg->map_offset[index], cfg->map_high[index], cfg->map_data[index]);
      break;
    case MAPTYPE_REGISTER:
    default:
      break;
  }

  printf("[CFG] [MAP %d] Added %s mapping for range %.8lX-%.8lX ID: %s\n", index, map_type_names[type], cfg->map_offset[index], cfg->map_high[index] - 1, cfg->map_id[index] ? cfg->map_id[index] : "None");

  return;

  mapping_failed:;
  cfg->map_type[index] = MAPTYPE_NONE;
  if (in)
    fclose(in);
}

void free_config_file(struct emulator_config *cfg) {
  if (!cfg) {
    printf("[CFG] Tried to free NULL config, aborting.\n");
  }

  if (cfg->platform) {
    cfg->platform->shutdown(cfg);
    free(cfg->platform);
    cfg->platform = NULL;
  }

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_data[i]) {
      if (cfg->map_type[i] != MAPTYPE_RAM_NOALLOC) {
        free(cfg->map_data[i]);
      }
      cfg->map_data[i] = NULL;
    }
    if (cfg->map_id[i]) {
      free(cfg->map_id[i]);
      cfg->map_id[i] = NULL;
    }
  }

  if (cfg->mouse_file) {
    free(cfg->mouse_file);
    cfg->mouse_file = NULL;
  }
  if (cfg->keyboard_file) {
    free(cfg->keyboard_file);
    cfg->keyboard_file = NULL;
  }

  m68k_clear_ranges();

  printf("[CFG] Config file freed. Maybe.\n");
}

struct emulator_config *load_config_file(char *filename) {
  FILE *in = fopen(filename, "rb");
  if (in == NULL) {
    printf("[CFG] Failed to open config file %s for reading.\n", filename);
    return NULL;
  }

  char *parse_line = NULL;
  char cur_cmd[128];
  struct emulator_config *cfg = NULL;
  int cur_line = 1;

  parse_line = (char *)calloc(1, 512);
  if (!parse_line) {
    printf("[CFG] Failed to allocate memory for config file line buffer.\n");
    return NULL;
  }
  cfg = (struct emulator_config *)calloc(1, sizeof(struct emulator_config));
  if (!cfg) {
    printf("[CFG] Failed to allocate memory for temporary emulator config.\n");
    goto load_failed;
  }

  memset(cfg, 0x00, sizeof(struct emulator_config));
  cfg->cpu_type = M68K_CPU_TYPE_68000;

  while (!feof(in)) {
    int str_pos = 0;
    memset(parse_line, 0x00, 512);
    fgets(parse_line, 512, in);

    if (strlen(parse_line) <= 2 || parse_line[0] == '#' || parse_line[0] == '/')
      goto skip_line;

    trim_whitespace(parse_line);

    get_next_string(parse_line, cur_cmd, &str_pos, ' ');

    switch (get_config_item_type(cur_cmd)) {
      case CONFITEM_CPUTYPE:
        cfg->cpu_type = get_m68k_cpu_type(parse_line + str_pos);
        break;
      case CONFITEM_MAP: {
        unsigned int maptype = 0, mapsize = 0, mapaddr = 0, autodump = 0;
        unsigned int mirraddr = ((unsigned int)-1);
        char mapfile[128], mapid[128];
        memset(mapfile, 0x00, 128);
        memset(mapid, 0x00, 128);

        while (str_pos < (int)strlen(parse_line)) {
          get_next_string(parse_line, cur_cmd, &str_pos, '=');
          switch(get_map_cmd(cur_cmd)) {
            case MAPCMD_TYPE:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              maptype = get_map_type(cur_cmd);
              //printf("Type! %s\n", map_type_names[maptype]);
              break;
            case MAPCMD_ADDRESS:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              mapaddr = get_int(cur_cmd);
              //printf("Address! %.8X\n", mapaddr);
              break;
            case MAPCMD_SIZE:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              mapsize = get_int(cur_cmd);
              //printf("Size! %.8X\n", mapsize);
              break;
            case MAPCMD_RANGE:
              get_next_string(parse_line, cur_cmd, &str_pos, '-');
              mapaddr = get_int(cur_cmd);
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              mapsize = get_int(cur_cmd) - 1 - mapaddr;
              //printf("Range! %d-%d\n", mapaddr, mapaddr + mapsize);
              break;
            case MAPCMD_FILENAME:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              strcpy(mapfile, cur_cmd);
              //printf("File! %s\n", mapfile);
              break;
            case MAPCMD_MAP_ID:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              strcpy(mapid, cur_cmd);
              //printf("File! %s\n", mapfile);
              break;
            case MAPCMD_OVL_REMAP:
              get_next_string(parse_line, cur_cmd, &str_pos, ' ');
              mirraddr = get_int(cur_cmd);
              break;
            case MAPCMD_AUTODUMP_FILE:
            case MAPCMD_AUTODUMP_MEM:
              autodump = get_map_cmd(cur_cmd);
              break;
            default:
              printf("[CFG] Unknown/unhandled map argument %s on line %d.\n", cur_cmd, cur_line);
              break;
          }
        }
        add_mapping(cfg, maptype, mapaddr, mapsize, mirraddr, mapfile, mapid, autodump);

        break;
      }
      case CONFITEM_LOOPCYCLES:
        cfg->loop_cycles = get_int(parse_line + str_pos);
        printf("[CFG] Set CPU loop cycles to %d.\n", cfg->loop_cycles);
        break;
      case CONFITEM_MOUSE:
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->mouse_file = (char *)calloc(1, strlen(cur_cmd) + 1);
        strcpy(cfg->mouse_file, cur_cmd);
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->mouse_toggle_key = cur_cmd[0];
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->mouse_autoconnect = (strcmp(cur_cmd, "autoconnect") == 0) ? 1 : 0;
        cfg->mouse_enabled = 1;
        printf("[CFG] Enabled mouse event forwarding from file %s, toggle key %c.\n", cfg->mouse_file, cfg->mouse_toggle_key);
        break;
      case CONFITEM_KEYBOARD:
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->keyboard_toggle_key = cur_cmd[0];
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->keyboard_grab = (strcmp(cur_cmd, "grab") == 0) ? 1 : 0;
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->keyboard_autoconnect = (strcmp(cur_cmd, "autoconnect") == 0) ? 1 : 0;
        printf("[CFG] Enabled keyboard event forwarding, toggle key %c", cfg->keyboard_toggle_key);
        if (cfg->keyboard_grab)
          printf(", locking from host when connected");
        if (cfg->keyboard_autoconnect)
          printf(", connected to guest at startup");
        printf(".\n");
        break;
      case CONFITEM_KBFILE:
        get_next_string(parse_line, cur_cmd, &str_pos, ' ');
        cfg->keyboard_file = (char *)calloc(1, strlen(cur_cmd) + 1);
        strcpy(cfg->keyboard_file, cur_cmd);
        printf("[CFG] Set keyboard event source file to %s.\n", cfg->keyboard_file);
        break;
      case CONFITEM_PLATFORM: {
        char platform_name[128], platform_sub[128];
        memset(platform_name, 0x00, 128);
        memset(platform_sub, 0x00, 128);
        get_next_string(parse_line, platform_name, &str_pos, ' ');
        printf("[CFG] Setting platform to %s", platform_name);
        get_next_string(parse_line, platform_sub, &str_pos, ' ');
        if (strlen(platform_sub))
          printf(" (sub: %s)", platform_sub);
        printf("\n");
        cfg->platform = make_platform_config(platform_name, platform_sub);
        break;
      }
      case CONFITEM_SETVAR: {
        if (!cfg->platform) {
          printf("[CFG] Warning: setvar used in config file with no platform specified.\n");
          break;
        }

        char var_name[128], var_value[128];
        memset(var_name, 0x00, 128);
        memset(var_value, 0x00, 128);
        get_next_string(parse_line, var_name, &str_pos, ' ');
        get_next_string(parse_line, var_value, &str_pos, ' ');
        cfg->platform->setvar(cfg, var_name, var_value);

        break;
      }
      case CONFITEM_NONE:
      default:
        printf("[CFG] Unknown config item %s on line %d.\n", cur_cmd, cur_line);
        break;
    }

  skip_line:
    cur_line++;
  }
  goto load_successful;

  load_failed:;
  if (cfg) {
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
      if (cfg->map_data[i])
        free(cfg->map_data[i]);
      cfg->map_data[i] = NULL;
    }
    free(cfg);
    cfg = NULL;
  }
  load_successful:;
  if (parse_line)
    free(parse_line);

  return cfg;
}

int get_named_mapped_item(struct emulator_config *cfg, char *name) {
  if (strlen(name) == 0)
    return -1;

  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE || !cfg->map_id[i])
      continue;
    if (strcmp(name, cfg->map_id[i]) == 0)
      return i;
  }

  return -1;
}

int get_mapped_item_by_address(struct emulator_config *cfg, uint32_t address) {
  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE || !cfg->map_data[i])
      continue;
    else if (address >= cfg->map_offset[i] && address < cfg->map_high[i]) {
      if (cfg->map_type[i] == MAPTYPE_RAM || cfg->map_type[i] == MAPTYPE_RAM_NOALLOC || cfg->map_type[i] == MAPTYPE_ROM)
        return i;
    }
  }

  return -1;
}

uint8_t *get_mapped_data_pointer_by_address(struct emulator_config *cfg, uint32_t address) {
  for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
    if (cfg->map_type[i] == MAPTYPE_NONE || !cfg->map_data[i])
      continue;
    else if (address >= cfg->map_offset[i] && address < cfg->map_high[i]) {
      if (cfg->map_type[i] == MAPTYPE_RAM || cfg->map_type[i] == MAPTYPE_RAM_NOALLOC || cfg->map_type[i] == MAPTYPE_ROM)
        return cfg->map_data[i] + (address - cfg->map_offset[i]);
    }
  }

  return NULL;
}
