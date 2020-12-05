#include "../m68k.h"

#define MAX_NUM_MAPPED_ITEMS 8
#define SIZE_KILO 1024
#define SIZE_MEGA (1024 * 1024)
#define SIZE_GIGA (1024 * 1024 * 1024)

typedef enum {
  MAPTYPE_NONE,
  MAPTYPE_ROM,
  MAPTYPE_RAM,
  MAPTYPE_REGISTER,
  MAPTYPE_NUM,
} map_types;

typedef enum {
  MAPCMD_UNKNOWN,
  MAPCMD_TYPE,
  MAPCMD_ADDRESS,
  MAPCMD_SIZE,
  MAPCMD_RANGE,
  MAPCMD_FILENAME,
  MAPCMD_OVL_REMAP,
  MAPCMD_MAP_ID,
  MAPCMD_NUM,
} map_cmds;

typedef enum {
  CONFITEM_NONE,
  CONFITEM_CPUTYPE,
  CONFITEM_MAP,
  CONFITEM_LOOPCYCLES,
  CONFITEM_MOUSE,
  CONFITEM_KEYBOARD,
  CONFITEM_PLATFORM,
  CONFITEM_NUM,
} config_items;

typedef enum {
  OP_TYPE_BYTE,
  OP_TYPE_WORD,
  OP_TYPE_LONGWORD,
  OP_TYPE_MEM,
  OP_TYPE_NUM,
} map_op_types;

struct emulator_config {
  unsigned int cpu_type;

  unsigned char map_type[MAX_NUM_MAPPED_ITEMS];
  long map_offset[MAX_NUM_MAPPED_ITEMS];
  unsigned int map_size[MAX_NUM_MAPPED_ITEMS];
  unsigned int rom_size[MAX_NUM_MAPPED_ITEMS];
  unsigned char *map_data[MAX_NUM_MAPPED_ITEMS];
  int map_mirror[MAX_NUM_MAPPED_ITEMS];
  char *map_id[MAX_NUM_MAPPED_ITEMS];

  struct platform_config *platform;

  char *mouse_file;

  char mouse_toggle_key, keyboard_toggle_key;
  unsigned char mouse_enabled, keyboard_enabled;

  unsigned int loop_cycles;
};

struct platform_config {
  char *subsys;

  int (*custom_read)(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type);
  int (*custom_write)(struct emulator_config *cfg, unsigned int addr, unsigned int val, unsigned char type);

  int (*register_read)(unsigned int addr, unsigned char type, unsigned int *val);
  int (*register_write)(unsigned int addr, unsigned int value, unsigned char type);

  int (*platform_initial_setup)(struct emulator_config *cfg);
  void (*setvar)(char *var, char *val);
};

unsigned int get_m68k_cpu_type(char *name);
struct emulator_config *load_config_file(char *filename);

int handle_mapped_read(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type, unsigned char mirror);
int handle_mapped_write(struct emulator_config *cfg, unsigned int addr, unsigned int value, unsigned char type, unsigned char mirror);
int get_named_mapped_item(struct emulator_config *cfg, char *name);
