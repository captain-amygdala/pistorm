#include <stdio.h>
#include <stdint.h>
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
        default:
            printf("[PISCSI] Unhandled %s register write to %.8X: %d\n", op_type_names[type], addr, val);
            break;
    }
}

uint32_t handle_piscsi_read(uint32_t addr, uint8_t type) {
    if (type) {}
    
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
