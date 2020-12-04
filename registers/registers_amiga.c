#include "../Gayle.h"
#include "../config_file/config_file.h"

#define GAYLEBASE 0xD80000  // D7FFFF
#define GAYLESIZE 0x6FFFF

int handle_register_read(unsigned int addr, unsigned char type, unsigned int *val) {
    if (addr > GAYLEBASE && addr < GAYLEBASE + GAYLESIZE) {
        switch(type) {
        case OP_TYPE_BYTE:
            *val = readGayleB(addr);
            return 1;
            break;
        case OP_TYPE_WORD:
            *val = readGayle(addr);
            return 1;
            break;
        case OP_TYPE_LONGWORD:
            *val = readGayleL(addr);
            return 1;
            break;
        case OP_TYPE_MEM:
            return -1;
            break;
        }
    }
    return -1;
}

int handle_register_write(unsigned int addr, unsigned int value, unsigned char type) {
    if (addr > GAYLEBASE && addr < GAYLEBASE + GAYLESIZE) {
        switch(type) {
        case OP_TYPE_BYTE:
            writeGayleB(addr, value);
            return 1;
            break;
        case OP_TYPE_WORD:
            writeGayle(addr, value);
            return 1;
            break;
        case OP_TYPE_LONGWORD:
            writeGayleL(addr, value);
            return 1;
            break;
        case OP_TYPE_MEM:
            return -1;
            break;
        }
    }
    return -1;
}
