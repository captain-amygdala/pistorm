// SPDX-License-Identifier: MIT

#include "m68k.h"
#include "emulator.h"
#include "platforms/platforms.h"
#include "input/input.h"
#include "m68kcpu.h"

#include "platforms/amiga/Gayle.h"
#include "platforms/amiga/amiga-registers.h"
#include "platforms/amiga/amiga-interrupts.h"
#include "platforms/amiga/rtg/rtg.h"
#include "platforms/amiga/hunk-reloc.h"
#include "platforms/amiga/piscsi/piscsi.h"
#include "platforms/amiga/piscsi/piscsi-enums.h"
#include "platforms/amiga/net/pi-net.h"
#include "platforms/amiga/net/pi-net-enums.h"
#include "platforms/amiga/pistorm-dev/pistorm-dev.h"
#include "platforms/amiga/pistorm-dev/pistorm-dev-enums.h"
#include "gpio/ps_protocol.h"

#include <assert.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KEY_POLL_INTERVAL_MSEC 5000

int kb_hook_enabled = 0;
int mouse_hook_enabled = 0;
int cpu_emulation_running = 1;
int swap_df0_with_dfx = 0;
int spoof_df0_id = 0;
int move_slow_to_chip = 0;

uint8_t mouse_dx = 0, mouse_dy = 0;
uint8_t mouse_buttons = 0;
uint8_t mouse_extra = 0;

extern uint8_t gayle_int;
extern uint8_t gayle_ide_enabled;
extern uint8_t gayle_emulation_enabled;
extern uint8_t gayle_a4k_int;
extern volatile unsigned int *gpio;
extern volatile uint16_t srdata;
extern uint8_t realtime_graphics_debug, emulator_exiting;
extern uint8_t rtg_on;
uint8_t realtime_disassembly, int2_enabled = 0;
uint32_t do_disasm = 0, old_level;
uint32_t last_irq = 8, last_last_irq = 8;

uint8_t ipl_enabled[8];

uint8_t end_signal = 0, load_new_config = 0;

char disasm_buf[4096];

#define KICKBASE 0xF80000
#define KICKSIZE 0x7FFFF

int mem_fd, mouse_fd = -1, keyboard_fd = -1;
int mem_fd_gpclk;
int irq;
int gayleirq;

#define MUSASHI_HAX

#ifdef MUSASHI_HAX
#include "m68kcpu.h"
extern m68ki_cpu_core m68ki_cpu;
extern int m68ki_initial_cycles;
extern int m68ki_remaining_cycles;

#define M68K_SET_IRQ(i) old_level = CPU_INT_LEVEL; \
	CPU_INT_LEVEL = (i << 8); \
	if(old_level != 0x0700 && CPU_INT_LEVEL == 0x0700) \
		m68ki_cpu.nmi_pending = TRUE;
#define M68K_END_TIMESLICE 	m68ki_initial_cycles = GET_CYCLES(); \
	SET_CYCLES(0);
#else
#define M68K_SET_IRQ m68k_set_irq
#define M68K_END_TIMESLICE m68k_end_timeslice()
#endif

#define NOP asm("nop"); asm("nop"); asm("nop"); asm("nop");

#define DEBUG_EMULATOR
#ifdef DEBUG_EMULATOR
#define DEBUG printf
#else
#define DEBUG(...)
#endif

// Configurable emulator options
unsigned int cpu_type = M68K_CPU_TYPE_68000;
unsigned int loop_cycles = 300, irq_status = 0;
struct emulator_config *cfg = NULL;
char keyboard_file[256] = "/dev/input/event1";

uint64_t trig_irq = 0, serv_irq = 0;
uint16_t irq_delay = 0;
unsigned int amiga_reset=0, amiga_reset_last=0;
unsigned int do_reset=0;

pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *ipl_task(void *args) {
  printf("IPL thread running\n");
  uint32_t value;

  pthread_mutex_lock(&lock);

  while (1) {
    value = *(gpio + 13);

    if (!(value & (1 << PIN_IPL_ZERO)) || ipl_enabled[amiga_emulated_ipl()]) {
      if (!irq) {
        M68K_END_TIMESLICE;
        irq = 1;
        pthread_cond_wait(&cond1, &lock);
        irq = 0;
      }
    }
    if(do_reset==0)
    {
      amiga_reset=(value & (1 << PIN_RESET));
      if(amiga_reset!=amiga_reset_last)
      {
        amiga_reset_last=amiga_reset;
        if(amiga_reset==0)
        {
          printf("Amiga Reset is down...\n");
          do_reset = 1;
          M68K_END_TIMESLICE;
        }
        else
        {
          printf("Amiga Reset is up...\n");
        }
      }
    }

    NOP NOP NOP NOP NOP NOP NOP NOP
    if (end_signal)
      goto end_ipl_thread;
  }
end_ipl_thread:
  pthread_mutex_unlock(&lock);

  return args;
}

