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
#include "main.h"
#include "gpio/gpio.h"
#include "platforms/amiga/gayle-ide/ide.h"

uint8_t garbege_datas[2 * 1024 * 1024];

struct timespec f2;

uint8_t gayle_int;
uint32_t mem_fd;
uint32_t errors = 0;

void sigint_handler(int sig_num) {
  //if (sig_num) { }
  //cpu_emulation_running = 0;

  //return;
  printf("Received sigint %d, exiting.\n", sig_num);
  if (mem_fd)
    close(mem_fd);

  exit(0);
}

int main() {
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &f2);
    srand((unsigned int)f2.tv_nsec);

    signal(SIGINT, sigint_handler);
    setup_io();

    printf("Enable 200MHz GPCLK0 on GPIO4\n");
    gpio_enable_200mhz();

    write_reg(0x01);
    usleep(100);
    usleep(1500);
    write_reg(0x00);
    usleep(100);

    usleep(1500);

    write_reg(0x00);
    // printf("Status Reg%x\n",read_reg());
    usleep(100000);
    write_reg(0x02);
    usleep(1500);

    write8(0xbfe201, 0x0101);       //CIA OVL
	write8(0xbfe001, 0x0000);       //CIA OVL LOW

    printf("Writing garbege datas.\n");
    for (uint32_t i = 0; i < 512 * 1024; i++) {
        garbege_datas[i] = (uint8_t)(rand() % 0xFF);
        write8(i, (uint32_t)garbege_datas[i]);
    }
    printf("Reading back garbege datas, read8()...\n");
    for (uint32_t i = 0; i < 512 * 1024; i++) {
        uint32_t c = read8(i);
        if (c != garbege_datas[i]) {
            if (errors < 512)
                printf("READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbege_datas[i]);
            errors++;
        }
    }
    printf("read8 errors total: %d.\n", errors);
    errors = 0;
    sleep (1);
    printf("Reading back garbege datas, read16(), even addresses...\n");
    for (uint32_t i = 0; i < (512 * 1024) - 2; i += 2) {
        uint32_t c = be16toh(read16(i));
        if (c != *((uint16_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read16 even errors total: %d.\n", errors);
    errors = 0;
    sleep (1);
    for (int x = 0; x < 20; x++) {
        printf("Reading back garbege datas, read16(), odd addresses...\n");
        for (uint32_t i = 1; i < (512 * 1024) - 2; i += 2) {
            uint32_t c = be16toh((read8(i) << 8) | read8(i + 1));
            if (c != *((uint16_t *)&garbege_datas[i])) {
                if (errors < 512)
                    printf("READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbege_datas[i]));
                errors++;
            }
        }
        printf("read16 odd loop %d errors total: %d.\n", x+1, errors);
        errors = 0;
    }
    sleep (1);
    printf("Reading back garbege datas, read32(), even addresses...\n");
    for (uint32_t i = 0; i < (512 * 1024) - 4; i += 2) {
        uint32_t c = be32toh(read32(i));
        if (c != *((uint32_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read32 even errors total: %d.\n", errors);
    errors = 0;
    sleep (1);
    for (int x = 0; x < 20; x++) {
        printf("Reading back garbege datas, read32(), odd addresses...\n");
        for (uint32_t i = 1; i < (512 * 1024) - 4; i += 2) {
            uint32_t c = be32toh(read32(i));
            c = (c >> 8) | (read8(i + 3) << 24);
            if (c != *((uint32_t *)&garbege_datas[i])) {
                if (errors < 512)
                    printf("READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbege_datas[i]));
                errors++;
            }
        }
        printf("read32 odd loop %d errors total: %d.\n", x+1, errors);
        errors = 0;
    }
    sleep (1);

    return 0;
}

void m68k_set_irq(unsigned int level) {
}

struct ide_controller *get_ide(int index) {
    return NULL;
}
