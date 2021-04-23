// SPDX-License-Identifier: MIT

#include "pistorm-dev.h"
#include "pistorm-dev-enums.h"
#include "platforms/platforms.h"
#include "gpio/ps_protocol.h"

#include <stdio.h>
#include <string.h>

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

extern uint8_t rtg_enabled, rtg_on, pinet_enabled, piscsi_enabled, load_new_config;
extern struct emulator_config *cfg;

char cfg_filename[256];

static uint8_t pi_byte[8];
static uint16_t pi_word[4];
static uint32_t pi_longword[4];
static uint32_t pi_string[4];

static uint32_t pi_dbg_val[4];
static uint32_t pi_dbg_string[4];

static uint32_t pi_cmd_result = 0;

int32_t grab_amiga_string(uint32_t addr, uint8_t *dest, uint32_t str_max_len) {
    int32_t r = get_mapped_item_by_address(cfg, addr);
    uint32_t index = 0;

    if (r == -1) {
        DEBUG("[GRAB_AMIGA_STRING] No mapped range found for address $%.8X. Grabbing string data over the bus.\n", addr);
        do {
            dest[index] = read8(addr + index);
            index++;
        } while (dest[index - 1] != 0x00 && index < str_max_len);
    }
    else {
        uint8_t *src = cfg->map_data[r] + (addr - cfg->map_offset[r]);
        do {
            dest[index] = src[index];
            index++;
        } while (dest[index - 1] != 0x00 && index < str_max_len);
    }
    if (index == str_max_len) {
        memset(dest, 0x00, str_max_len + 1);
        return -1;
    }
    DEBUG("[GRAB_AMIGA_STRING] Grabbed string: %s\n", dest);
    return (int32_t)strlen((const char*)dest);
}

char *get_pistorm_devcfg_filename() {
    return cfg_filename;
}

void handle_pistorm_dev_write(uint32_t addr_, uint32_t val, uint8_t type) {
    uint32_t addr = (addr_ & 0xFFFF);

    switch((addr)) {
        case PI_DBG_MSG:
            // Output debug message based on value written and data in val/str registers.
            break;
        case PI_DBG_VAL1: case PI_DBG_VAL2: case PI_DBG_VAL3: case PI_DBG_VAL4:
        case PI_DBG_VAL5: case PI_DBG_VAL6: case PI_DBG_VAL7: case PI_DBG_VAL8:
            DEBUG("[PISTORM-DEV] Set DEBUG VALUE %d to %d ($%.8X)\n", (addr - PI_DBG_VAL1) / 4, val, val);
            pi_dbg_val[(addr - PI_DBG_VAL1) / 4] = val;
            break;
        case PI_DBG_STR1: case PI_DBG_STR2: case PI_DBG_STR3: case PI_DBG_STR4:
            DEBUG("[PISTORM-DEV] Set DEBUG STRING POINTER %d to $%.8X\n", (addr - PI_DBG_STR1) / 4, val);
            pi_dbg_string[(addr - PI_DBG_STR1) / 4] = val;
            break;

        case PI_BYTE1: case PI_BYTE2: case PI_BYTE3: case PI_BYTE4:
        case PI_BYTE5: case PI_BYTE6: case PI_BYTE7: case PI_BYTE8:
            DEBUG("[PISTORM-DEV] Set BYTE %d to %d ($%.2X)\n", addr - PI_BYTE1, (val & 0xFF), (val & 0xFF));
            pi_byte[addr - PI_BYTE1] = (val & 0xFF);
            break;
        case PI_WORD1: case PI_WORD2: case PI_WORD3: case PI_WORD4:
            DEBUG("[PISTORM-DEV] Set WORD %d to %d ($%.4X)\n", (addr - PI_WORD1) / 2, (val & 0xFFFF), (val & 0xFFFF));
            pi_word[(addr - PI_WORD1) / 2] = (val & 0xFFFF);
            break;
        case PI_LONGWORD1: case PI_LONGWORD2: case PI_LONGWORD3: case PI_LONGWORD4:
            DEBUG("[PISTORM-DEV] Set LONGWORD %d to %d ($%.8X)\n", (addr - PI_LONGWORD1) / 4, val, val);
            pi_longword[(addr - PI_LONGWORD1) / 4] = val;
            break;
        case PI_STR1: case PI_STR2: case PI_STR3: case PI_STR4:
            DEBUG("[PISTORM-DEV] Set STRING POINTER %d to $%.8X\n", (addr - PI_STR1) / 4, val);
            pi_string[(addr - PI_STR1) / 4] = val;
            break;

        case PI_CMD_RESET:
            DEBUG("[PISTORM-DEV] System reset called, code %d\n", (val & 0xFFFF));
            do_reset = 1;
            break;
        case PI_CMD_SWITCHCONFIG:
            DEBUG("[PISTORM-DEV] Config switch called, command: ");
            switch (val) {
                case PICFG_LOAD:
                    DEBUG("LOAD\n");
                    if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)cfg_filename, 255) == -1) {
                        printf("[PISTORM-DEV] Failed to grab string for config filename. Aborting.\n");
                    } else {
                        FILE *tmp = fopen(cfg_filename, "rb");
                        if (tmp == NULL) {
                            printf("[PISTORM-DEV] Failed to open config file %s for reading. Aborting.\n", cfg_filename);
                        } else {
                            printf("[PISTORM-DEV] Attempting to load config file %s...\n", cfg_filename);
                            load_new_config = 1;
                        }
                    }
                    pi_string[0] = 0;
                    break;
                case PICFG_RELOAD:
                    DEBUG("RELOAD\n");
                    break;
                case PICFG_DEFAULT:
                    DEBUG("DEFAULT\n");
                    break;
                default:
                    DEBUG("UNKNOWN. Command ignored.\n");
                    break;
            }
            break;
        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register write to %.4X: %d\n", op_type_names[type], addr - pistorm_dev_base, val);
            break;
    }
}
 
