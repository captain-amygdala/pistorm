// SPDX-License-Identifier: MIT

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "rominfo.h"

static void getDiagRom(uint8_t *address, struct romInfo *info)
{
    uint8_t *ptr = address + 0xC8;
    uint8_t data = *ptr;
    char *endptr = NULL;
    if (data != 0x56) // V
    {
        return;
    }
    ptr++;
    info->major = strtoul((const char*)ptr, &endptr, 10);
    if (!endptr)
    {
        return;
    }
    data = *endptr;
    if (data != '.')
    {
        info->minor = 0;
        return;
    }
    endptr++;
    info->minor = strtoul(endptr, &endptr, 10);
    if (!endptr)
    {
        return;
    }
    info->isDiagRom = true;
    data = *endptr;
    if (data != '.')
    {
        info->extra = 0;
        return;
    }
    endptr++;
    info->extra = strtoul(endptr, NULL, 10);
}

static void swapRom(uint8_t *address, size_t length)
{
    uint8_t *ptr = address;
    for (size_t pos = 0; pos < length; pos = pos + 2)
    {
        uint8_t low = ptr[pos];
        uint8_t high = ptr[pos+1];
        ptr[pos] = high;
        ptr[pos+1] = low;
    }
}

static void getRomInfo(uint8_t *address, size_t length, struct romInfo *info)
{
    uint8_t *ptr = address;
    uint8_t data = *ptr;
    info->isDiagRom = false;

    if ((ptr[2] == 0xF9) && (ptr[3] == 0x4E))
    {
        // ROM byte swapped!
        printf("[CFG] Byte swapped ROM found, swapping back\n");
        swapRom(address, length);
        data = *ptr;
    }

    if (data != 0x11)
    {
        info->id = ROM_TYPE_UNKNOWN;

        return;
    }
    ptr++;
    data = *ptr;
    switch (data)
    {
        case 0x11:
            info->id = ROM_TYPE_256;
            break;
        case 0x14:
        case 0x16: // kick40063.A600
            info->id = ROM_TYPE_512;
            break;
        default:
            info->id = ROM_TYPE_UNKNOWN;
            return;
            break;
    }
    ptr++;
    data = *ptr;
    if (data != 0x4E) // 256K byte swapped
    {
        return;
    }
    // This is wrong endian for us
    uint16_t ver_read;
    memcpy(&ver_read, address+12, 2);
    info->major = (ver_read >> 8) | (ver_read << 8);
    memcpy(&ver_read, address+14, 2);
    info->minor = (ver_read >> 8) | (ver_read << 8);
    // We hit part of a memory ptr for DiagROM, it will be > 200
    if (info->major > 100)
    {
        getDiagRom(address, info);
    }
    return;
}

void displayRomInfo(uint8_t *address, size_t length)
{
    struct romInfo info = {0};
    const char *kversion;
    const char *size;

    getRomInfo(address, length, &info);

    if ((!info.isDiagRom) && (info.id != ROM_TYPE_UNKNOWN))
    {
        switch(info.major)
        {
            case 27:
                kversion = "Kickstart 0.7";
                break;
            case 30:
                kversion = "Kickstart 1.0";
                break;
            case 31:
                kversion = "Kickstart 1.1";
                break;
            case 33:
                kversion = "Kickstart 1.2";
                break;
            case 34:
                kversion = "Kickstart 1.3";
                break;
            case 36:
                {
                    // 36.002 and 36.015 were 1.4
                    // 36.028, 36.067, 36.141 and 36.143 were 2.0
                    // 36.207 was 2.02
                    if (info.minor < 20)
                    {
                        kversion = "Kickstart 1.4";
                    }
                    else if (info.minor < 200)
                    {
                        kversion = "Kickstart 2.0";
                    }
                    else
                    {
                        kversion = "Kickstart 2.02";
                    }
                    break;
                }
            case 37:
                {
                    // 37.074 and 37.092 were 2.03
                    // 37.175 was 2.04
                    // 37.210, 37.299, 37.300 and 37.350 were 2.05
                    if (info.minor < 100)
                    {
                        kversion = "Kickstart 2.03";
                    }
                    else if (info.minor < 200)
                    {
                        kversion = "Kickstart 2.04";
                    }
                    else
                    {
                        kversion = "Kickstart 2.05";
                    }
                    break;
                }
                break;
            case 39:
                kversion = "Kickstart 3.0";
                break;
            case 40:
                kversion = "Kickstart 3.1";
                break;
            case 45:
                kversion = "Kickstart 3.x";
                break;
            case 46:
                kversion = "Kickstart 3.1.4";
                break;
            case 47:
                kversion = "Kickstart 3.2";
                break;
            default:
                kversion = "Unknown";
                break;
        }
    }
    switch (info.id)
    {
        case ROM_TYPE_256:
            size = "256KB";
            break;
        case ROM_TYPE_512:
            size = "512KB";
            break;
        default:
            size = "";
            break;
    }

    if (info.isDiagRom)
    {
        printf("[CFG] ROM identified: DiagRom V%hu.%hu.%hu %s\n", info.major, info.minor, info.extra, size);
    }
    else if (info.id == ROM_TYPE_UNKNOWN)
    {
        printf("[CFG] ROM cannot be identified\n");
    }
    else
    {
        printf("[CFG] ROM identified: %s (%hu.%hu) %s\n", kversion, info.major, info.minor, size);
    }
}
