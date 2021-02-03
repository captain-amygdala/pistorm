#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <endian.h>
#include "piscsi.h"
#include "piscsi-enums.h"
#include "../../../config_file/config_file.h"
#include "../../../gpio/gpio.h"

// Comment this line to restore debug output:
#define printf(...)

struct piscsi_dev devs[8];
uint8_t piscsi_cur_drive = 0;
uint32_t piscsi_u32[4];
uint32_t piscsi_dbg[8];
uint32_t piscsi_rom_size = 0;
uint8_t *piscsi_rom_ptr;

extern unsigned char ac_piscsi_rom[];

static const char *op_type_names[4] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};

void piscsi_init() {
    for (int i = 0; i < 8; i++) {
        devs[i].fd = -1;
        devs[i].lba = 0;
        devs[i].c = devs[i].h = devs[i].s = 0;
    }

    FILE *in = fopen("./platforms/amiga/piscsi/piscsi.rom", "rb");
    if (in == NULL) {
        printf("[PISCSI] Could not open PISCSI Boot ROM file for reading.\n");
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
    printf("[PISCSI] Loaded Boot ROM.\n");
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
    lseek(tmp_fd, 0, SEEK_SET);
    printf("[PISCSI] Map %d: [%s] - %llu bytes.\n", index, filename, file_size);
    d->h = 64;
    d->s = 63;
    d->c = (file_size / 512) / (d->s * d->h);
    printf("[PISCSI] CHS: %d %d %d\n", d->c, d->h, d->s);
    d->fs = file_size;
    d->fd = tmp_fd;
}

void piscsi_unmap_drive(uint8_t index) {
    if (devs[index].fd != -1) {
        printf("[PISCSI] Unmapped drive %d.\n", index);
        close (devs[index].fd);
        devs[index].fd = -1;
    }
}

#define	TDF_EXTCOM (1<<15)

#define CMD_INVALID	0
#define CMD_RESET	1
#define CMD_READ	2
#define CMD_WRITE	3
#define CMD_UPDATE	4
#define CMD_CLEAR	5
#define CMD_STOP	6
#define CMD_START	7
#define CMD_FLUSH	8
#define CMD_NONSTD	9

#define	TD_MOTOR	(CMD_NONSTD+0)
#define	TD_SEEK		(CMD_NONSTD+1)
#define	TD_FORMAT	(CMD_NONSTD+2)
#define	TD_REMOVE	(CMD_NONSTD+3)
#define	TD_CHANGENUM	(CMD_NONSTD+4)
#define	TD_CHANGESTATE	(CMD_NONSTD+5)
#define	TD_PROTSTATUS	(CMD_NONSTD+6)
#define	TD_RAWREAD	(CMD_NONSTD+7)
#define	TD_RAWWRITE	(CMD_NONSTD+8)
#define	TD_GETDRIVETYPE	(CMD_NONSTD+9)
#define	TD_GETNUMTRACKS	(CMD_NONSTD+10)
#define	TD_ADDCHANGEINT	(CMD_NONSTD+11)
#define	TD_REMCHANGEINT	(CMD_NONSTD+12)
#define TD_GETGEOMETRY	(CMD_NONSTD+13)
#define TD_EJECT	(CMD_NONSTD+14)
#define	TD_LASTCOMM	(CMD_NONSTD+15)

#define	ETD_WRITE	(CMD_WRITE|TDF_EXTCOM)
#define	ETD_READ	(CMD_READ|TDF_EXTCOM)
#define	ETD_MOTOR	(TD_MOTOR|TDF_EXTCOM)
#define	ETD_SEEK	(TD_SEEK|TDF_EXTCOM)
#define	ETD_FORMAT	(TD_FORMAT|TDF_EXTCOM)
#define	ETD_UPDATE	(CMD_UPDATE|TDF_EXTCOM)
#define	ETD_CLEAR	(CMD_CLEAR|TDF_EXTCOM)
#define	ETD_RAWREAD	(TD_RAWREAD|TDF_EXTCOM)
#define	ETD_RAWWRITE	(TD_RAWWRITE|TDF_EXTCOM)

#define HD_SCSICMD 28

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
        case TD_LASTCOMM: return "LASTCOMM";
        case HD_SCSICMD: return "HD_SCSICMD";
        default:
            return "!!!Unhandled IO command";
    }
}

char *scsi_cmd_name(int index) {
    switch(index) {
        case 0x00: return "TEST UNIT READY";
        case 0x12: return "INQUIRY";
        case 0x08: return "READ";
        case 0x0A: return "WRITE";
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
            printf("[PISCSI] WARN: IO command %d (%s) is unhandled by driver.\n", piscsi_dbg[0], io_cmd_name(piscsi_dbg[0]));
    }
}

