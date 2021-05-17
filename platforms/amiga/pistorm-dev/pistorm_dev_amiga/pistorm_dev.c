// SPDX-License-Identifier: MIT

#include "../pistorm-dev-enums.h"

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
#include <clib/expansion_protos.h>

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
unsigned int long_val;

unsigned int pi_find_pistorm(void) {
    unsigned int board_addr = 0xFFFFFFFF;
    struct ExpansionBase *expansionbase = (struct ExpansionBase *)OpenLibrary((STRPTR)"expansion.library", 0L);

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
	pistorm_base_addr = board_addr;
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

unsigned short pi_memcpy(unsigned char *dst, unsigned char *src, unsigned int size) {
	WRITELONG(PI_PTR1, (unsigned int)src);
	WRITELONG(PI_PTR2, (unsigned int)dst);
	WRITELONG(PI_CMD_MEMCPY, size);

	RETURN_CMDRES;
}

unsigned short pi_memset(unsigned char *dst, unsigned char val, unsigned int size) {
	WRITELONG(PI_PTR1, (unsigned int)dst);
	WRITEBYTE(PI_BYTE1, val);
	WRITELONG(PI_CMD_MEMSET, size);

	RETURN_CMDRES;
}

// Generic memory copyrect, assuming that the src/dst offsets are already adjusted for X/Y coordinates.
void pi_copyrect(unsigned char *dst, unsigned char *src,
				 unsigned short src_pitch, unsigned short dst_pitch,
				 unsigned short w, unsigned short h) {
	WRITELONG(PI_PTR1, (unsigned int)src);
	WRITELONG(PI_PTR2, (unsigned int)dst);
	WRITESHORT(PI_WORD1, src_pitch);
	WRITESHORT(PI_WORD2, dst_pitch);
	WRITESHORT(PI_WORD3, w);
	WRITESHORT(PI_WORD4, h);

	WRITESHORT(PI_CMD_COPYRECT, 1);
}

// Extended memory copyrect, allowing specifying of source/dest X/Y coordinates.
void pi_copyrect_ex(unsigned char *dst, unsigned char *src,
					unsigned short src_pitch, unsigned short dst_pitch,
					unsigned short src_x, unsigned short src_y,
					unsigned short dst_x, unsigned short dst_y,
					unsigned short w, unsigned short h) {
	WRITELONG(PI_PTR1, (unsigned int)src);
	WRITELONG(PI_PTR2, (unsigned int)dst);
	WRITESHORT(PI_WORD1, src_pitch);
	WRITESHORT(PI_WORD2, dst_pitch);
	WRITESHORT(PI_WORD3, w);
	WRITESHORT(PI_WORD4, h);

	WRITESHORT(PI_WORD5, src_x);
	WRITESHORT(PI_WORD6, src_y);
	WRITESHORT(PI_WORD7, dst_x);
	WRITESHORT(PI_WORD8, dst_y);

	WRITESHORT(PI_CMD_COPYRECT_EX, 1);
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

void pi_reload_config(void) {
	WRITESHORT(PI_CMD_SWITCHCONFIG, PICFG_RELOAD);
}

void pi_load_default_config(void) {
	WRITESHORT(PI_CMD_SWITCHCONFIG, PICFG_DEFAULT);
}

unsigned short pi_handle_config(unsigned char cmd, char *str) {
	if (cmd == PICFG_LOAD) {
		WRITELONG(PI_STR1, (unsigned int)str);
	}
	WRITESHORT(PI_CMD_SWITCHCONFIG, cmd);

	RETURN_CMDRES;
}

// Simple feature status write functions
void pi_enable_rtg(unsigned short val)
{
    WRITESHORT(PI_CMD_RTGSTATUS, val);
}
void pi_enable_net(unsigned short val)
{
    WRITESHORT(PI_CMD_NETSTATUS, val);
}
void pi_enable_piscsi(unsigned short val)
{
    WRITESHORT(PI_CMD_PISCSI_CTRL, val);
}

// Generic feature status setting function.
// Example: pi_set_feature_status(PI_CMD_RTGSTATUS, 1) to enable RTG
//          pi_set_feature_status(PI_CMD_PISCSI_CTRL, PISCSI_CTRL_ENABLE) to enable PiSCSI
void pi_set_feature_status(unsigned short cmd, unsigned char value) {
	WRITESHORT(cmd, value);
}

#define SIMPLEREAD_SHORT(a, b) \
    unsigned short a(void) { READSHORT(b, short_val); return short_val; }

unsigned int pi_get_fb(void) {
	READLONG(PI_CMD_GET_FB, long_val);
	return long_val;
}

// Simple feature status read functions
unsigned short pi_get_hw_rev(void)
{
    READSHORT(PI_CMD_HWREV, short_val);
    return short_val;
}
unsigned short pi_get_sw_rev(void)
{
    READSHORT(PI_CMD_SWREV, short_val);
    return short_val;
}
unsigned short pi_get_rtg_status(void)
{
    READSHORT(PI_CMD_RTGSTATUS, short_val);
    return short_val;
}
unsigned short pi_get_net_status(void)
{
    READSHORT(PI_CMD_NETSTATUS, short_val);
    return short_val;
}
unsigned short pi_get_piscsi_status(void)
{
    READSHORT(PI_CMD_PISCSI_CTRL, short_val);
    return short_val;
}
unsigned short pi_get_cmd_result(void)
{
    READSHORT(PI_CMDRESULT, short_val);
    return short_val;
}
