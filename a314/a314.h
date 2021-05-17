/*
 * Copyright 2020-2021 Niklas Ekström
 * A314 emulation header
 */

#ifndef A314_H
#define A314_H

#ifdef __cplusplus
extern "C" {
#endif

#define A314_ENABLED 1

extern unsigned int a314_base;
extern int a314_base_configured;

#define A314_COM_AREA_SIZE (64 * 1024)

int a314_init();
void a314_set_mem_base_size(unsigned int base, unsigned int size);
void a314_process_events();
void a314_set_config_file(char *filename);

unsigned int a314_read_memory_8(unsigned int address);
unsigned int a314_read_memory_16(unsigned int address);
unsigned int a314_read_memory_32(unsigned int address);

void a314_write_memory_8(unsigned int address, unsigned int value);
void a314_write_memory_16(unsigned int address, unsigned int value);
void a314_write_memory_32(unsigned int address, unsigned int value);

#ifdef __cplusplus
}
#endif

#endif /* A314_H */
