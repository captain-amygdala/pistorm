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
#include <sys/ioctl.h>
#include "Gayle.h"
#include "ide.h"
#include "m68k.h"
#include "main.h"
#include "platforms/platforms.h"
#include "input/input.h"

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

#define FASTBASE 0x07FFFFFF
#define FASTSIZE 0xFFFFFFF
#define GAYLEBASE 0xD80000  // D7FFFF
#define GAYLESIZE 0x6FFFF

#define JOY0DAT 0xDFF00A
#define JOY1DAT 0xDFF00C
#define CIAAPRA 0xBFE001
#define POTGOR  0xDFF016

int kb_hook_enabled = 0;
int mouse_hook_enabled = 0;
int cpu_emulation_running = 1;

char mouse_dx = 0, mouse_dy = 0;
char mouse_buttons = 0;

#define KICKBASE 0xF80000
#define KICKSIZE 0x7FFFF

int mem_fd, mouse_fd = -1, keyboard_fd = -1;
int mem_fd_gpclk;
int gayle_emulation_enabled = 1;
void *gpio_map;
void *gpclk_map;

// Configurable emulator options
unsigned int cpu_type = M68K_CPU_TYPE_68000;
unsigned int loop_cycles = 300;
struct emulator_config *cfg = NULL;
char keyboard_file[256] = "/dev/input/event1";

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

//unsigned char g_kick[524288];
//unsigned char g_ram[FASTSIZE + 1]; /* RAM */
unsigned char toggle;
static volatile unsigned char ovl;
static volatile unsigned char maprom;