static inline unsigned int inline_read_status_reg() {
  *(gpio + 7) = (REG_STATUS << PIN_A0);
  *(gpio + 7) = 1 << PIN_RD;
  *(gpio + 7) = 1 << PIN_RD;
  *(gpio + 7) = 1 << PIN_RD;
  *(gpio + 7) = 1 << PIN_RD;

  NOP NOP NOP NOP NOP NOP 
  unsigned int value = *(gpio + 13);

  *(gpio + 10) = 0xffffec;

  return (value >> 8) & 0xffff;
}

void *cpu_task() {
	m68ki_cpu_core *state = &m68ki_cpu;
	m68k_pulse_reset(state);

cpu_loop:
  if (mouse_hook_enabled) {
    get_mouse_status(&mouse_dx, &mouse_dy, &mouse_buttons, &mouse_extra);
  }

  if (realtime_disassembly && (do_disasm || cpu_emulation_running)) {
    m68k_disassemble(disasm_buf, m68k_get_reg(NULL, M68K_REG_PC), cpu_type);
    printf("REGA: 0:$%.8X 1:$%.8X 2:$%.8X 3:$%.8X 4:$%.8X 5:$%.8X 6:$%.8X 7:$%.8X\n", m68k_get_reg(NULL, M68K_REG_A0), m68k_get_reg(NULL, M68K_REG_A1), m68k_get_reg(NULL, M68K_REG_A2), m68k_get_reg(NULL, M68K_REG_A3), \
            m68k_get_reg(NULL, M68K_REG_A4), m68k_get_reg(NULL, M68K_REG_A5), m68k_get_reg(NULL, M68K_REG_A6), m68k_get_reg(NULL, M68K_REG_A7));
    printf("REGD: 0:$%.8X 1:$%.8X 2:$%.8X 3:$%.8X 4:$%.8X 5:$%.8X 6:$%.8X 7:$%.8X\n", m68k_get_reg(NULL, M68K_REG_D0), m68k_get_reg(NULL, M68K_REG_D1), m68k_get_reg(NULL, M68K_REG_D2), m68k_get_reg(NULL, M68K_REG_D3), \
            m68k_get_reg(NULL, M68K_REG_D4), m68k_get_reg(NULL, M68K_REG_D5), m68k_get_reg(NULL, M68K_REG_D6), m68k_get_reg(NULL, M68K_REG_D7));
    printf("%.8X (%.8X)]] %s\n", m68k_get_reg(NULL, M68K_REG_PC), (m68k_get_reg(NULL, M68K_REG_PC) & 0xFFFFFF), disasm_buf);
    if (do_disasm)
      do_disasm--;
	  m68k_execute(state, 1);
  }
  else {
    if (cpu_emulation_running) {
		if (irq)
			m68k_execute(state, 5);
		else
			m68k_execute(state, loop_cycles);
    }
  }

  if (irq) {
      last_irq = ((inline_read_status_reg() & 0xe000) >> 13);
      uint8_t amiga_irq = amiga_emulated_ipl();
      if (amiga_irq >= last_irq) {
          last_irq = amiga_irq;
      }
      if (last_irq != 0 && last_irq != last_last_irq) {
        last_last_irq = last_irq;
        M68K_SET_IRQ(last_irq);
      }
      pthread_cond_signal(&cond1);
      m68k_execute(state, 5);
  }
  if (!irq && last_last_irq != 0) {
    M68K_SET_IRQ(0);
    last_last_irq = 0;
  }

  if (do_reset) {
    cpu_pulse_reset();
    do_reset=0;
    usleep(1000000); // 1sec
    rtg_on=0;
//    while(amiga_reset==0);
//    printf("CPU emulation reset.\n");
  }

  if (mouse_hook_enabled && (mouse_extra != 0x00)) {
    // mouse wheel events have occurred; unlike l/m/r buttons, these are queued as keypresses, so add to end of buffer
    switch (mouse_extra) {
      case 0xff:
        // wheel up
        queue_keypress(0xfe, KEYPRESS_PRESS, PLATFORM_AMIGA);
        break;
      case 0x01:
        // wheel down
        queue_keypress(0xff, KEYPRESS_PRESS, PLATFORM_AMIGA);
        break;
    }

    // dampen the scroll wheel until next while loop iteration
    mouse_extra = 0x00;
  }

  if (load_new_config) {
    printf("[CPU] Loading new config file.\n");
    goto stop_cpu_emulation;
  }

  if (end_signal)
	  goto stop_cpu_emulation;

  goto cpu_loop;

stop_cpu_emulation:
  printf("[CPU] End of CPU thread\n");
  return (void *)NULL;
}

