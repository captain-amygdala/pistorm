// SPDX-License-Identifier: MIT

/*
 * Copyright 2022 Claude Schwarz
 * Copyright 2022 Niklas Ekström
 */

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "ps_protocol.h"

#define LOGGER_INFO 1
#define logger_info(...) do { if (LOGGER_INFO) fprintf(stdout, __VA_ARGS__); } while (0)

typedef unsigned int uint;

extern void m68k_pulse_bus_error(void);
//void m68k_pulse_bus_error(void) {}


#define PF(f, i) (f << ((i % 10) * 3)) // Pin function.
#define GO(i) PF(1, i) // GPIO output.

#define SPLIT_DATA(x)   (x)
#define MERGE_DATA(x)   (x)

#define GPFSEL0_INPUT (GO(PIN_WR) | GO(PIN_RD))
#define GPFSEL1_INPUT (0)
#define GPFSEL2_INPUT (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | PF(28,5) | PF(29,5))

#define GPFSEL0_OUTPUT (GO(PIN_D(1)) | GO(PIN_D(0)) | GO(PIN_WR) | GO(PIN_RD))
#define GPFSEL1_OUTPUT (GO(PIN_D(11)) | GO(PIN_D(10)) | GO(PIN_D(9)) | GO(PIN_D(8)) | GO(PIN_D(7)) | GO(PIN_D(6)) | GO(PIN_D(5)) | GO(PIN_D(4)) | GO(PIN_D(3)) | GO(PIN_D(2)))
#define GPFSEL2_OUTPUT (GO(PIN_A(2)) | GO(PIN_A(1)) | GO(PIN_A(0)) | GO(PIN_D(15)) | GO(PIN_D(14)) | GO(PIN_D(13)) | GO(PIN_D(12)) | PF(28,5) | PF(29,5))

#define REG_DATA_LO     0
#define REG_DATA_HI     1
#define REG_ADDR_LO     2
#define REG_ADDR_HI     3
#define REG_STATUS      4
#define REG_CONTROL     4

#define TXN_SIZE_SHIFT  8
#define TXN_RW_SHIFT    10
#define TXN_FC_SHIFT    11

#define SIZE_BYTE       0
#define SIZE_WORD       1
#define SIZE_LONG       3

#define TXN_READ        (1 << TXN_RW_SHIFT)
#define TXN_WRITE       (0 << TXN_RW_SHIFT)

#define GPIO_ADDR 0x200000
#define GPCLK_ADDR 0x101000

#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define GPIO_PULL *(gpio + 37)      // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio + 38)  // Pull up/pull down clock

#define DIRECTION_INPUT 0
#define DIRECTION_OUTPUT 1

volatile unsigned int *gpio;
unsigned int g_polltxn = 1;
volatile unsigned int *gpset;
volatile unsigned int *gpreset;
volatile unsigned int *gpread;


static void create_dev_mem_mapping() {

  int fd = open("/dev/gpiomem", O_RDWR | O_SYNC);
  if (fd < 0) {
    logger_info("Unable to open /dev/gpiomem.\n");
    exit(-1);
  }

  void *gpio_map = mmap(
      NULL,                    // Any adddress in our space will do
      (4*1024),		       // Map length
      PROT_READ | PROT_WRITE,  // Enable reading & writting to mapped memory
      MAP_SHARED,              // Shared with other processes
      fd,                      // File to map
      0			       // Offset to GPIO peripheral
  );

  close(fd);



  if (gpio_map == MAP_FAILED) {
    logger_info("mmap failed, errno = %d\n", errno);
    exit(-1);
  }

  gpio = ((volatile unsigned *)gpio_map);
  gpset = ((volatile unsigned *)gpio) + 7;// *(gpio + 7);
  gpreset = ((volatile unsigned *)gpio) + 10;// *(gpio + 10);
  gpread = ((volatile unsigned *)gpio) + 13;// *(gpio + 13);

}

static inline void set_input() {
  *(gpio + 0) = GPFSEL0_INPUT;
  *(gpio + 1) = GPFSEL1_INPUT;
  *(gpio + 2) = GPFSEL2_INPUT;
}

static inline void set_output() {
  *(gpio + 0) = GPFSEL0_OUTPUT;
  *(gpio + 1) = GPFSEL1_OUTPUT;
  *(gpio + 2) = GPFSEL2_OUTPUT;
}

