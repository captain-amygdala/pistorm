#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rtg.h"
#include "../../../config_file/config_file.h"

static uint16_t palette[256];

static uint8_t rtg_u8[4];
static uint16_t rtg_x[3], rtg_y[3];
static uint16_t rtg_format;
static uint32_t rtg_address[2];
static uint32_t rtg_rgb[2];

static uint8_t display_enabled;

uint16_t rtg_display_width, rtg_display_height;
uint16_t rtg_display_format;
uint16_t rtg_pitch, rtg_total_rows;
uint16_t rtg_offset_x, rtg_offset_y;

uint8_t *rtg_mem; // FIXME

uint32_t framebuffer_addr;

static void handle_rtg_command(uint32_t cmd);
static struct timespec f1, f2;

static const char *op_type_names[OP_TYPE_NUM] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};

static const char *rtg_format_names[RTGFMT_NUM] = {
    "8BPP CLUT",
    "16BPP RGB (565)",
    "32BPP RGB (RGBA)",
    "15BPP RGB (555)",
};

int init_rtg_data() {
    rtg_mem = calloc(1, 32 * SIZE_MEGA);
    if (!rtg_mem) {
        printf("Failed to allocate RTG video memory.\n");
        return 0;
    }

    return 1;
}

extern uint8_t busy, rtg_on;
void rtg_update_screen();

unsigned int rtg_read(uint32_t address, uint8_t mode) {
    //printf("%s read from RTG: %.8X\n", op_type_names[mode], address);
    if (address >= PIGFX_REG_SIZE) {
        if (rtg_mem) {
            switch (mode) {
                case OP_TYPE_BYTE:
                    return (rtg_mem[address - PIGFX_REG_SIZE]);
                    break;
                case OP_TYPE_WORD:
                    return *(( uint16_t *) (&rtg_mem[address - PIGFX_REG_SIZE]));
                    break;
                case OP_TYPE_LONGWORD:
                    return *(( uint32_t *) (&rtg_mem[address - PIGFX_REG_SIZE]));
                    break;
                default:
                    return 0;
            }
        }
    }

    return 0;
}

struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

void rtg_write(uint32_t address, uint32_t value, uint8_t mode) {
    //printf("%s write to RTG: %.8X (%.8X)\n", op_type_names[mode], address, value);
    if (address >= PIGFX_REG_SIZE) {
        if (rtg_mem) {
            switch (mode) {
                case OP_TYPE_BYTE:
                    rtg_mem[address - PIGFX_REG_SIZE] = value;
                    break;
                case OP_TYPE_WORD:
                    *(( uint16_t *) (&rtg_mem[address - PIGFX_REG_SIZE])) = value;
                    break;
                case OP_TYPE_LONGWORD:
                    *(( uint32_t *) (&rtg_mem[address - PIGFX_REG_SIZE])) = value;
                    break;
                default:
                    return;
            }
        }
    }
    else {
        switch (mode) {
            case OP_TYPE_BYTE:
                switch (address) {
                    case RTG_U81:
                    case RTG_U82:
                    case RTG_U83:
                    case RTG_U84:
                        rtg_u8[address - RTG_U81] = value;
                }
                break;
            case OP_TYPE_WORD:
                switch (address) {
                    case RTG_X1:
                        rtg_x[0] = value;
                        break;
                    case RTG_X2:
                        rtg_x[1] = value;
                        break;
                    case RTG_X3:
                        rtg_x[2] = value;
                        break;
                    case RTG_Y1:
                        rtg_y[0] = value;
                        break;
                    case RTG_Y2:
                        rtg_y[1] = value;
                        break;
                    case RTG_Y3:
                        rtg_y[2] = value;
                        break;
                    case RTG_FORMAT:
                        rtg_format = value;
                        break;
                    case RTG_COMMAND:
                        handle_rtg_command(value);
                        break;
                }
                break;
            case OP_TYPE_LONGWORD:
                switch (address) {
                    case RTG_ADDR1:
                        rtg_address[0] = value;
                        break;
                    case RTG_ADDR2:
                        rtg_address[1] = value;
                        break;
                    case RTG_RGB1:
                        rtg_rgb[0] = value;
                        break;
                    case RTG_RGB2:
                        rtg_rgb[1] = value;
                        break;
                }
                break;
        }
    }

    if (rtg_on) {
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &f2);
        if (diff(f1,f2).tv_nsec / 1000000.0 > 100.00) {
            rtg_update_screen();
            f1 = f2;
        }
    }

    return;
}

