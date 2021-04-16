// SPDX-License-Identifier: MIT

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define BOOTLDR_SIZE 0x1000
#define DIAG_TOTAL_SIZE 0x4000

char *rombuf, *zerobuf, *devicebuf;

int main(int argc, char *argv[]) {
    FILE *rom = fopen("bootrom", "rb");
    if (!rom) {
        printf("Could not open file bootrom for reading.\n");
        return 1;
    }
    FILE *out = fopen("../piscsi.rom", "wb+");
    if (!out) {
        printf("Could not open file piscsi.rom for writing.\n");
        fclose(rom);
        return 1;
    }
    FILE *device = NULL;
    if (argc > 1) {
        device = fopen(argv[1], "rb");
    }
    else {
        device = fopen("pi-scsi.device", "rb");
    }
    if (!device) {
        printf("Could not open device file for reading.\n");
        fclose(rom);
        fclose(out);
        return 1;
    }

    fseek(device, 0, SEEK_END);
    fseek(rom, 0, SEEK_END);
    uint32_t rom_size = ftell(rom);
    uint32_t device_size = ftell(device);
    fseek(rom, 0, SEEK_SET);
    fseek(device, 0, SEEK_SET);

    uint32_t pad_size = BOOTLDR_SIZE - rom_size;

    rombuf = malloc(rom_size);
    devicebuf = malloc(device_size);
    zerobuf = malloc(pad_size);
    memset(zerobuf, 0x00, pad_size);

    fread(rombuf, rom_size, 1, rom);
    fread(devicebuf, device_size, 1, device);

    fwrite(rombuf, rom_size, 1, out);
    fwrite(zerobuf, pad_size, 1, out);
    fwrite(devicebuf, device_size, 1, out);

    free(zerobuf);
    zerobuf = malloc(DIAG_TOTAL_SIZE - (rom_size + pad_size + device_size));
    memset(zerobuf, 0x00, DIAG_TOTAL_SIZE - (rom_size + pad_size + device_size));
    fwrite(zerobuf, DIAG_TOTAL_SIZE - (rom_size + pad_size + device_size), 1, out);

    printf("piscsi.rom successfully created.\n");

    free(rombuf);
    free(zerobuf);
    free(devicebuf);
    
    fclose(out);
    fclose(device);
    fclose(rom);

    return 0;
}
