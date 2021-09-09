// SPDX-License-Identifier: MIT

#include "../pistorm-dev-enums.h"
#include "pistorm_dev.h"

//#define SHUTUP_VSCODE

#ifdef SHUTUP_VSCODE
#define __stdargs
#else
#include <exec/types.h>
#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>

#include <devices/trackdisk.h>
#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <libraries/filehandler.h>

#include <proto/exec.h>
#include <proto/disk.h>
#include <proto/expansion.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOADLIB(a, b) if ((a = (struct a*)OpenLibrary(b,0L))==NULL) { \
    printf("Failed to load %s.\n", b); \
    return 1; \
  }

void print_usage(char *exe);
int get_command(char *cmd);
unsigned short get_scale_mode(char *mode);
unsigned short get_scale_filter(char *mode);

extern unsigned int pistorm_base_addr;

unsigned short cmd_arg = 0;
unsigned short scale_rect = 0;

int __stdargs main (int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    int command = get_command(argv[1]);
    if (command == -1) {
        printf("Unknown command %s.\n", argv[1]);
        return 1;
    }

    pistorm_base_addr = pi_find_pistorm();

    if (pistorm_base_addr == 0xFFFFFFFF) {
        printf("Unable to find PiStorm autoconf device.\n");
        return 1;
    }
    else {
        printf("PiStorm autoconf device found at $%.X\n", pistorm_base_addr);
    }

    unsigned int tmpvalue = 0;
    unsigned short tmpshort = 0;

    if (tmpvalue) {};

    switch (command) {
        case PI_CMD_RESET:
            if (argc >= 3)
                tmpshort = (unsigned short)atoi(argv[2]);
            pi_reset_amiga(tmpshort);
            break;
        case PI_CMD_SHUTDOWN: {
            unsigned short shutdown_confirm = pi_shutdown_pi(0);
            pi_confirm_shutdown(shutdown_confirm);
            break;
        }
        case PI_CMD_SWREV:
            printf("PiStorm ----------------------------\n");
            printf("Hardware revision: %d.%d\n", (pi_get_hw_rev() >> 8), (pi_get_hw_rev() & 0xFF));
            printf("Software revision: %d.%d\n", (pi_get_sw_rev() >> 8), (pi_get_sw_rev() & 0xFF));
            printf("RTG: %s - %s\n", (pi_get_rtg_status() & 0x01) ? "Enabled" : "Disabled", (pi_get_rtg_status() & 0x02) ? "In use" : "Not in use");
            printf("NET: %s\n", pi_get_net_status() ? "Enabled" : "Disabled");
            printf("PiSCSI: %s\n", pi_get_piscsi_status() ? "Enabled" : "Disabled");
            break;
        case PI_CMD_SWITCHCONFIG:
            if (cmd_arg == PICFG_LOAD) {
                if (argc < 3) {
                    printf("User asked to load config, but no config filename specified.\n");
                }
            }
            pi_handle_config(cmd_arg, argv[2]);
            break;
        case PI_CMD_TRANSFERFILE:
            if (argc < 4) {
                printf("Please specify a source and destination filename in addition to the command.\n");
                printf("Example: %s --transfer platforms/platform.h snakes.h\n", argv[0]);
            }
            if (pi_get_filesize(argv[2], &tmpvalue) == PI_RES_FILENOTFOUND) {
                printf("File %s not found on the Pi side.\n", argv[2]);
            } else {
                unsigned int filesize = tmpvalue;
                unsigned char *dest = calloc(filesize, 1);

                if (dest == NULL) {
                    printf("Failed to allocate memory buffer for file. Aborting file transfer.\n");
                } else {
                    printf("Found a %u byte file on the Pi side. Eating it.\n", filesize);
                    if (pi_transfer_file(argv[2], dest) != PI_RES_OK) {
                        printf("Something went horribly wrong during the file transfer.\n");
                    } else {
                        FILE *out = fopen(argv[3], "wb+");
                        if (out == NULL) {
                            printf("Failed to open output file %s for writing.\n", argv[3]);
                        } else {
                            fwrite(dest, filesize, 1, out);
                            fclose(out);
                            printf("%u bytes transferred to file %s.\n", filesize, argv[3]);
                        }
                    }
                    free(dest);
                }
            }
            break;
        case PI_CMD_GET_TEMP: {
            unsigned short temp = pi_get_temperature();
            if (temp == 0) {
                printf("Error getting temperature\n");
            } else {
                printf("CPU temp: %u%cC\n", temp, 0xb0);
            }
            break;
        }
        case PI_CMD_RTG_SCALE_FILTER: {
            if (argc < 3) {
                printf("Failed to set RTG scale filter: No scale filter specified.\n");
                break;
            }
            unsigned short scale_filter = get_scale_filter(argv[2]);
            if (scale_filter != PIGFX_FILTER_NUM) {
                pi_set_rtg_scale_filter(scale_filter);
            } else {
                printf("Failed to set RTG scale filter: Unknown scale filter %s.\n", argv[2]);
            }

            break;
        }
        case PI_CMD_RTG_SCALING:
            if (scale_rect != 0) {
                if (argc < 6) {
                    printf("Missing command line arguments for RTG scale rect. Example:\n");
                    if (scale_rect == 1) {
                        printf("%s --rtg-rect 16 16 640 480\n", argv[0]);
                        printf("Arguments being a rect: [X] [Y] [Width] [Height]\n");
                    }
                    if (scale_rect == 2) {
                        printf("%s --rtg-rect 16 16 1264 705\n", argv[0]);
                        printf("Arguments being coordinates of rect corners: [X] [Y] [X2] [Y2]\n");
                    }
                    break;
                } else {
                    signed short x1, y1, x2, y2;
                    x1 = atoi(argv[2]);
                    y1 = atoi(argv[3]);
                    x2 = atoi(argv[4]);
                    y2 = atoi(argv[5]);
                    pi_set_rtg_scale_rect(scale_rect == 1 ? PIGFX_SCALE_CUSTOM_RECT : PIGFX_SCALE_CUSTOM, x1, y1, x2, y2);
                    break;
                }
            } else {
                if (argc < 3) {
                    printf("Failed to set RTG scale mode: No scale mode type specified.\n");
                    break;
                }
                unsigned short scale_mode = get_scale_mode(argv[2]);
                if (scale_mode != PIGFX_SCALE_NUM) {
                    pi_set_rtg_scale_mode(scale_mode);
                } else {
                    printf("Failed to set RTG scale mode: Unknown RTG scale mode %s.\n", argv[2]);
                    printf("Valid options are 4:3, 16:9, 1:1, full, aspect and integer\n");
                }
            }
            break;
        default:
            printf("Unhandled command %s.\n", argv[1]);
            return 1;
            break;
    }

    return 0;
}