void *keyboard_task() {
  struct pollfd kbdpoll[1];
  int kpollrc;
  char c = 0, c_code = 0, c_type = 0;
  char grab_message[] = "[KBD] Grabbing keyboard from input layer\n",
       ungrab_message[] = "[KBD] Ungrabbing keyboard\n";

  printf("[KBD] Keyboard thread started\n");

  // because we permit the keyboard to be grabbed on startup, quickly check if we need to grab it
  if (kb_hook_enabled && cfg->keyboard_grab) {
    printf(grab_message);
    grab_device(keyboard_fd);
  }

  kbdpoll[0].fd = keyboard_fd;
  kbdpoll[0].events = POLLIN;

key_loop:
  kpollrc = poll(kbdpoll, 1, KEY_POLL_INTERVAL_MSEC);
  if ((kpollrc > 0) && (kbdpoll[0].revents & POLLHUP)) {
    // in the event that a keyboard is unplugged, keyboard_task will whiz up to 100% utilisation
    // this is undesired, so if the keyboard HUPs, end the thread without ending the emulation
    printf("[KBD] Keyboard node returned HUP (unplugged?)\n");
    goto key_end;
  }

  // if kpollrc > 0 then it contains number of events to pull, also check if POLLIN is set in revents
  if ((kpollrc <= 0) || !(kbdpoll[0].revents & POLLIN)) {
    if (cfg->platform->id == PLATFORM_AMIGA && last_irq != 2 && get_num_kb_queued()) {
      amiga_emulate_irq(PORTS);
    }
    goto key_loop;
  }

  while (get_key_char(&c, &c_code, &c_type)) {
    if (c && c == cfg->keyboard_toggle_key && !kb_hook_enabled) {
      kb_hook_enabled = 1;
      printf("[KBD] Keyboard hook enabled.\n");
      if (cfg->keyboard_grab) {
        grab_device(keyboard_fd);
        printf(grab_message);
      }
    } else if (kb_hook_enabled) {
      if (c == 0x1B && c_type) {
        kb_hook_enabled = 0;
        printf("[KBD] Keyboard hook disabled.\n");
        if (cfg->keyboard_grab) {
          release_device(keyboard_fd);
          printf(ungrab_message);
        }
      } else {
        if (queue_keypress(c_code, c_type, cfg->platform->id)) {
          if (cfg->platform->id == PLATFORM_AMIGA && last_irq != 2) {
            amiga_emulate_irq(PORTS);
          }
        }
      }
    }

    // pause pressed; trigger nmi (int level 7)
    if (c == 0x01 && c_type) {
      printf("[INT] Sending NMI\n");
      M68K_SET_IRQ(7);
    }

    if (!kb_hook_enabled && c_type) {
      if (c && c == cfg->mouse_toggle_key) {
        mouse_hook_enabled ^= 1;
        printf("Mouse hook %s.\n", mouse_hook_enabled ? "enabled" : "disabled");
        mouse_dx = mouse_dy = mouse_buttons = mouse_extra = 0;
      }
      if (c == 'r') {
        cpu_emulation_running ^= 1;
        printf("CPU emulation is now %s\n", cpu_emulation_running ? "running" : "stopped");
      }
      if (c == 'g') {
        realtime_graphics_debug ^= 1;
        printf("Real time graphics debug is now %s\n", realtime_graphics_debug ? "on" : "off");
      }
      if (c == 'R') {
        cpu_pulse_reset();
        //m68k_pulse_reset();
        printf("CPU emulation reset.\n");
      }
      if (c == 'q') {
        printf("Quitting and exiting emulator.\n");
	      end_signal = 1;
        goto key_end;
      }
      if (c == 'd') {
        realtime_disassembly ^= 1;
        do_disasm = 1;
        printf("Real time disassembly is now %s\n", realtime_disassembly ? "on" : "off");
      }
      if (c == 'D') {
        int r = get_mapped_item_by_address(cfg, 0x08000000);
        if (r != -1) {
          printf("Dumping first 16MB of mapped range %d.\n", r);
          FILE *dmp = fopen("./memdmp.bin", "wb+");
          fwrite(cfg->map_data[r], 16 * SIZE_MEGA, 1, dmp);
          fclose(dmp);
        }
      }
      if (c == 's' && realtime_disassembly) {
        do_disasm = 1;
      }
      if (c == 'S' && realtime_disassembly) {
        do_disasm = 128;
      }
    }
  }

  goto key_loop;

key_end:
  printf("[KBD] Keyboard thread ending\n");
  if (cfg->keyboard_grab) {
    printf(ungrab_message);
    release_device(keyboard_fd);
  }
  return (void*)NULL;
}

