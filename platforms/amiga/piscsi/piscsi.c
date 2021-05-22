// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>

#include "config_file/config_file.h"
#include "gpio/ps_protocol.h"
#include "piscsi-enums.h"
#include "piscsi.h"
#include "platforms/amiga/hunk-reloc.h"

#define BE(val) be32toh(val)
#define BE16(val) be16toh(val)

// Uncomment the line below to enable debug output
//#define PISCSI_DEBUG

#ifdef PISCSI_DEBUG
#define DEBUG printf
//#define DEBUG_TRIVIAL printf
#define DEBUG_TRIVIAL(...)

//extern void stop_cpu_emulation(uint8_t disasm_cur);
#define stop_cpu_emulation(...)

static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};
#else
#define DEBUG(...)
#define DEBUG_TRIVIAL(...)
#define stop_cpu_emulation(...)
#endif

#ifdef FAKESTORM
#define lseek64 lseek
#endif

extern struct emulator_config *cfg;

struct piscsi_dev devs[8];
struct piscsi_fs filesystems[NUM_FILESYSTEMS];

uint8_t piscsi_num_fs = 0;

uint8_t piscsi_cur_drive = 0;
uint32_t piscsi_u32[4];
uint32_t piscsi_dbg[8];
uint32_t piscsi_rom_size = 0;
uint8_t *piscsi_rom_ptr;

uint32_t rom_partitions[128];
uint32_t rom_partition_prio[128];
uint32_t rom_partition_dostype[128];
uint32_t rom_cur_partition = 0, rom_cur_fs = 0;

extern unsigned char ac_piscsi_rom[];

//static const char *partition_marker = "PART";

struct hunk_info piscsi_hinfo;
struct hunk_reloc piscsi_hreloc[256];

void piscsi_init() {
    for (int i = 0; i < 8; i++) {
        devs[i].fd = -1;
        devs[i].lba = 0;
        devs[i].c = devs[i].h = devs[i].s = 0;
    }

    if (piscsi_rom_ptr == NULL) {
        FILE *in = fopen("./platforms/amiga/piscsi/piscsi.rom", "rb");
        if (in == NULL) {
            printf("[PISCSI] Could not open PISCSI Boot ROM file for reading!\n");
            // Zero out the boot ROM offset from the autoconfig ROM.
            ac_piscsi_rom[20] = 0;
            ac_piscsi_rom[21] = 0;
            ac_piscsi_rom[22] = 0;
            ac_piscsi_rom[23] = 0;
            return;
        }
        fseek(in, 0, SEEK_END);
        piscsi_rom_size = ftell(in);
        fseek(in, 0, SEEK_SET);
        piscsi_rom_ptr = malloc(piscsi_rom_size);
        fread(piscsi_rom_ptr, piscsi_rom_size, 1, in);

        fseek(in, PISCSI_DRIVER_OFFSET, SEEK_SET);
        process_hunks(in, &piscsi_hinfo, piscsi_hreloc, PISCSI_DRIVER_OFFSET);

        fclose(in);

        printf("[PISCSI] Loaded Boot ROM.\n");
    } else {
        printf("[PISCSI] Boot ROM already loaded.\n");
    }
    fflush(stdout);
}

void piscsi_shutdown() {
    printf("[PISCSI] Shutting down PiSCSI.\n");
    for (int i = 0; i < 8; i++) {
        if (devs[i].fd != -1) {
            close(devs[i].fd);
            devs[i].fd = -1;
            devs[i].block_size = 0;
        }
    }

    for (int i = 0; i < NUM_FILESYSTEMS; i++) {
        if (filesystems[i].binary_data) {
            free(filesystems[i].binary_data);
            filesystems[i].binary_data = NULL;
        }
        if (filesystems[i].fhb) {
            free(filesystems[i].fhb);
            filesystems[i].fhb = NULL;
        }
        filesystems[i].h_info.current_hunk = 0;
        filesystems[i].h_info.reloc_hunks = 0;
        filesystems[i].FS_ID = 0;
        filesystems[i].handler = 0;
    }
}

void piscsi_find_partitions(struct piscsi_dev *d) {
    int fd = d->fd;
    int cur_partition = 0;
    uint8_t tmp;

    for (int i = 0; i < 16; i++) {
        if (d->pb[i]) {
            free(d->pb[i]);
            d->pb[i] = NULL;
        }
    }

    if (!d->rdb || d->rdb->rdb_PartitionList == 0) {
        DEBUG("[PISCSI] No partitions on disk.\n");
        return;
    }

    char *block = malloc(d->block_size);

    lseek(fd, BE(d->rdb->rdb_PartitionList) * d->block_size, SEEK_SET);
next_partition:;
    read(fd, block, d->block_size);

    uint32_t first = be32toh(*((uint32_t *)&block[0]));
    if (first != PART_IDENTIFIER) {
        DEBUG("Entry at block %d is not a valid partition. Aborting.\n", BE(d->rdb->rdb_PartitionList));
        return;
    }

    struct PartitionBlock *pb = (struct PartitionBlock *)block;
    tmp = pb->pb_DriveName[0];
    pb->pb_DriveName[tmp + 1] = 0x00;
    DEBUG("[PISCSI] Partition %d: %s\n", cur_partition, pb->pb_DriveName + 1);
    DEBUG("Checksum: %.8X HostID: %d\n", BE(pb->pb_ChkSum), BE(pb->pb_HostID));
    DEBUG("Flags: %d (%.8X) Devflags: %d (%.8X)\n", BE(pb->pb_Flags), BE(pb->pb_Flags), BE(pb->pb_DevFlags), BE(pb->pb_DevFlags));
    d->pb[cur_partition] = pb;

    if (d->pb[cur_partition]->pb_Next != 0xFFFFFFFF) {
        uint64_t next = be32toh(pb->pb_Next);
        block = malloc(d->block_size);
        lseek64(fd, next * d->block_size, SEEK_SET);
        cur_partition++;
        DEBUG("[PISCSI] Next partition at block %d.\n", be32toh(pb->pb_Next));
        goto next_partition;
    }
    DEBUG("[PISCSI] No more partitions on disk.\n");
    d->num_partitions = cur_partition + 1;
    d->fshd_offs = lseek64(fd, 0, SEEK_CUR);

    return;
}

