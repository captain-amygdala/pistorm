#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include "piscsi.h"
#include "piscsi-enums.h"
#include "../hunk-reloc.h"
#include "../../../config_file/config_file.h"
#include "../../../gpio/ps_protocol.h"

#define BE(val) be32toh(val)

// Comment these lines to restore debug output:
#define printf(...)
#define stop_cpu_emulation(...)

#ifdef FAKESTORM
#define lseek64 lseek
#endif

extern struct emulator_config *cfg;
extern void stop_cpu_emulation(uint8_t disasm_cur);

struct piscsi_dev devs[8];
uint8_t piscsi_cur_drive = 0;
uint32_t piscsi_u32[4];
uint32_t piscsi_dbg[8];
uint32_t piscsi_rom_size = 0;
uint8_t *piscsi_rom_ptr;

uint32_t rom_partitions[128];
uint32_t rom_partition_prio[128];
uint32_t rom_cur_partition = 0;

extern unsigned char ac_piscsi_rom[];

static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};

//static const char *partition_marker = "PART";

struct hunk_info piscsi_hinfo;
struct hunk_reloc piscsi_hreloc[256];

void piscsi_init() {
    for (int i = 0; i < 8; i++) {
        devs[i].fd = -1;
        devs[i].lba = 0;
        devs[i].c = devs[i].h = devs[i].s = 0;
    }

    FILE *in = fopen("./platforms/amiga/piscsi/piscsi.rom", "rb");
    if (in == NULL) {
        printf("[PISCSI] Could not open PISCSI Boot ROM file for reading.\n");
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
    fclose(in);

    // Parse the hunks in the device driver to find relocation offsets
    in = fopen("./platforms/amiga/piscsi/device_driver_amiga/pi-scsi.device", "rb");
    fseek(in, 0x0, SEEK_SET);
    process_hunks(in, &piscsi_hinfo, piscsi_hreloc);

    fclose(in);
    printf("[PISCSI] Loaded Boot ROM.\n");
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
        printf("[PISCSI] No partitions on disk.\n");
        return;
    }

    char *block = malloc(512);

    lseek(fd, be32toh(d->rdb->rdb_PartitionList) * 512, SEEK_SET);
next_partition:;
    read(fd, block, 512);

    struct PartitionBlock *pb = (struct PartitionBlock *)block;
    tmp = pb->pb_DriveName[0];
    pb->pb_DriveName[tmp + 1] = 0x00;
    printf("[PISCSI] Partition %d: %s\n", cur_partition, pb->pb_DriveName + 1);
    printf("Checksum: %.8X HostID: %d\n", BE(pb->pb_ChkSum), BE(pb->pb_HostID));
    printf("Flags: %d (%.8X) Devflags: %d (%.8X)\n", BE(pb->pb_Flags), BE(pb->pb_Flags), BE(pb->pb_DevFlags), BE(pb->pb_DevFlags));
    d->pb[cur_partition] = pb;

    if (d->pb[cur_partition]->pb_Next != 0xFFFFFFFF) {
        uint64_t next = be32toh(pb->pb_Next);
        block = malloc(512);
        lseek64(fd, next * 512, SEEK_SET);
        cur_partition++;
        printf("[PISCSI] Next partition at block %d.\n", be32toh(pb->pb_Next));
        goto next_partition;
    }
    printf("[PISCSI] No more partitions on disk.\n");
    d->num_partitions = cur_partition + 1;

    return;
}

int piscsi_parse_rdb(struct piscsi_dev *d) {
    int fd = d->fd;
    int i = 0;
    uint8_t *block = malloc(512);

    lseek(fd, 0, SEEK_SET);
    for (i = 0; i < RDB_BLOCK_LIMIT; i++) {
        read(fd, block, 512);
        uint32_t first = be32toh(*((uint32_t *)&block[0]));
        if (first == RDB_IDENTIFIER)
            goto rdb_found;
    }
    goto no_rdb_found;
rdb_found:;
    struct RigidDiskBlock *rdb = (struct RigidDiskBlock *)block;
    printf("[PISCSI] RDB found at block %d.\n", i);
    d->c = be32toh(rdb->rdb_Cylinders);
    d->h = be32toh(rdb->rdb_Heads);
    d->s = be32toh(rdb->rdb_Sectors);
    printf("[PISCSI] RDB - first partition at block %d.\n", be32toh(rdb->rdb_PartitionList));
    d->rdb = rdb;
    sprintf(d->rdb->rdb_DriveInitName, "pi-scsi.device");
    return 0;

no_rdb_found:;
    if (block)
        free(block);

    return -1;
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
        printf("[PISCSI] No RDB found on disk, making up some CHS values.\n");
        d->h = 64;
        d->s = 63;
        d->c = (file_size / 512) / (d->s * d->h);
    }
    printf("[PISCSI] CHS: %d %d %d\n", d->c, d->h, d->s);

    piscsi_find_partitions(d);
    //stop_cpu_emulation(1);
}