void sigint_handler(int sig_num) {
  //if (sig_num) { }
  //cpu_emulation_running = 0;

  //return;
  printf("Received sigint %d, exiting.\n", sig_num);
  if (mouse_fd != -1)
    close(mouse_fd);
  if (mem_fd)
    close(mem_fd);

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
    else if (strcmp(argv[g], "--cpu_type") == 0 || strcmp(argv[g], "--cpu") == 0) {
      if (g + 1 >= argc) {
        printf("%s switch found, but no CPU type specified.\n", argv[g]);
      } else {
        g++;
        cpu_type = get_m68k_cpu_type(argv[g]);
      }
    }
    else if (strcmp(argv[g], "--config-file") == 0 || strcmp(argv[g], "--config") == 0) {
      if (g + 1 >= argc) {
        printf("%s switch found, but no config filename specified.\n", argv[g]);
      } else {
        g++;
        cfg = load_config_file(argv[g]);
      }
    }
  }

  if (!cfg) {
    printf("No config file specified. Trying to load default.cfg...\n");
    cfg = load_config_file("default.cfg");
    if (!cfg) {
      printf("Couldn't load default.cfg, empty emulator config will be used.\n");
      cfg = (struct emulator_config *)calloc(1, sizeof(struct emulator_config));
      if (!cfg) {
        printf("Failed to allocate memory for emulator config!\n");
        return 1;
      }
      memset(cfg, 0x00, sizeof(struct emulator_config));
    }
  }

  if (cfg) {
    if (cfg->cpu_type) cpu_type = cfg->cpu_type;
    if (cfg->loop_cycles) loop_cycles = cfg->loop_cycles;

    if (!cfg->platform)
      cfg->platform = make_platform_config("none", "generic");
    cfg->platform->platform_initial_setup(cfg);
  }

  if (cfg->mouse_enabled) {
    mouse_fd = open(cfg->mouse_file, O_RDONLY | O_NONBLOCK);
    if (mouse_fd == -1) {
      printf("Failed to open %s, can't enable mouse hook.\n", cfg->mouse_file);
      cfg->mouse_enabled = 0;
    }
  }

  keyboard_fd = open(keyboard_file, O_RDONLY | O_NONBLOCK);
  if (keyboard_fd == -1) {
    printf("Failed to open keyboard event source.\n");
  }

  sched_setscheduler(0, SCHED_FIFO, &priority);
  mlockall(MCL_CURRENT);  // lock in memory to keep us from paging out

  InitGayle();

  signal(SIGINT, sigint_handler);
  setup_io();

  //goto skip_everything;

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

  // reset amiga and statemachine
  skip_everything:;
  cpu_pulse_reset();
  ovl = 1;
  m68k_write_memory_8(0xbfe201, 0x0001);  // AMIGA OVL
  m68k_write_memory_8(0xbfe001, 0x0001);  // AMIGA OVL high (ROM@0x0)

  usleep(1500);

  m68k_init();
  printf("Setting CPU type to %d.\n", cpu_type);
  m68k_set_cpu_type(cpu_type);
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
  char c = 0;

  m68k_pulse_reset();
  while (42) {
    if (mouse_hook_enabled) {
      if (get_mouse_status(&mouse_dx, &mouse_dy, &mouse_buttons)) {
        //printf("Maus: %d (%.2X), %d (%.2X), B:%.2X\n", mouse_dx, mouse_dx, mouse_dy, mouse_dy, mouse_buttons);
      }
    }

    if (cpu_emulation_running)
      m68k_execute(loop_cycles);
    
    // FIXME: Rework this to use keyboard events instead.
    while (get_key_char(&c)) {
      if (c == cfg->keyboard_toggle_key && !kb_hook_enabled) {
        kb_hook_enabled = 1;
        printf("Keyboard hook enabled.\n");
      }
      else if (c == 0x1B && kb_hook_enabled) {
        kb_hook_enabled = 0;
        printf("Keyboard hook disabled.\n");
      }
      if (!kb_hook_enabled) {
        if (c == cfg->mouse_toggle_key) {
          mouse_hook_enabled ^= 1;
          printf("Mouse hook %s.\n", mouse_hook_enabled ? "enabled" : "disabled");
          mouse_dx = mouse_dy = mouse_buttons = 0;
        }
        if (c == 'r') {
          cpu_emulation_running ^= 1;
          printf("CPU emulation is now %s\n", cpu_emulation_running ? "running" : "stopped");
        }
        if (c == 'R') {
          cpu_pulse_reset();
          m68k_pulse_reset();
          printf("CPU emulation reset.\n");
        }
        if (c == 'q') {
          printf("Quitting and exiting emulator.\n");
          goto stop_cpu_emulation;
        }
      }
    }
/*
    if (toggle == 1){
      srdata = read_reg();
      m68k_set_irq((srdata >> 13) & 0xff);
    } else {
         m68k_set_irq(0);
    };
    usleep(1);
*/


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

  stop_cpu_emulation:;

  if (mouse_fd != -1)
    close(mouse_fd);
  if (mem_fd)
    close(mem_fd);

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

static unsigned int target = 0;

unsigned int m68k_read_memory_8(unsigned int address) {
  if (cfg->platform->custom_read && cfg->platform->custom_read(cfg, address, &target, OP_TYPE_BYTE) != -1) {
    return target;
  }

  if (cfg) {
    int ret = handle_mapped_read(cfg, address, &target, OP_TYPE_BYTE, ovl);
    if (ret != -1)
      return target;
  }

    address &=0xFFFFFF;
//  if (address < 0xffffff) {
    return read8((uint32_t)address);
//  }

//  return 1;
}

unsigned int m68k_read_memory_16(unsigned int address) {
  if (cfg->platform->custom_read && cfg->platform->custom_read(cfg, address, &target, OP_TYPE_WORD) != -1) {
    return target;
  }

  if (cfg) {
    int ret = handle_mapped_read(cfg, address, &target, OP_TYPE_WORD, ovl);
    if (ret != -1)
      return target;
  }

  if (mouse_hook_enabled) {
    if (address == JOY0DAT) {
      // Forward mouse valueses to Amyga.
      unsigned short result = (mouse_dy << 8) | (mouse_dx);
      mouse_dx = mouse_dy = 0;
      return (unsigned int)result;
    }
    if (address == CIAAPRA) {
      unsigned short result = (unsigned int)read16((uint32_t)address);
      if (mouse_buttons & 0x01) {
        mouse_buttons -= 1;
        return (unsigned int)(result | 0x40);
      }
      else
          return (unsigned int)result;
    }
    if (address == POTGOR) {
      unsigned short result = (unsigned int)read16((uint32_t)address);
      if (mouse_buttons & 0x02) {
        mouse_buttons -= 2;
        return (unsigned int)(result | 0x2);
      }
      else
          return (unsigned int)result;
    }
  }

//  if (address < 0xffffff) {
    address &=0xFFFFFF;
    return (unsigned int)read16((uint32_t)address);
//  }

//  return 1;
}

unsigned int m68k_read_memory_32(unsigned int address) {
  if (cfg->platform->custom_read && cfg->platform->custom_read(cfg, address, &target, OP_TYPE_LONGWORD) != -1) {
    return target;
  }

  if (cfg) {
    int ret = handle_mapped_read(cfg, address, &target, OP_TYPE_LONGWORD, ovl);
    if (ret != -1)
      return target;
  }

//  if (address < 0xffffff) {
    address &=0xFFFFFF;
    uint16_t a = read16(address);
    uint16_t b = read16(address + 2);
    return (a << 16) | b;
//  }

//  return 1;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
  if (cfg->platform->custom_write && cfg->platform->custom_write(cfg, address, value, OP_TYPE_BYTE) != -1) {
    return;
  }

  if (cfg) {
    int ret = handle_mapped_write(cfg, address, value, OP_TYPE_BYTE, ovl);
    if (ret != -1)
      return;
  }

  if (address == 0xbfe001) {
    ovl = (value & (1 << 0));
    printf("OVL:%x\n", ovl);
  }

//  if (address < 0xffffff) {
    address &=0xFFFFFF;
    write8((uint32_t)address, value);
    return;
//  }

//  return;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
  if (cfg->platform->custom_write && cfg->platform->custom_write(cfg, address, value, OP_TYPE_WORD) != -1) {
    return;
  }

  if (cfg) {
    int ret = handle_mapped_write(cfg, address, value, OP_TYPE_WORD, ovl);
    if (ret != -1)
      return;
  }

//  if (address < 0xffffff) {
    address &=0xFFFFFF;
    write16((uint32_t)address, value);
    return;
//  }
//  return;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
  if (cfg->platform->custom_write && cfg->platform->custom_write(cfg, address, value, OP_TYPE_LONGWORD) != -1) {
    return;
  }

  if (cfg) {
    int ret = handle_mapped_write(cfg, address, value, OP_TYPE_LONGWORD, ovl);
    if (ret != -1)
      return;
  }

//  if (address < 0xffffff) {
    address &=0xFFFFFF;
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
