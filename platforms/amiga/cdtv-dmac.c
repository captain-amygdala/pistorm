#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "config_file/config_file.h"

uint8_t dmac_reg_idx = 0;
uint8_t dmac_reg_values[0xFFFF];

uint8_t cdtv_dmac_reg_idx_read() {
    return dmac_reg_idx;
}

/* DMAC Registers
R   0x06 [B]    - Something

    0x40 [W]    - ISTR
RW  0x42 [W]    - CNTR

    0x80 [L]    - WTC
    0x84 [L]    - ACR

    0x8E [B]    - SASR
W   0x8F [B]    - Something
    0x90        - SCMD
    0x91 [B]    - Something
    0x92 [B]    - Something?
RW  0x93 [B]    - Something

R   0xA2?[W?]   - Some status thing?
W   0xA4?[W?]   - Something
W   0xA6?[W?]   - Something
W   0xA8?[W?]   - Something

    0xDE [W]    - ST_DMA
    0xE0 [W]    - SP_DMA
    0xE2 [W]    - CINT
    0xE4 [W]    - Something
    0xE4-0xE5   - Nothing
    0xE6 [W]    - Flush
*/

void cdtv_dmac_reg_idx_write(uint8_t value) {
    dmac_reg_idx = value;
}

uint32_t cdtv_dmac_read(uint32_t address, uint8_t type) {
    uint32_t ret = 0;

    switch (type) {
        case OP_TYPE_BYTE:
            return dmac_reg_values[address];
        case OP_TYPE_WORD:
            return be16toh(*((uint16_t *)&dmac_reg_values[address]));
        default:
            break;
    }

    return ret;
}

void cdtv_dmac_write(uint32_t address, uint32_t value, uint8_t type) {
    switch (type) {
        case OP_TYPE_BYTE:
            dmac_reg_values[address] = (uint8_t)value;
            return ;
        case OP_TYPE_WORD:
            printf("Help, it's a scary word write.\n");
            *((uint16_t *)&dmac_reg_values[address]) = htobe16(value);
            return;
    }
}
