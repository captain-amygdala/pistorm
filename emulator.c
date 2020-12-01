/*
Copyright 2020 Claude Schwartz
*/

#include <assert.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "Gayle.h"
#include "a314/a314.h"
#include "ide.h"
#include "m68k.h"
#include "main.h"

//#define BCM2708_PERI_BASE        0x20000000  //pi0-1
//#define BCM2708_PERI_BASE	0xFE000000     //pi4
#define BCM2708_PERI_BASE 0x3F000000  // pi3
#define BCM2708_PERI_SIZE 0x01000000
#define GPIO_BASE (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
#define GPCLK_BASE (BCM2708_PERI_BASE + 0x101000)
#define GPIO_ADDR 0x200000 /* GPIO controller */
#define GPCLK_ADDR 0x101000
#define CLK_PASSWD 0x5a000000
#define CLK_GP0_CTL 0x070
#define CLK_GP0_DIV 0x074

#define SA0 5
#define SA1 3
#define SA2 2

#define STATUSREGADDR  \
  GPIO_CLR = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_SET = 1 << SA2;
#define W16            \
  GPIO_CLR = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define R16            \
  GPIO_SET = 1 << SA0; \
  GPIO_CLR = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define W8             \
  GPIO_CLR = 1 << SA0; \
  GPIO_SET = 1 << SA1; \
  GPIO_CLR = 1 << SA2;
#define R8             \
  GPIO_SET = 1 << SA0; \
  GPIO_SET = 1 << SA1; \
  GPIO_CLR = 1 << SA2;

#define PAGE_SIZE (4 * 1024)
#define BLOCK_SIZE (4 * 1024)

#define GPIOSET(no, ishigh) \
  do {                      \
    if (ishigh)             \
      set |= (1 << (no));   \
    else                    \
      reset |= (1 << (no)); \
  } while (0)

int fast_base_configured;
unsigned int fast_base;
#define FAST_SIZE (8 * 1024 * 1024)

#define GAYLEBASE 0xD80000
#define GAYLESIZE (448 * 1024)

#define KICKBASE 0xF80000
#define KICKSIZE (512 * 1024)

#define AC_BASE 0xE80000
#define AC_SIZE (64 * 1024)

#define AC_PIC_COUNT 2
int ac_current_pic = 0;
int ac_done = 0;

int mem_fd;
int mem_fd_gpclk;
int gayle_emulation_enabled = 1;
void *gpio_map;
void *gpclk_map;

// I/O access
volatile unsigned int *gpio;
volatile unsigned int *gpclk;
volatile unsigned int gpfsel0;
volatile unsigned int gpfsel1;
volatile unsigned int gpfsel2;
volatile unsigned int gpfsel0_o;
volatile unsigned int gpfsel1_o;
volatile unsigned int gpfsel2_o;

// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or
// SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio + ((g) / 10)) &= ~(7 << (((g) % 10) * 3))
#define OUT_GPIO(g) *(gpio + ((g) / 10)) |= (1 << (((g) % 10) * 3))
#define SET_GPIO_ALT(g, a)  \
  *(gpio + (((g) / 10))) |= \
      (((a) <= 3 ? (a) + 4 : (a) == 4 ? 3 : 2) << (((g) % 10) * 3))

