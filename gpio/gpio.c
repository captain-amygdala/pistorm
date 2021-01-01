#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "../m68k.h"
#include "gpio.h"
#include "../platforms/amiga/Gayle.h"

// I/O access
volatile unsigned int *gpio;
volatile unsigned int *gpclk;
volatile unsigned int gpfsel0;
volatile unsigned int gpfsel1;
volatile unsigned int gpfsel2;
volatile unsigned int gpfsel0_o;
volatile unsigned int gpfsel1_o;
volatile unsigned int gpfsel2_o;

volatile uint16_t srdata;
volatile uint32_t srdata2;
volatile uint32_t srdata2_old;

extern int mem_fd, mouse_fd, keyboard_fd;
extern int mem_fd_gpclk;

void *gpio_map;
void *gpclk_map;

unsigned char toggle;

static int g = 0;

inline void write16(uint32_t address, uint32_t data) {
  uint32_t addr_h_s = (address & 0x0000ffff) << 8;
  uint32_t addr_h_r = (~address & 0x0000ffff) << 8;
  uint32_t addr_l_s = (address >> 16) << 8;
  uint32_t addr_l_r = (~address >> 16) << 8;
  uint32_t data_s = (data & 0x0000ffff) << 8;
  uint32_t data_r = (~data & 0x0000ffff) << 8;

  //      asm volatile ("dmb" ::: "memory");
  W16
  *(gpio) = gpfsel0_o;
  *(gpio + 1) = gpfsel1_o;
  *(gpio + 2) = gpfsel2_o;

  *(gpio + 7) = addr_h_s;
  *(gpio + 10) = addr_h_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio + 7) = addr_l_s;
  *(gpio + 10) = addr_l_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  // write phase
  *(gpio + 7) = data_s;
  *(gpio + 10) = data_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;
  while ((GET_GPIO(0)))
    ;
  //     asm volatile ("dmb" ::: "memory");
}

inline void write8(uint32_t address, uint32_t data) {
  if ((address & 1) == 0)
    data = data + (data << 8);  // EVEN, A0=0,UDS
  else
    data = data & 0xff;  // ODD , A0=1,LDS
  uint32_t addr_h_s = (address & 0x0000ffff) << 8;
  uint32_t addr_h_r = (~address & 0x0000ffff) << 8;
  uint32_t addr_l_s = (address >> 16) << 8;
  uint32_t addr_l_r = (~address >> 16) << 8;
  uint32_t data_s = (data & 0x0000ffff) << 8;
  uint32_t data_r = (~data & 0x0000ffff) << 8;

  //   asm volatile ("dmb" ::: "memory");
  W8
  *(gpio) = gpfsel0_o;
  *(gpio + 1) = gpfsel1_o;
  *(gpio + 2) = gpfsel2_o;

  *(gpio + 7) = addr_h_s;
  *(gpio + 10) = addr_h_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio + 7) = addr_l_s;
  *(gpio + 10) = addr_l_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  // write phase
  *(gpio + 7) = data_s;
  *(gpio + 10) = data_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;
  while ((GET_GPIO(0)))
    ;
  //   asm volatile ("dmb" ::: "memory");
}

inline uint32_t read16(uint32_t address) {
  volatile int val;
  uint32_t addr_h_s = (address & 0x0000ffff) << 8;
  uint32_t addr_h_r = (~address & 0x0000ffff) << 8;
  uint32_t addr_l_s = (address >> 16) << 8;
  uint32_t addr_l_r = (~address >> 16) << 8;

  //   asm volatile ("dmb" ::: "memory");
  R16
  *(gpio) = gpfsel0_o;
  *(gpio + 1) = gpfsel1_o;
  *(gpio + 2) = gpfsel2_o;

  *(gpio + 7) = addr_h_s;
  *(gpio + 10) = addr_h_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio + 7) = addr_l_s;
  *(gpio + 10) = addr_l_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  // read phase
  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;
  GPIO_CLR = 1 << 6;
  while (!(GET_GPIO(0)))
    ;
  GPIO_CLR = 1 << 6;
  val = *(gpio + 13);
  GPIO_SET = 1 << 6;
  //    asm volatile ("dmb" ::: "memory");
  return (val >> 8) & 0xffff;
}

inline uint32_t read8(uint32_t address) {
  int val;
  uint32_t addr_h_s = (address & 0x0000ffff) << 8;
  uint32_t addr_h_r = (~address & 0x0000ffff) << 8;
  uint32_t addr_l_s = (address >> 16) << 8;
  uint32_t addr_l_r = (~address >> 16) << 8;

  //    asm volatile ("dmb" ::: "memory");
  R8
  *(gpio) = gpfsel0_o;
  *(gpio + 1) = gpfsel1_o;
  *(gpio + 2) = gpfsel2_o;

  *(gpio + 7) = addr_h_s;
  *(gpio + 10) = addr_h_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  *(gpio + 7) = addr_l_s;
  *(gpio + 10) = addr_l_r;
  GPIO_CLR = 1 << 7;
  GPIO_SET = 1 << 7;

  // read phase
  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;

  GPIO_CLR = 1 << 6;
  while (!(GET_GPIO(0)))
    ;
  GPIO_CLR = 1 << 6;
  val = *(gpio + 13);
  GPIO_SET = 1 << 6;
  //    asm volatile ("dmb" ::: "memory");

  val = (val >> 8) & 0xffff;
  if ((address & 1) == 0)
    return (val >> 8) & 0xff;  // EVEN, A0=0,UDS
  else
    return val & 0xff;  // ODD , A0=1,LDS
}

