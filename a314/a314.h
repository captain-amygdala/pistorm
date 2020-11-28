// A314 emulation.

#ifndef A314_H
#define A314_H

#ifdef __cplusplus
extern "C" {
#endif

#define A314_ENABLED 1

// TODO: Base address should be obtained dynamically through Auto-Config.
#define A314_COM_AREA_BASE 0xE90000
#define A314_COM_AREA_SIZE (64*1024)

int a314_init();
void a314_process_events();

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