void stop_cpu_emulation(uint8_t disasm_cur) {
  M68K_END_TIMESLICE;
  if (disasm_cur) {
    m68k_disassemble(disasm_buf, m68k_get_reg(NULL, M68K_REG_PC), cpu_type);
    printf("REGA: 0:$%.8X 1:$%.8X 2:$%.8X 3:$%.8X 4:$%.8X 5:$%.8X 6:$%.8X 7:$%.8X\n", m68k_get_reg(NULL, M68K_REG_A0), m68k_get_reg(NULL, M68K_REG_A1), m68k_get_reg(NULL, M68K_REG_A2), m68k_get_reg(NULL, M68K_REG_A3), \
            m68k_get_reg(NULL, M68K_REG_A4), m68k_get_reg(NULL, M68K_REG_A5), m68k_get_reg(NULL, M68K_REG_A6), m68k_get_reg(NULL, M68K_REG_A7));
    printf("REGD: 0:$%.8X 1:$%.8X 2:$%.8X 3:$%.8X 4:$%.8X 5:$%.8X 6:$%.8X 7:$%.8X\n", m68k_get_reg(NULL, M68K_REG_D0), m68k_get_reg(NULL, M68K_REG_D1), m68k_get_reg(NULL, M68K_REG_D2), m68k_get_reg(NULL, M68K_REG_D3), \
            m68k_get_reg(NULL, M68K_REG_D4), m68k_get_reg(NULL, M68K_REG_D5), m68k_get_reg(NULL, M68K_REG_D6), m68k_get_reg(NULL, M68K_REG_D7));
    printf("%.8X (%.8X)]] %s\n", m68k_get_reg(NULL, M68K_REG_PC), (m68k_get_reg(NULL, M68K_REG_PC) & 0xFFFFFF), disasm_buf);
    realtime_disassembly = 1;
  }

  cpu_emulation_running = 0;
  do_disasm = 0;
}

unsigned int ovl;
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

  while (!emulator_exiting) {
    emulator_exiting = 1;
    usleep(0);
  }

  printf("IRQs triggered: %lld\n", trig_irq);
  printf("IRQs serviced: %lld\n", serv_irq);
  printf("Last serviced IRQ: %d\n", last_last_irq);

  exit(0);
}

