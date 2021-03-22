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
#include "platforms/amiga/gayle-ide/ide.h"

#define SIZE_KILO 1024
#define SIZE_MEGA (1024 * 1024)
#define SIZE_GIGA (1024 * 1024 * 1024)

uint8_t garbege_datas[2 * SIZE_MEGA];

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

    if (argc > 1) {
        test_size = atoi(argv[1]) * SIZE_KILO;
        if (test_size == 0 || test_size > 2 * SIZE_MEGA) {
            test_size = 512 * SIZE_KILO;
        }
        printf("Testing %d KB of memory.\n", test_size / SIZE_KILO);
        if (argc > 2) {
            if (strcmp(argv[2], "l") == 0) {
                printf("Looping tests.\n");
                loop_tests = 1;
            }
        }
    }

test_loop:;
    printf("Writing garbege datas.\n");
    for (uint32_t i = 0; i < test_size; i++) {
        while(garbege_datas[i] == 0x00)
            garbege_datas[i] = (uint8_t)(rand() % 0xFF);
        write8(i, (uint32_t)garbege_datas[i]);
    }

    printf("Reading back garbege datas, read8()...\n");
    for (uint32_t i = 0; i < test_size; i++) {
        uint32_t c = read8(i);
        if (c != garbege_datas[i]) {
            if (errors < 512)
                printf("READ8: Garbege data mismatch at $%.6X: %.2X should be %.2X.\n", i, c, garbege_datas[i]);
            errors++;
        }
    }
    printf("read8 errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;
    sleep (1);

    printf("Reading back garbege datas, read16(), even addresses...\n");
    for (uint32_t i = 0; i < (test_size) - 2; i += 2) {
        uint32_t c = be16toh(read16(i));
        if (c != *((uint16_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ16_EVEN: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read16 even errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;
    sleep (1);

    printf("Reading back garbege datas, read16(), odd addresses...\n");
    for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
        uint32_t c = be16toh((read8(i) << 8) | read8(i + 1));
        if (c != *((uint16_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read16 odd errors total: %d.\n", errors);
    errors = 0;
    sleep (1);

    printf("Reading back garbege datas, read32(), even addresses...\n");
    for (uint32_t i = 0; i < (test_size) - 4; i += 2) {
        uint32_t c = be32toh(read32(i));
        if (c != *((uint32_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ32_EVEN: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read32 even errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;
    sleep (1);

    printf("Reading back garbege datas, read32(), odd addresses...\n");
    for (uint32_t i = 1; i < (test_size) - 4; i += 2) {
        uint32_t c = read8(i);
        c |= (be16toh(read16(i + 1)) << 8);
        c |= (read8(i + 3) << 24);
        if (c != *((uint32_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read32 odd errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;
    sleep (1);

    printf("Clearing %d KB of Chip again\n", test_size / SIZE_KILO);
    for (uint32_t i = 0; i < test_size; i++) {
        write8(i, (uint32_t)0x0);
    }

    printf("[WORD] Writing garbege datas to Chip, unaligned...\n");
    for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
        uint16_t v = *((uint16_t *)&garbege_datas[i]);
        write8(i, (v & 0x00FF));
        write8(i + 1, (v >> 8));
    }

    sleep (1);
    printf("Reading back garbege datas, read16(), odd addresses...\n");
    for (uint32_t i = 1; i < (test_size) - 2; i += 2) {
        uint32_t c = be16toh((read8(i) << 8) | read8(i + 1));
        if (c != *((uint16_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ16_ODD: Garbege data mismatch at $%.6X: %.4X should be %.4X.\n", i, c, *((uint16_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read16 odd errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;

    printf("Clearing %d KB of Chip again\n", test_size / SIZE_KILO);
    for (uint32_t i = 0; i < test_size; i++) {
        write8(i, (uint32_t)0x0);
    }

    printf("[LONG] Writing garbege datas to Chip, unaligned...\n");
    for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
        uint32_t v = *((uint32_t *)&garbege_datas[i]);
        write8(i , v & 0x0000FF);
        write16(i + 1, htobe16(((v & 0x00FFFF00) >> 8)));
        write8(i + 3 , (v & 0xFF000000) >> 24);
    }

    sleep (1);
    printf("Reading back garbege datas, read32(), odd addresses...\n");
    for (uint32_t i = 1; i < (test_size) - 4; i += 4) {
        uint32_t c = read8(i);
        c |= (be16toh(read16(i + 1)) << 8);
        c |= (read8(i + 3) << 24);
        if (c != *((uint32_t *)&garbege_datas[i])) {
            if (errors < 512)
                printf("READ32_ODD: Garbege data mismatch at $%.6X: %.8X should be %.8X.\n", i, c, *((uint32_t *)&garbege_datas[i]));
            errors++;
        }
    }
    printf("read32 odd errors total: %d.\n", errors);
    total_errors += errors;
    errors = 0;

    if (loop_tests) {
        printf ("Loop %d done. Begin loop %d.\n", cur_loop + 1, cur_loop + 2);
        printf ("Current total errors: %d.\n", total_errors);
        goto test_loop;
    }

    return 0;
}

void m68k_set_irq(unsigned int level) {
}

struct ide_controller *get_ide(int index) {
    return NULL;
}
