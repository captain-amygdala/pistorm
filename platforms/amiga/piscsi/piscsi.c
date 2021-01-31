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

struct piscsi_dev devs[8];
uint8_t piscsi_cur_drive = 0;
uint32_t piscsi_u32[4];
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

extern struct emulator_config *cfg;
extern void stop_cpu_emulation(uint8_t disasm_cur);

void handle_piscsi_write(uint32_t addr, uint32_t val, uint8_t type) {
    int32_t r;

    struct piscsi_dev *d = &devs[piscsi_cur_drive];

    switch (addr & 0xFFFF) {
        case PISCSI_CMD_READ:
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted read from unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }
            printf("[PISCSI] %d byte READ from block %d to address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
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
            if (d->fd == -1) {
                printf ("[PISCSI] BUG: Attempted write to unmapped drive %d.\n", piscsi_cur_drive);
                break;
            }
            printf("[PISCSI] %d byte WRITE to block %d to address %.8X\n", piscsi_u32[1], piscsi_u32[0], piscsi_u32[2]);
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
        case PISCSI_CMD_ADDR1:
            piscsi_u32[0] = val;
            printf("[PISCSI] Write to ADDR1: %.8x\n", piscsi_u32[0]);
            break;
        case PISCSI_CMD_ADDR2:
            piscsi_u32[1] = val;
            printf("[PISCSI] Write to ADDR2: %.8x\n", piscsi_u32[1]);
            break;
        case PISCSI_CMD_ADDR3:
            piscsi_u32[2] = val;
            printf("[PISCSI] Write to ADDR3: %.8x\n", piscsi_u32[2]);
            break;
        case PISCSI_CMD_ADDR4:
            piscsi_u32[3] = val;
            printf("[PISCSI] Write to ADDR4: %.8x\n", piscsi_u32[3]);
            break;
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
            printf("[PISCSI] DebugMe triggered.\n");
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
        default:
            printf("[PISCSI] Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

uint8_t piscsi_diag_area[] = {
    0x90,
    0x00,
    0x00, 0x40,
    0x2C, 0x00,
    0x2C, 0x00,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x00,
};

uint8_t fastata_diag_area[] = {
    0x90,
    0x00,
    0x00, 0x10,
    0x9e, 0x08,
    0x00, 0x00,
    0x00, 0x00,
    0x00, 0x02,
    0x00, 0x00,
};

uint8_t piscsi_diag_read;

#define PIB 0x00

uint32_t handle_piscsi_read(uint32_t addr, uint8_t type) {
    if (type) {}
    uint8_t *diag_area = piscsi_diag_area;

    if ((addr & 0xFFFF) >= PISCSI_CMD_ROM) {
        uint32_t romoffs = (addr & 0xFFFF) - PISCSI_CMD_ROM;
        /*if (romoffs < 14 && !piscsi_diag_read) {
            printf("[PISCSI] %s read from DiagArea @$%.4X: ", op_type_names[type], romoffs);
            uint32_t v = 0;
            switch (type) {
                case OP_TYPE_BYTE:
                    v = diag_area[romoffs];
                    printf("%.2X\n", v);
                    break;
                case OP_TYPE_WORD:
                    v = *((uint16_t *)&diag_area[romoffs]);
                    printf("%.4X\n", v);
                    break;
                case OP_TYPE_LONGWORD:
                    v = (*((uint16_t *)&diag_area[romoffs]) << 16) | *((uint16_t *)&diag_area[romoffs + 2]);
                    //v = *((uint32_t *)&diag_area[romoffs]);
                    printf("%.8X\n", v);
                    break;
            }
            if (romoffs == 0x0D)
                piscsi_diag_read = 1;
            return v;   
        }*/
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
                    //v = (*((uint16_t *)&piscsi_rom_ptr[romoffs - 14]) << 16) | *((uint16_t *)&piscsi_rom_ptr[romoffs - 12]);
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

void piscsi_block_op(uint8_t type, uint8_t num, uint32_t dest, uint32_t len) {
    if (type || num || dest || len) {}
}
