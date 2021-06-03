// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pistorm-dev.h"
#include "pistorm-dev-enums.h"
#include "platforms/platforms.h"
#include "gpio/ps_protocol.h"
#include "platforms/amiga/rtg/rtg.h"
#include "platforms/amiga/piscsi/piscsi.h"
#include "platforms/amiga/net/pi-net.h"

#include <linux/reboot.h>
#include <sys/reboot.h>

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

#define PIDEV_SWREV 0x0105

extern uint32_t pistorm_dev_base;
extern uint32_t do_reset;

extern void adjust_ranges_amiga(struct emulator_config *cfg);
extern uint8_t rtg_enabled, rtg_on, pinet_enabled, piscsi_enabled, load_new_config, end_signal;
extern struct emulator_config *cfg;

char cfg_filename[256] = "default.cfg";
char tmp_string[256];

static uint8_t pi_byte[32];
static uint16_t pi_word[32];
static uint32_t pi_longword[32];
static uint32_t pi_string[32];
static uint32_t pi_ptr[32];

static uint32_t pi_dbg_val[32];
static uint32_t pi_dbg_string[32];

static uint32_t pi_cmd_result = 0, shutdown_confirm = 0xFFFFFFFF;

int32_t grab_amiga_string(uint32_t addr, uint8_t *dest, uint32_t str_max_len) {
    int32_t r = get_mapped_item_by_address(cfg, addr);
    uint32_t index = 0;

    if (r == -1) {
        DEBUG("[GRAB_AMIGA_STRING] No mapped range found for address $%.8X. Grabbing string data over the bus.\n", addr);
        do {
            dest[index] = (unsigned char)m68k_read_memory_8(addr + index);
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

int32_t amiga_transfer_file(uint32_t addr, char *filename) {
    FILE *in = fopen(filename, "rb");
    if (in == NULL) {
        DEBUG("[AMIGA_TRANSFER_FILE] Failed to open file %s for reading.\n", filename);
        return -1;
    }
    fseek(in, 0, SEEK_END);

    int32_t r = get_mapped_item_by_address(cfg, addr);
    uint32_t filesize = ftell(in);

    fseek(in, 0, SEEK_SET);
    if (r == -1) {
        DEBUG("[GRAB_AMIGA_STRING] No mapped range found for address $%.8X. Transferring file data over the bus.\n", addr);
        uint8_t tmp_read = 0;

        for (uint32_t i = 0; i < filesize; i++) {
            tmp_read = (uint8_t)fgetc(in);
            m68k_write_memory_8(addr + i, tmp_read);
        }
    } else {
        uint8_t *dst = cfg->map_data[r] + (addr - cfg->map_offset[r]);
        fread(dst, filesize, 1, in);
    }
    fclose(in);
    DEBUG("[AMIGA_TRANSFER_FILE] Copied %d bytes to address $%.8X.\n", filesize, addr);

    return 0;
}

char *get_pistorm_devcfg_filename() {
    return cfg_filename;
}

void set_pistorm_devcfg_filename(char *filename) {
    strcpy(cfg_filename, filename);
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
            //DEBUG("[PISTORM-DEV] Set WORD %d to %d ($%.4X)\n", (addr - PI_WORD1) / 2, (val & 0xFFFF), (val & 0xFFFF));
            pi_word[(addr - PI_WORD1) / 2] = (val & 0xFFFF);
            break;
        case PI_WORD5: case PI_WORD6: case PI_WORD7: case PI_WORD8:
        case PI_WORD9: case PI_WORD10: case PI_WORD11: case PI_WORD12:
            //DEBUG("[PISTORM-DEV] Set WORD %d to %d ($%.4X)\n", (addr - PI_WORD5) / 2, (val & 0xFFFF), (val & 0xFFFF));
            pi_word[(addr - PI_WORD5) / 2] = (val & 0xFFFF);
            break;
        case PI_LONGWORD1: case PI_LONGWORD2: case PI_LONGWORD3: case PI_LONGWORD4:
            //DEBUG("[PISTORM-DEV] Set LONGWORD %d to %d ($%.8X)\n", (addr - PI_LONGWORD1) / 4, val, val);
            pi_longword[(addr - PI_LONGWORD1) / 4] = val;
            break;
        case PI_STR1: case PI_STR2: case PI_STR3: case PI_STR4:
            DEBUG("[PISTORM-DEV] Set STRING POINTER %d to $%.8X\n", (addr - PI_STR1) / 4, val);
            pi_string[(addr - PI_STR1) / 4] = val;
            break;
        case PI_PTR1: case PI_PTR2: case PI_PTR3: case PI_PTR4:
            //DEBUG("[PISTORM-DEV] Set DATA POINTER %d to $%.8X\n", (addr - PI_PTR1) / 4, val);
            pi_ptr[(addr - PI_PTR1) / 4] = val;
            break;

        case PI_CMD_TRANSFERFILE:
            DEBUG("[PISTORM-DEV] Write to TRANSFERFILE.\n");
            if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)tmp_string, 255) == -1)  {
                printf("[PISTORM-DEV] No or invalid filename for TRANSFERFILE. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else if (pi_ptr[0] == 0) {
                printf("[PISTORM-DEV] Null pointer specified for TRANSFERFILE destination. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else {
                if (amiga_transfer_file(pi_ptr[0], tmp_string) == -1) {
                    pi_cmd_result = PI_RES_FAILED;
                } else {
                    pi_cmd_result = PI_RES_OK;
                }
            }
            pi_string[0] = 0;
            pi_ptr[0] = 0;
            break;
        case PI_CMD_MEMCPY:
            //DEBUG("[PISTORM-DEV} Write to MEMCPY: %d (%.8X)\n", val, val);
            if (pi_ptr[0] == 0 || pi_ptr[1] == 0) {
                printf("[PISTORM-DEV] MEMCPY from/to null pointer not allowed. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else if (val == 0) {
                printf("[PISTORM-DEV] MEMCPY called with size 0. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else {
                int32_t src = get_mapped_item_by_address(cfg, pi_ptr[0]);
                int32_t dst = get_mapped_item_by_address(cfg, pi_ptr[1]);
                if (cfg->map_type[dst] == MAPTYPE_ROM)
                    break;
                if (dst != -1 && src != -1) {
                    uint8_t *src_ptr = &cfg->map_data[src][(pi_ptr[0] - cfg->map_offset[src])];
                    uint8_t *dst_ptr = &cfg->map_data[dst][(pi_ptr[1] - cfg->map_offset[dst])];
                    memcpy(dst_ptr, src_ptr, val);
                } else {
                    uint8_t tmp = 0;
                    uint16_t tmps = 0;
                    for (uint32_t i = 0; i < val; i++) {
                        while (i + 2 < val) {
                            if (src == -1) tmps = (uint16_t)m68k_read_memory_16(pi_ptr[0] + i);
                            else memcpy(&tmps, &cfg->map_data[src][pi_ptr[0] - cfg->map_offset[src] + i], 2);

                            if (dst == -1) m68k_write_memory_16(pi_ptr[1] + i, tmps);
                            else memcpy(&cfg->map_data[dst][pi_ptr[1] - cfg->map_offset[dst] + i], &tmps, 2);
                            i += 2;
                        }
                        if (src == -1) tmp = (uint8_t)m68k_read_memory_8(pi_ptr[0] + i);
                        else tmp = cfg->map_data[src][pi_ptr[0] - cfg->map_offset[src] + i];
                        
                        if (dst == -1) m68k_write_memory_8(pi_ptr[1] + i, tmp);
                        else cfg->map_data[dst][pi_ptr[1] - cfg->map_offset[dst] + i] = tmp;
                    }
                }
                //DEBUG("[PISTORM-DEV] Copied %d bytes from $%.8X to $%.8X\n", val, pi_ptr[0], pi_ptr[1]);
            }
            break;
        case PI_CMD_MEMSET:
            //DEBUG("[PISTORM-DEV} Write to MEMSET: %d (%.8X)\n", val, val);
            if (pi_ptr[0] == 0) {
                printf("[PISTORM-DEV] MEMSET with null pointer not allowed. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else if (val == 0) {
                printf("[PISTORM-DEV] MEMSET called with size 0. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else {
                int32_t dst = get_mapped_item_by_address(cfg, pi_ptr[0]);
                if (cfg->map_type[dst] == MAPTYPE_ROM)
                    break;
                if (dst != -1) {
                    uint8_t *dst_ptr = &cfg->map_data[dst][(pi_ptr[0] - cfg->map_offset[dst])];
                    memset(dst_ptr, pi_byte[0], val);
                } else {
                    for (uint32_t i = 0; i < val; i++) {
                        m68k_write_memory_8(pi_ptr[0] + i, pi_byte[0]);
                    }
                }
            }
            break;
        case PI_CMD_COPYRECT:
        case PI_CMD_COPYRECT_EX:
            if (pi_ptr[0] == 0 || pi_ptr[1] == 0) {
                printf("[PISTORM-DEV] COPYRECT/EX from/to null pointer not allowed. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else if (pi_word[2] == 0 || pi_word[3] == 0) {
                printf("[PISTORM-DEV] COPYRECT/EX called with a width/height of 0. Aborting.\n");
                pi_cmd_result = PI_RES_INVALIDVALUE;
            } else {
                int32_t src = get_mapped_item_by_address(cfg, pi_ptr[0]);
                int32_t dst = get_mapped_item_by_address(cfg, pi_ptr[1]);

                if (addr != PI_CMD_COPYRECT_EX) {
                    // Clear out the src/dst coordinates in case something else set them previously.
                    pi_word[4] = pi_word[5] = pi_word[6] = pi_word[7] = 0;
                }

                if (dst != -1 && src != -1) {
                    uint8_t *src_ptr = &cfg->map_data[src][(pi_ptr[0] - cfg->map_offset[src])];
                    uint8_t *dst_ptr = &cfg->map_data[dst][(pi_ptr[1] - cfg->map_offset[dst])];

                    if (addr == PI_CMD_COPYRECT_EX) {
                        // Adjust pointers in the case of available src/dst coordinates.
                        src_ptr += pi_word[4] + (pi_word[5] * pi_word[0]);
                        dst_ptr += pi_word[6] + (pi_word[7] * pi_word[1]);
                    }

                    for (int i = 0; i < pi_word[3]; i++) {
                        memcpy(dst_ptr, src_ptr, pi_word[2]);

                        src_ptr += pi_word[0];
                        dst_ptr += pi_word[1];
                    }
                } else {
                    uint32_t src_offset = 0, dst_offset = 0;
                    uint8_t tmp = 0;

                    if (addr == PI_CMD_COPYRECT_EX) {
                        src_offset += pi_word[4] + (pi_word[5] * pi_word[0]);
                        dst_offset += pi_word[6] + (pi_word[7] * pi_word[1]);
                    }

                    for (uint32_t y = 0; y < pi_word[3]; y++) {
                        for (uint32_t x = 0; x < pi_word[2]; x++) {
                            if (src == -1) tmp = (unsigned char)m68k_read_memory_8(pi_ptr[0] + src_offset + x);
                            else tmp = cfg->map_data[src][(pi_ptr[0] + src_offset + x) - cfg->map_offset[src]];
                            
                            if (dst == -1) m68k_write_memory_8(pi_ptr[1] + dst_offset + x, tmp);
                            else cfg->map_data[dst][(pi_ptr[1] + dst_offset + x) - cfg->map_offset[dst]] = tmp;
                        }
                        src_offset += pi_word[0];
                        dst_offset += pi_word[1];
                    }
                }
            }
            break;

        case PI_CMD_SHOWFPS: rtg_show_fps((uint8_t)val); break;
        case PI_CMD_PALETTEDEBUG: rtg_palette_debug((uint8_t)val); break;
        case PI_CMD_RTGSTATUS:
            DEBUG("[PISTORM-DEV] Write to RTGSTATUS: %d\n", val);
            if (val == 1 && !rtg_enabled) {
                init_rtg_data(cfg);
                rtg_enabled = 1;
                pi_cmd_result = PI_RES_OK;
            } else if (val == 0 && rtg_enabled) {
                if (!rtg_on) {
                    shutdown_rtg();
                    rtg_enabled = 0;
                    pi_cmd_result = PI_RES_OK;
                } else {
                    // Refuse to disable RTG if it's currently in use.
                    pi_cmd_result = PI_RES_FAILED;
                }
            } else {
                pi_cmd_result = PI_RES_NOCHANGE;
            }
            adjust_ranges_amiga(cfg);
            break;
        case PI_CMD_NETSTATUS:
            DEBUG("[PISTORM-DEV] Write to NETSTATUS: %d\n", val);
            if (val == 1 && !pinet_enabled) {
                pinet_init(NULL);
                pinet_enabled = 1;
                pi_cmd_result = PI_RES_OK;
            } else if (val == 0 && pinet_enabled) {
                pinet_shutdown();
                pinet_enabled = 0;
                pi_cmd_result = PI_RES_OK;
            } else {
                pi_cmd_result = PI_RES_NOCHANGE;
            }
            adjust_ranges_amiga(cfg);
            break;
        case PI_CMD_PISCSI_CTRL:
            DEBUG("[PISTORM-DEV] Write to PISCSI_CTRL: ");
            switch(val) {
                case PISCSI_CTRL_DISABLE:
                    DEBUG("DISABLE\n");
                    if (piscsi_enabled) {
                        piscsi_shutdown();
                        piscsi_enabled = 0;
                        // Probably not OK... depends on if you booted from floppy, I guess.
                        pi_cmd_result = PI_RES_OK;
                    } else {
                        pi_cmd_result = PI_RES_NOCHANGE;
                    }
                    break;
                case PISCSI_CTRL_ENABLE:
                    DEBUG("ENABLE\n");
                    if (!piscsi_enabled) {
                        piscsi_init();
                        piscsi_enabled = 1;
                        piscsi_refresh_drives();
                        pi_cmd_result = PI_RES_OK;
                    } else {
                        pi_cmd_result = PI_RES_NOCHANGE;
                    }
                    break;
                case PISCSI_CTRL_MAP:
                    DEBUG("MAP\n");
                    if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)tmp_string, 255) == -1)  {
                        printf("[PISTORM-DEV] Failed to grab string for PISCSI drive filename. Aborting.\n");
                        pi_cmd_result = PI_RES_FAILED;
                    } else {
                        FILE *tmp = fopen(tmp_string, "rb");
                        if (tmp == NULL) {
                            printf("[PISTORM-DEV] Failed to open file %s for PISCSI drive mapping. Aborting.\n", tmp_string);
                            pi_cmd_result = PI_RES_FILENOTFOUND;
                        } else {
                            fclose(tmp);
                            printf("[PISTORM-DEV] Attempting to map file %s as PISCSI drive %d...\n", tmp_string, pi_word[0]);
                            piscsi_unmap_drive(pi_word[0]);
                            piscsi_map_drive(tmp_string, pi_word[0]);
                            pi_cmd_result = PI_RES_OK;
                        }
                    }
                    pi_string[0] = 0;
                    break;
                case PISCSI_CTRL_UNMAP:
                    DEBUG("UNMAP\n");
                    if (pi_word[0] > 7) {
                        printf("[PISTORM-DEV] Invalid drive ID %d for PISCSI unmap command.", pi_word[0]);
                        pi_cmd_result = PI_RES_INVALIDVALUE;
                    } else {
                        if (piscsi_get_dev(pi_word[0])->fd != -1) {
                            piscsi_unmap_drive(pi_word[0]);
                            pi_cmd_result = PI_RES_OK;
                        } else {
                            pi_cmd_result = PI_RES_NOCHANGE;
                        }
                    }
                    break;
                case PISCSI_CTRL_EJECT:
                    DEBUG("EJECT (NYI)\n");
                    pi_cmd_result = PI_RES_NOCHANGE;
                    break;
                case PISCSI_CTRL_INSERT:
                    DEBUG("INSERT (NYI)\n");
                    pi_cmd_result = PI_RES_NOCHANGE;
                    break;
                default:
                    DEBUG("UNKNOWN/UNHANDLED. Aborting.\n");
                    pi_cmd_result = PI_RES_INVALIDVALUE;
                    break;
            }
            adjust_ranges_amiga(cfg);
            break;
        
        case PI_CMD_KICKROM:
            DEBUG("[PISTORM-DEV] Write to KICKROM.\n");
            if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)tmp_string, 255) == -1)  {
                printf("[PISTORM-DEV] Failed to grab string KICKROM filename. Aborting.\n");
                pi_cmd_result = PI_RES_FAILED;
            } else {
                FILE *tmp = fopen(tmp_string, "rb");
                if (tmp == NULL) {
                    printf("[PISTORM-DEV] Failed to open file %s for KICKROM mapping. Aborting.\n", tmp_string);
                    pi_cmd_result = PI_RES_FILENOTFOUND;
                } else {
                    fclose(tmp);
                    if (get_named_mapped_item(cfg, "kickstart") != -1) {
                        uint32_t index = get_named_mapped_item(cfg, "kickstart");
                        free(cfg->map_data[index]);
                        free(cfg->map_id[index]);
                        cfg->map_type[index] = MAPTYPE_NONE;
                        // Dirty hack, I am sleepy and lazy.
                        add_mapping(cfg, MAPTYPE_ROM, cfg->map_offset[index], cfg->map_size[index], 0, tmp_string, "kickstart", 0);
                        pi_cmd_result = PI_RES_OK;
                        do_reset = 1;
                    } else {
                        printf ("[PISTORM-DEV] Could not find mapped range 'kickstart', cannot remap KICKROM.\n");
                        pi_cmd_result = PI_RES_FAILED;
                    }
                }
            }
            adjust_ranges_amiga(cfg);
            pi_string[0] = 0;
            break;
        case PI_CMD_EXTROM:
            DEBUG("[PISTORM-DEV] Write to EXTROM.\n");
            if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)tmp_string, 255) == -1)  {
                printf("[PISTORM-DEV] Failed to grab string EXTROM filename. Aborting.\n");
                pi_cmd_result = PI_RES_FAILED;
            } else {
                FILE *tmp = fopen(tmp_string, "rb");
                if (tmp == NULL) {
                    printf("[PISTORM-DEV] Failed to open file %s for EXTROM mapping. Aborting.\n", tmp_string);
                    pi_cmd_result = PI_RES_FILENOTFOUND;
                } else {
                    fclose(tmp);
                    if (get_named_mapped_item(cfg, "extended") != -1) {
                        uint32_t index = get_named_mapped_item(cfg, "extended");
                        free(cfg->map_data[index]);
                        free(cfg->map_id[index]);
                        cfg->map_type[index] = MAPTYPE_NONE;
                        // Dirty hack, I am tired and lazy.
                        add_mapping(cfg, MAPTYPE_ROM, cfg->map_offset[index], cfg->map_size[index], 0, tmp_string, "extended", 0);
                        pi_cmd_result = PI_RES_OK;
                        do_reset = 1;
                    } else {
                        printf ("[PISTORM-DEV] Could not find mapped range 'extrom', cannot remap EXTROM.\n");
                        pi_cmd_result = PI_RES_FAILED;
                    }
                }
            }
            adjust_ranges_amiga(cfg);
            pi_string[0] = 0;
            break;

        case PI_CMD_RESET:
            DEBUG("[PISTORM-DEV] System reset called, code %d\n", (val & 0xFFFF));
            do_reset = 1;
            break;
        case PI_CMD_SHUTDOWN:
            DEBUG("[PISTORM-DEV] Shutdown requested. Confirm by replying with return value to CONFIRMSHUTDOWN.\n");
            shutdown_confirm = rand() % 0xFFFF;
            pi_cmd_result = shutdown_confirm;
            break;
        case PI_CMD_CONFIRMSHUTDOWN:
            if (val != shutdown_confirm) {
                DEBUG("[PISTORM-DEV] Attempted shutdown with wrong shutdown confirm value. Not shutting down.\n");
                shutdown_confirm = 0xFFFFFFFF;
                pi_cmd_result = PI_RES_FAILED;
            } else {
                printf("[PISTORM-DEV] Shutting down the PiStorm. Good night, fight well, until we meet again.\n");
                reboot(LINUX_REBOOT_CMD_POWER_OFF);
                pi_cmd_result = PI_RES_OK;
                end_signal = 1;
            }
            break;
        case PI_CMD_SWITCHCONFIG:
            DEBUG("[PISTORM-DEV] Config switch called, command: ");
            switch (val) {
                case PICFG_LOAD:
                    DEBUG("LOAD\n");
                    if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)cfg_filename, 255) == -1) {
                        printf("[PISTORM-DEV] Failed to grab string for CONFIG filename. Aborting.\n");
                        pi_cmd_result = PI_RES_FAILED;
                    } else {
                        FILE *tmp = fopen(cfg_filename, "rb");
                        if (tmp == NULL) {
                            printf("[PISTORM-DEV] Failed to open CONFIG file %s for reading. Aborting.\n", cfg_filename);
                            pi_cmd_result = PI_RES_FILENOTFOUND;
                        } else {
                            fclose(tmp);
                            printf("[PISTORM-DEV] Attempting to load config file %s...\n", cfg_filename);
                            load_new_config = val + 1;
                            pi_cmd_result = PI_RES_OK;
                        }
                    }
                    pi_string[0] = 0;
                    break;
                case PICFG_RELOAD:
                    DEBUG("RELOAD\n");
                    printf("[PISTORM-DEV] Reloading current config file (%s)...\n", cfg_filename);
                    load_new_config = val + 1;
                    break;
                case PICFG_DEFAULT:
                    DEBUG("DEFAULT\n");
                    printf("[PISTORM-DEV] Loading default.cfg...\n");
                    load_new_config = val + 1;
                    break;
                default:
                    DEBUG("UNKNOWN/UNHANDLED. Command ignored.\n");
                    pi_cmd_result = PI_RES_INVALIDVALUE;
                    break;
            }
            break;
        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register write to %.4X: %d\n", op_type_names[type], addr - pistorm_dev_base, val);
            pi_cmd_result = PI_RES_INVALIDCMD;
            break;
    }
}
 