static void handle_rtg_command(uint32_t cmd) {
    //printf("Handling RTG command %d (%.8X)\n", cmd, cmd);
    switch (cmd) {
        case RTGCMD_SETGC:
            rtg_display_format = rtg_format;
            rtg_display_width = rtg_x[0];
            rtg_display_height = rtg_y[0];
            if (rtg_u8[0]) {
                rtg_pitch = rtg_x[1];
                rtg_total_rows = rtg_y[1];
            }
            else {
                rtg_pitch = rtg_x[1];
                rtg_total_rows = rtg_y[1];
            }
            //printf("Set RTG mode:\n");
            //printf("%dx%d pixels\n", rtg_display_width, rtg_display_height);
            //printf("Pixel format: %s\n", rtg_format_names[rtg_display_format]);
            break;
        case RTGCMD_SETPAN:
            //printf("Command: SetPan.\n");
            framebuffer_addr = rtg_address[0] - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
            rtg_offset_x = rtg_x[1];
            rtg_offset_y = rtg_y[1];
            rtg_pitch = (rtg_x[0] << rtg_display_format);
            //printf("Set panning to $%.8X\n", framebuffer_addr);
            //printf("Offset X/Y: %d/%d\n", rtg_offset_x, rtg_offset_y);
            //printf("Pitch: %d (%d bytes)\n", rtg_x[0], rtg_pitch);
            break;
        case RTGCMD_SETCLUT: {
            //printf("Command: SetCLUT.\n");
            //printf("Set palette entry %d to %d, %d, %d\n", rtg_u8[0], rtg_u8[1], rtg_u8[2], rtg_u8[3]);
            /*int r = (int)((float)rtg_u8[1] / 255.0f * 31.0f);
            int g = (int)((float)rtg_u8[2] / 255.0f * 63.0f);
            int b = (int)((float)rtg_u8[3] / 255.0f * 31.0f);
            palette[rtg_u8[0]] = ((r & 0x1F) << 11) | ((g & 0x3F) << 6) | (b & 0x1F);*/
            rtg_set_clut_entry(rtg_u8[0], rtg_u8[1], rtg_u8[2], rtg_u8[3]);
            break;
        }
        case RTGCMD_SETDISPLAY:
            // I remeber wrongs.
            //printf("Command: SetDisplay.\n");
            break;
        case RTGCMD_ENABLE:
        case RTGCMD_SETSWITCH:
            if (display_enabled != rtg_u8[1]) {
                //printf("RTG Display %s\n", (rtg_u8[1]) ? "enabled" : "disabled");
                display_enabled = rtg_u8[1];
                if (display_enabled) {
                    rtg_init_display();
                    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &f1);
                }
                else
                    rtg_shutdown_display();
            }
            break;
        case RTGCMD_FILLRECT:
            rtg_fillrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_rgb[0], rtg_x[2], rtg_format, 0xFF);
            break;
    }
}

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask) {
    if (mask) {}
    switch(format) {
        case RTGFMT_8BIT:
            break;
        case RTGFMT_RBG565: {
            uint16_t *ptr = (uint16_t *)&rtg_mem[framebuffer_addr + (x << format) + (y * pitch)];
            for (int xs = 0; xs < w; xs++) {
                ptr[xs] = (color >> 16);
            }
            for (int ys = 1; ys < h; ys++) {
                ptr += (rtg_pitch >> format);
                memcpy(ptr, (void *)(size_t)(ptr - (rtg_pitch >> format)), rtg_pitch);
            }
            break;
        }
        case RTGFMT_RGB32:
            break;
    }
}
