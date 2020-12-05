#define AC_BASE 0xE80000
#define AC_SIZE (64 * 1024)
#define AC_PIC_LIMIT 8

#define AC_MEM_SIZE_8MB 0
#define AC_MEM_SIZE_64KB 1
#define AC_MEM_SIZE_128KB 2
#define AC_MEM_SIZE_256KB 3
#define AC_MEM_SIZE_512KB 4
#define AC_MEM_SIZE_1MB 5
#define AC_MEM_SIZE_2MB 6
#define AC_MEM_SIZE_4MB 7

enum autoconf_types {
    ACTYPE_MAPFAST_Z2,
    ACTYPE_MAPFAST_Z3,
    ACTYPE_A314,
    ACTYPE_NUM,
};

unsigned int autoconfig_read_memory_8(struct emulator_config *cfg, unsigned int address);
void autoconfig_write_memory_8(struct emulator_config *cfg, unsigned int address, unsigned int value);
