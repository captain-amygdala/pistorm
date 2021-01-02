#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../platforms.h"
#include "amiga-autoconf.h"
#include "amiga-registers.h"
#include "../shared/rtc.h"

int handle_register_read_amiga(unsigned int addr, unsigned char type, unsigned int *val);
int handle_register_write_amiga(unsigned int addr, unsigned int value, unsigned char type);

extern int ac_z2_done;
extern int ac_z2_pic_count;
extern int ac_z2_type[AC_PIC_LIMIT];
extern int ac_z2_index[AC_PIC_LIMIT];

extern int ac_z3_pic_count;
extern int ac_z3_done;
extern int ac_z3_type[AC_PIC_LIMIT];
extern int ac_z3_index[AC_PIC_LIMIT];
extern int gayle_emulation_enabled;

char *z2_autoconf_id = "z2_autoconf_fast";
char *z2_autoconf_zap_id = "^2_autoconf_fast";
char *z3_autoconf_id = "z3_autoconf_fast";
char *z3_autoconf_zap_id = "^3_autoconf_fast";

extern const char *op_type_names[OP_TYPE_NUM];
extern uint8_t cdtv_mode;
extern uint8_t rtc_type;
extern unsigned char cdtv_sram[32 * SIZE_KILO];

#define min(a, b) (a < b) ? a : b
#define max(a, b) (a > b) ? a : b

inline int custom_read_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type) {
    if (!ac_z2_done && addr >= AC_Z2_BASE && addr < AC_Z2_BASE + AC_SIZE) {
        if (ac_z2_pic_count == 0) {
            ac_z2_done = 1;
            return -1;
        }

        if (type == OP_TYPE_BYTE) {
            *val = autoconfig_read_memory_8(cfg, addr);
            return 1;
        }
    }
    if (!ac_z3_done && addr >= AC_Z3_BASE && addr < AC_Z3_BASE + AC_SIZE) {
        if (ac_z3_pic_count == 0) {
            ac_z3_done = 1;
            return -1;
        }

        if (type == OP_TYPE_BYTE) {
            *val = autoconfig_read_memory_z3_8(cfg, addr);
            return 1;
        }
        else {
            printf("Unexpected %s read from Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z3_BASE);
            //stop_emulation();
        }
    }

    return -1;
}

inline int custom_write_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int val, unsigned char type) {
    if (!ac_z2_done && addr >= AC_Z2_BASE && addr < AC_Z2_BASE + AC_SIZE) {
        if (type == OP_TYPE_BYTE) {
            if (ac_z2_pic_count == 0) {
                ac_z2_done = 1;
                return -1;
            }

            //printf("Write to Z2 autoconf area.\n");
            autoconfig_write_memory_8(cfg, addr, val);
            return 1;
        }
    }

    if (!ac_z3_done && addr >= AC_Z3_BASE && addr < AC_Z3_BASE + AC_SIZE) {
        if (type == OP_TYPE_BYTE) {
            if (ac_z3_pic_count == 0) {
                ac_z3_done = 1;
                return -1;
            }

            //printf("Write to autoconf area.\n");
            autoconfig_write_memory_z3_8(cfg, addr, val);
            return 1;
        }
        else if (type == OP_TYPE_WORD) {
            autoconfig_write_memory_z3_16(cfg, addr, val);
            return 1;
        }
        else {
            printf("Unexpected %s write to Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z3_BASE);
            //stop_emulation();
        }
    }

    return -1;
}

void adjust_ranges_amiga(struct emulator_config *cfg) {
    cfg->mapped_high = 0;
    cfg->mapped_low = 0;
    cfg->custom_high = 0;
    cfg->custom_low = 0;

    // Set up the min/max ranges for mapped reads/writes
    if (gayle_emulation_enabled) {
        cfg->mapped_low = GAYLEBASE;
        cfg->mapped_high = GAYLEBASE + GAYLESIZE;
    }
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
        if (cfg->map_type[i] != MAPTYPE_NONE) {
            if ((cfg->map_offset[i] != 0 && cfg->map_offset[i] < cfg->mapped_low) || cfg->mapped_low == 0)
                cfg->mapped_low = cfg->map_offset[i];
            if (cfg->map_offset[i] + cfg->map_size[i] > cfg->mapped_high)
                cfg->mapped_high = cfg->map_offset[i] + cfg->map_size[i];
        }
    }

    if (ac_z2_pic_count && !ac_z2_done) {
        if (cfg->custom_low == 0)
            cfg->custom_low = AC_Z2_BASE;
        else
            cfg->custom_low = min(cfg->custom_low, AC_Z2_BASE);
        cfg->custom_high = max(cfg->custom_high, AC_Z2_BASE + AC_SIZE);
    }
    if (ac_z3_pic_count && !ac_z3_done) {
        if (cfg->custom_low == 0)
            cfg->custom_low = AC_Z3_BASE;
        else
            cfg->custom_low = min(cfg->custom_low, AC_Z3_BASE);
        cfg->custom_high = max(cfg->custom_high, AC_Z3_BASE + AC_SIZE);
    }

    printf("Platform custom range: %.8X-%.8X\n", cfg->custom_low, cfg->custom_high);
    printf("Platform mapped range: %.8X-%.8X\n", cfg->mapped_low, cfg->mapped_high);
}

