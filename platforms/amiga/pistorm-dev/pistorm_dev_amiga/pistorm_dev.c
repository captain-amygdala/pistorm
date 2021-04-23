// SPDX-License-Identifier: MIT

#include "../pistorm-dev-enums.h"

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

#ifdef HAS_STDLIB
#include <stdio.h>
#endif

unsigned int pistorm_base_addr = 0xFFFFFFFF;

#define WRITESHORT(cmd, val) *(unsigned short *)((unsigned int)(pistorm_base_addr+cmd)) = val;
#define WRITELONG(cmd, val) *(unsigned int *)((unsigned int)(pistorm_base_addr+cmd)) = val;
#define WRITEBYTE(cmd, val) *(unsigned char *)((unsigned int)(pistorm_base_addr+cmd)) = val;

#define READSHORT(cmd, var) var = *(volatile unsigned short *)(pistorm_base_addr + cmd);
#define READLONG(cmd, var) var = *(volatile unsigned int *)(pistorm_base_addr + cmd);
#define READBYTE(cmd, var) var = *(volatile unsigned short *)(pistorm_base_addr + cmd);

unsigned short short_val;

unsigned int pi_find_pistorm() {
    unsigned int board_addr = 0xFFFFFFFF;
    struct ExpansionBase *expansionbase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0L);
	
    if (expansionbase == NULL) {
#ifdef HAS_STDLIB
	    printf("Failed to open expansion.library.\n");
#endif
  	}
	else {
		struct ConfigDev* cd = NULL;
		cd = (struct ConfigDev*)FindConfigDev(cd, 2011, 0x6B);
		if (cd != NULL)
			board_addr = (unsigned int)cd->cd_BoardAddr;
        CloseLibrary((struct Library *)expansionbase);
	}
    return board_addr;
}

void pi_reset_amiga(unsigned short reset_code) {
    WRITESHORT(PI_CMD_RESET, reset_code);
}

void pi_handle_config(unsigned char cmd, char *str) {
	if (cmd == PICFG_LOAD) {
		WRITELONG(PI_STR1, (unsigned int)str);
	}
	WRITESHORT(PI_CMD_SWITCHCONFIG, cmd);
}

#define SIMPLEREAD_SHORT(a, b) \
    unsigned short a() { READSHORT(b, short_val); return short_val; }

SIMPLEREAD_SHORT(pi_get_hw_rev, PI_CMD_HWREV);
SIMPLEREAD_SHORT(pi_get_sw_rev, PI_CMD_SWREV);
SIMPLEREAD_SHORT(pi_get_rtg_status, PI_CMD_RTGSTATUS);
SIMPLEREAD_SHORT(pi_get_net_status, PI_CMD_NETSTATUS);
