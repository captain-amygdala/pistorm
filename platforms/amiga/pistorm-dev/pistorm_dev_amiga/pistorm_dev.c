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

#define RETURN_CMDRES READSHORT(PI_CMDRESULT, short_val); return short_val;

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

unsigned short pi_shutdown_pi(unsigned short shutdown_code) {
	WRITESHORT(PI_CMD_SHUTDOWN, shutdown_code);

	RETURN_CMDRES;
}

unsigned short pi_confirm_shutdown(unsigned short shutdown_code) {
	WRITESHORT(PI_CMD_CONFIRMSHUTDOWN, shutdown_code);

	RETURN_CMDRES;
}

// Kickstart/Extended ROM stuff
unsigned short pi_remap_kickrom(char *filename) {
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITESHORT(PI_CMD_KICKROM, 1);

	RETURN_CMDRES;
}

unsigned short pi_remap_extrom(char *filename) {
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITESHORT(PI_CMD_EXTROM, 1);

	RETURN_CMDRES;
}

// File operation things
unsigned short pi_get_filesize(char *filename, unsigned int *file_size) {
	WRITELONG(PI_STR1, (unsigned int)filename);
	READLONG(PI_CMD_FILESIZE, *file_size);

	RETURN_CMDRES;
}

unsigned short pi_transfer_file(char *filename, unsigned char *dest_ptr) {
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITELONG(PI_PTR1, (unsigned int)dest_ptr);
	WRITESHORT(PI_CMD_TRANSFERFILE, 1);

	RETURN_CMDRES;
}

// PiSCSI stuff
// TODO: There's currently no way to read back what drives are mounted at which SCSI index.
unsigned short pi_piscsi_map_drive(char *filename, unsigned char index) {
	WRITESHORT(PI_WORD1, index);
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITESHORT(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_MAP);

	RETURN_CMDRES;
}

unsigned short pi_piscsi_unmap_drive(unsigned char index) {
	WRITESHORT(PI_WORD1, index);
	WRITESHORT(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_UNMAP);

	RETURN_CMDRES;
}

// For virtual removable media. Not yet implemented.
unsigned short pi_piscsi_insert_media(char *filename, unsigned char index) {
	WRITESHORT(PI_WORD1, index);
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITESHORT(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_INSERT);

	RETURN_CMDRES;
}

unsigned short pi_piscsi_eject_media(unsigned char index) {
	WRITESHORT(PI_WORD1, index);
	WRITESHORT(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_EJECT);

	RETURN_CMDRES;
}

// Config file stuff
unsigned short pi_load_config(char *filename) {
	WRITELONG(PI_STR1, (unsigned int)filename);
	WRITESHORT(PI_CMD_SWITCHCONFIG, PICFG_LOAD);

	RETURN_CMDRES;
}

void pi_reload_config() {
	WRITESHORT(PI_CMD_SWITCHCONFIG, PICFG_RELOAD);
}

void pi_load_default_config() {
	WRITESHORT(PI_CMD_SWITCHCONFIG, PICFG_DEFAULT);
}

unsigned short pi_handle_config(unsigned char cmd, char *str) {
	if (cmd == PICFG_LOAD) {
		WRITELONG(PI_STR1, (unsigned int)str);
	}
	WRITESHORT(PI_CMD_SWITCHCONFIG, cmd);

	RETURN_CMDRES;
}

// Generic stuff
#define SIMPLEWRITE_SHORT(a, b) \
    void a(unsigned short val) { WRITESHORT(b, val); }

// Simple feature status write functions
SIMPLEWRITE_SHORT(pi_enable_rtg, PI_CMD_RTGSTATUS);
SIMPLEWRITE_SHORT(pi_enable_net, PI_CMD_NETSTATUS);
SIMPLEWRITE_SHORT(pi_enable_piscsi, PI_CMD_RTGSTATUS);

// Generic feature status setting function.
// Example: pi_set_feature_status(PI_CMD_RTGSTATUS, 1) to enable RTG
//          pi_set_feature_status(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_ENABLE) to enable PiSCSI
void pi_set_feature_status(unsigned short cmd, unsigned char value) {
	WRITESHORT(cmd, value);
}

#define SIMPLEREAD_SHORT(a, b) \
    unsigned short a() { READSHORT(b, short_val); return short_val; }

// Simple feature status read functions
SIMPLEREAD_SHORT(pi_get_hw_rev, PI_CMD_HWREV);
SIMPLEREAD_SHORT(pi_get_sw_rev, PI_CMD_SWREV);
SIMPLEREAD_SHORT(pi_get_rtg_status, PI_CMD_RTGSTATUS);
SIMPLEREAD_SHORT(pi_get_net_status, PI_CMD_NETSTATUS);
SIMPLEREAD_SHORT(pi_get_piscsi_status, PI_CMD_PISCSI_CTRL);
SIMPLEREAD_SHORT(pi_get_cmd_result, PI_CMDRESULT);