int get_command(char *cmd) {
    if (strcmp(cmd, "--shutdown") == 0 || strcmp(cmd, "--safe-shutdown") == 0 || strcmp(cmd, "--:)") == 0) {
        return PI_CMD_SHUTDOWN;
    }
    if (strcmp(cmd, "--restart") == 0 || strcmp(cmd, "--reboot") == 0 || strcmp(cmd, "--reset") == 0) {
        return PI_CMD_RESET;
    }
    if (strcmp(cmd, "--check") == 0 || strcmp(cmd, "--find") == 0 || strcmp(cmd, "--info") == 0) {
        return PI_CMD_SWREV;
    }
    if (strcmp(cmd, "--config") == 0 || strcmp(cmd, "--config-file") == 0 || strcmp(cmd, "--cfg") == 0) {
        cmd_arg = PICFG_LOAD;
        return PI_CMD_SWITCHCONFIG;
    }
    if (strcmp(cmd, "--config-reload") == 0 || strcmp(cmd, "--reload-config") == 0 || strcmp(cmd, "--reloadcfg") == 0) {
        cmd_arg = PICFG_RELOAD;
        return PI_CMD_SWITCHCONFIG;
    }
    if (strcmp(cmd, "--config-default") == 0 || strcmp(cmd, "--default-config") == 0 || strcmp(cmd, "--defcfg") == 0) {
        cmd_arg = PICFG_DEFAULT;
        return PI_CMD_SWITCHCONFIG;
    }
    if (strcmp(cmd, "--transfer-file") == 0 || strcmp(cmd, "--transfer") == 0 || strcmp(cmd, "--getfile") == 0) {
        return PI_CMD_TRANSFERFILE;
    }
    if (strcmp(cmd, "--get-temperature") == 0 || strcmp(cmd, "--get-temp") == 0 || strcmp(cmd, "--temp") == 0) {
        return PI_CMD_GET_TEMP;
    }

    if (strcmp(cmd, "--rtg-scaling") == 0 || strcmp(cmd, "--rtg-scale-mode") == 0 || strcmp(cmd, "--rtgscale") == 0) {
        return PI_CMD_RTG_SCALING;
    }
    if (strcmp(cmd, "--rtg-rect") == 0 || strcmp(cmd, "--rtg-scale-rect") == 0 || strcmp(cmd, "--rtgrect") == 0) {
        scale_rect = 1;
        return PI_CMD_RTG_SCALING;
    }
    if (strcmp(cmd, "--rtg-custom") == 0 || strcmp(cmd, "--rtg-scale-custom") == 0 || strcmp(cmd, "--rtgcustom") == 0) {
        scale_rect = 2;
        return PI_CMD_RTG_SCALING;
    }
    if (strcmp(cmd, "--rtg-filter") == 0 || strcmp(cmd, "--rtg-scale-filter") == 0 || strcmp(cmd, "--rtgfilter") == 0) {
        return PI_CMD_RTG_SCALE_FILTER;
    }

    return -1;
}

