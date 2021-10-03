// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "amiga-autoconf.h"
#include "amiga-registers.h"
#include "amiga-interrupts.h"
#include "gpio/ps_protocol.h"
#include "hunk-reloc.h"
#include "net/pi-net-enums.h"
#include "net/pi-net.h"
#include "piscsi/piscsi-enums.h"
#include "piscsi/piscsi.h"
#include "ahi/pi_ahi.h"
#include "ahi/pi-ahi-enums.h"
#include "pistorm-dev/pistorm-dev-enums.h"
#include "pistorm-dev/pistorm-dev.h"
#include "platforms/platforms.h"
#include "platforms/shared/rtc.h"
#include "rtg/rtg.h"
#include "a314/a314.h"

//#define DEBUG_AMIGA_PLATFORM

#ifdef DEBUG_AMIGA_PLATFORM
#define DEBUG printf
#else
#define DEBUG(...)
#endif

int handle_register_read_amiga(unsigned int addr, unsigned char type, unsigned int *val);
int handle_register_write_amiga(unsigned int addr, unsigned int value, unsigned char type);

extern int ac_z2_current_pic;
extern int ac_z2_done;
extern int ac_z2_pic_count;
extern int ac_z2_type[AC_PIC_LIMIT];
extern int ac_z2_index[AC_PIC_LIMIT];

extern int ac_z3_current_pic;
extern int ac_z3_pic_count;
extern int ac_z3_done;
extern int ac_z3_type[AC_PIC_LIMIT];
extern int ac_z3_index[AC_PIC_LIMIT];
extern uint8_t gayle_emulation_enabled;

char *z2_autoconf_id = "z2_autoconf_fast";
char *z2_autoconf_zap_id = "^2_autoconf_fast";
char *z3_autoconf_id = "z3_autoconf_fast";
char *z3_autoconf_zap_id = "^3_autoconf_fast";

extern const char *op_type_names[OP_TYPE_NUM];
extern uint8_t cdtv_mode;
extern uint8_t rtc_type;
extern unsigned char cdtv_sram[32 * SIZE_KILO];
extern unsigned int a314_base;

extern int kb_hook_enabled;
extern int mouse_hook_enabled;

extern int swap_df0_with_dfx;
extern int spoof_df0_id;
extern int move_slow_to_chip;
extern int force_move_slow_to_chip;

#define min(a, b) (a < b) ? a : b
#define max(a, b) (a > b) ? a : b

uint8_t rtg_enabled = 0, piscsi_enabled = 0, pinet_enabled = 0, kick13_mode = 0, pistorm_dev_enabled = 1, pi_ahi_enabled = 0;
uint8_t a314_emulation_enabled = 0, a314_initialized = 0;

extern uint32_t piscsi_base, pistorm_dev_base;
extern uint8_t rtg_dpms;

extern void stop_cpu_emulation(uint8_t disasm_cur);

static uint32_t ac_waiting_for_physical_pic = 0;