void ps_setup_protocol() {
  create_dev_mem_mapping();

  *(gpio + 10) = 0x0fffff3f;

  set_input();
}

static inline void write_ps_reg(unsigned int address, unsigned int data) {
  *(gpset) = (SPLIT_DATA(data) << PIN_D(0)) | (address << PIN_A(0));
  //Delay for Pi4, 2*3.5nS
  *(gpreset) = 1 << PIN_WR;*(gpreset) = 1 << PIN_WR;*(gpreset) = (1 << PIN_WR) ;
  *(gpset) = 1 << PIN_WR;
  *(gpreset) = 0x0fffff3f;
}

static inline unsigned int read_ps_reg(unsigned int address) {
  *(gpset) = (address << PIN_A(0));
  //Delay for Pi3, 3*7.5nS , or 3*3.5nS for
  *(gpreset) = 1 << PIN_RD;*(gpreset) = 1 << PIN_RD;*(gpreset) = 1 << PIN_RD;*(gpreset) = (1 << PIN_RD);

  unsigned int data = *(gpread);
  *(gpset) = (1 << PIN_RD);
  *(gpreset) = 0x0fffff3f;

  data = MERGE_DATA(data >> PIN_D(0)) & 0xffff;
  return data;
}


void ps_set_control(unsigned int value) {
  set_output();
  write_ps_reg(REG_CONTROL, 0x8000 | (value & 0x7fff));
  set_input();
}

void ps_clr_control(unsigned int value) {
  set_output();
  write_ps_reg(REG_CONTROL, value & 0x7fff);
  set_input();
}

unsigned int ps_read_status() {
  return read_ps_reg(REG_STATUS);
}

static uint g_fc = 0;

void cpu_set_fc(unsigned int fc) {
  g_fc = fc;
}

void ps_write_8(unsigned int address, unsigned int data) {
  set_output();
  write_ps_reg(REG_DATA_LO, data & 0xffff);
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_BYTE << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  g_polltxn=1;
}

void ps_write_16(unsigned int address, unsigned int data) {
  set_output();
  write_ps_reg(REG_DATA_LO, data & 0xffff);
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_WORD << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  g_polltxn=1;
}

void ps_write_32(unsigned int address, unsigned int data) {
  set_output();
  write_ps_reg(REG_DATA_LO, data & 0xffff);
  write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  g_polltxn=1;
}

unsigned int ps_read_8(unsigned int address) {
  set_output();
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_BYTE << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  while (*(gpread) & (1 << PIN_TXN)) {}
  unsigned int data = read_ps_reg(REG_DATA_LO);
  data &= 0xff;
  g_polltxn=0;

  return data;
}

unsigned int ps_read_16(unsigned int address) {
  set_output();
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_WORD << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  while (*(gpread) & (1 << PIN_TXN)) {}
  unsigned int data = read_ps_reg(REG_DATA_LO);
  g_polltxn=0;

  return data;
}

unsigned int ps_read_32(unsigned int address) {
  set_output();
  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  if(g_polltxn) while (*(gpread) & (1 << PIN_TXN)) {}
  write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (SIZE_LONG << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));
  set_input();
  while (*(gpread) & (1 << PIN_TXN)) {}
  unsigned int data = read_ps_reg(REG_DATA_LO);
  data |= read_ps_reg(REG_DATA_HI) << 16;
  g_polltxn=0;

  return data;
}



