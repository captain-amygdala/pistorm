// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include "pi-net.h"
#include "pi-net-enums.h"
#include "config_file/config_file.h"
#include "gpio/ps_protocol.h"

uint32_t pinet_u32[4];
static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};

void pinet_init(char *dev) {
    // Initialize them nets.
    (void)dev;
}

void pinet_shutdown() {
    // Aaahh!
}

uint8_t PI_MAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
uint8_t PI_IP[4] = { 192, 168, 1, 9 };

void handle_pinet_write(uint32_t addr, uint32_t val, uint8_t type) {
//    int32_t r;

    switch (addr & 0xFFFF) {
        case PINET_CMD_READ:
            printf("[PI-NET] Read.\n");
            break;
        case PINET_CMD_WRITE:
            printf("[PI-NET] Write.\n");
            break;
        case PINET_CMD_ADDR1:
            pinet_u32[0] = val;
            printf("[PI-NET] Write to ADDR1: %.8x\n", pinet_u32[0]);
            break;
        case PINET_CMD_ADDR2:
            pinet_u32[1] = val;
            printf("[PI-NET] Write to ADDR2: %.8x\n", pinet_u32[1]);
            break;
        case PINET_CMD_ADDR3:
            pinet_u32[2] = val;
            printf("[PI-NET] Write to ADDR3: %.8x\n", pinet_u32[2]);
            break;
        case PINET_CMD_ADDR4:
            pinet_u32[3] = val;
            printf("[PI-NET] Write to ADDR4: %.8x\n", pinet_u32[3]);
            break;
        default:
            printf("[PI-NET] Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

uint32_t handle_pinet_read(uint32_t addr_, uint8_t type) {
    uint32_t addr = addr_ & 0xFFFF;

    if (addr >= PINET_CMD_MAC && addr < PINET_CMD_IP) {
        printf("[PI-NET] Read from MAC: %.2X. (%.8X)\n", PI_MAC[addr - PINET_CMD_MAC], addr_);
        return PI_MAC[addr - PINET_CMD_MAC];
    }
    if (addr >= PINET_CMD_IP && addr < PINET_CMD_BEEF) {
        printf("[PI-NET] Read from IP: %.2X. (%.8X)\n", PI_IP[addr - PINET_CMD_IP], addr_);
        return PI_IP[addr - PINET_CMD_IP];
    }

    switch (addr & 0xFFFF) {
        default:
            printf("[PI-NET] Unhandled %s register read from %.8X\n", op_type_names[type], addr);
            break;
    }

    return 0;
}