int main(int argc, char *argv[]) {
  int g;

  ps_setup_protocol();

  //const struct sched_param priority = {99};

  // Some command line switch stuffles
  for (g = 1; g < argc; g++) {
    if (strcmp(argv[g], "--cpu_type") == 0 || strcmp(argv[g], "--cpu") == 0) {
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
        FILE *chk = fopen(argv[g], "rb");
        if (chk == NULL) {
          printf("Config file %s does not exist, please check that you've specified the path correctly.\n", argv[g]);
        } else {
          fclose(chk);
          load_new_config = 1;
          set_pistorm_devcfg_filename(argv[g]);
        }
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

switch_config:
  srand(clock());

  ps_reset_state_machine();
  ps_pulse_reset();
  usleep(1500);

  if (load_new_config != 0) {
    uint8_t config_action = load_new_config - 1;
    load_new_config = 0;
    if (cfg) {
      free_config_file(cfg);
      free(cfg);
      cfg = NULL;
    }

    switch(config_action) {
      case PICFG_LOAD:
      case PICFG_RELOAD:
        cfg = load_config_file(get_pistorm_devcfg_filename());
        break;
      case PICFG_DEFAULT:
        cfg = load_config_file("default.cfg");
        break;
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
    mouse_fd = open(cfg->mouse_file, O_RDWR | O_NONBLOCK);
    if (mouse_fd == -1) {
      printf("Failed to open %s, can't enable mouse hook.\n", cfg->mouse_file);
      cfg->mouse_enabled = 0;
    } else {
      /**
       * *-*-*-* magic numbers! *-*-*-*
       * great, so waaaay back in the history of the pc, the ps/2 protocol set the standard for mice
       * and in the process, the mouse sample rate was defined as a way of putting mice into vendor-specific modes.
       * as the ancient gpm command explains, almost everything except incredibly old mice talk the IntelliMouse
       * protocol, which reports four bytes. by default, every mouse starts in 3-byte mode (don't report wheel or
       * additional buttons) until imps2 magic is sent. so, command $f3 is "set sample rate", followed by a byte.
       */
      uint8_t mouse_init[] = { 0xf4, 0xf3, 0x64 }; // enable, then set sample rate 100
      uint8_t imps2_init[] = { 0xf3, 0xc8, 0xf3, 0x64, 0xf3, 0x50 }; // magic sequence; set sample 200, 100, 80
      if (write(mouse_fd, mouse_init, sizeof(mouse_init)) != -1) {
        if (write(mouse_fd, imps2_init, sizeof(imps2_init)) == -1)
          printf("[MOUSE] Couldn't enable scroll wheel events; is this mouse from the 1980s?\n");
      } else
        printf("[MOUSE] Mouse didn't respond to normal PS/2 init; have you plugged a brick in by mistake?\n");
    }
  }

  if (cfg->keyboard_file)
    keyboard_fd = open(cfg->keyboard_file, O_RDONLY | O_NONBLOCK);
  else
    keyboard_fd = open(keyboard_file, O_RDONLY | O_NONBLOCK);

  if (keyboard_fd == -1) {
    printf("Failed to open keyboard event source.\n");
  }

  if (cfg->mouse_autoconnect)
    mouse_hook_enabled = 1;

  if (cfg->keyboard_autoconnect)
    kb_hook_enabled = 1;

  InitGayle();

  signal(SIGINT, sigint_handler);

  ps_reset_state_machine();
  ps_pulse_reset();
  usleep(1500);

  m68k_init();
  printf("Setting CPU type to %d.\n", cpu_type);
	m68k_set_cpu_type(&m68ki_cpu, cpu_type);
  cpu_pulse_reset();

  pthread_t ipl_tid = 0, cpu_tid, kbd_tid;
  int err;
  if (ipl_tid == 0) {
    err = pthread_create(&ipl_tid, NULL, &ipl_task, NULL);
    if (err != 0)
      printf("[ERROR] Cannot create IPL thread: [%s]", strerror(err));
    else {
      pthread_setname_np(ipl_tid, "pistorm: ipl");
      printf("IPL thread created successfully\n");
    }
  }

  // create keyboard task
  err = pthread_create(&kbd_tid, NULL, &keyboard_task, NULL);
  if (err != 0)
    printf("[ERROR] Cannot create keyboard thread: [%s]", strerror(err));
  else {
    pthread_setname_np(kbd_tid, "pistorm: kbd");
    printf("[MAIN] Keyboard thread created successfully\n");
  }

  // create cpu task
  err = pthread_create(&cpu_tid, NULL, &cpu_task, NULL);
  if (err != 0)
    printf("[ERROR] Cannot create CPU thread: [%s]", strerror(err));
  else {
    pthread_setname_np(cpu_tid, "pistorm: cpu");
    printf("[MAIN] CPU thread created successfully\n");
  }

  // wait for cpu task to end before closing up and finishing
  pthread_join(cpu_tid, NULL);

  while (!emulator_exiting) {
    emulator_exiting = 1;
    usleep(0);
  }

  if (load_new_config == 0)
    printf("[MAIN] All threads appear to have concluded; ending process\n");

  if (mouse_fd != -1)
    close(mouse_fd);
  if (mem_fd)
    close(mem_fd);

  if (load_new_config != 0)
    goto switch_config;

  if (cfg->platform->shutdown) {
    cfg->platform->shutdown(cfg);
  }

  return 0;
}

void cpu_pulse_reset(void) {
	m68ki_cpu_core *state = &m68ki_cpu;
  ps_pulse_reset();

  ovl = 1;
  for (int i = 0; i < 8; i++) {
    ipl_enabled[i] = 0;
  }

  if (cfg->platform->handle_reset)
    cfg->platform->handle_reset(cfg);

	m68k_pulse_reset(state);
}

unsigned int cpu_irq_ack(int level) {
  //printf("cpu irq ack\n");
  return 24 + level;
}

static unsigned int target = 0;
static uint32_t platform_res, rres;

uint8_t cdtv_dmac_reg_idx_read();
void cdtv_dmac_reg_idx_write(uint8_t value);
uint32_t cdtv_dmac_read(uint32_t address, uint8_t type);
void cdtv_dmac_write(uint32_t address, uint32_t value, uint8_t type);

unsigned int garbage = 0;

static inline void inline_write_16(unsigned int address, unsigned int data) {
  *(gpio + 0) = GPFSEL0_OUTPUT;
  *(gpio + 1) = GPFSEL1_OUTPUT;
  *(gpio + 2) = GPFSEL2_OUTPUT;

  *(gpio + 7) = ((data & 0xffff) << 8) | (REG_DATA << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((0x0000 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 0) = GPFSEL0_INPUT;
  *(gpio + 1) = GPFSEL1_INPUT;
  *(gpio + 2) = GPFSEL2_INPUT;

  while (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS)) {}
  NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP
}

static inline void inline_write_8(unsigned int address, unsigned int data) {
  if ((address & 1) == 0)
    data = data + (data << 8);  // EVEN, A0=0,UDS
  else
    data = data & 0xff;  // ODD , A0=1,LDS

  *(gpio + 0) = GPFSEL0_OUTPUT;
  *(gpio + 1) = GPFSEL1_OUTPUT;
  *(gpio + 2) = GPFSEL2_OUTPUT;

  *(gpio + 7) = ((data & 0xffff) << 8) | (REG_DATA << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((0x0100 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 0) = GPFSEL0_INPUT;
  *(gpio + 1) = GPFSEL1_INPUT;
  *(gpio + 2) = GPFSEL2_INPUT;

  while (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS)) {}
  NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP NOP
}

static inline void inline_write_32(unsigned int address, unsigned int value) {
  inline_write_16(address, value >> 16);
  inline_write_16(address + 2, value);
}

static inline unsigned int inline_read_16(unsigned int address) {
  *(gpio + 0) = GPFSEL0_OUTPUT;
  *(gpio + 1) = GPFSEL1_OUTPUT;
  *(gpio + 2) = GPFSEL2_OUTPUT;

  *(gpio + 7) = ((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((0x0200 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 0) = GPFSEL0_INPUT;
  *(gpio + 1) = GPFSEL1_INPUT;
  *(gpio + 2) = GPFSEL2_INPUT;

  *(gpio + 7) = (REG_DATA << PIN_A0);
  *(gpio + 7) = 1 << PIN_RD;

  while (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS)) {}
  unsigned int value = *(gpio + 13);

  *(gpio + 10) = 0xffffec;

  return (value >> 8) & 0xffff;
}

static inline unsigned int inline_read_8(unsigned int address) {
  *(gpio + 0) = GPFSEL0_OUTPUT;
  *(gpio + 1) = GPFSEL1_OUTPUT;
  *(gpio + 2) = GPFSEL2_OUTPUT;

  *(gpio + 7) = ((address & 0xffff) << 8) | (REG_ADDR_LO << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 7) = ((0x0300 | (address >> 16)) << 8) | (REG_ADDR_HI << PIN_A0);
  *(gpio + 7) = 1 << PIN_WR;
  *(gpio + 10) = 1 << PIN_WR;
  *(gpio + 10) = 0xffffec;

  *(gpio + 0) = GPFSEL0_INPUT;
  *(gpio + 1) = GPFSEL1_INPUT;
  *(gpio + 2) = GPFSEL2_INPUT;

  *(gpio + 7) = (REG_DATA << PIN_A0);
  *(gpio + 7) = 1 << PIN_RD;

  while (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS)) {}
  unsigned int value = *(gpio + 13);

  *(gpio + 10) = 0xffffec;

  value = (value >> 8) & 0xffff;

  if ((address & 1) == 0)
    return (value >> 8) & 0xff;  // EVEN, A0=0,UDS
  else
    return value & 0xff;  // ODD , A0=1,LDS
}

static inline unsigned int inline_read_32(unsigned int address) {
  unsigned int a = inline_read_16(address);
  unsigned int b = inline_read_16(address + 2);
  return (a << 16) | b;
}

static inline uint32_t ps_read(uint8_t type, uint32_t addr) {
  switch (type) {
    case OP_TYPE_BYTE:
      return inline_read_8(addr);
    case OP_TYPE_WORD:
      return inline_read_16(addr);
    case OP_TYPE_LONGWORD:
      return inline_read_32(addr);
  }
  // This shouldn't actually happen.
  return 0;
}

static inline void ps_write(uint8_t type, uint32_t addr, uint32_t val) {
  switch (type) {
    case OP_TYPE_BYTE:
      inline_write_8(addr, val);
      return;
    case OP_TYPE_WORD:
      inline_write_16(addr, val);
      return;
    case OP_TYPE_LONGWORD:
      inline_write_32(addr, val);
      return;
  }
  // This shouldn't actually happen.
  return;
}

static inline int32_t platform_read_check(uint8_t type, uint32_t addr, uint32_t *res) {
  switch (cfg->platform->id) {
    case PLATFORM_AMIGA:
      switch (addr) {
        case INTREQR:
          return amiga_handle_intrqr_read(res);
          break;
        case CIAAPRA:
          if (mouse_hook_enabled && (mouse_buttons & 0x01)) {
            rres = (uint32_t)ps_read(type, addr);
            *res = (rres ^ 0x40);
            return 1;
          }
          if (swap_df0_with_dfx && spoof_df0_id) {
            // DF0 doesn't emit a drive type ID on RDY pin
            // If swapping DF0 with DF1-3 we need to provide this ID so that DF0 continues to function.
            rres = (uint32_t)ps_read(type, addr);
            *res = (rres & 0xDF); // Spoof drive id for swapped DF0 by setting RDY low
            return 1;
          }
          return 0;
          break;
        case CIAAICR:
          if (kb_hook_enabled && get_num_kb_queued() && amiga_emulating_irq(PORTS)) {
            *res = 0x88;
            return 1;
          }
          return 0;
          break;
        case CIAADAT:
          if (kb_hook_enabled && amiga_emulating_irq(PORTS)) {
            uint8_t c = 0, t = 0;
            pop_queued_key(&c, &t);
            t ^= 0x01;
            rres = ((c << 1) | t) ^ 0xFF;
            *res = rres;
            return 1;
          }
          return 0;
          break;
        case JOY0DAT:
          if (mouse_hook_enabled) {
            unsigned short result = (mouse_dy << 8) | (mouse_dx);
            *res = (unsigned int)result;
            return 1;
          }
          return 0;
          break;
        case INTENAR: {
          // This code is kind of strange and should probably be reworked/revoked.
          uint8_t enable = 1;
          rres = (uint16_t)ps_read(type, addr);
          uint16_t val = rres;
          if (val & 0x0007) {
            ipl_enabled[1] = enable;
          }
          if (val & 0x0008) {
            ipl_enabled[2] = enable;
          }
          if (val & 0x0070) {
            ipl_enabled[3] = enable;
          }
          if (val & 0x0780) {
            ipl_enabled[4] = enable;
          }
          if (val & 0x1800) {
            ipl_enabled[5] = enable;
          }
          if (val & 0x2000) {
            ipl_enabled[6] = enable;
          }
          if (val & 0x4000) {
            ipl_enabled[7] = enable;
          }
          //printf("Interrupts enabled: M:%d 0-6:%d%d%d%d%d%d\n", ipl_enabled[7], ipl_enabled[6], ipl_enabled[5], ipl_enabled[4], ipl_enabled[3], ipl_enabled[2], ipl_enabled[1]);
          *res = rres;
          return 1;
          break;
        }
        case POTGOR:
          if (mouse_hook_enabled) {
            unsigned short result = (unsigned short)ps_read(type, addr);
            // bit 1 rmb, bit 2 mmb
            if (mouse_buttons & 0x06) {
              *res = (unsigned int)((result ^ ((mouse_buttons & 0x02) << 9))   // move rmb to bit 10
                                  & (result ^ ((mouse_buttons & 0x04) << 6))); // move mmb to bit 8
              return 1;
            }
            *res = (unsigned int)(result & 0xfffd);
            return 1;
          }
          return 0;
          break;
        case CIABPRB:
          if (swap_df0_with_dfx) {
            uint32_t result = (uint32_t)ps_read(type, addr);
            // SEL0 = 0x80, SEL1 = 0x10, SEL2 = 0x20, SEL3 = 0x40
            if (((result >> SEL0_BITNUM) & 1) != ((result >> (SEL0_BITNUM + swap_df0_with_dfx)) & 1)) { // If the value for SEL0/SELx differ
              result ^= ((1 << SEL0_BITNUM) | (1 << (SEL0_BITNUM + swap_df0_with_dfx)));                // Invert both bits to swap them around
            }
            *res = result;
            return 1;
          }
          return 0;
          break;
        default:
          break;
      }

      if (move_slow_to_chip && addr >= 0x080000 && addr <= 0x0FFFFF) {
        // A500 JP2 connects Agnus' A19 input to A23 instead of A19 by default, and decodes trapdoor memory at 0xC00000 instead of 0x080000.
        // We can move the trapdoor to chipram simply by rewriting the address.
        addr += 0xB80000;
        *res = ps_read(type, addr);
        return 1;
      }

      if (move_slow_to_chip && addr >= 0xC00000 && addr <= 0xC7FFFF) {
        // Block accesses through to trapdoor at slow ram address, otherwise it will be detected at 0x080000 and 0xC00000.
        *res = 0;
        return 1;
      }

      if (addr >= cfg->custom_low && addr < cfg->custom_high) {
        if (addr >= PISCSI_OFFSET && addr < PISCSI_UPPER) {
          *res = handle_piscsi_read(addr, type);
          return 1;
        }
        if (addr >= PINET_OFFSET && addr < PINET_UPPER) {
          *res = handle_pinet_read(addr, type);
          return 1;
        }
        if (addr >= PIGFX_RTG_BASE && addr < PIGFX_UPPER) {
          *res = rtg_read((addr & 0x0FFFFFFF), type);
          return 1;
        }
        if (custom_read_amiga(cfg, addr, &target, type) != -1) {
          *res = target;
          return 1;
        }
      }

      break;
    default:
      break;
  }

  if (ovl || (addr >= cfg->mapped_low && addr < cfg->mapped_high)) {
    if (handle_mapped_read(cfg, addr, &target, type) != -1) {
      *res = target;
      return 1;
    }
  }

  return 0;
}

unsigned int m68k_read_memory_8(unsigned int address) {
  if (platform_read_check(OP_TYPE_BYTE, address, &platform_res)) {
    return platform_res;
  }

  if (address & 0xFF000000)
    return 0;

  return (unsigned int)inline_read_8((uint32_t)address);
}

unsigned int m68k_read_memory_16(unsigned int address) {
  if (platform_read_check(OP_TYPE_WORD, address, &platform_res)) {
    return platform_res;
  }

  if (address & 0xFF000000)
    return 0;

  if (address & 0x01) {
    return ((inline_read_8(address) << 8) | inline_read_8(address + 1));
  }
  return (unsigned int)inline_read_16((uint32_t)address);
}

unsigned int m68k_read_memory_32(unsigned int address) {
  if (platform_read_check(OP_TYPE_LONGWORD, address, &platform_res)) {
    return platform_res;
  }

  if (address & 0xFF000000)
    return 0;

  if (address & 0x01) {
    uint32_t c = inline_read_8(address);
    c |= (be16toh(inline_read_16(address+1)) << 8);
    c |= (inline_read_8(address + 3) << 24);
    return htobe32(c);
  }
  uint16_t a = inline_read_16(address);
  uint16_t b = inline_read_16(address + 2);
  return (a << 16) | b;
}

static inline int32_t platform_write_check(uint8_t type, uint32_t addr, uint32_t val) {
  switch (cfg->platform->id) {
    case PLATFORM_MAC:
      switch (addr) {
        case 0xEFFFFE: // VIA1?
          if (val & 0x10 && !ovl) {
              ovl = 1;
              printf("[MAC] OVL on.\n");
              handle_ovl_mappings_mac68k(cfg);
          } else if (ovl) {
            ovl = 0;
            printf("[MAC] OVL off.\n");
            handle_ovl_mappings_mac68k(cfg);
          }
          break;
      }
      break;
    case PLATFORM_AMIGA:
      switch (addr) {
        case INTREQ:
          return amiga_handle_intrq_write(val);
          break;
        case CIAAPRA:
          if (ovl != (val & (1 << 0))) {
            ovl = (val & (1 << 0));
            printf("OVL:%x\n", ovl);
          }
          return 0;
          break;
        case SERDAT: {
          char *serdat = (char *)&val;
          // SERDAT word. see amiga dev docs appendix a; upper byte is control codes, and bit 0 is always 1.
          // ignore this upper byte as it's not viewable data, only display lower byte.
          printf("%c", serdat[0]);
          return 0;
          break;
        }
        case INTENA: {
          // This code is kind of strange and should probably be reworked/revoked.
          uint8_t enable = 1;
          if (!(val & 0x8000))
            enable = 0;
          if (val & 0x0007) {
            ipl_enabled[1] = enable;
          }
          if (val & 0x0008) {
            ipl_enabled[2] = enable;
          }
          if (val & 0x0070) {
            ipl_enabled[3] = 1;
          }
          if (val & 0x0780) {
            ipl_enabled[4] = enable;
          }
          if (val & 0x1800) {
            ipl_enabled[5] = enable;
          }
          if (val & 0x2000) {
            ipl_enabled[6] = enable;
          }
          if (val & 0x4000 && enable) {
            ipl_enabled[7] = 1;
          }
          //printf("Interrupts enabled: M:%d 0-6:%d%d%d%d%d%d\n", ipl_enabled[7], ipl_enabled[6], ipl_enabled[5], ipl_enabled[4], ipl_enabled[3], ipl_enabled[2], ipl_enabled[1]);
          return 0;
          break;
        }
        case CIABPRB:
          if (swap_df0_with_dfx) {
            if ((val & ((1 << (SEL0_BITNUM + swap_df0_with_dfx)) | 0x80)) == 0x80) {
              // If drive selected but motor off, Amiga is reading drive ID.
              spoof_df0_id = 1;
            } else {
              spoof_df0_id = 0;
            }

            if (((val >> SEL0_BITNUM) & 1) != ((val >> (SEL0_BITNUM + swap_df0_with_dfx)) & 1)) { // If the value for SEL0/SELx differ
              val ^= ((1 << SEL0_BITNUM) | (1 << (SEL0_BITNUM + swap_df0_with_dfx)));             // Invert both bits to swap them around
            }
            ps_write(type,addr,val);
            return 1;
          }
          return 0;
          break;
        default:
          break;
      }

      if (move_slow_to_chip && addr >= 0x080000 && addr <= 0x0FFFFF) {
        // A500 JP2 connects Agnus' A19 input to A23 instead of A19 by default, and decodes trapdoor memory at 0xC00000 instead of 0x080000.
        // We can move the trapdoor to chipram simply by rewriting the address.
        addr += 0xB80000;
        ps_write(type,addr,val);
        return 1;
      }

      if (move_slow_to_chip && addr >= 0xC00000 && addr <= 0xC7FFFF) {
        // Block accesses through to trapdoor at slow ram address, otherwise it will be detected at 0x080000 and 0xC00000.
        return 1;
      }

      if (addr >= cfg->custom_low && addr < cfg->custom_high) {
        if (addr >= PISCSI_OFFSET && addr < PISCSI_UPPER) {
          handle_piscsi_write(addr, val, type);
          return 1;
        }
        if (addr >= PINET_OFFSET && addr < PINET_UPPER) {
          handle_pinet_write(addr, val, type);
          return 1;
        }
        if (addr >= PIGFX_RTG_BASE && addr < PIGFX_UPPER) {
          rtg_write((addr & 0x0FFFFFFF), val, type);
          return 1;
        }
        if (custom_write_amiga(cfg, addr, val, type) != -1) {
          return 1;
        }
      }

      break;
    default:
      break;
  }

  if (ovl || (addr >= cfg->mapped_low && addr < cfg->mapped_high)) {
    if (handle_mapped_write(cfg, addr, val, type) != -1) {
      return 1;
    }
  }

  return 0;
}

void m68k_write_memory_8(unsigned int address, unsigned int value) {
  if (platform_write_check(OP_TYPE_BYTE, address, value))
    return;

  if (address & 0xFF000000)
    return;

  inline_write_8((uint32_t)address, value);
  return;
}

void m68k_write_memory_16(unsigned int address, unsigned int value) {
  if (platform_write_check(OP_TYPE_WORD, address, value))
    return;

  if (address & 0xFF000000)
    return;

  if (address & 0x01) {
    inline_write_8(value & 0xFF, address);
    inline_write_8((value >> 8) & 0xFF, address + 1);
    return;
  }

  inline_write_16((uint32_t)address, value);
  return;
}

void m68k_write_memory_32(unsigned int address, unsigned int value) {
  if (platform_write_check(OP_TYPE_LONGWORD, address, value))
    return;

  if (address & 0xFF000000)
    return;

  if (address & 0x01) {
    inline_write_8(value & 0xFF, address);
    inline_write_16(htobe16(((value >> 8) & 0xFFFF)), address + 1);
    inline_write_8((value >> 24), address + 3);
    return;
  }

  inline_write_16(address, value >> 16);
  inline_write_16(address + 2, value);
  return;
}