uint32_t handle_pistorm_dev_read(uint32_t addr_, uint8_t type) {
    uint32_t addr = (addr_ & 0xFFFF);

    switch((addr)) {
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

        case PI_DBG_VAL1: case PI_DBG_VAL2: case PI_DBG_VAL3: case PI_DBG_VAL4:
        case PI_DBG_VAL5: case PI_DBG_VAL6: case PI_DBG_VAL7: case PI_DBG_VAL8:
            DEBUG("[PISTORM-DEV] Read DEBUG VALUE %d (%d / $%.8X)\n", (addr - PI_DBG_VAL1) / 4, pi_dbg_val[(addr - PI_DBG_VAL1) / 4], pi_dbg_val[(addr - PI_DBG_VAL1) / 4]);
            return pi_dbg_val[(addr - PI_DBG_VAL1) / 4];
            break;

        case PI_BYTE1: case PI_BYTE2: case PI_BYTE3: case PI_BYTE4:
        case PI_BYTE5: case PI_BYTE6: case PI_BYTE7: case PI_BYTE8:
            DEBUG("[PISTORM-DEV] Read BYTE %d (%d / $%.2X)\n", addr - PI_BYTE1, pi_byte[addr - PI_BYTE1], pi_byte[addr - PI_BYTE1]);
            return pi_byte[addr - PI_BYTE1];
            break;
        case PI_WORD1: case PI_WORD2: case PI_WORD3: case PI_WORD4:
            DEBUG("[PISTORM-DEV] Read WORD %d (%d / $%.4X)\n", (addr - PI_WORD1) / 2, pi_word[(addr - PI_WORD1) / 2], pi_word[(addr - PI_WORD1) / 2]);
            return pi_word[(addr - PI_WORD1) / 2];
            break;
        case PI_LONGWORD1: case PI_LONGWORD2: case PI_LONGWORD3: case PI_LONGWORD4:
            DEBUG("[PISTORM-DEV] Read LONGWORD %d (%d / $%.8X)\n", (addr - PI_LONGWORD1) / 4, pi_longword[(addr - PI_LONGWORD1) / 4], pi_longword[(addr - PI_LONGWORD1) / 4]);
            return pi_longword[(addr - PI_LONGWORD1) / 4];
            break;

        case PI_CMDRESULT:
            DEBUG("[PISTORM-DEV] %s Read from CMDRESULT\n", op_type_names[type]);
            return pi_cmd_result;
            break;

        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register read from %.4X\n", op_type_names[type], addr - pistorm_dev_base);
            break;
    }
    return 0;
}
