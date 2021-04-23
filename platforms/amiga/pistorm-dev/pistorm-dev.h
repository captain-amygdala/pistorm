// SPDX-License-Identifier: MIT

#include <stdint.h>

void handle_pistorm_dev_write(uint32_t addr, uint32_t val, uint8_t type);
uint32_t handle_pistorm_dev_read(uint32_t addr, uint8_t type);
char *get_pistorm_devcfg_filename();