#define GPIO_SET \
  *(gpio + 7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR \
  *(gpio + 10)  // clears bits which are 1 ignores bits which are 0

#define GET_GPIO(g) (*(gpio + 13) & (1 << g))  // 0 if LOW, (1<<g) if HIGH

#define GPIO_PULL *(gpio + 37)      // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio + 38)  // Pull up/pull down clock

void setup_io();

uint32_t read8(uint32_t address);
void write8(uint32_t address, uint32_t data);

uint32_t read16(uint32_t address);
void write16(uint32_t address, uint32_t data);

void write32(uint32_t address, uint32_t data);
uint32_t read32(uint32_t address);

uint16_t read_reg(void);
void write_reg(unsigned int value);

volatile uint16_t srdata;
volatile uint32_t srdata2;
volatile uint32_t srdata2_old;

unsigned char g_kick[KICKSIZE];
unsigned char fast_ram_array[FAST_SIZE]; /* RAM */
unsigned char toggle;
static volatile unsigned char ovl;
static volatile unsigned char maprom;

void sigint_handler(int sig_num) {
  printf("\n Exit Ctrl+C %d\n", sig_num);
  exit(0);
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

int main(int argc, char *argv[]) {
  int g;
  const struct sched_param priority = {99};

  // Some command line switch stuffles
  for (g = 1; g < argc; g++) {
    if (strcmp(argv[g], "--disable-gayle") == 0) {
      gayle_emulation_enabled = 0;
    }
  }

#if A314_ENABLED
  int err = a314_init();
  if (err < 0) {
    printf("Unable to initialize A314 emulation\n");
    return -1;
  }
#endif

  sched_setscheduler(0, SCHED_FIFO, &priority);
  mlockall(MCL_CURRENT);  // lock in memory to keep us from paging out

  InitGayle();

  signal(SIGINT, sigint_handler);
  setup_io();

  // Enable 200MHz CLK output on GPIO4, adjust divider and pll source depending
  // on pi model
  printf("Enable 200MHz GPCLK0 on GPIO4\n");

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

  // reset cpld statemachine first

  write_reg(0x01);
  usleep(100);
  usleep(1500);
  write_reg(0x00);
  usleep(100);

  // load kick.rom if present
  maprom = 1;
  int fd = 0;
  fd = open("kick.rom", O_RDONLY);
  if (fd < 1) {
    printf("Failed loading kick.rom, using motherboard kickstart\n");
    maprom = 0;
  } else {
    int size = (int)lseek(fd, 0, SEEK_END);
    if (size == 0x40000) {
      lseek(fd, 0, SEEK_SET);
      read(fd, &g_kick, size);
      lseek(fd, 0, SEEK_SET);
      read(fd, &g_kick[0x40000], size);
    } else {
      lseek(fd, 0, SEEK_SET);
      read(fd, &g_kick, size);
    }
    printf("Loaded kick.rom with size %d kib\n", size / 1024);
  }

  // reset amiga and statemachine
  cpu_pulse_reset();
  ovl = 1;
  m68k_write_memory_8(0xbfe201, 0x0001);  // AMIGA OVL
  m68k_write_memory_8(0xbfe001, 0x0001);  // AMIGA OVL high (ROM@0x0)

  usleep(1500);

  m68k_init();
  m68k_set_cpu_type(M68K_CPU_TYPE_68020);
  m68k_pulse_reset();

  if (maprom == 1) {
    m68k_set_reg(M68K_REG_PC, 0xF80002);
  } else {
    m68k_set_reg(M68K_REG_PC, 0x0);
  }

  /*
          pthread_t id;
          int err;
          err = pthread_create(&id, NULL, &iplThread, NULL);
          if (err != 0)
              printf("\ncan't create IPL thread :[%s]", strerror(err));
          else
              printf("\n IPL Thread created successfully\n");
*/

  m68k_pulse_reset();
  while (42) {
    m68k_execute(300);
    /*
    if (toggle == 1){
      srdata = read_reg();
      m68k_set_irq((srdata >> 13) & 0xff);
    } else {
         m68k_set_irq(0);
    };
    usleep(1);
*/

#if A314_ENABLED
    a314_process_events();
#endif

    if (GET_GPIO(1) == 0) {
      srdata = read_reg();
      m68k_set_irq((srdata >> 13) & 0xff);
    } else {
      if (CheckIrq() == 1) {
        write16(0xdff09c, 0x8008);
        m68k_set_irq(2);
      } else
        m68k_set_irq(0);
    };
  }

  return 0;
}

void cpu_pulse_reset(void) {
  write_reg(0x00);
  // printf("Status Reg%x\n",read_reg());
  usleep(100000);
  write_reg(0x02);
  // printf("Status Reg%x\n",read_reg());
}

int cpu_irq_ack(int level) {
  printf("cpu irq ack\n");
  return level;
}

#define AC_MEM_SIZE_8MB 0
#define AC_MEM_SIZE_64KB 1
#define AC_MEM_SIZE_128KB 2
#define AC_MEM_SIZE_256KB 3
#define AC_MEM_SIZE_512KB 4
#define AC_MEM_SIZE_1MB 5
#define AC_MEM_SIZE_2MB 6
#define AC_MEM_SIZE_4MB 7

static unsigned char ac_fast_ram_rom[] = {
    0xe, AC_MEM_SIZE_8MB,                   // 00/02, link into memory free list, 8 MB
    0x6, 0x9,                               // 04/06, product id
    0x8, 0x0,                               // 08/0a, preference to 8 MB space
    0x0, 0x0,                               // 0c/0e, reserved
    0x0, 0x7, 0xd, 0xb,                     // 10/12/14/16, mfg id
    0x0, 0x0, 0x0, 0x0, 0x0, 0x4, 0x2, 0x0  // 18/.../26, serial
};

static unsigned char ac_a314_rom[] = {
    0xc, AC_MEM_SIZE_64KB,                  // 00/02, 64 kB
    0xa, 0x3,                               // 04/06, product id
    0x0, 0x0,                               // 08/0a, any space okay
    0x0, 0x0,                               // 0c/0e, reserved
    0x0, 0x7, 0xd, 0xb,                     // 10/12/14/16, mfg id
    0xa, 0x3, 0x1, 0x4, 0x0, 0x0, 0x0, 0x0  // 18/.../26, serial
};

static unsigned int autoconfig_read_memory_8(unsigned int address) {
  unsigned char *rom = NULL;

  if (ac_current_pic == 0)
    rom = ac_fast_ram_rom;
  else if (ac_current_pic == 1)
    rom = ac_a314_rom;

  unsigned char val = 0;
  if ((address & 1) == 0 && (address / 2) < sizeof(ac_fast_ram_rom))
    val = rom[address / 2];
  val <<= 4;
  if (address != 0 && address != 2 && address != 40 && address != 42)
    val ^= 0xf0;
  return (unsigned int)val;
}

static void autoconfig_write_memory_8(unsigned int address, unsigned int value) {
  int done = 0;

  unsigned int *base = NULL;
  int *base_configured = NULL;

  if (ac_current_pic == 0) {
    base = &fast_base;
    base_configured = &fast_base_configured;
  } else if (ac_current_pic == 1) {
    base = &a314_base;
    base_configured = &a314_base_configured;
  }

  if (address == 0x4a) {  // base[19:16]
    *base = (value & 0xf0) << (16 - 4);
  } else if (address == 0x48) {  // base[23:20]
    *base &= 0xff0fffff;
    *base |= (value & 0xf0) << (20 - 4);
    *base_configured = 1;

    if (ac_current_pic == 0)  // fast ram
      a314_set_mem_base_size(*base, FAST_SIZE);

    done = 1;
  } else if (address == 0x4c) {  // shut up
    done = 1;
  }

  if (done) {
    ac_current_pic++;
    if (ac_current_pic == AC_PIC_COUNT)
      ac_done = 1;
  }
}

unsigned int m68k_read_memory_8(unsigned int address) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    return fast_ram_array[address - fast_base];
  }

  if (!ac_done && address >= AC_BASE && address < AC_BASE + AC_SIZE) {
    return autoconfig_read_memory_8(address - AC_BASE);
  }

  if (maprom == 1) {
    if (address >= KICKBASE && address < KICKBASE + KICKSIZE) {
      return g_kick[address - KICKBASE];
    }
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      return readGayleB(address);
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    return a314_read_memory_8(address - a314_base);
  }
#endif

  address &= 0xFFFFFF;
  //  if (address < 0xffffff) {
  return read8((uint32_t)address);
  //  }

  //  return 1;
}

