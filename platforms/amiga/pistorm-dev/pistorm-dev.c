// SPDX-License-Identifier: MIT

#include "pistorm-dev.h"
#include "pistorm-dev-enums.h"
#include <stdio.h>

#define DEBUG_PISTORM_DEVICE

#ifdef DEBUG_PISTORM_DEVICE
#define DEBUG printf

#define PIDEV_SWREV 0x0105

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

extern uint8_t rtg_enabled, rtg_on, pinet_enabled, piscsi_enabled;

void handle_pistorm_dev_write(uint32_t addr, uint32_t val, uint8_t type) {
    switch((addr & 0xFFFF)) {
        case PI_CMD_RESET:
            DEBUG("[PISTORM-DEV] System reset called through PiStorm interaction device, code %d\n", (val & 0xFFFF));
            do_reset = 1;
            break;
        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register write to %.4X: %d\n", op_type_names[type], addr - pistorm_dev_base, val);
            break;
    }
}
 
uint32_t handle_pistorm_dev_read(uint32_t addr, uint8_t type) {
    switch((addr & 0xFFFF)) {
        case PI_CMD_HWREV:
            // Probably replace this with some read from the CPLD to get a simple hardware revision.
            DEBUG("[PISTORM-DEV] %s Read from HWREV\n", op_type_names[type]);
            return 0x0101; // 1.1
            break;
        case PI_CMD_SWREV:
            DEBUG("[PISTORM-DEV] %s Read from SWREV\n", op_type_names[type]);
            return PIDEV_SWREV;
            break;
        case PI_CMD_RTGSTATUS:
            DEBUG("[PISTORM-DEV] %s Read from RTGSTATUS\n", op_type_names[type]);
            return (rtg_on << 1) | rtg_enabled;
            break;
        case PI_CMD_NETSTATUS:
            DEBUG("[PISTORM-DEV] %s Read from NETSTATUS\n", op_type_names[type]);
            return pinet_enabled;
            break;
        case PI_CMD_PISCSI_CTRL:
            DEBUG("[PISTORM-DEV] %s Read from PISCSI_CTRL\n", op_type_names[type]);
            return piscsi_enabled;
            break;
        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register read from %.4X\n", op_type_names[type], addr - pistorm_dev_base);
            break;
    }
    return 0;
}