int setup_platform_amiga(struct emulator_config *cfg) {
    printf("Performing setup for Amiga platform.\n");
    // Look for Z2 autoconf Fast RAM by id
    int index = get_named_mapped_item(cfg, z2_autoconf_id);
    more_z2_fast:;
    if (index != -1) {
        // "Zap" config items as they are processed.
        cfg->map_id[index][0] = '^';
        int resize_data = 0;
        if (cfg->map_size[index] > 8 * SIZE_MEGA) {
            printf("Attempted to configure more than 8MB of Z2 Fast RAM, downsizng to 8MB.\n");
            resize_data = 8 * SIZE_MEGA;
        }
        else if(cfg->map_size[index] != 2 * SIZE_MEGA && cfg->map_size[index] != 4 * SIZE_MEGA && cfg->map_size[index] != 8 * SIZE_MEGA) {
            printf("Z2 Fast RAM may only provision 2, 4 or 8MB of memory, resizing to ");
            if (cfg->map_size[index] > 8 * SIZE_MEGA)
                resize_data = 8 * SIZE_MEGA;
            else if (cfg->map_size[index] > 4 * SIZE_MEGA)
                resize_data = 4 * SIZE_MEGA;
            else
                resize_data = 2 * SIZE_MEGA;
            printf("%dMB.\n", resize_data / SIZE_MEGA);
        }
        if (resize_data) {
            free(cfg->map_data[index]);
            cfg->map_size[index] = resize_data;
            cfg->map_data[index] = (unsigned char *)malloc(cfg->map_size[index]);
        }
        printf("%dMB of Z2 Fast RAM configured at $%lx\n", cfg->map_size[index] / SIZE_MEGA, cfg->map_offset[index]);
        ac_z2_type[ac_z2_pic_count] = ACTYPE_MAPFAST_Z2;
        ac_z2_index[ac_z2_pic_count] = index;
        ac_z2_pic_count++;
    }
    else
        printf("No Z2 Fast RAM configured.\n");

    index = get_named_mapped_item(cfg, z2_autoconf_id);
    if (index != -1)
        goto more_z2_fast;
    
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i ++) {
        // Restore any "zapped" autoconf items so they can be reinitialized if needed.
        if (cfg->map_id[i] && strcmp(cfg->map_id[i], z2_autoconf_zap_id) == 0) {
            cfg->map_id[i][0] = z2_autoconf_id[0];
        }
    }

    index = get_named_mapped_item(cfg, z3_autoconf_id);
    more_z3_fast:;
    if (index != -1) {
        cfg->map_id[index][0] = '^';
        printf("%dMB of Z3 Fast RAM configured at $%lx\n", cfg->map_size[index] / SIZE_MEGA, cfg->map_offset[index]);
        ac_z3_type[ac_z3_pic_count] = ACTYPE_MAPFAST_Z3;
        ac_z3_index[ac_z3_pic_count] = index;
        ac_z3_pic_count++;
    }
    else
        printf("No Z3 Fast RAM configured.\n");
    index = get_named_mapped_item(cfg, z3_autoconf_id);
    if (index != -1)
        goto more_z3_fast;
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
        if (cfg->map_id[i] && strcmp(cfg->map_id[i], z3_autoconf_zap_id) == 0) {
            cfg->map_id[i][0] = z3_autoconf_id[0];
        }
    }

    index = get_named_mapped_item(cfg, "cpu_slot_ram");
    if (index != -1) {
        m68k_add_ram_range((uint32_t)cfg->map_offset[index], (uint32_t)cfg->map_high[index], cfg->map_data[index]);
    }

    adjust_ranges_amiga(cfg);

    if (cdtv_mode) {
        FILE *in = fopen("data/cdtv.sram", "rb");
        if (in != NULL) {
            printf("Loaded CDTV SRAM.\n");
            fread(cdtv_sram, 32 * SIZE_KILO, 1, in);
            fclose(in);
        }
    }
    
    return 0;
}