/******************************************************/

void write_reg(unsigned int value) {
  STATUSREGADDR
  *(gpio) = gpfsel0_o;
  *(gpio + 1) = gpfsel1_o;
  *(gpio + 2) = gpfsel2_o;
  *(gpio + 7) = (value & 0xffff) << 8;
  *(gpio + 10) = (~value & 0xffff) << 8;
  GPIO_CLR = 1 << 7;
  GPIO_CLR = 1 << 7;  // delay
  GPIO_SET = 1 << 7;
  GPIO_SET = 1 << 7;
  // Bus HIGH-Z
  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;
}

uint16_t read_reg(void) {
  uint32_t val;
  STATUSREGADDR
  // Bus HIGH-Z
  *(gpio) = gpfsel0;
  *(gpio + 1) = gpfsel1;
  *(gpio + 2) = gpfsel2;
  GPIO_CLR = 1 << 6;
  GPIO_CLR = 1 << 6;  // delay
  GPIO_CLR = 1 << 6;
  GPIO_CLR = 1 << 6;
  val = *(gpio + 13);
  GPIO_SET = 1 << 6;
  return (uint16_t)(val >> 8);
}

//
// Set up a memory regions to access GPIO
//
void setup_io() {
  /* open /dev/mem */
  if ((mem_fd = open("/dev/mem", O_RDWR | O_SYNC)) < 0) {
    printf("can't open /dev/mem \n");
    exit(-1);
  }

  /* mmap GPIO */
  gpio_map = mmap(
      NULL,                    // Any adddress in our space will do
      BCM2708_PERI_SIZE,       // Map length
      PROT_READ | PROT_WRITE,  // Enable reading & writting to mapped memory
      MAP_SHARED,              // Shared with other processes
      mem_fd,                  // File to map
      BCM2708_PERI_BASE        // Offset to GPIO peripheral
  );

  close(mem_fd);  // No need to keep mem_fd open after mmap

  if (gpio_map == MAP_FAILED) {
    printf("gpio mmap error %d\n", (int)gpio_map);  // errno also set!
    exit(-1);
  }

  gpio = ((volatile unsigned *)gpio_map) + GPIO_ADDR / 4;
  gpclk = ((volatile unsigned *)gpio_map) + GPCLK_ADDR / 4;

}  // setup_io

void gpio_enable_200mhz() {
  *(gpclk + (CLK_GP0_CTL / 4)) = CLK_PASSWD | (1 << 5);
  usleep(10);
  while ((*(gpclk + (CLK_GP0_CTL / 4))) & (1 << 7))
    ;
  usleep(100);
  *(gpclk + (CLK_GP0_DIV / 4)) =
      CLK_PASSWD | (6 << 12);  // divider , 6=200MHz on pi3
  usleep(10);
  *(gpclk + (CLK_GP0_CTL / 4)) =
      CLK_PASSWD | 5 | (1 << 4);  // pll? 6=plld, 5=pllc
  usleep(10);
  while (((*(gpclk + (CLK_GP0_CTL / 4))) & (1 << 7)) == 0)
    ;
  usleep(100);

  SET_GPIO_ALT(4, 0);  // gpclk0

  // set SA to output
  INP_GPIO(2);
  OUT_GPIO(2);
  INP_GPIO(3);
  OUT_GPIO(3);
  INP_GPIO(5);
  OUT_GPIO(5);

  // set gpio0 (aux0) and gpio1 (aux1) to input
  INP_GPIO(0);
  INP_GPIO(1);

  // Set GPIO pins 6,7 and 8-23 to output
  for (g = 6; g <= 23; g++) {
    INP_GPIO(g);
    OUT_GPIO(g);
  }
  printf("Precalculate GPIO8-23 as Output\n");
  gpfsel0_o = *(gpio);  // store gpio ddr
  printf("gpfsel0: %#x\n", gpfsel0_o);
  gpfsel1_o = *(gpio + 1);  // store gpio ddr
  printf("gpfsel1: %#x\n", gpfsel1_o);
  gpfsel2_o = *(gpio + 2);  // store gpio ddr
  printf("gpfsel2: %#x\n", gpfsel2_o);

  // Set GPIO pins 8-23 to input
  for (g = 8; g <= 23; g++) {
    INP_GPIO(g);
  }
  printf("Precalculate GPIO8-23 as Input\n");
  gpfsel0 = *(gpio);  // store gpio ddr
  printf("gpfsel0: %#x\n", gpfsel0);
  gpfsel1 = *(gpio + 1);  // store gpio ddr
  printf("gpfsel1: %#x\n", gpfsel1);
  gpfsel2 = *(gpio + 2);  // store gpio ddr
  printf("gpfsel2: %#x\n", gpfsel2);

  GPIO_CLR = 1 << 2;
  GPIO_CLR = 1 << 3;
  GPIO_SET = 1 << 5;

  GPIO_SET = 1 << 6;
  GPIO_SET = 1 << 7;
}

void gpio_handle_irq() {
  if (GET_GPIO(1) == 0) {
    srdata = read_reg();
    m68k_set_irq((srdata >> 13) & 0xff);
  } else {
    if (CheckIrq() == 1) {
      write16(0xdff09c, 0x8008);
      m68k_set_irq(2);
    }
    else
        m68k_set_irq(0);
  };
}

void *iplThread(void *args) {
  printf("IPL thread running/n");

  while (42) {

    if (GET_GPIO(1) == 0) {
      toggle = 1;
      m68k_end_timeslice();
   //printf("thread!/n");
    } else {
      toggle = 0;
    };
    usleep(1);
  }

}