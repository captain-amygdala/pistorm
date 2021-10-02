// SPDX-License-Identifier: MIT
/**
 * pistorm
 * emulator function declarations
 */

#ifndef _EMULATOR_H
#define _EMULATOR_H

// see feature_set_macros(7)
#define _GNU_SOURCE

#include <stdint.h>

/*
void write16(uint32_t address,uint16_t data);
uint16_t read16(uint32_t address);
void write8(uint32_t address,uint16_t data);
uint16_t read8(uint32_t address);
*/

void cpu_pulse_reset(void);
void m68ki_int_ack(uint8_t int_level);
unsigned int cpu_irq_ack(int level);
unsigned int m68k_read_memory_8(unsigned int address);
unsigned int m68k_read_memory_16(unsigned int address);
unsigned int m68k_read_memory_32(unsigned int address);
void m68k_write_memory_8(unsigned int address, unsigned int value);
void m68k_write_memory_16(unsigned int address, unsigned int value);
void m68k_write_memory_32(unsigned int address, unsigned int value);

void stop_cpu_emulation(uint8_t disasm_cur);

#endif /* _EMULATOR_H */
