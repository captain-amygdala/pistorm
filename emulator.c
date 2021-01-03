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
#include "m68k.h"
#include "main.h"
#include "platforms/platforms.h"
#include "input/input.h"

#include "platforms/amiga/Gayle.h"
#include "platforms/amiga/gayle-ide/ide.h"
#include "platforms/amiga/amiga-registers.h"
#include "platforms/amiga/rtg/rtg.h"
#include "gpio/gpio.h"

int kb_hook_enabled = 0;
int mouse_hook_enabled = 0;
int cpu_emulation_running = 1;

char mouse_dx = 0, mouse_dy = 0;
char mouse_buttons = 0;

extern volatile unsigned int *gpio;
extern volatile uint16_t srdata;

#define KICKBASE 0xF80000
#define KICKSIZE 0x7FFFF

int mem_fd, mouse_fd = -1, keyboard_fd = -1;
int mem_fd_gpclk;
int gayle_emulation_enabled = 1;

// Configurable emulator options
unsigned int cpu_type = M68K_CPU_TYPE_68000;
unsigned int loop_cycles = 300;
struct emulator_config *cfg = NULL;
char keyboard_file[256] = "/dev/input/event0";

//unsigned char g_kick[524288];
//unsigned char g_ram[FASTSIZE + 1]; /* RAM */
int ovl;
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

  if (cfg->platform->shutdown) {
    cfg->platform->shutdown(cfg);
  }

  exit(0);
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
    else if (strcmp(argv[g], "--keyboard-file") == 0 || strcmp(argv[g], "--kbfile") == 0) {
      if (g + 1 >= argc) {
        printf("%s switch found, but no keyboard device path specified.\n", argv[g]);
      } else {
        g++;
        strcpy(keyboard_file, argv[g]);
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
  gpio_enable_200mhz();

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
    /*while (get_key_char(&c)) {
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
    }*/

    //gpio_handle_irq();
    GPIO_HANDLE_IRQ;
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

#define PLATFORM_CHECK_READ(a) \
  if (address >= cfg->custom_low && address < cfg->custom_high) { \
    unsigned int target = 0; \
    switch(cfg->platform->id) { \
      case PLATFORM_AMIGA: { \
        if (address >= PIGFX_RTG_BASE && address < PIGFX_RTG_BASE + PIGFX_RTG_SIZE) { \
          return rtg_read((address & 0x0FFFFFFF), a); \
        } \
        if (custom_read_amiga(cfg, address, &target, a) != -1) { \
          return target; \
        } \
        break; \
      } \
      default: \
        break; \
    } \
  } \
  if (ovl || (address >= cfg->mapped_low && address < cfg->mapped_high)) { \
    if (handle_mapped_read(cfg, address, &target, a) != -1) \
      return target; \
  }

unsigned int m68k_read_memory_8(unsigned int address) {
  PLATFORM_CHECK_READ(OP_TYPE_BYTE);

    address &=0xFFFFFF;
    return read8((uint32_t)address);
}

unsigned int m68k_read_memory_16(unsigned int address) {
  PLATFORM_CHECK_READ(OP_TYPE_WORD);

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


  address &=0xFFFFFF;
  return (unsigned int)read16((uint32_t)address);
}

unsigned int m68k_read_memory_32(unsigned int address) {
  PLATFORM_CHECK_READ(OP_TYPE_LONGWORD);

  address &=0xFFFFFF;
  uint16_t a = read16(address);
  uint16_t b = read16(address + 2);
  return (a << 16) | b;
}

#define PLATFORM_CHECK_WRITE(a) \
  if (address >= cfg->custom_low && address < cfg->custom_high) { \
    switch(cfg->platform->id) { \
      case PLATFORM_AMIGA: { \
        if (address >= PIGFX_RTG_BASE && address < PIGFX_RTG_BASE + PIGFX_RTG_SIZE) { \
          rtg_write((address & 0x0FFFFFFF), value, a); \
          return; \
        } \
        if (custom_write_amiga(cfg, address, value, a) != -1) { \
          return; \
        } \
        break; \
      } \
      default: \
        break; \
    } \
  } \
  if (address >= cfg->mapped_low && address < cfg->mapped_high) { \
    if (handle_mapped_write(cfg, address, value, a) != -1) \
      return; \
  }

void m68k_write_memory_8(unsigned int address, unsigned int value) {
  PLATFORM_CHECK_WRITE(OP_TYPE_BYTE);

  if (address == 0xbfe001) {
    if (ovl != (value & (1 << 0))) {
      ovl = (value & (1 << 0));
      printf("OVL:%x\n", ovl);
    }
  }

  address &=0xFFFFFF;
  write8((uint32_t)address, value);
  return;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
  PLATFORM_CHECK_WRITE(OP_TYPE_WORD);

  address &=0xFFFFFF;
  write16((uint32_t)address, value);
  return;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
  PLATFORM_CHECK_WRITE(OP_TYPE_LONGWORD);

  address &=0xFFFFFF;
  write16(address, value >> 16);
  write16(address + 2, value);
  return;
}