int piscsi_parse_rdb(struct piscsi_dev *d) {
    int fd = d->fd;
    int i = 0;
    uint8_t *block = malloc(PISCSI_MAX_BLOCK_SIZE);

    lseek(fd, 0, SEEK_SET);
    for (i = 0; i < RDB_BLOCK_LIMIT; i++) {
        read(fd, block, PISCSI_MAX_BLOCK_SIZE);
        uint32_t first = be32toh(*((uint32_t *)&block[0]));
        if (first == RDB_IDENTIFIER)
            goto rdb_found;
    }
    goto no_rdb_found;
rdb_found:;
    struct RigidDiskBlock *rdb = (struct RigidDiskBlock *)block;
    DEBUG("[PISCSI] RDB found at block %d.\n", i);
    d->c = be32toh(rdb->rdb_Cylinders);
    d->h = be32toh(rdb->rdb_Heads);
    d->s = be32toh(rdb->rdb_Sectors);
    d->num_partitions = 0;
    DEBUG("[PISCSI] RDB - first partition at block %d.\n", be32toh(rdb->rdb_PartitionList));
    d->block_size = be32toh(rdb->rdb_BlockBytes);
    DEBUG("[PISCSI] Block size: %d. (%d)\n", be32toh(rdb->rdb_BlockBytes), d->block_size);
    if (d->rdb)
        free(d->rdb);
    d->rdb = rdb;
    sprintf(d->rdb->rdb_DriveInitName, "pi-scsi.device");
    return 0;

no_rdb_found:;
    if (block)
        free(block);

    return -1;
}

void piscsi_refresh_drives() {
    piscsi_num_fs = 0;

    for (int i = 0; i < NUM_FILESYSTEMS; i++) {
        if (filesystems[i].binary_data) {
            free(filesystems[i].binary_data);
            filesystems[i].binary_data = NULL;
        }
        if (filesystems[i].fhb) {
            free(filesystems[i].fhb);
            filesystems[i].fhb = NULL;
        }
        filesystems[i].h_info.current_hunk = 0;
        filesystems[i].h_info.reloc_hunks = 0;
        filesystems[i].FS_ID = 0;
        filesystems[i].handler = 0;
    }

    rom_cur_fs = 0;

    for (int i = 0; i < NUM_UNITS; i++) {
        if (devs[i].fd != -1) {
            piscsi_parse_rdb(&devs[i]);
            piscsi_find_partitions(&devs[i]);
            piscsi_find_filesystems(&devs[i]);
        }
    }
}

