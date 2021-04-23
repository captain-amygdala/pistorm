// SPDX-License-Identifier: MIT

#include "../pistorm-dev-enums.h"
#include "pistorm_dev.h"

#include <exec/resident.h>
#include <exec/errors.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/tasks.h>
#include <exec/io.h>
#include <exec/execbase.h>

#include <libraries/expansion.h>

#include <devices/trackdisk.h>
#include <devices/timer.h>
#include <devices/scsidisk.h>

#include <dos/filehandler.h>

#include <proto/exec.h>
#include <proto/disk.h>
#include <proto/expansion.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOADLIB(a, b) if ((a = (struct a*)OpenLibrary(b,0L))==NULL) { \
    printf("Failed to load %s.\n", b); \
    return 1; \
  } \

void print_usage(char *exe);
int get_command(char *cmd);

extern unsigned int pistorm_base_addr;

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
        printf ("Unable to find PiStorm autoconf device.\n");
        return 1;
    }
    else {
        printf ("PiStorm autoconf device found at $%.X\n", pistorm_base_addr);
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
        default:
            printf ("Unhandled command %s.\n", argv[1]);
            return 1;
            break;
    }

    return 0;
}

int get_command(char *cmd) {
    if (strcmp(cmd, "--restart") == 0 || strcmp(cmd, "--reboot") || strcmp(cmd, "--reset") == 0) {
        return PI_CMD_RESET;
    }

    return -1;
}

void print_usage(char *exe) {
    printf ("Usage: %s --[command] (arguments)\n", exe);
    printf ("Example: %s --restart, --reboot or --reset\n", exe);
    printf ("         Restarts the Amiga.\n");
    printf ("         %s --check or --find\n", exe);
    printf ("         Finds the PiStorm device and prints some data.\n");

    return;
}
