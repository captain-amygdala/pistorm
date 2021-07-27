// SPDX-License-Identifier: MIT

#include <assert.h>
#include <dirent.h>
#include <endian.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "emulator.h"
#include "gpio/ps_protocol.h"

#define SIZE_KILO 1024
#define SIZE_MEGA (1024 * 1024)
#define SIZE_GIGA (1024 * 1024 * 1024)

#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)

uint8_t garbege_datas[2 * SIZE_MEGA];
extern volatile unsigned int *gpio;

struct timespec f2;

uint8_t gayle_int;
uint32_t mem_fd;
uint32_t errors = 0;
uint8_t loop_tests = 0, total_errors = 0;

void sigint_handler(int sig_num) {
  printf("Received sigint %d, exiting.\n", sig_num);
  printf("Total number of transaction errors occured: %d\n", total_errors);
  if (mem_fd)
    close(mem_fd);

  exit(0);
}

void ps_reinit() {
    ps_reset_state_machine();
    ps_pulse_reset();

    usleep(1500);

    write8(0xbfe201, 0x0101);       //CIA OVL
	write8(0xbfe001, 0x0000);       //CIA OVL LOW
}

unsigned int dump_read_8(unsigned int address) {
    uint32_t bwait = 0;

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


    while (bwait < 10000 && (*(gpio + 13) & (1 << PIN_TXN_IN_PROGRESS))) {
        bwait++;
    }

    unsigned int value = *(gpio + 13);

    *(gpio + 10) = 0xffffec;

    value = (value >> 8) & 0xffff;

    if (bwait == 10000) {
        ps_reinit();
    }

    if ((address & 1) == 0)
        return (value >> 8) & 0xff;  // EVEN, A0=0,UDS
    else
        return value & 0xff;  // ODD , A0=1,LDS
}

int main(int argc, char *argv[]) {
    uint32_t test_size = 512 * SIZE_KILO, cur_loop = 0;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &f2);
    srand((unsigned int)f2.tv_nsec);

    signal(SIGINT, sigint_handler);

    ps_setup_protocol();
    ps_reset_state_machine();
    ps_pulse_reset();

    usleep(1500);

    write8(0xbfe201, 0x0101);       //CIA OVL
	write8(0xbfe001, 0x0000);       //CIA OVL LOW

    if (argc > 1) 
	{
        printf("No arguments supported\n");
		
    }

test_loop:;
    printf("Reading bus patterns for 2 mins. press ctrl-c to quit looping early\n");
	//clear the data bus
	write32(0x01,0x00000000);
    for (uint32_t j = 1; j <= 240; ++j)
    {
      uint32_t c = read16(j);

      printf("READ16: read data: 0x%.4X (hex) "  
	         PRINTF_BINARY_PATTERN_INT16 " (binary)\n",
             c, PRINTF_BYTE_TO_BINARY_INT16(c));
       
      usleep(500000);
    }


    return 0;
}

void m68k_set_irq(unsigned int level) {
}