void piscsi_unmap_drive(uint8_t index) {
    if (devs[index].fd != -1) {
        printf("[PISCSI] Unmapped drive %d.\n", index);
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
            return "!!!Unhandled IO command";
    }
}

char *scsi_cmd_name(int index) {
    switch(index) {
        case 0x00: return "TEST UNIT READY";
        case 0x12: return "INQUIRY";
        case 0x08: return "READ (6)";
        case 0x0A: return "WRITE (6)";
        case 0x28: return "READ (10)";
        case 0x2A: return "WRITE (10)";
        case 0x25: return "READ CAPACITY";
        case 0x1A: return "MODE SENSE";
        case 0x37: return "READ DEFECT DATA";
        default:
            return "!!!Unhandled SCSI command";
    }
}

void print_piscsi_debug_message(int index) {
    switch (index) {
        case DBG_INIT:
            printf("[PISCSI] Initializing devices.\n");
            break;
        case DBG_OPENDEV:
            printf("[PISCSI] Opening device %d (%d). Flags: %d (%.2X)\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2], piscsi_dbg[2]);
            break;
        case DBG_CLEANUP:
            printf("[PISCSI] Cleaning up.\n");
            break;
        case DBG_CHS:
            printf("[PISCSI] C/H/S: %d / %d / %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_BEGINIO:
            printf("[PISCSI] BeginIO: io_Command: %d - io_Flags = %d - quick: %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2]);
            break;
        case DBG_ABORTIO:
            printf("[PISCSI] AbortIO!\n");
            break;
        case DBG_SCSICMD:
            printf("[PISCSI] SCSI Command %d (%s)\n", piscsi_dbg[1], scsi_cmd_name(piscsi_dbg[1]));
            printf("Len: %d - %.2X %.2X %.2X - Command Length: %d\n", piscsi_dbg[0], piscsi_dbg[1], piscsi_dbg[2], piscsi_dbg[3], piscsi_dbg[4]);
            break;
        case DBG_SCSI_UNKNOWN_MODESENSE:
            printf("SCSI: Unknown modesense %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSI_UNKNOWN_COMMAND:
            printf("SCSI: Unknown command %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_SCSIERR:
            printf("SCSI: An error occured: %.4X\n", piscsi_dbg[0]);
            break;
        case DBG_IOCMD:
            printf("[PISCSI] IO Command %d (%s)\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
            break;
        case DBG_IOCMD_UNHANDLED:
            printf("[PISCSI] WARN: IO command %.4X (%s) is unhandled by driver.\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
            break;
    }
}

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type) {
    int32_t r;

    struct piscsi_dev *d = &devs[piscsi_cur_drive];

    uint16_t cmd = (addr & 0xFFFF);

    switch (cmd) {
        case PISCSI_CMD_READ64:
        case PISCSI_CMD_READ:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted read from unmapped drive %d.\n", val);
                break;
            }

            if (cmd == PISCSI_CMD_READ) {
                printf("[PISCSI] %d byte READ from block %d to address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                printf("[PISCSI] %d byte READ64 from block %lld to address %.8X\n", piscsi_u32[1], (src / 512), piscsi_u32[2]);
                d->lba = (src / 512);
                lseek64(d->fd, src, SEEK_SET);
            }

            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1 && cfg->map_type[r] == MAPTYPE_RAM) {
                printf("[PISCSI] \"DMA\" Read goes to mapped range %d.\n", r);
                read(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for read.\n");
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    read(d->fd, &c, 1);
                    write8(piscsi_u32[2] + i, (uint32_t)c);
                }
            }
            break;
        case PISCSI_CMD_WRITE64:
        case PISCSI_CMD_WRITE:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted write to unmapped drive %d.\n", val);
                break;
            }

            if (cmd == PISCSI_CMD_WRITE) {
                printf("[PISCSI] %d byte WRITE to block %d from address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
                d->lba = piscsi_u32[0];
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
            }
            else {
                uint64_t src = piscsi_u32[3];
                src = (src << 32) | piscsi_u32[0];
                printf("[PISCSI] %d byte WRITE64 to block %lld from address %.8X\n", piscsi_u32[1], (src / 512), piscsi_u32[2]);
                d->lba = (src / 512);
                lseek64(d->fd, src, SEEK_SET);
            }

            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1) {
                printf("[PISCSI] \"DMA\" Write comes from mapped range %d.\n", r);
                write(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for write.\n");
                uint8_t c = 0;
                for (uint32_t i = 0; i < piscsi_u32[1]; i++) {
                    c = read8(piscsi_u32[2] + i);
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
            if (val % 10 != 0)
                piscsi_cur_drive = 255;
            else
                piscsi_cur_drive = val / 10;
            printf("[PISCSI] (%s) Drive number set to %d (%d)\n", op_type_names[type], piscsi_cur_drive, val);
            break;
        case PISCSI_CMD_DEBUGME:
            printf("[PISCSI] DebugMe triggered (%d).\n", val);
            stop_cpu_emulation(1);
            break;
        case PISCSI_CMD_DRIVER: {
            printf("[PISCSI] Driver copy/patch called, destination address %.8X.\n", val);
            int r = get_mapped_item_by_address(cfg, val);
            if (r != -1) {
                uint32_t addr = val - cfg->map_offset[r];
                uint8_t *dst_data = cfg->map_data[r];
                uint8_t cur_partition = 0;
                memcpy(dst_data + addr, piscsi_rom_ptr + 0x400, 0x3C00);

                piscsi_hinfo.base_offset = val;
                
                reloc_hunks(piscsi_hreloc, dst_data + addr, &piscsi_hinfo);
                stop_cpu_emulation(1);

                #define PUTNODELONG(val) *(uint32_t *)&dst_data[p_offs] = htobe32(val); p_offs += 4;
                #define PUTNODELONGBE(val) *(uint32_t *)&dst_data[p_offs] = val; p_offs += 4;

                for (int i = 0; i < 128; i++) {
                    rom_partitions[i] = 0;
                    rom_partition_prio[i] = 0;
                }
                rom_cur_partition = 0;

                uint32_t data_addr = addr + 0x3F00;
                sprintf((char *)dst_data + data_addr, "pi-scsi.device");
                uint32_t addr2 = addr + 0x4000;
                for (int i = 0; i < NUM_UNITS; i++) {
                    piscsi_find_partitions(&devs[i]);
                    if (devs[i].num_partitions) {
                        uint32_t p_offs = addr2;
                        printf("[PISCSI] Adding %d partitions for unit %d\n", devs[i].num_partitions, i);
                        for (uint32_t j = 0; j < devs[i].num_partitions; j++) {
                            printf("Partition %d: %s\n", j, devs[i].pb[j]->pb_DriveName + 1);
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
                                printf("Partition is bootable.\n");
                                rom_partition_prio[cur_partition] = 0;
                                dat->priority = 0;
                            }
                            else {
                                printf("Partition is not bootable.\n");
                                rom_partition_prio[cur_partition] = -128;
                                dat->priority = htobe32(-128);
                            }

                            printf("DOSNode Data:\n");
                            printf("Name: %s Device: %s\n", dst_data + addr2, dst_data + data_addr);
                            printf("Unit: %d Flags: %d Pad1: %d\n", BE(dat->unit), BE(dat->flags), BE(dat->pad1));
                            printf("Node len: %d Block len: %d\n", BE(dat->node_len) * 4, BE(dat->block_len) * 4);
                            printf("H: %d SPB: %d BPS: %d\n", BE(dat->surf), BE(dat->secs_per_block), BE(dat->blocks_per_track));
                            printf("Reserved: %d Prealloc: %d\n", BE(dat->reserved_blocks), BE(dat->pad2));
                            printf("Interleaved: %d Buffers: %d Memtype: %d\n", BE(dat->interleave), BE(dat->buffers), BE(dat->mem_type));
                            printf("Lowcyl: %d Highcyl: %d Prio: %d\n", BE(dat->lowcyl), BE(dat->highcyl), BE(dat->priority));
                            printf("Maxtransfer: %.8X Mask: %.8X\n", BE(dat->maxtransfer), BE(dat->transfer_mask));
                            printf("DOSType: %.8X\n", BE(dat->dostype));

                            rom_partitions[cur_partition] = addr2 + 0x20 + cfg->map_offset[r];
                            cur_partition++;
                            addr2 += 0x100;
                            p_offs = addr2;
                        }
                    }
                }
            }
           
            break;
        }
        case PISCSI_CMD_NEXTPART:
            printf("[PISCSI] Switch partition %d -> %d\n", rom_cur_partition, rom_cur_partition + 1);
            rom_cur_partition++;
            break;
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
            printf("[PISCSI] WARN: Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

#define PIB 0x00

uint32_t handle_piscsi_read(uint32_t addr, uint8_t type) {
    if (type) {}

    if ((addr & 0xFFFF) >= PISCSI_CMD_ROM) {
        uint32_t romoffs = (addr & 0xFFFF) - PISCSI_CMD_ROM;
        if (romoffs < (piscsi_rom_size + PIB)) {
            //printf("[PISCSI] %s read from Boot ROM @$%.4X (%.8X): ", op_type_names[type], romoffs, addr);
            uint32_t v = 0;
            switch (type) {
                case OP_TYPE_BYTE:
                    v = piscsi_rom_ptr[romoffs - PIB];
                    //printf("%.2X\n", v);
                    break;
                case OP_TYPE_WORD:
                    v = be16toh(*((uint16_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //printf("%.4X\n", v);
                    break;
                case OP_TYPE_LONGWORD:
                    v = be32toh(*((uint32_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    //printf("%.8X\n", v);
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
                printf("[PISCSI] %s Read from DRVTYPE %d, drive not attached.\n", op_type_names[type], piscsi_cur_drive);
                return 0;
            }
            printf("[PISCSI] %s Read from DRVTYPE %d, drive attached.\n", op_type_names[type], piscsi_cur_drive);
            return 1;
            break;
        case PISCSI_CMD_DRVNUM:
            return piscsi_cur_drive;
            break;
        case PISCSI_CMD_CYLS:
            printf("[PISCSI] %s Read from CYLS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].c);
            return devs[piscsi_cur_drive].c;
            break;
        case PISCSI_CMD_HEADS:
            printf("[PISCSI] %s Read from HEADS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].h);
            return devs[piscsi_cur_drive].h;
            break;
        case PISCSI_CMD_SECS:
            printf("[PISCSI] %s Read from SECS %d: %d\n", op_type_names[type], piscsi_cur_drive, devs[piscsi_cur_drive].s);
            return devs[piscsi_cur_drive].s;
            break;
        case PISCSI_CMD_BLOCKS: {
            uint32_t blox = devs[piscsi_cur_drive].fs / 512;
            printf("[PISCSI] %s Read from BLOCKS %d: %d\n", op_type_names[type], piscsi_cur_drive, (uint32_t)(devs[piscsi_cur_drive].fs / 512));
            printf("fs: %lld (%d)\n", devs[piscsi_cur_drive].fs, blox);
            return blox;
            break;
        }
        case PISCSI_CMD_GETPART: {
            printf("[PISCSI] Get ROM partition %d offset: %.8X\n", rom_cur_partition, rom_partitions[rom_cur_partition]);
            return rom_partitions[rom_cur_partition];
            break;
        }
        case PISCSI_CMD_GETPRIO:
            printf("[PISCSI] Get partition %d boot priority: %d\n", rom_cur_partition, rom_partition_prio[rom_cur_partition]);
            return rom_partition_prio[rom_cur_partition];
            break;
        default:
            printf("[PISCSI] Unhandled %s register read from %.8X\n", op_type_names[type], addr);
            break;
    }

    return 0;
}
