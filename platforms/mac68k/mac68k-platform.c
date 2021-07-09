// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "platforms/platforms.h"
#include "platforms/shared/rtc.h"

//#define DEBUG_MAC_PLATFORM

#ifdef DEBUG_MAC_PLATFORM
#define DEBUG printf
#else
#define DEBUG(...)
#endif

#define min(a, b) (a < b) ? a : b
#define max(a, b) (a > b) ? a : b

extern void stop_cpu_emulation(uint8_t disasm_cur);

uint8_t iscsi_enabled;

extern int kb_hook_enabled;
extern int mouse_hook_enabled;
extern unsigned int ovl;

uint32_t ovl_sysrom_pos = 0x400000;

void adjust_ranges_mac68k(struct emulator_config *cfg) {
    cfg->mapped_high = 0;
    cfg->mapped_low = 0;
    cfg->custom_high = 0;
    cfg->custom_low = 0;

    // Set up the min/max ranges for mapped reads/writes
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
        if (cfg->map_type[i] != MAPTYPE_NONE) {
            if ((cfg->map_offset[i] != 0 && cfg->map_offset[i] < cfg->mapped_low) || cfg->mapped_low == 0)
                cfg->mapped_low = cfg->map_offset[i];
            if (cfg->map_offset[i] + cfg->map_size[i] > cfg->mapped_high)
                cfg->mapped_high = cfg->map_offset[i] + cfg->map_size[i];
        }
    }

    printf("[MAC68K] Platform custom range: %.8X-%.8X\n", cfg->custom_low, cfg->custom_high);
    printf("[MAC68K] Platform mapped range: %.8X-%.8X\n", cfg->mapped_low, cfg->mapped_high);
}


int setup_platform_mac68k(struct emulator_config *cfg) {
    printf("Performing setup for Mac68k platform.\n");

    if (strlen(cfg->platform->subsys)) {
        printf("Sub system %sd specified, but no handler is available for this.\n", cfg->platform->subsys);
    }
    else
        printf("No sub system specified.\n");

    handle_ovl_mappings_mac68k(cfg);

    return 0;
}

#define CHKVAR(a) (strcmp(var, a) == 0)

void setvar_mac68k(struct emulator_config *cfg, char *var, char *val) {
    if (!var)
        return;

    // FIXME: Silence unused variable warnings.
    if (var || cfg || val) {}

    if (CHKVAR("sysrom_pos") && val) {
        ovl_sysrom_pos = get_int(val);
        printf("[MAC68K] System ROM/RAM OVL position set to %.8X\n", ovl_sysrom_pos);
    }

    if (CHKVAR("iscsi") && !iscsi_enabled) {
        printf("[MAC68K] iSCSI Interface Enabled... well, not really.\n");
        iscsi_enabled = 1;
        //iscsi_init();
        //adjust_ranges_mac68k(cfg);
    }
}

void handle_ovl_mappings_mac68k(struct emulator_config *cfg) {
    int32_t index = -1;

    index = get_named_mapped_item(cfg, "sysrom");
    if (index != -1) {
        cfg->map_offset[index] = (ovl) ? 0x0 : ovl_sysrom_pos;
        cfg->map_high[index] = cfg->map_size[index];
        m68k_remove_range(cfg->map_data[index]);
        m68k_add_rom_range((uint32_t)cfg->map_offset[index], (uint32_t)cfg->map_high[index], cfg->map_data[index]);
        printf("[MAC68K] Added memory mapping for Mac68k System ROM.\n");
    } else {
        printf ("[MAC68K] No sysrom mapping found. If you intended to memory map a system ROM, make sure it has the correct ID.\n");
    }
    index = get_named_mapped_item(cfg, "sysram");
    if (index != -1) {
        cfg->map_offset[index] = (ovl) ? ovl_sysrom_pos : 0x0;
        cfg->map_high[index] = cfg->map_size[index];
        m68k_remove_range(cfg->map_data[index]);
        m68k_add_ram_range((uint32_t)cfg->map_offset[index], (uint32_t)cfg->map_high[index], cfg->map_data[index]);
        printf("[MAC68K] Added memory mapping for Mac68k System RAM.\n");
    } else {
        printf ("[MAC68K] No sysram mapping found. If you intended to memory map a system RAM, make sure it has the correct ID.\n");
    }

    adjust_ranges_mac68k(cfg);
}

void handle_reset_mac68k(struct emulator_config *cfg) {
    DEBUG("[MAC68K] Reset handler.\n");

    if (iscsi_enabled) {
        //iscsi_refresh_drives();
    }

    handle_ovl_mappings_mac68k(cfg);
}

void shutdown_platform_mac68k(struct emulator_config *cfg) {
    printf("[MAC68K] Performing Mac68k platform shutdown.\n");
    if (cfg) {}

    if (cfg->platform->subsys) {
        free(cfg->platform->subsys);
        cfg->platform->subsys = NULL;
    }
    if (iscsi_enabled) {
        //iscsi_shutdown();
        iscsi_enabled = 0;
    }

    mouse_hook_enabled = 0;
    kb_hook_enabled = 0;

    printf("[MAC68K] Platform shutdown completed.\n");
}

void create_platform_mac68k(struct platform_config *cfg, char *subsys) {
    cfg->register_read = NULL;
    cfg->register_write = NULL;
    cfg->custom_read = NULL;
    cfg->custom_write = NULL;
    cfg->platform_initial_setup = setup_platform_mac68k;
    cfg->handle_reset = handle_reset_mac68k;
    cfg->shutdown = shutdown_platform_mac68k;

    cfg->setvar = setvar_mac68k;
    cfg->id = PLATFORM_MAC;

    if (subsys) {
        cfg->subsys = malloc(strlen(subsys) + 1);
        strcpy(cfg->subsys, subsys);
        for (unsigned int i = 0; i < strlen(cfg->subsys); i++) {
            cfg->subsys[i] = tolower(cfg->subsys[i]);
        }
    }
}