inline int custom_read_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int *val, unsigned char type) {
    if (kick13_mode)
        ac_z3_done = 1;

    if ((!ac_z2_done || !ac_z3_done) && addr >= AC_Z2_BASE && addr < AC_Z2_BASE + AC_SIZE) {
        if (addr == AC_Z2_BASE) {
            uint8_t zchk = ps_read_8(addr);
            DEBUG("[AUTOCONF] Read from AC_Z2_BASE: %.2X\n", zchk);
            // This check may look a bit strange, but it appears that some boards invert the lower four nibbles
            // for the boardtype bits, although this isn't required, and if it isn't "clear" in one way or the
            // other (either 1111 or 0000), there's no board responding on this address.
            // It apparently also isn't a reliable way to check if there's a board detected...
            //if ((zchk & 0x0F) == 0x0F || (zchk & 0x0F) == 0x00) {
            // Try checking if the byte comes back identifying as a Zorro II or III board instead.
            if (((zchk & BOARDTYPE_Z2) == BOARDTYPE_Z2) || ((zchk & BOARDTYPE_Z3) == BOARDTYPE_Z3)) {
                if (!ac_waiting_for_physical_pic) {
                    printf("[AUTOCONF] Found physical Zorro board, pausing processing until done.\n");
                    ac_waiting_for_physical_pic = 1;
                }
                *val = zchk;
                return 1;
            } else {
                if (ac_waiting_for_physical_pic) {
                    printf("[AUTOCONF] Resuming virtual Zorro board processing.\n");
                    ac_waiting_for_physical_pic = 0;
                }
            }
        }
        if (ac_waiting_for_physical_pic) {
            return -1;
        }
        if (!ac_z2_done && ac_z2_current_pic < ac_z2_pic_count) {
            if (type == OP_TYPE_BYTE) {
                *val = autoconfig_read_memory_8(cfg, addr - AC_Z2_BASE);
                return 1;
            }
            printf("Unexpected %s read from Z2 autoconf addr %.X\n", op_type_names[type], addr - AC_Z2_BASE);
            return -1;
        }
        if (!ac_z3_done && ac_z3_current_pic < ac_z3_pic_count) {
            uint32_t addr_ = addr - AC_Z2_BASE;
            if (addr_ & 0x02) {
                addr_ = (addr_ - 2) + 0x100;
            }
            if (type == OP_TYPE_BYTE) {
                *val = autoconfig_read_memory_z3_8(cfg, addr_ - AC_Z2_BASE);
                return 1;
            }
            printf("Unexpected %s read from Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z2_BASE);
            return -1;
        }
    }
    if (!ac_z3_done && addr >= AC_Z3_BASE && addr < AC_Z3_BASE + AC_SIZE) {
        if (ac_z3_pic_count == 0) {
            ac_z3_done = 1;
            return -1;
        }

        if (type == OP_TYPE_BYTE) {
            *val = autoconfig_read_memory_z3_8(cfg, addr - AC_Z3_BASE);
            return 1;
        }
        printf("Unexpected %s read from Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z3_BASE);
        return -1;
    }

    if (pistorm_dev_enabled && addr >= pistorm_dev_base && addr < pistorm_dev_base + (64 * SIZE_KILO)) {
        *val = handle_pistorm_dev_read(addr, type);
        return 1;
    }

    if (piscsi_enabled && addr >= piscsi_base && addr < piscsi_base + (64 * SIZE_KILO)) {
        //printf("[Amiga-Custom] %s read from PISCSI base @$%.8X.\n", op_type_names[type], addr);
        //stop_cpu_emulation(1);
        *val = handle_piscsi_read(addr, type);
        return 1;
    }

    if (a314_emulation_enabled && addr >= a314_base && addr < a314_base + (64 * SIZE_KILO)) {
        //printf("%s read from A314 @$%.8X\n", op_type_names[type], addr);
        switch (type) {
            case OP_TYPE_BYTE:
                *val = a314_read_memory_8(addr - a314_base);
                return 1;
                break;
            case OP_TYPE_WORD:
                *val = a314_read_memory_16(addr - a314_base);
                return 1;                
                break;
            case OP_TYPE_LONGWORD:
                *val = a314_read_memory_32(addr - a314_base);
                return 1;
                break;
            default:
                break;
        }
        return 1;
    }

    return -1;
}

inline int custom_write_amiga(struct emulator_config *cfg, unsigned int addr, unsigned int val, unsigned char type) {
    if (kick13_mode)
        ac_z3_done = 1;

    if ((!ac_z2_done || !ac_z3_done) && addr >= AC_Z2_BASE && addr < AC_Z2_BASE + AC_SIZE) {
        if (ac_waiting_for_physical_pic) {
            return -1;
        }
        if (!ac_z2_done && ac_z2_current_pic < ac_z2_pic_count) {
            if (type == OP_TYPE_BYTE) {
                autoconfig_write_memory_8(cfg, addr - AC_Z2_BASE, val);
                return 1;
            }
            printf("Unexpected %s write to Z2 autoconf addr %.X\n", op_type_names[type], addr - AC_Z2_BASE);
            return -1;
        }
        if (!ac_z3_done && ac_z3_current_pic < ac_z3_pic_count) {
            uint32_t addr_ = addr - AC_Z2_BASE;
            if (addr_ & 0x02) {
                addr_ = (addr_ - 2) + 0x100;
            }
            if (type == OP_TYPE_BYTE) {
                autoconfig_write_memory_z3_8(cfg, addr_ - AC_Z2_BASE, val);
                return 1;
            }
            else if (type == OP_TYPE_WORD) {
                autoconfig_write_memory_z3_16(cfg, addr_ - AC_Z2_BASE, val);
                return 1;
            }
            printf("Unexpected %s write to Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z2_BASE);
            return -1;
        }
    }

    if (!ac_z3_done && addr >= AC_Z3_BASE && addr < AC_Z3_BASE + AC_SIZE) {
        if (type == OP_TYPE_BYTE) {
            if (ac_z3_pic_count == 0) {
                ac_z3_done = 1;
                return -1;
            }

            //printf("Write to autoconf area.\n");
            autoconfig_write_memory_z3_8(cfg, addr - AC_Z3_BASE, val);
            return 1;
        }
        else if (type == OP_TYPE_WORD) {
            autoconfig_write_memory_z3_16(cfg, addr - AC_Z3_BASE, val);
            return 1;
        }
        else {
            printf("Unexpected %s write to Z3 autoconf addr %.X\n", op_type_names[type], addr - AC_Z3_BASE);
            //stop_emulation();
        }
    }

    if (pistorm_dev_enabled && addr >= pistorm_dev_base && addr < pistorm_dev_base + (64 * SIZE_KILO)) {
        handle_pistorm_dev_write(addr, val, type);
        return 1;
    }

    if (piscsi_enabled && addr >= piscsi_base && addr < piscsi_base + (64 * SIZE_KILO)) {
        //printf("[Amiga-Custom] %s write to PISCSI base @$%.8x: %.8X\n", op_type_names[type], addr, val);
        handle_piscsi_write(addr, val, type);
        return 1;
    }

    if (a314_emulation_enabled && addr >= a314_base && addr < a314_base + (64 * SIZE_KILO)) {
        //printf("%s write to A314 @$%.8X: %d\n", op_type_names[type], addr, val);
        switch (type) {
            case OP_TYPE_BYTE:
                a314_write_memory_8(addr - a314_base, val);
                return 1;
                break;
            case OP_TYPE_WORD:
                // Not implemented in a314.cc
                //a314_write_memory_16(addr, val);
                return -1;
                break;
            case OP_TYPE_LONGWORD:
                // Not implemented in a314.cc
                // a314_write_memory_32(addr, val);
                return -1;
                break;
            default:
                break;
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
    for (int i = 0; i < MAX_NUM_MAPPED_ITEMS; i++) {
        if (cfg->map_type[i] != MAPTYPE_NONE) {
            if ((cfg->map_offset[i] != 0 && cfg->map_offset[i] < cfg->mapped_low) || cfg->mapped_low == 0)
                cfg->mapped_low = cfg->map_offset[i];
            if (cfg->map_offset[i] + cfg->map_size[i] > cfg->mapped_high)
                cfg->mapped_high = cfg->map_offset[i] + cfg->map_size[i];
        }
    }

    if ((ac_z2_pic_count || ac_z3_pic_count) && (!ac_z2_done || !ac_z3_done)) {
        if (cfg->custom_low == 0)
            cfg->custom_low = AC_Z2_BASE;
        else
            cfg->custom_low = min(cfg->custom_low, AC_Z2_BASE);
        cfg->custom_high = max(cfg->custom_high, AC_Z2_BASE + AC_SIZE);
    }
    /*if (ac_z3_pic_count && !ac_z3_done) {
        if (cfg->custom_low == 0)
            cfg->custom_low = AC_Z3_BASE;
        else
            cfg->custom_low = min(cfg->custom_low, AC_Z3_BASE);
        cfg->custom_high = max(cfg->custom_high, AC_Z3_BASE + AC_SIZE);
    }*/
    if (rtg_enabled) {
        if (cfg->custom_low == 0)
            cfg->custom_low = PIGFX_RTG_BASE;
        else
            cfg->custom_low = min(cfg->custom_low, PIGFX_RTG_BASE);
        cfg->custom_high = max(cfg->custom_high, PIGFX_UPPER);
    }
    if (piscsi_enabled) {
        if (cfg->custom_low == 0)
            cfg->custom_low = PISCSI_OFFSET;
        else
            cfg->custom_low = min(cfg->custom_low, PISCSI_OFFSET);
        cfg->custom_high = max(cfg->custom_high, PISCSI_UPPER);
        if (piscsi_base != 0) {
            cfg->custom_low = min(cfg->custom_low, piscsi_base);
        }
    }
    if (pi_ahi_enabled) {
        if (cfg->custom_low == 0)
            cfg->custom_low = PI_AHI_OFFSET;
        else
            cfg->custom_low = min(cfg->custom_low, PI_AHI_OFFSET);
        cfg->custom_high = max(cfg->custom_high, PI_AHI_UPPER);
    }
    if (pinet_enabled) {
        if (cfg->custom_low == 0)
            cfg->custom_low = PINET_OFFSET;
        else
            cfg->custom_low = min(cfg->custom_low, PINET_OFFSET);
        cfg->custom_high = max(cfg->custom_high, PINET_UPPER);
    }

    printf("Platform custom range: %.8X-%.8X\n", cfg->custom_low, cfg->custom_high);
    printf("Platform mapped range: %.8X-%.8X\n", cfg->mapped_low, cfg->mapped_high);
}

int setup_platform_amiga(struct emulator_config *cfg) {
    printf("Performing setup for Amiga platform.\n");

    if (strlen(cfg->platform->subsys)) {
        printf("Subsystem is [%s]\n", cfg->platform->subsys);
        if (strcmp(cfg->platform->subsys, "4000") == 0 || strcmp(cfg->platform->subsys, "3000") == 0) {
            printf("Adjusting Gayle accesses for A3000/4000 Kickstart.\n");
            adjust_gayle_4000();
        }
        else if (strcmp(cfg->platform->subsys, "1200") == 0 || strcmp(cfg->platform->subsys, "cd32") == 0) {
            printf("Adjusting Gayle accesses for A1200/CD32 Kickstart.\n");
            adjust_gayle_1200();
        }
        else if (strcmp(cfg->platform->subsys, "cdtv") == 0) {
            printf("Configuring platform for CDTV emulation.\n");
            cdtv_mode = 1;
            rtc_type = RTC_TYPE_MSM;
        }
    }
    else
        printf("No sub system specified.\n");

    // Look for Z2 autoconf Fast RAM by id
    int index = get_named_mapped_item(cfg, z2_autoconf_id);
    more_z2_fast:;
    if (index != -1) {
        // "Zap" config items as they are processed.
        cfg->map_id[index][0] = '^';
        int resize_data = 0;
        if (cfg->map_size[index] > 8 * SIZE_MEGA) {
            printf("Attempted to configure more than 8MB of Z2 Fast RAM, downsizing to 8MB.\n");
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
        add_z2_pic(ACTYPE_MAPFAST_Z2, index);
        //ac_z2_type[ac_z2_pic_count] = ACTYPE_MAPFAST_Z2;
        //ac_z2_index[ac_z2_pic_count] = index;
        //ac_z2_pic_count++;
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

    if (pistorm_dev_enabled) {
        add_z2_pic(ACTYPE_PISTORM_DEV, 0);
    }

    return 0;
}

#define CHKVAR(a) (strcmp(var, a) == 0)

void setvar_amiga(struct emulator_config *cfg, char *var, char *val) {
    if (!var)
        return;

    if CHKVAR("enable_rtc_emulation") {
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
    if CHKVAR("hdd0") {
        if (val && strlen(val) != 0)
            set_hard_drive_image_file_amiga(0, val);
    }
    if CHKVAR("hdd1") {
        if (val && strlen(val) != 0)
            set_hard_drive_image_file_amiga(1, val);
    }
    if CHKVAR("cdtv") {
        printf("[AMIGA] CDTV mode enabled.\n");
        cdtv_mode = 1;
    }
    if (CHKVAR("rtg") && !rtg_enabled) {
        if (init_rtg_data(cfg)) {
            printf("[AMIGA] RTG Enabled.\n");
            rtg_enabled = 1;
            adjust_ranges_amiga(cfg);
        }
        else
            printf("[AMIGA] Failed to enable RTG.\n");
    }
    if (CHKVAR("rtg-dpms")) {
        rtg_dpms = 1;
        printf("[AMIGA] DPMS enabled for RTG.\n");
    }
    if CHKVAR("kick13") {
        printf("[AMIGA] Kickstart 1.3 mode enabled, Z3 PICs will not be enumerated.\n");
        kick13_mode = 1;
    }
    if CHKVAR("a314") {
        if (!a314_initialized) {
            int32_t res = a314_init();
            if (res != 0) {
                printf("[AMIGA] Failed to enable A314 emulation, error return code: %d.\n", res);
            } else {
                printf("[AMIGA] A314 emulation enabled.\n");
                add_z2_pic(ACTYPE_A314, 0);
                a314_emulation_enabled = 1;
                a314_initialized = 1;
            }
        } else {
            add_z2_pic(ACTYPE_A314, 0);
            a314_emulation_enabled = 1;
        }
    }
    if CHKVAR("a314_conf") {
        if (val && strlen(val) != 0) {
            a314_set_config_file(val);
        }
    }

    // PiSCSI stuff
    if (CHKVAR("piscsi") && !piscsi_enabled) {
        printf("[AMIGA] PISCSI Interface Enabled.\n");
        piscsi_enabled = 1;
        piscsi_init();
        add_z2_pic(ACTYPE_PISCSI, 0);
        adjust_ranges_amiga(cfg);
    }
    if (piscsi_enabled) {
        if CHKVAR("piscsi0") {
            piscsi_map_drive(val, 0);
        }
        if CHKVAR("piscsi1") {
            piscsi_map_drive(val, 1);
        }
        if CHKVAR("piscsi2") {
            piscsi_map_drive(val, 2);
        }
        if CHKVAR("piscsi3") {
            piscsi_map_drive(val, 3);
        }
        if CHKVAR("piscsi4") {
            piscsi_map_drive(val, 4);
        }
        if CHKVAR("piscsi5") {
            piscsi_map_drive(val, 5);
        }
        if CHKVAR("piscsi6") {
            piscsi_map_drive(val, 6);
        }
    }

    // Pi-Net stuff
    if (CHKVAR("pi-net") && !pinet_enabled) {
        printf("[AMIGA] PI-NET Interface Enabled.\n");
        pinet_enabled = 1;
        pinet_init(val);
        adjust_ranges_amiga(cfg);
    }

    // Pi-AHI stuff
    if (CHKVAR("pi-ahi") && !pi_ahi_enabled) {
        printf("[AMIGA] PI-AHI Audio Card Enabled.\n");
        uint32_t res = 1;
        if (val && strlen(val) != 0)
            res = pi_ahi_init(val);
        else
            res = pi_ahi_init("plughw:1,0");
        if (res == 0) {
            pi_ahi_enabled = 1;
            adjust_ranges_amiga(cfg);
        } else {
            printf("[AMIGA] Failed to enable PI-AHI.\n");
        }
    }
    if (CHKVAR("pi-ahi-samplerate")) {
        if (val && strlen(val) != 0) {
            pi_ahi_set_playback_rate(get_int(val));
        }
    }

    if CHKVAR("no-pistorm-dev") {
        pistorm_dev_enabled = 0;
        printf("[AMIGA] Disabling PiStorm interaction device.\n");
    }

    // RTC stuff
    if CHKVAR("rtc_type") {
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

    if CHKVAR("swap-df0-df")  {
        if (val && strlen(val) != 0 && get_int(val) >= 1 && get_int(val) <= 3) {
           swap_df0_with_dfx = get_int(val);
           printf("[AMIGA] DF0 and DF%d swapped.\n",swap_df0_with_dfx);
        }
    }

    if CHKVAR("move-slow-to-chip") {
        move_slow_to_chip = 1;
        printf("[AMIGA] Slow ram moved to Chip.\n");
    }

    if CHKVAR("force-move-slow-to-chip") {
        force_move_slow_to_chip = 1;
        printf("[AMIGA] Forcing slowram move to chip, bypassing Agnus version check.\n");
    }
}

void handle_reset_amiga(struct emulator_config *cfg) {
    ac_z2_done = (ac_z2_pic_count == 0);
    ac_z3_done = (ac_z3_pic_count == 0);
    ac_z2_current_pic = 0;
    ac_z3_current_pic = 0;
    ac_waiting_for_physical_pic = 0;

    spoof_df0_id = 0;

    DEBUG("[AMIGA] Reset handler.\n");
    DEBUG("[AMIGA] AC done - Z2: %d Z3: %d.\n", ac_z2_done, ac_z3_done);

    if (piscsi_enabled)
        piscsi_refresh_drives();

    if (move_slow_to_chip && !force_move_slow_to_chip) {
      ps_write_16(VPOSW,0x00); // Poke poke... wake up Agnus!
      int agnus_rev = ((ps_read_16(VPOSR) >> 8) & 0x6F);
      if (agnus_rev != 0x20) {
        move_slow_to_chip = 0;
        printf("[AMIGA] Requested move slow ram to chip but 8372 Agnus not found - Disabling.\n");
      }
    }

    amiga_clear_emulating_irq();
    adjust_ranges_amiga(cfg);
}

void shutdown_platform_amiga(struct emulator_config *cfg) {
    printf("[AMIGA] Performing Amiga platform shutdown.\n");
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
    if (cfg->platform->subsys) {
        free(cfg->platform->subsys);
        cfg->platform->subsys = NULL;
    }
    if (piscsi_enabled) {
        piscsi_shutdown();
        piscsi_enabled = 0;
    }
    if (rtg_enabled) {
        shutdown_rtg();
        rtg_enabled = 0;
    }
    if (pinet_enabled) {
        pinet_enabled = 0;
    }
    if (pi_ahi_enabled) {
        pi_ahi_shutdown();
        pi_ahi_enabled = 0;
    }
    if (a314_emulation_enabled) {
        a314_emulation_enabled = 0;
    }

    mouse_hook_enabled = 0;
    kb_hook_enabled = 0;

    kick13_mode = 0;
    cdtv_mode = 0;

    swap_df0_with_dfx = 0;
    spoof_df0_id = 0;
    move_slow_to_chip = 0;
    force_move_slow_to_chip = 0;

    autoconfig_reset_all();
    printf("[AMIGA] Platform shutdown completed.\n");
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
        for (unsigned int i = 0; i < strlen(cfg->subsys); i++) {
            cfg->subsys[i] = tolower(cfg->subsys[i]);
        }
    }
}