void setvar_amiga(char *var, char *val) {
    if (!var)
        return;

    if (strcmp(var, "enable_rtc_emulation") == 0) {
        int8_t rtc_enabled = 0;
        if (!val || strlen(val) == 0)
            rtc_enabled = 1;
        else {
            rtc_enabled = get_int(val);
        }
        if (rtc_enabled != -1) {
            configure_rtc_emulation_amiga(rtc_enabled);
        }
    }
    if (strcmp(var, "hdd0") == 0) {
        if (val && strlen(val) != 0)
            set_hard_drive_image_file_amiga(0, val);
    }
    if (strcmp(var, "cdtv") == 0) {
        printf("[AMIGA] CDTV mode enabled.\n");
        cdtv_mode = 1;
    }
    if (strcmp(var, "rtc_type") == 0) {
        if (val && strlen(val) != 0) {
            if (strcmp(val, "msm") == 0) {
                printf("[AMIGA] RTC type set to MSM.\n");
                rtc_type = RTC_TYPE_MSM;
            }
            else {
                printf("[AMIGA] RTC type set to Ricoh.\n");
                rtc_type = RTC_TYPE_RICOH;
            }
        }
    }
}

void handle_reset_amiga(struct emulator_config *cfg) {
    ac_z3_done = 0;
    ac_z2_done = 0;

    adjust_ranges_amiga(cfg);
}

void shutdown_platform_amiga(struct emulator_config *cfg) {
    if (cfg) {}
    if (cdtv_mode) {
        FILE *out = fopen("data/cdtv.sram", "wb+");
        if (out != NULL) {
            printf("Saving CDTV SRAM.\n");
            fwrite(cdtv_sram, 32 * SIZE_KILO, 1, out);
            fclose(out);
        }
        else {
            printf("Failed to write CDTV SRAM to disk.\n");
        }
    }
}

void create_platform_amiga(struct platform_config *cfg, char *subsys) {
    cfg->register_read = handle_register_read_amiga;
    cfg->register_write = handle_register_write_amiga;
    cfg->custom_read = custom_read_amiga;
    cfg->custom_write = custom_write_amiga;
    cfg->platform_initial_setup = setup_platform_amiga;
    cfg->handle_reset = handle_reset_amiga;
    cfg->shutdown = shutdown_platform_amiga;

    cfg->setvar = setvar_amiga;
    cfg->id = PLATFORM_AMIGA;

    if (subsys) {
        cfg->subsys = malloc(strlen(subsys) + 1);
        strcpy(cfg->subsys, subsys);
    }
}