extern struct emulator_config *cfg;
extern void stop_cpu_emulation(uint8_t disasm_cur);

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type) {
    int32_t r;

    struct piscsi_dev *d = &devs[piscsi_cur_drive];

    switch (addr & 0xFFFF) {
        case PISCSI_CMD_READ:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted read from unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }
            printf("[PISCSI] %d byte READ from block %d to address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
            d->lba = piscsi_u32[0];
            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1 && cfg->map_type[r] == MAPTYPE_RAM) {
                printf("[PISCSI] \"DMA\" Read goes to mapped range %d.\n", r);
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
                read(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for read.\n");
                uint8_t c = 0;
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
                for (int i = 0; i < piscsi_u32[1]; i++) {
                    read(d->fd, &c, 1);
#ifndef FAKESTORM
                    write8(piscsi_u32[2] + i, (uint32_t)c);
#endif
                }
            }
            break;
        case PISCSI_CMD_WRITE:
            d = &devs[val];
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted write to unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }
            d->lba = piscsi_u32[0];
            printf("[PISCSI] %d byte WRITE to block %d from address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
            r = get_mapped_item_by_address(cfg, piscsi_u32[2]);
            if (r != -1) {
                printf("[PISCSI] \"DMA\" Write comes from mapped range %d.\n", r);
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
                write(d->fd, cfg->map_data[r] + piscsi_u32[2] - cfg->map_offset[r], piscsi_u32[1]);
            }
            else {
                printf("[PISCSI] No mapped range found for write.\n");
                uint8_t c = 0;
                lseek(d->fd, (piscsi_u32[0] * 512), SEEK_SET);
                for (int i = 0; i < piscsi_u32[1]; i++) {
#ifndef FAKESTORM
                    c = read8(piscsi_u32[2] + i);
#endif
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
            if (val != 0) {
                if (val < 10) // Kludge for GiggleDisk
                    piscsi_cur_drive = val;
                else if (val >= 10 && val % 10 != 0)
                    piscsi_cur_drive = 255;
                else
                    piscsi_cur_drive = val / 10;
            }
            else
                piscsi_cur_drive = val;
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
                uint32_t rt_offs = 0;
                uint8_t *dst_data = cfg->map_data[r];
                memcpy(dst_data + addr, piscsi_rom_ptr + 0x400, 0x3C00);
                
                uint32_t base_offs = be32toh(*((uint32_t *)&dst_data[addr + 0x170])) + 2;
                rt_offs = val + 0x16E;
                printf ("Offset 1: %.8X -> %.8X\n", base_offs, rt_offs);
                *((uint32_t *)&dst_data[addr + 0x170]) = htobe32(rt_offs);

                uint32_t offs = be32toh(*((uint32_t *)&dst_data[addr + 0x174]));
                printf ("Offset 2: %.8X -> %.8X\n", offs, (offs - base_offs) + rt_offs);
                *((uint32_t *)&dst_data[addr + 0x174]) = htobe32((offs - base_offs) + rt_offs);

                dst_data[addr + 0x178] |= 0x07;

                offs = be32toh(*((uint32_t *)&dst_data[addr + 0x17C]));
                printf ("Offset 3: %.8X -> %.8X\n", offs, (offs - base_offs) + rt_offs);
                *((uint32_t *)&dst_data[addr + 0x17C]) = htobe32((offs - base_offs) + rt_offs);

                offs = be32toh(*((uint32_t *)&dst_data[addr + 0x180]));
                printf ("Offset 4: %.8X -> %.8X\n", offs, (offs - base_offs) + rt_offs);
                *((uint32_t *)&dst_data[addr + 0x180]) = htobe32((offs - base_offs) + rt_offs);

                offs = be32toh(*((uint32_t *)&dst_data[addr + 0x184]));
                printf ("Offset 5: %.8X -> %.8X\n", offs, (offs - base_offs) + rt_offs);
                *((uint32_t *)&dst_data[addr + 0x184]) = htobe32((offs - base_offs) + rt_offs);

            }
            else {
                for (int i = 0; i < 0x3C00; i++) {
                    uint8_t src = piscsi_rom_ptr[0x400 + i];
                    write8(addr + i, src);
                }
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
            printf("[PISCSI] %s read from Boot ROM @$%.4X (%.8X): ", op_type_names[type], romoffs, addr);
            uint32_t v = 0;
            switch (type) {
                case OP_TYPE_BYTE:
                    v = piscsi_rom_ptr[romoffs - PIB];
                    printf("%.2X\n", v);
                    break;
                case OP_TYPE_WORD:
                    v = be16toh(*((uint16_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    printf("%.4X\n", v);
                    break;
                case OP_TYPE_LONGWORD:
                    v = be32toh(*((uint32_t *)&piscsi_rom_ptr[romoffs - PIB]));
                    printf("%.8X\n", v);
                    break;
            }
            return v;
        }
        return 0;
    }
    
    switch (addr & 0xFFFF) {
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
        default:
            printf("[PISCSI] Unhandled %s register read from %.8X\n", op_type_names[type], addr);
            break;
    }

    return 0;
}
