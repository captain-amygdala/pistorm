// SPDX-License-Identifier: MIT

#include "config_file/config_file.h"

enum base_platforms {
    PLATFORM_NONE,
    PLATFORM_AMIGA,
    PLATFORM_MAC,
    PLATFORM_X68000,
    PLATFORM_NUM,
};

struct platform_config *make_platform_config(char *name, char *subsys);

void dump_range_to_file(uint32_t addr, uint32_t size, char *filename);
uint8_t *dump_range_to_memory(uint32_t addr, uint32_t size);