unsigned short get_scale_mode(char *mode) {
    if (strcmp(mode, "4:3") == 0) return PIGFX_SCALE_FULL_43;
    if (strcmp(mode, "16:9") == 0) return PIGFX_SCALE_FULL_169;
    if (strcmp(mode, "1:1") == 0) return PIGFX_SCALE_NONE;
    if (strcmp(mode, "full") == 0) return PIGFX_SCALE_FULL;
    if (strcmp(mode, "aspect") == 0) return PIGFX_SCALE_FULL_ASPECT;
    if (strcmp(mode, "integer") == 0) return PIGFX_SCALE_INTEGER_MAX;

    return PIGFX_SCALE_NUM;
}

unsigned short get_scale_filter(char *mode) {
    if (strcmp(mode, "sharp") == 0) return PIGFX_FILTER_POINT;
    if (strcmp(mode, "point") == 0) return PIGFX_FILTER_POINT;
    if (strcmp(mode, "smooth") == 0) return PIGFX_FILTER_SMOOTH;
    if (strcmp(mode, "blurry") == 0) return PIGFX_FILTER_SMOOTH;
    if (strcmp(mode, "bilinear") == 0) return PIGFX_FILTER_SMOOTH;
    if (strcmp(mode, "linear") == 0) return PIGFX_FILTER_SMOOTH;
    if (strcmp(mode, "shader") == 0) return PIGFX_FILTER_SHADER;

    return PIGFX_FILTER_NUM;
}

void print_usage(char *exe) {
    printf("Usage: %s --[command] (arguments)\n", exe);
    printf("Example: %s --restart, --reboot or --reset\n", exe);
    printf("         Restarts the Amiga.\n");
    printf("         %s --check, --find or --info\n", exe);
    printf("         Finds the PiStorm device and prints some data.\n");

    return;
}