void piscsi_find_filesystems(struct piscsi_dev *d) {
    if (!d->num_partitions)
        return;

    uint8_t fs_found = 0;

    uint8_t *fhb_block = malloc(d->block_size);

    lseek64(d->fd, d->fshd_offs, SEEK_SET);

    struct FileSysHeaderBlock *fhb = (struct FileSysHeaderBlock *)fhb_block;
    read(d->fd, fhb_block, d->block_size);

    while (BE(fhb->fhb_ID) == FS_IDENTIFIER) {
        char *dosID = (char *)&fhb->fhb_DosType;
#ifdef PISCSI_DEBUG
        uint16_t *fsVer = (uint16_t *)&fhb->fhb_Version;

        DEBUG("[FSHD] FSHD Block found.\n");
        DEBUG("[FSHD] HostID: %d Next: %d Size: %d\n", BE(fhb->fhb_HostID), BE(fhb->fhb_Next), BE(fhb->fhb_SummedLongs));
        DEBUG("[FSHD] Flags: %.8X DOSType: %c%c%c/%d\n", BE(fhb->fhb_Flags), dosID[0], dosID[1], dosID[2], dosID[3]);
        DEBUG("[FSHD] Version: %d.%d\n", BE16(fsVer[0]), BE16(fsVer[1]));
        DEBUG("[FSHD] Patchflags: %d Type: %d\n", BE(fhb->fhb_PatchFlags), BE(fhb->fhb_Type));
        DEBUG("[FSHD] Task: %d Lock: %d\n", BE(fhb->fhb_Task), BE(fhb->fhb_Lock));
        DEBUG("[FSHD] Handler: %d StackSize: %d\n", BE(fhb->fhb_Handler), BE(fhb->fhb_StackSize));
        DEBUG("[FSHD] Prio: %d Startup: %d\n", BE(fhb->fhb_Priority), BE(fhb->fhb_Startup));
        DEBUG("[FSHD] SegListBlocks: %d GlobalVec: %d\n", BE(fhb->fhb_Priority), BE(fhb->fhb_Startup));
        DEBUG("[FSHD] FileSysName: %s\n", fhb->fhb_FileSysName + 1);
#endif

        for (int i = 0; i < NUM_FILESYSTEMS; i++) {
            if (filesystems[i].FS_ID == fhb->fhb_DosType) {
                DEBUG("[FSHD] File system %c%c%c/%d already loaded. Skipping.\n", dosID[0], dosID[1], dosID[2], dosID[3]);
                if (BE(fhb->fhb_Next) == 0xFFFFFFFF)
                    goto fs_done;

                goto skip_fs_load_lseg;
            }
        }

        if (load_lseg(d->fd, &filesystems[piscsi_num_fs].binary_data, &filesystems[piscsi_num_fs].h_info, filesystems[piscsi_num_fs].relocs, d->block_size) != -1) {
            filesystems[piscsi_num_fs].FS_ID = fhb->fhb_DosType;
            filesystems[piscsi_num_fs].fhb = fhb;
            printf("[FSHD] Loaded and set up file system %d: %c%c%c/%d\n", piscsi_num_fs + 1, dosID[0], dosID[1], dosID[2], dosID[3]);
            piscsi_num_fs++;
        }

skip_fs_load_lseg:;
        fs_found++;
        lseek64(d->fd, BE(fhb->fhb_Next) * d->block_size, SEEK_SET);
        fhb_block = malloc(d->block_size);
        fhb = (struct FileSysHeaderBlock *)fhb_block;
        read(d->fd, fhb_block, d->block_size);
    }

    if (!fs_found) {
        DEBUG("[!!!FSHD] No file systems found on hard drive!\n");
    }

fs_done:;
    if (fhb_block)
        free(fhb_block);
}

struct piscsi_dev *piscsi_get_dev(uint8_t index) {
    return &devs[index];
}

void piscsi_map_drive(char *filename, uint8_t index) {
    if (index > 7) {
        printf("[PISCSI] Drive index %d out of range.\nUnable to map file %s to drive.\n", index, filename);
        return;
    }

    int32_t tmp_fd = open(filename, O_RDWR);
    if (tmp_fd == -1) {
        printf("[PISCSI] Failed to open file %s, could not map drive %d.\n", filename, index);
        return;
    }

    struct piscsi_dev *d = &devs[index];

    uint64_t file_size = lseek(tmp_fd, 0, SEEK_END);
    d->fs = file_size;
    d->fd = tmp_fd;
    lseek(tmp_fd, 0, SEEK_SET);
    printf("[PISCSI] Map %d: [%s] - %llu bytes.\n", index, filename, file_size);

    if (piscsi_parse_rdb(d) == -1) {
        DEBUG("[PISCSI] No RDB found on disk, making up some CHS values.\n");
        d->h = 16;
        d->s = 63;
        d->c = (file_size / 512) / (d->s * d->h);
        d->block_size = 512;
    }
    printf("[PISCSI] CHS: %d %d %d\n", d->c, d->h, d->s);

    printf ("Finding partitions.\n");
    piscsi_find_partitions(d);
    printf ("Finding file systems.\n");
    piscsi_find_filesystems(d);
    printf ("Done.\n");
}

void piscsi_unmap_drive(uint8_t index) {
    if (devs[index].fd != -1) {
        DEBUG("[PISCSI] Unmapped drive %d.\n", index);
        close (devs[index].fd);
        devs[index].fd = -1;
    }
}

char *io_cmd_name(int index) {
    switch (index) {
        case CMD_INVALID: return "INVALID";
        case CMD_RESET: return "RESET";
        case CMD_READ: return "READ";
        case CMD_WRITE: return "WRITE";
        case CMD_UPDATE: return "UPDATE";
        case CMD_CLEAR: return "CLEAR";
        case CMD_STOP: return "STOP";
        case CMD_START: return "START";
        case CMD_FLUSH: return "FLUSH";
        case TD_MOTOR: return "TD_MOTOR";
        case TD_SEEK: return "SEEK";
        case TD_FORMAT: return "FORMAT";
        case TD_REMOVE: return "REMOVE";
        case TD_CHANGENUM: return "CHANGENUM";
        case TD_CHANGESTATE: return "CHANGESTATE";
        case TD_PROTSTATUS: return "PROTSTATUS";
        case TD_RAWREAD: return "RAWREAD";
        case TD_RAWWRITE: return "RAWWRITE";
        case TD_GETDRIVETYPE: return "GETDRIVETYPE";
        case TD_GETNUMTRACKS: return "GETNUMTRACKS";
        case TD_ADDCHANGEINT: return "ADDCHANGEINT";
        case TD_REMCHANGEINT: return "REMCHANGEINT";
        case TD_GETGEOMETRY: return "GETGEOMETRY";
        case TD_EJECT: return "EJECT";
        case TD_LASTCOMM: return "LASTCOMM/READ64";
        case TD_WRITE64: return "WRITE64";
        case HD_SCSICMD: return "HD_SCSICMD";
        case NSCMD_DEVICEQUERY: return "NSCMD_DEVICEQUERY";
        case NSCMD_TD_READ64: return "NSCMD_TD_READ64";
        case NSCMD_TD_WRITE64: return "NSCMD_TD_WRITE64";
        case NSCMD_TD_FORMAT64: return "NSCMD_TD_FORMAT64";

        default:
            return "[!!!PISCSI] Unhandled IO command";
    }
}

