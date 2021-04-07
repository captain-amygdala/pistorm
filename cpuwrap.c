#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "mem.h"
#include "cpu/m68000.h"
#include "gpio/ps_protocol.h"
#include "m68k.h"

volatile int lastipl;

int get_interrupt_level(void) {
  unsigned int ipl;
  if (!ps_get_ipl_zero()) {
    unsigned int status = ps_read_status_reg();
    ipl = (status & 0xe000) >> 13;
  } else {
    ipl = 0;
  }

 if (ipl != lastipl){
   printf("IPL:%d\n",ipl);
   lastipl = ipl;
   return ipl;
  }
// lastipl = ipl
 return lastipl;
}

uint32_t get_long(uint32_t addr) {
	uint32_t ret = m68k_read_memory_32(addr);
	return ret;
}
uint32_t get_word(uint32_t addr) {
	uint32_t ret = m68k_read_memory_16(addr);
	return ret;
}
uint32_t get_byte(uint32_t addr) {
	uint32_t ret = m68k_read_memory_8(addr);
	return ret;
}
void put_long(uint32_t addr, uint32_t l) {
	m68k_write_memory_32(addr, l);
}
void put_word(uint32_t addr, uint32_t w) {
	m68k_write_memory_16(addr, w);
}
void put_byte(uint32_t addr, uint32_t b) {
	m68k_write_memory_8(addr, b);
}

void m68k_end_timeslice(void){}
void m68k_pulse_reset(void){}
unsigned int m68k_get_reg(void* context, m68k_register_t reg){}
unsigned int m68k_disassemble(char* str_buff, unsigned int pc, unsigned int cpu_type){}
int m68k_execute(int num_cycles){}
void m68k_set_irq(unsigned int int_level){}
void m68k_set_cpu_type(unsigned int cpu_type){}
void m68k_add_ram_range(uint32_t addr, uint32_t upper, unsigned char *ptr){}
void m68k_add_rom_range(uint32_t addr, uint32_t upper, unsigned char *ptr){}
void m68k_init(void){}
/*
void m68k_unset_stop(void){}
void m68k_set_irq (int ipl){}
m68ki_initial_cycles (int cycles)
int m68ki_remaining_cycles
int m68k_get_reg
int m68k_disassemble
init m68k_get_reg
*/
