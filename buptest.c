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

uint8_t *garbege_datas;
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

    if (argc > 1) {
        if (strcmp(argv[1], "dumpkick") == 0) {
            printf ("Dumping onboard Amiga kickstart from $F80000 to file kick.rom.\n");
            printf ("Note that this will always dump 512KB of data, even if your Kickstart is 256KB.\n");
            FILE *out = fopen("kick.rom", "wb+");
            if (out == NULL) {
                printf ("Failed to open kick.rom for writing.\nKickstart has not been dumped.\n");
                return 1;
            }

            for (int i = 0; i < 512 * SIZE_KILO; i++) {
                unsigned char in = read8(0xF80000 + i);
                fputc(in, out);
            }

            fclose(out);
            printf ("Amiga Kickstart ROM dumped.\n");
            return 0;
        }

        if (strcmp(argv[1], "dump") == 0) {
            printf ("Dumping EVERYTHING to dump.bin.\n");
            FILE *out = fopen("dump.bin", "wb+");
            if (out == NULL) {
                printf ("Failed to open dump.bin for writing.\nEverything has not been dumped.\n");
                return 1;
            }

            for (int i = 0; i < 16 * SIZE_MEGA; i++) {
                unsigned char in = dump_read_8(i);
                fputc(in, out);
                if (!(i % 1024))
                    printf (".");
            }
            printf ("\n");

            fclose(out);
            printf ("Dumped everything.\n");
            return 0;
        }

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

    garbege_datas = malloc(test_size);
    if (!garbege_datas) {
        printf ("Failed to allocate memory for garbege datas.\n");
        return 1;
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
