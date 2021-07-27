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
    printf("Writing and reading bus patterns.\n");
    for (uint32_t j = 0; j <= 23; ++j)
    {
      uint32_t ja = 1 << j;
      printf("Address testing. Testing addr: $%.6X...\n",ja);
      for (uint32_t i = 0; i < test_size; i++) 
	  {
		  //write 512k writes on each address pin (A1-23)
          write16(ja, 0xFFFF);
      }
    }
	printf("Testing Data bus output pins individually...\n");
	int j=0;
    for (;j<16;++j)
    {
      printf("write16: data = %.4X\n", 1 << j);
      for (uint32_t i = 0; i < test_size; i++) 
	  {
          while(garbege_datas[i] == 0x00)
		  {
              garbege_datas[i] = (uint8_t)(rand() % 0xFF);
                  }
			  write32(i, (uint16_t)(1 << j));
		  
      }
    }
	printf("And back down... \n");
	for (j=15;j>=0;--j)
    {
      printf("write16: data = %.4X\n", 1 << j);
      for (uint32_t i = 0; i < test_size; i++) 
	  {
          while(garbege_datas[i] == 0x00)
		  {
              garbege_datas[i] = (uint8_t)(rand() % 0xFF);
                  }
			  write32(i, (uint16_t)(1 << j));
		  
      }
    }
	
	printf("The following test only works on non-A variant flip-flops (373 or 374's not 373A or 374A\n");
	for (j=0;j<16;++j)
    {
	  uint32_t tmp = 1 << j;
      printf("write and read back data bus pins (read16/write16...\n");
      write16(j+1, tmp);
	  sleep(1);
      uint32_t c = read16(j+1);
      if (c != tmp) 
	  {
          printf("READ16: write/read data mismatch: read %.2X should be %.2X.\n",  c, tmp);
          errors++;
      }

    }
	for (j=15;j>=0;--j)
    {
	  uint32_t tmp = 1 << j;
      printf("write and read back data bus pins (read16/write16...\n");
      write16(j+1, tmp);
	  sleep(1);
      uint32_t c = read16(j+1);
      if (c != tmp) 
	  {
          printf("READ16: write/read data mismatch: read %.2X should be %.2X.\n",  c, tmp);
          errors++;
      }

    }
	
    printf("read16 errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;
    sleep (1);

    if (loop_tests) {
        printf ("Loop %d done. Begin loop %d.\n", cur_loop + 1, cur_loop + 2);
        printf ("Current total errors: %d.\n", total_errors);
        goto test_loop;
    }

    return 0;
}

void m68k_set_irq(unsigned int level) {
}