static void do_write_access(unsigned int address, unsigned int data, unsigned int size) {

  set_output();

  write_ps_reg(REG_DATA_LO, data & 0xffff);
  if (size == SIZE_LONG)
    write_ps_reg(REG_DATA_HI, (data >> 16) & 0xffff);

  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  write_ps_reg(REG_ADDR_HI, TXN_WRITE | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

 set_input();
 while (*(gpio + 13) & (1 << PIN_TXN)) {}
}

static int do_read_access(unsigned int address, unsigned int size) {

  if (address == 0xf00000)
    usleep(1000);

  set_output();

  write_ps_reg(REG_ADDR_LO, address & 0xffff);
  write_ps_reg(REG_ADDR_HI, TXN_READ | (g_fc << TXN_FC_SHIFT) | (size << TXN_SIZE_SHIFT) | ((address >> 16) & 0xff));

  set_input();
  unsigned int data;

 while (*(gpio + 13) & (1 << PIN_TXN)) {}

  data = read_ps_reg(REG_DATA_LO);
  if (size == SIZE_BYTE)
    data &= 0xff;
  else if (size == SIZE_LONG)
    data |= read_ps_reg(REG_DATA_HI) << 16;

  return data;
}

/*
void ps_write_8(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_BYTE);
}

void ps_write_16(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_WORD);
}

void ps_write_32(unsigned int address, unsigned int data) {
  do_write_access(address, data, SIZE_LONG);
}

unsigned int ps_read_8(unsigned int address) {
  return do_read_access(address, SIZE_BYTE);
}

unsigned int ps_read_16(unsigned int address) {
  return do_read_access(address, SIZE_WORD);
}

unsigned int ps_read_32(unsigned int address) {
  return do_read_access(address, SIZE_LONG);
}
*/

void ps_reset_state_machine() {
}

void ps_pulse_reset() {
  logger_info("Set REQUEST_BM\n");
  ps_set_control(CONTROL_REQ_BM);
  usleep(100000);

  logger_info("Set DRIVE_RESET\n");
  ps_set_control(CONTROL_DRIVE_RESET);
  usleep(150000);

  logger_info("Clear DRIVE_RESET\n");
  ps_clr_control(CONTROL_DRIVE_RESET);
}


void ps_efinix_setup() {
 //set programming pins to output
 *(gpio + 0) = (GO(PIN_CRESET1) | GO(PIN_CRESET2) | GO(PIN_CDI2) | GO(PIN_CDI3) | GO(PIN_CDI5));//GPIO 0..9
 *(gpio + 1) = (GO(PIN_CDI0) | GO(PIN_CDI4) | GO(PIN_CDI7) | GO(PIN_CDI6) | GO(PIN_CBUS0) | GO(PIN_CBUS1) | GO(PIN_CBUS2) | GO(PIN_TESTN));//GPIO 10..19
 *(gpio + 2) = (GO(PIN_CCK) | GO(PIN_CDI1) | GO(PIN_SS)); //GPIO 20..29

 //make sure FPGA is not in reset
 *(gpio + 7) = (1 << PIN_CRESET1) | (1 << PIN_CRESET2) ;

 /*
  valid cbus options are
  x1 SPI => CBUS 3'b111
  x2 SPI => CBUS 3'b110
  x4 SPI => CBUS 3'b101
  x8 SPI => CBUS 3'b100
 */
 //set cbus (x1 spi)
 *(gpio + 7) = (1 << PIN_CBUS0) | (1 << PIN_CBUS1)| (1 << PIN_CBUS2);

 //set other relevant pins for programming to correct level
 *(gpio + 7) = (1 << PIN_TESTN) | (1 << PIN_CCK);
 *(gpio + 10) = (1 << PIN_SS);

 //reset fpga, latching cbus and mode pins
 *(gpio + 10) = (1 << PIN_CRESET1) | (1 << PIN_CRESET2);
 usleep(10000); //wait a bit for ps32-lite glitch filter RC to discharge
 *(gpio + 7) = (1 << PIN_CRESET1) | (1 << PIN_CRESET2);
}

void ps_efinix_write(unsigned char data_out) {
//thats just bitbanged 1x SPI, takes ~100mS to load
unsigned char loop, mask;
for (loop=0,mask=0x80;loop<8;loop++, mask=mask>>1)
    {
    *(gpio + 10) = (1 << PIN_CCK);
    if (data_out & mask) *(gpio + 7) = (1 << PIN_CDI0);
  	 else *(gpio + 10) = (1 << PIN_CDI0);
     *(gpio + 7) = (1 << PIN_CCK);
     *(gpio + 7) = (1 << PIN_CCK); //to get closer to 50/50 duty cycle
    }
 *(gpio + 10) = (1 << PIN_CCK);
}

void ps_efinix_load(char* buffer, long length) {
long i;
 logger_info("Loading FPGA\n");
for (i = 0; i < length; ++i)
 {
  ps_efinix_write(buffer[i]);
 }
//1000 dummy clocks for startup of user logic
for (i = 0; i < 1000; ++i)
 {
  ps_efinix_write(0x00);
 }
 logger_info("FPGA loaded\n");
}
