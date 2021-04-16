// SPDX-License-Identifier: MIT

int handle_register_read_dummy(unsigned int addr, unsigned char type, unsigned int *val) {
    if (addr) {}
    if (type) {}
    if (val) {}
    return -1;
}

int handle_register_write_dummy(unsigned int addr, unsigned int value, unsigned char type) {
    if (addr) {}
    if (type) {}
    if (value) {}
    return -1;
}