#define GETSCSINAME(a) case a: return ""#a"";
#define SCSIUNHANDLED(a) return "[!!!PISCSI] Unhandled SCSI command "#a"";

char *scsi_cmd_name(int index) {
    switch(index) {
        GETSCSINAME(SCSICMD_TEST_UNIT_READY);
        GETSCSINAME(SCSICMD_INQUIRY);
        GETSCSINAME(SCSICMD_READ_6);
        GETSCSINAME(SCSICMD_WRITE_6);
        GETSCSINAME(SCSICMD_READ_10);
        GETSCSINAME(SCSICMD_WRITE_10);
        GETSCSINAME(SCSICMD_READ_CAPACITY_10);
        GETSCSINAME(SCSICMD_MODE_SENSE_6);
        GETSCSINAME(SCSICMD_READ_DEFECT_DATA_10);
        default:
            return "[!!!PISCSI] Unhandled SCSI command";
    }
}

void print_piscsi_debug_message(int index) {
    int32_t r = 0;

    switch (index) {
        case DBG_INIT:
            DEBUG("[PISCSI] Initializing devices.\n");
            break;
        case DBG_OPENDEV:
            if (piscsi_dbg[0] != 255) {
                DEBUG("[PISCSI] Opening device %d (%d). Flags: %d (%.2X)\n", piscsi_dbg[0], piscsi_dbg[2], piscsi_dbg[1], piscsi_dbg[1]);
            }
            break;
        case DBG_CLEANUP:
            DEBUG("[PISCSI] Cleaning up.\n");
            break;
        case DBG_CHS:
            DEBUG("[PISCSI] C/H/S: %d / %d / %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_BEGINIO:
            DEBUG("[PISCSI] BeginIO: io_Command: %d (%s) - io_Flags = %d - quick: %d\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]), piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_ABORTIO:
            DEBUG("[PISCSI] AbortIO!\n");
            break;
        case DBG_SCSICMD:
            DEBUG("[PISCSI] SCSI Command %d (%s)\n", piscsi_dbg[1], scsi_cmd_name(piscsi_dbg[1]));
            DEBUG("Len: %d - %.2X %.2X %.2X - Command Length: %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2], piscsi_dbg[3], piscsi_dbg[4]);
            break;
        case DBG_SCSI_UNKNOWN_MODESENSE:
            DEBUG("[!!!PISCSI] SCSI: Unknown modesense %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSI_UNKNOWN_COMMAND:
            DEBUG("[!!!PISCSI] SCSI: Unknown command %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSIERR:
            DEBUG("[!!!PISCSI] SCSI: An error occured: %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_IOCMD:
            DEBUG_TRIVIAL("[PISCSI] IO Command %d (%s)\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
            break;
        case DBG_IOCMD_UNHANDLED:
            DEBUG("[!!!PISCSI] WARN: IO command %.4X (%s) is unhandled by driver.\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
            break;
        case DBG_SCSI_FORMATDEVICE:
            DEBUG("[PISCSI] Get SCSI FormatDevice MODE SENSE.\n");
            break;
        case DBG_SCSI_RDG:
            DEBUG("[PISCSI] Get SCSI RDG MODE SENSE.\n");
            break;
        case DBG_SCSICMD_RW10:
#ifdef PISCSI_DEBUG
            r = get_mapped_item_by_address(cfg, piscsi_dbg[0]);
            struct SCSICmd_RW10 *rwdat = NULL;
            char data[10];
            if (r != -1) {
                uint32_t addr = piscsi_dbg[0] - cfg->map_offset[r];
                rwdat = (struct SCSICmd_RW10 *)(&cfg->map_data[r][addr]);
            }
            else {
                DEBUG_TRIVIAL("[RW10] scsiData: %.8X\n", piscsi_dbg[0]);
                for (int i = 0; i < 10; i++) {
                    data[i] = read8(piscsi_dbg[0] + i);
                }
                rwdat = data;
            }
            if (rwdat) {
                DEBUG_TRIVIAL("[RW10] CMD: %.2X\n", rwdat->opcode);
                DEBUG_TRIVIAL("[RW10] RDP: %.2X\n", rwdat->rdprotect_flags);
                DEBUG_TRIVIAL("[RW10] Block: %d (%d)\n", rwdat->block, BE(rwdat->block));
                DEBUG_TRIVIAL("[RW10] Res_Group: %.2X\n", rwdat->res_groupnum);
                DEBUG_TRIVIAL("[RW10] Len: %d (%d)\n", rwdat->len, BE16(rwdat->len));
            }
#endif
            break;
        case DBG_SCSI_DEBUG_MODESENSE_6:
            DEBUG_TRIVIAL("[PISCSI] SCSI ModeSense debug. Data: %.8X\n", piscsi_dbg[0]);
            r = get_mapped_item_by_address(cfg, piscsi_dbg[0]);
            if (r != -1) {
#ifdef PISCSI_DEBUG
                uint32_t addr = piscsi_dbg[0] - cfg->map_offset[r];
                struct SCSICmd_ModeSense6 *sense = (struct SCSICmd_ModeSense6 *)(&cfg->map_data[r][addr]);
                DEBUG_TRIVIAL("[SenseData] CMD: %.2X\n", sense->opcode);
                DEBUG_TRIVIAL("[SenseData] DBD: %d\n", sense->reserved_dbd & 0x04);
                DEBUG_TRIVIAL("[SenseData] PC: %d\n", (sense->pc_pagecode & 0xC0 >> 6));
                DEBUG_TRIVIAL("[SenseData] PageCodes: %.2X %.2X\n", (sense->pc_pagecode & 0x3F), sense->subpage_code);
                DEBUG_TRIVIAL("[SenseData] AllocLen: %d\n", sense->alloc_len);
                DEBUG_TRIVIAL("[SenseData] Control: %.2X (%d)\n", sense->control, sense->control);
#endif
            }
            else {
                DEBUG("[!!!PISCSI] ModeSense data not immediately available.\n");
            }
            break;
        default:
            DEBUG("[!!!PISCSI] No debug message available for index %d.\n", index);
            break;
    }
}

#define DEBUGME_SIMPLE(i, s) case i: DEBUG(s); break;

void piscsi_debugme(uint32_t index) {
    switch (index) {
        DEBUGME_SIMPLE(1, "[PISCSI-DEBUGME] Arrived at DiagEntry.\n");
        DEBUGME_SIMPLE(3, "[PISCSI-DEBUGME] Init: Interrupt disable.\n");
        DEBUGME_SIMPLE(4, "[PISCSI-DEBUGME] Init: Copy/reloc driver.\n");
        DEBUGME_SIMPLE(5, "[PISCSI-DEBUGME] Init: InitResident.\n");
        DEBUGME_SIMPLE(7, "[PISCSI-DEBUGME] Init: Begin partition loop.\n");
        DEBUGME_SIMPLE(8, "[PISCSI-DEBUGME] Init: Partition loop done. Cleaning up and returning to Exec.\n");
        DEBUGME_SIMPLE(9, "[PISCSI-DEBUGME] Init: Load file systems.\n");
        DEBUGME_SIMPLE(10, "[PISCSI-DEBUGME] Init: AllocMem for resident.\n");
        DEBUGME_SIMPLE(11, "[PISCSI-DEBUGME] Init: Checking if resident is loaded.\n");
        DEBUGME_SIMPLE(22, "[PISCSI-DEBUGME] Arrived at BootEntry.\n");
        DEBUGME_SIMPLE(30, "[PISCSI-DEBUGME] LoadFileSystems: Opening FileSystem.resource.\n");
        DEBUGME_SIMPLE(33, "[PISCSI-DEBUGME] FileSystem.resource not available, creating.\n");
        case 31:
            DEBUG("[PISCSI-DEBUGME] OpenResource result: %d\n", piscsi_u32[0]);
            break;
        case 32:
            DEBUG("AAAAHH!\n");
            break;
        default:
            DEBUG("[!!!PISCSI-DEBUGME] No debugme message for index %d!\n", index);
            break;
    }

    if (index == 8) {
        stop_cpu_emulation(1);
    }
}

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type) {
    int32_t r;
    uint8_t *map;
#ifndef PISCSI_DEBUG
    if (type) {}
#endif

    struct piscsi_dev *d = &devs[piscsi_cur_drive];

    uint16_t cmd = (addr & 0xFFFF);

    switch (cmd) {
        case PISCSI_CMD_READ64:
        case PISCSI_CMD_READ:
        case PISCSI_CMD_READBYTES:
            d = &devs[val];
            if (d->fd == -1) {
                DEBUG("[!!!PISCSI] BUG: Attempted read from unmapped drive %d.\n", val);
                break;
            }

            if (cmd == PISCSI_CMD_READBYTES) {
                DEBUG("[PISCSI-%d] %d byte READBYTES from block %d to address %.8X\n", val, piscsi_u32[1], piscsi_u32[0] / d->block_size, piscsi_u32[2]);
                uint32_t src = piscsi_u32[0];
                d->lba = (src / d->block_size);
                lseek(d->fd, src, SEEK_SET);
            }
            else if (cmd == PISCSI_CMD_READ) {
                DEBUG("[PISCSI-%d] %d byte READ from block %d to address %.8X\n", val, piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * d->block_size), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                DEBUG("[PISCSI-%d] %d byte READ64 from block %lld to address %.8X\n", val, piscsi_u32[1], (src / d->block_size), piscsi_u32[2]);
                d->lba = (src / d->block_size);
                lseek64(d->fd, src, SEEK_SET);
            }

            map = get_mapped_data_pointer_by_address(cfg, piscsi_u32[2]);
            if (map) {
                DEBUG_TRIVIAL("[PISCSI-%d] \"DMA\" Read goes to mapped range %d.\n", val, r);
                read(d->fd, map, piscsi_u32[1]);
            }
            else {
                DEBUG_TRIVIAL("[PISCSI-%d] No mapped range found for read.\n", val);
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    read(d->fd, &c, 1);
                    m68k_write_memory_8(piscsi_u32[2] + i, (uint32_t)c);
                }
            }
            break;
        case PISCSI_CMD_WRITE64:
        case PISCSI_CMD_WRITE:
        case PISCSI_CMD_WRITEBYTES:
            d = &devs[val];
            if (d->fd == -1) {
                DEBUG ("[PISCSI] BUG: Attempted write to unmapped drive %d.\n", val);
                break;
            }

            if (cmd == PISCSI_CMD_WRITEBYTES) {
                DEBUG("[PISCSI-%d] %d byte WRITEBYTES to block %d from address %.8X\n", val, piscsi_u32[1], piscsi_u32[0] / d->block_size, piscsi_u32[2]);
                uint32_t src = piscsi_u32[0];
                d->lba = (src / d->block_size);
                lseek(d->fd, src, SEEK_SET);
            }
            else if (cmd == PISCSI_CMD_WRITE) {
                DEBUG("[PISCSI-%d] %d byte WRITE to block %d from address %.8X\n", val, piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * d->block_size), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                DEBUG("[PISCSI-%d] %d byte WRITE64 to block %lld from address %.8X\n", val, piscsi_u32[1], (src / d->block_size), piscsi_u32[2]);
                d->lba = (src / d->block_size);
                lseek64(d->fd, src, SEEK_SET);
            }

            map = get_mapped_data_pointer_by_address(cfg, piscsi_u32[2]);
            if (map) {
                DEBUG_TRIVIAL("[PISCSI-%d] \"DMA\" Write comes from mapped range %d.\n", val, r);
                write(d->fd, map, piscsi_u32[1]);
            }
            else {
                DEBUG_TRIVIAL("[PISCSI-%d] No mapped range found for write.\n", val);
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    c = m68k_read_memory_8(piscsi_u32[2] + i);
                    write(d->fd, &c, 1);
                }
            }
            break;
        case PISCSI_CMD_ADDR1: case PISCSI_CMD_ADDR2: case PISCSI_CMD_ADDR3: case PISCSI_CMD_ADDR4: {
            int i = ((addr & 0xFFFF) - PISCSI_CMD_ADDR1) / 4;
            piscsi_u32[i] = val;
            break;
        }
        case PISCSI_CMD_DRVNUM:
            if (val % 10 != 0) {
                piscsi_cur_drive = 255;
            }
            else
                piscsi_cur_drive = val / 10;
            if (piscsi_cur_drive > NUM_UNITS) {
                piscsi_cur_drive = 255;
            }
            if (piscsi_cur_drive != 255) {
                DEBUG("[PISCSI] (%s) Drive number set to %d (%d)\n", op_type_names[type], piscsi_cur_drive, val);
            }
            break;
        case PISCSI_CMD_DRVNUMX:
            piscsi_cur_drive = val;
            DEBUG("[PISCSI] DRVNUMX: %d.\n", val);
            break;
        case PISCSI_CMD_DEBUGME:
            piscsi_debugme(val);
            break;
        case PISCSI_CMD_DRIVER:
            DEBUG("[PISCSI] Driver copy/patch called, destination address %.8X.\n", val);
            r = get_mapped_item_by_address(cfg, val);
            if (r != -1) {
                uint32_t addr = val - cfg->map_offset[r];
                uint8_t *dst_data = cfg->map_data[r];
                uint8_t cur_partition = 0;
                memcpy(dst_data + addr, piscsi_rom_ptr + PISCSI_DRIVER_OFFSET, 0x4000 - PISCSI_DRIVER_OFFSET);

                piscsi_hinfo.base_offset = val;

                reloc_hunks(piscsi_hreloc, dst_data + addr, &piscsi_hinfo);

                #define PUTNODELONG(val) *(uint32_t *)&dst_data[p_offs] = htobe32(val); p_offs += 4;
                #define PUTNODELONGBE(val) *(uint32_t *)&dst_data[p_offs] = val; p_offs += 4;

                for (int i = 0; i < 128; i++) {
                    rom_partitions[i] = 0;
                    rom_partition_prio[i] = 0;
                    rom_partition_dostype[i] = 0;
                }
                rom_cur_partition = 0;

                uint32_t data_addr = addr + 0x3F00;
                sprintf((char *)dst_data + data_addr, "pi-scsi.device");
                uint32_t addr2 = addr + 0x4000;
                for (int i = 0; i < NUM_UNITS; i++) {
                    if (devs[i].fd != -1)
                        piscsi_find_partitions(&devs[i]);
                    else
                        goto skip_disk;

                    if (devs[i].num_partitions) {
                        uint32_t p_offs = addr2;
                        DEBUG("[PISCSI] Adding %d partitions for unit %d\n", devs[i].num_partitions, i);
                        for (uint32_t j = 0; j < devs[i].num_partitions; j++) {
                            DEBUG("Partition %d: %s\n", j, devs[i].pb[j]->pb_DriveName + 1);
                            sprintf((char *)dst_data + p_offs, "%s", devs[i].pb[j]->pb_DriveName + 1);
                            p_offs += 0x20;
                            PUTNODELONG(addr2 + cfg->map_offset[r]);
                            PUTNODELONG(data_addr + cfg->map_offset[r]);
                            PUTNODELONG((i * 10));
                            PUTNODELONG(0);
                            uint32_t nodesize = (be32toh(devs[i].pb[j]->pb_Environment[0]) + 1) * 4;
                            memcpy(dst_data + p_offs, devs[i].pb[j]->pb_Environment, nodesize);

                            struct pihd_dosnode_data *dat = (struct pihd_dosnode_data *)(&dst_data[addr2+0x20]);

                            if (BE(devs[i].pb[j]->pb_Flags) & 0x01) {
                                DEBUG("Partition is bootable.\n");
                                rom_partition_prio[cur_partition] = 0;
                                dat->priority = 0;
                            }
                            else {
                                DEBUG("Partition is not bootable.\n");
                                rom_partition_prio[cur_partition] = -128;
                                dat->priority = htobe32(-128);
                            }

                            DEBUG("DOSNode Data:\n");
                            DEBUG("Name: %s Device: %s\n", dst_data + addr2, dst_data + data_addr);
                            DEBUG("Unit: %d Flags: %d Pad1: %d\n", BE(dat->unit), BE(dat->flags), BE(dat->pad1));
                            DEBUG("Node len: %d Block len: %d\n", BE(dat->node_len) * 4, BE(dat->block_len) * 4);
                            DEBUG("H: %d SPB: %d BPS: %d\n", BE(dat->surf), BE(dat->secs_per_block), BE(dat->blocks_per_track));
                            DEBUG("Reserved: %d Prealloc: %d\n", BE(dat->reserved_blocks), BE(dat->pad2));
                            DEBUG("Interleaved: %d Buffers: %d Memtype: %d\n", BE(dat->interleave), BE(dat->buffers), BE(dat->mem_type));
                            DEBUG("Lowcyl: %d Highcyl: %d Prio: %d\n", BE(dat->lowcyl), BE(dat->highcyl), BE(dat->priority));
                            DEBUG("Maxtransfer: %.8X Mask: %.8X\n", BE(dat->maxtransfer), BE(dat->transfer_mask));
                            DEBUG("DOSType: %.8X\n", BE(dat->dostype));

                            rom_partitions[cur_partition] = addr2 + 0x20 + cfg->map_offset[r];
                            rom_partition_dostype[cur_partition] = dat->dostype;
                            cur_partition++;
                            addr2 += 0x100;
                            p_offs = addr2;
                        }
                    }
skip_disk:;
                }
            }

            break;
        case PISCSI_CMD_NEXTPART:
            DEBUG("[PISCSI] Switch partition %d -> %d\n", rom_cur_partition, rom_cur_partition + 1);
            rom_cur_partition++;
            break;
        case PISCSI_CMD_NEXTFS:
            DEBUG("[PISCSI] Switch file file system %d -> %d\n", rom_cur_fs, rom_cur_fs + 1);
            rom_cur_fs++;
            break;
        case PISCSI_CMD_COPYFS:
            DEBUG("[PISCSI] Copy file system %d to %.8X and reloc.\n", rom_cur_fs, piscsi_u32[2]);
            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1) {
                uint32_t addr = piscsi_u32[2] - cfg->map_offset[r];
                memset(cfg->map_data[r] + addr, 0x00, filesystems[rom_cur_fs].h_info.alloc_size);
                memcpy(cfg->map_data[r] + addr, filesystems[rom_cur_fs].binary_data, filesystems[rom_cur_fs].h_info.byte_size);
                filesystems[rom_cur_fs].h_info.base_offset = piscsi_u32[2];
                reloc_hunks(filesystems[rom_cur_fs].relocs, cfg->map_data[r] + addr, &filesystems[rom_cur_fs].h_info);
                filesystems[rom_cur_fs].handler = piscsi_u32[2];
            }
            break;
        case PISCSI_CMD_SETFSH: {
            int i = 0;
            DEBUG("[PISCSI] Set handler for partition %d (DeviceNode: %.8X)\n", rom_cur_partition, val);
            r = get_mapped_item_by_address(cfg, val);
            if (r != -1) {
                uint32_t addr = val - cfg->map_offset[r];
                struct DeviceNode *node = (struct DeviceNode *)(cfg->map_data[r] + addr);
                char *dosID = (char *)&rom_partition_dostype[rom_cur_partition];
#ifdef PISCSI_DEBUG
#endif
                DEBUG("[PISCSI] Partition DOSType is %c%c%c/%d\n", dosID[0], dosID[1], dosID[2], dosID[3]);
                for (i = 0; i < piscsi_num_fs; i++) {
                    if (rom_partition_dostype[rom_cur_partition] == filesystems[i].FS_ID) {
                        node->dn_SegList = htobe32((filesystems[i].handler >> 2));
                        node->dn_GlobalVec = 0xFFFFFFFF;
                        goto fs_found;
                    }
                }
                printf("[!!!PISCSI] Found no handler for file system %s!\n", dosID);
fs_found:;
                DEBUG("[FS-HANDLER] Next: %d Type: %.8X\n", BE(node->dn_Next), BE(node->dn_Type));
                DEBUG("[FS-HANDLER] Task: %d Lock: %d\n", BE(node->dn_Task), BE(node->dn_Lock));
                DEBUG("[FS-HANDLER] Handler: %d Stacksize: %d\n", BE((uint32_t)node->dn_Handler), BE(node->dn_StackSize));
                DEBUG("[FS-HANDLER] Priority: %d Startup: %d\n", BE((uint32_t)node->dn_Priority), BE(node->dn_Startup));
                DEBUG("[FS-HANDLER] SegList: %.8X GlobalVec: %d\n", BE((uint32_t)node->dn_SegList), BE(node->dn_GlobalVec));
                DEBUG("[PISCSI] Handler for partition %.8X set to %.8X (%.8X).\n", BE((uint32_t)node->dn_Name), filesystems[i].FS_ID, filesystems[i].handler);
            }
            break;
        }
        case PISCSI_DBG_VAL1: case PISCSI_DBG_VAL2: case PISCSI_DBG_VAL3: case PISCSI_DBG_VAL4:
        case PISCSI_DBG_VAL5: case PISCSI_DBG_VAL6: case PISCSI_DBG_VAL7: case PISCSI_DBG_VAL8: {
            int i = ((addr & 0xFFFF) - PISCSI_DBG_VAL1) / 4;
            piscsi_dbg[i] = val;
            break;
        }
        case PISCSI_DBG_MSG:
            print_piscsi_debug_message(val);
            break;
        default:
            DEBUG("[!!!PISCSI] WARN: Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

#define PIB 0x00

uint32_t handle_piscsi_read(uint32_t addr, uint8_t type) {
    if (type) {}

    if ((addr & 0xFFFF) >= PISCSI_CMD_ROM) {
        uint32_t romoffs = (addr & 0xFFFF) - PISCSI_CMD_ROM;
        if (romoffs < (piscsi_rom_size + PIB)) {
            //DEBUG("[PISCSI] %s read from Boot ROM @$%.4X (%.8X): ", op_type_names[type], romoffs, addr);
            uint32_t v = 0;
            switch (type) {
                case OP_TYPE_BYTE:
                    v = piscsi_rom_ptr[romoffs - PIB];
                    //DEBUG("%.2X\n", v);
                    break;
                case OP_TYPE_WORD:
                    v = be16toh(*((uint16_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //DEBUG("%.4X\n", v);
                    break;
                case OP_TYPE_LONGWORD:
                    v = be32toh(*((uint32_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //DEBUG("%.8X\n", v);
                    break;
            }
            return v;
        }
        return 0;
    }

    switch (addr & 0xFFFF) {
        case PISCSI_CMD_ADDR1: case PISCSI_CMD_ADDR2: case PISCSI_CMD_ADDR3: case PISCSI_CMD_ADDR4: {
            int i = ((addr & 0xFFFF) - PISCSI_CMD_ADDR1) / 4;
            return piscsi_u32[i];
            break;
        }
        case PISCSI_CMD_DRVTYPE:
            if (devs[piscsi_cur_drive].fd == -1) {
                DEBUG("[PISCSI] %s Read from DRVTYPE %d, drive not attached.\n", op_type_names[type], piscsi_cur_drive);
                return 0;
            }
            DEBUG("[PISCSI] %s Read from DRVTYPE %d, drive attached.\n", op_type_names[type], piscsi_cur_drive);
            return 1;
            break;
        case PISCSI_CMD_DRVNUM:
            return piscsi_cur_drive;
            break;
        case PISCSI_CMD_CYLS:
            DEBUG("[PISCSI] %s Read from CYLS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].c);
            return devs[piscsi_cur_drive].c;
            break;
        case PISCSI_CMD_HEADS:
            DEBUG("[PISCSI] %s Read from HEADS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].h);
            return devs[piscsi_cur_drive].h;
            break;
        case PISCSI_CMD_SECS:
            DEBUG("[PISCSI] %s Read from SECS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].s);
            return devs[piscsi_cur_drive].s;
            break;
        case PISCSI_CMD_BLOCKS: {
            uint32_t blox = devs[piscsi_cur_drive].fs / devs[piscsi_cur_drive].block_size;
            DEBUG("[PISCSI] %s Read from BLOCKS %d: %d\n", op_type_names[type], piscsi_cur_drive, (uint32_t)(devs[piscsi_cur_drive].fs / devs[piscsi_cur_drive].block_size));
            DEBUG("fs: %lld (%d)\n", devs[piscsi_cur_drive].fs, blox);
            return blox;
            break;
        }
        case PISCSI_CMD_GETPART: {
            DEBUG("[PISCSI] Get ROM partition %d offset: %.8X\n", rom_cur_partition, rom_partitions[rom_cur_partition]);
            return rom_partitions[rom_cur_partition];
            break;
        }
        case PISCSI_CMD_GETPRIO:
            DEBUG("[PISCSI] Get partition %d boot priority: %d\n", rom_cur_partition, rom_partition_prio[rom_cur_partition]);
            return rom_partition_prio[rom_cur_partition];
            break;
        case PISCSI_CMD_CHECKFS:
            DEBUG("[PISCSI] Get current loaded file system: %.8X\n", filesystems[rom_cur_fs].FS_ID);
            return filesystems[rom_cur_fs].FS_ID;
        case PISCSI_CMD_FSSIZE:
            DEBUG("[PISCSI] Get alloc size of loaded file system: %d\n", filesystems[rom_cur_fs].h_info.alloc_size);
            return filesystems[rom_cur_fs].h_info.alloc_size;
        case PISCSI_CMD_BLOCKSIZE:
            DEBUG("[PISCSI] Get block size of drive %d: %d\n", piscsi_cur_drive, devs[piscsi_cur_drive].block_size);
            return devs[piscsi_cur_drive].block_size;
        default:
            DEBUG("[!!!PISCSI] WARN: Unhandled %s register read from %.8X\n", op_type_names[type], addr);
            break;
    }

    return 0;
}
