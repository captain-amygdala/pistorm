// SPDX-License-Identifier: MIT

#include "pistorm-dev.h"
#include "pistorm-dev-enums.h"
#include <stdio.h>

#define DEBUG_PISTORM_DEVICE

#ifdef DEBUG_PISTORM_DEVICE
#define DEBUG printf

static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};
#else
#define DEBUG(...)
#endif

extern uint32_t pistorm_dev_base;
extern uint32_t do_reset;

void handle_pistorm_dev_write(uint32_t addr, uint32_t val, uint8_t type) {
    switch((addr & 0xFFFF)) {
        case PI_CMD_RESET:
            DEBUG("[PISTORM-DEV] System reset called through PiStorm interaction device, code %.4X\n", (val & 0xFFFF));
            do_reset = 1;
            break;
        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register write to %.4X: %d\n", op_type_names[type], addr - pistorm_dev_base, val);
            break;
    }
}
 
uint32_t handle_pistorm_dev_read(uint32_t addr, uint8_t type) {
    switch((addr & 0xFFFF)) {

        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register read from %.4X\n", op_type_names[type], addr - pistorm_dev_base);
            break;
    }
    return 0;
}
