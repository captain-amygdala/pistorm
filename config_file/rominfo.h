// SPDX-License-Identifier: MIT

#ifndef __ROMINFO__
#define __ROMINFO__

#include <stdbool.h>
#include <inttypes.h>

enum romType {
    ROM_TYPE_UNKNOWN,
    ROM_TYPE_256,
    ROM_TYPE_512,
};

struct romInfo {
    enum romType id;
    uint16_t major;
    uint16_t minor;
    uint16_t extra;
    bool isDiagRom;
};

enum romErrCode {
    ERR_NO_ERR,
    ERR_NOT_ROM,
    ERR_ROM_UNKNOWN
};

void displayRomInfo(uint8_t *address);
#endif /* __ROMINFO */