uint32_t handle_pistorm_dev_read(uint32_t addr_, uint8_t type) {
    uint32_t addr = (addr_ & 0xFFFF);

    switch((addr)) {
        case PI_CMD_FILESIZE:
            DEBUG("[PISTORM-DEV] %s read from FILESIZE.\n", op_type_names[type]);
            if (pi_string[0] == 0 || grab_amiga_string(pi_string[0], (uint8_t *)tmp_string, 255) == -1)  {
                DEBUG("[PISTORM-DEV] Failed to grab string for FILESIZE command. Aborting.\n");
                pi_cmd_result = PI_RES_FAILED;
                pi_longword[0] = 0;
                return 0;
            } else {
                FILE *tmp = fopen(tmp_string, "rb");
                if (tmp == NULL) {
                    DEBUG("[PISTORM-DEV] Failed to open file %s for FILESIZE command. Aborting.\n", tmp_string);
                    pi_longword[0] = 0;
                    pi_cmd_result = PI_RES_FILENOTFOUND;
                } else {
                    fseek(tmp, 0, SEEK_END);
                    pi_longword[0] = ftell(tmp);
                    DEBUG("[PISTORM-DEV] Returning file size for file %s: %d bytes.\n", tmp_string, pi_longword[0]);
                    fclose(tmp);
                    pi_cmd_result = PI_RES_OK;
                }
            }
            pi_string[0] = 0;
            return pi_longword[0];
            break;

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
        case PI_CMD_GET_FB:
            //DEBUG("[PISTORM-DEV] %s read from GET_FB: %.8X\n", op_type_names[type], rtg_get_fb());
            return rtg_get_fb();

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
        case PI_WORD5: case PI_WORD6: case PI_WORD7: case PI_WORD8: 
        case PI_WORD9: case PI_WORD10: case PI_WORD11: case PI_WORD12: 
            DEBUG("[PISTORM-DEV] Read WORD %d (%d / $%.4X)\n", (addr - PI_WORD5) / 2, pi_word[(addr - PI_WORD5) / 2], pi_word[(addr - PI_WORD5) / 2]);
            return pi_word[(addr - PI_WORD5) / 2];
            break;
        case PI_LONGWORD1: case PI_LONGWORD2: case PI_LONGWORD3: case PI_LONGWORD4:
            DEBUG("[PISTORM-DEV] Read LONGWORD %d (%d / $%.8X)\n", (addr - PI_LONGWORD1) / 4, pi_longword[(addr - PI_LONGWORD1) / 4], pi_longword[(addr - PI_LONGWORD1) / 4]);
            return pi_longword[(addr - PI_LONGWORD1) / 4];
            break;

        case PI_CMDRESULT:
            //DEBUG("[PISTORM-DEV] %s Read from CMDRESULT: %d\n", op_type_names[type], pi_cmd_result);
            return pi_cmd_result;
            break;

        default:
            DEBUG("[PISTORM-DEV] WARN: Unhandled %s register read from %.4X\n", op_type_names[type], addr - pistorm_dev_base);
            break;
    }
    return 0;
}