unsigned int m68k_read_memory_16(unsigned int address) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    return be16toh(*(uint16_t *)&fast_ram_array[address - fast_base]);
  }

  if (maprom == 1) {
    if (address >= KICKBASE && address < KICKBASE + KICKSIZE) {
      return be16toh(*(uint16_t *)&g_kick[address - KICKBASE]);
    }
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      return readGayle(address);
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    return a314_read_memory_16(address - a314_base);
  }
#endif

  //  if (address < 0xffffff) {
  address &= 0xFFFFFF;
  return (unsigned int)read16((uint32_t)address);
  //  }

  //  return 1;
}

unsigned int m68k_read_memory_32(unsigned int address) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    return be32toh(*(uint32_t *)&fast_ram_array[address - fast_base]);
  }

  if (maprom == 1) {
    if (address >= KICKBASE && address < KICKBASE + KICKSIZE) {
      return be32toh(*(uint32_t *)&g_kick[address - KICKBASE]);
    }
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      return readGayleL(address);
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    return a314_read_memory_32(address - a314_base);
  }
#endif

  //  if (address < 0xffffff) {
  address &= 0xFFFFFF;
  uint16_t a = read16(address);
  uint16_t b = read16(address + 2);
  return (a << 16) | b;
  //  }

  //  return 1;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    fast_ram_array[address - fast_base] = value;
    return;
  }

  if (!ac_done && address >= AC_BASE && address < AC_BASE + AC_SIZE) {
    autoconfig_write_memory_8(address - AC_BASE, value);
    return;
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      writeGayleB(address, value);
      return;
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    a314_write_memory_8(address - a314_base, value);
    return;
  }
#endif

  /*
  if (address == 0xbfe001) {
    ovl = (value & (1 << 0));
    printf("OVL:%x\n", ovl);
  }
*/
  //  if (address < 0xffffff) {
  address &= 0xFFFFFF;
  write8((uint32_t)address, value);
  return;
  //  }

  //  return;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    *(uint16_t *)&fast_ram_array[address - fast_base] = htobe16(value);
    return;
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      writeGayle(address, value);
      return;
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    a314_write_memory_16(address - a314_base, value);
    return;
  }
#endif

  //  if (address < 0xffffff) {
  address &= 0xFFFFFF;
  write16((uint32_t)address, value);
  return;
  //  }
  //  return;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
  if (fast_base_configured && address >= fast_base && address < fast_base + FAST_SIZE) {
    *(uint32_t *)&fast_ram_array[address - fast_base] = htobe32(value);
    return;
  }

  if (gayle_emulation_enabled) {
    if (address >= GAYLEBASE && address < GAYLEBASE + GAYLESIZE) {
      writeGayleL(address, value);
    }
  }

#if A314_ENABLED
  if (a314_base_configured && address >= a314_base && address < a314_base + A314_COM_AREA_SIZE) {
    a314_write_memory_32(address - a314_base, value);
    return;
  }
#endif

  //  if (address < 0xffffff) {
  address &= 0xFFFFFF;
  write16(address, value >> 16);
  write16(address + 2, value);
  return;
  //  }

  //  return;
}

void write16(uint32_t address, uint32_t data) {
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

void write8(uint32_t address, uint32_t data) {
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

uint32_t read16(uint32_t address) {
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

uint32_t read8(uint32_t address) {
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
