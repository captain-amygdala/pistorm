#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rtg.h"
#include "../../../config_file/config_file.h"

static uint8_t rtg_u8[4];
static uint16_t rtg_x[8], rtg_y[8];
static uint16_t rtg_user[8];
static uint16_t rtg_format;
static uint32_t rtg_address[2];
static uint32_t rtg_rgb[2];

static uint8_t display_enabled = 0xFF;

uint16_t rtg_display_width, rtg_display_height;
uint16_t rtg_display_format;
uint16_t rtg_pitch, rtg_total_rows;
uint16_t rtg_offset_x, rtg_offset_y;

uint8_t *rtg_mem; // FIXME

uint32_t framebuffer_addr;
uint32_t framebuffer_addr_adj;

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
                    return be16toh(*(( uint16_t *) (&rtg_mem[address - PIGFX_REG_SIZE])));
                    break;
                case OP_TYPE_LONGWORD:
                    return be32toh(*(( uint32_t *) (&rtg_mem[address - PIGFX_REG_SIZE])));
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

#define CHKREG(a, b) case a: b = value; break;

void rtg_write(uint32_t address, uint32_t value, uint8_t mode) {
    //printf("%s write to RTG: %.8X (%.8X)\n", op_type_names[mode], address, value);
    if (address >= PIGFX_REG_SIZE) {
        /*if ((address - PIGFX_REG_SIZE) < framebuffer_addr) {// || (address - PIGFX_REG_SIZE) > framebuffer_addr + ((rtg_display_width << rtg_display_format) * rtg_display_height)) {
            printf("Write to RTG memory outside frame buffer %.8X (%.8X).\n", (address - PIGFX_REG_SIZE), framebuffer_addr);
        }*/
        if (rtg_mem) {
            switch (mode) {
                case OP_TYPE_BYTE:
                    rtg_mem[address - PIGFX_REG_SIZE] = value;
                    break;
                case OP_TYPE_WORD:
                    *(( uint16_t *) (&rtg_mem[address - PIGFX_REG_SIZE])) = htobe16(value);
                    break;
                case OP_TYPE_LONGWORD:
                    *(( uint32_t *) (&rtg_mem[address - PIGFX_REG_SIZE])) = htobe32(value);
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
                    CHKREG(RTG_U81, rtg_u8[0]);
                    CHKREG(RTG_U82, rtg_u8[1]);
                    CHKREG(RTG_U83, rtg_u8[2]);
                    CHKREG(RTG_U84, rtg_u8[3]);
                }
                break;
            case OP_TYPE_WORD:
                switch (address) {
                    CHKREG(RTG_X1, rtg_x[0]);
                    CHKREG(RTG_X2, rtg_x[1]);
                    CHKREG(RTG_X3, rtg_x[2]);
                    CHKREG(RTG_X4, rtg_x[3]);
                    CHKREG(RTG_X5, rtg_x[4]);
                    CHKREG(RTG_Y1, rtg_y[0]);
                    CHKREG(RTG_Y2, rtg_y[1]);
                    CHKREG(RTG_Y3, rtg_y[2]);
                    CHKREG(RTG_Y4, rtg_y[3]);
                    CHKREG(RTG_Y5, rtg_y[4]);
                    CHKREG(RTG_U1, rtg_user[0]);
                    CHKREG(RTG_U2, rtg_user[1]);
                    CHKREG(RTG_FORMAT, rtg_format);
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
                //rtg_pitch = rtg_display_width << rtg_format;
                framebuffer_addr_adj = framebuffer_addr + (rtg_offset_x << rtg_display_format) + (rtg_offset_y * rtg_pitch);
                rtg_total_rows = rtg_y[1];
            }
            else {
                //rtg_pitch = rtg_display_width << rtg_format;
                framebuffer_addr_adj = framebuffer_addr + (rtg_offset_x << rtg_display_format) + (rtg_offset_y * rtg_pitch);
                rtg_total_rows = rtg_y[1];
            }
            printf("Set RTG mode:\n");
            printf("%dx%d pixels\n", rtg_display_width, rtg_display_height);
            printf("Pixel format: %s\n", rtg_format_names[rtg_display_format]);
            break;
        case RTGCMD_SETPAN:
            //printf("Command: SetPan.\n");
            rtg_offset_x = rtg_x[1];
            rtg_offset_y = rtg_y[1];
            rtg_pitch = (rtg_x[0] << rtg_display_format);
            framebuffer_addr = rtg_address[0] - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
            framebuffer_addr_adj = framebuffer_addr + (rtg_offset_x << rtg_display_format) + (rtg_offset_y * rtg_pitch);
            printf("Set panning to $%.8X (%.8X)\n", framebuffer_addr, rtg_address[0]);
            printf("(Panned: $%.8X)\n", framebuffer_addr_adj);
            printf("Offset X/Y: %d/%d\n", rtg_offset_x, rtg_offset_y);
            printf("Pitch: %d (%d bytes)\n", rtg_x[0], rtg_pitch);
            break;
        case RTGCMD_SETCLUT: {
            //printf("Command: SetCLUT.\n");
            //printf("Set palette entry %d to %d, %d, %d\n", rtg_u8[0], rtg_u8[1], rtg_u8[2], rtg_u8[3]);
            //printf("Set palette entry %d to 32-bit palette color: %.8X\n", rtg_u8[0], rtg_rgb[0]);
            rtg_set_clut_entry(rtg_u8[0], rtg_rgb[0]);
            break;
        }
        case RTGCMD_SETDISPLAY:
            //printf("RTG SetDisplay %s\n", (rtg_u8[1]) ? "enabled" : "disabled");
            // I remeber wrongs.
            //printf("Command: SetDisplay.\n");
            break;
        case RTGCMD_ENABLE:
        case RTGCMD_SETSWITCH:
            //printf("RTG SetSwitch %s\n", ((rtg_x[0]) & 0x01) ? "enabled" : "disabled");
            //printf("LAL: %.4X\n", rtg_x[0]);
            if (display_enabled != ((rtg_x[0]) & 0x01)) {
                display_enabled = ((rtg_x[0]) & 0x01);
                if (display_enabled) {
                    rtg_init_display();
                }
                else
                    rtg_shutdown_display();
            }
            break;
        case RTGCMD_FILLRECT:
            rtg_fillrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_rgb[0], rtg_x[2], rtg_format, 0xFF);
            break;
        case RTGCMD_BLITRECT:
            rtg_blitrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_x[3], rtg_format, 0xFF);
            break;
    }
}

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask) {
    if (mask) {}
    uint8_t *dptr = &rtg_mem[framebuffer_addr + (x << format) + (y * pitch)];
    //printf("FillRect: %d,%d to %d,%d C%.8X, p:%d dp: %d m:%.2X\n", x, y, x+w, y+h, color, pitch, rtg_pitch, mask);
    //printf("%.8X - %.8X (%p)\n", framebuffer_addr, framebuffer_addr_adj, dptr);
    switch(format) {
        case RTGFMT_8BIT: {
            //printf("Incoming 8-bit color: %.8X\n", color);
            for (int xs = 0; xs < w; xs++) {
                dptr[xs] = color & 0xFF;
            }
            break;
        }
        case RTGFMT_RBG565: {
            //printf("Incoming raw 16-bit color: %.8X\n", htobe32(color));
            color = htobe16((color & 0xFFFF));
            //printf("Incoming 16-bit color: %.8X\n", color);
            uint16_t *ptr = (uint16_t *)dptr;
            for (int xs = 0; xs < w; xs++) {
                ptr[xs] = color;
            }
            break;
        }
        case RTGFMT_RGB32: {
            color = htobe32(color);
            //printf("Incoming 32-bit color: %.8X\n", color);
            uint32_t *ptr = (uint32_t *)dptr;
            for (int xs = 0; xs < w; xs++) {
                ptr[xs] = color;
            }
            break;
        }
    }
    for (int ys = 1; ys < h; ys++) {
        dptr += pitch;
        memcpy(dptr, (void *)(size_t)(dptr - pitch), (w << format));
    }
}

void rtg_blitrect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask) {
    if (mask) {}
    //printf("BlitRect: %d,%d to %d,%d (%dx%d) p:%d dp: %d\n", x, y, dx, dy, w, h, pitch, rtg_pitch);
    uint8_t *sptr = &rtg_mem[framebuffer_addr + (x << format) + (y * pitch)];
    uint8_t *dptr = &rtg_mem[framebuffer_addr + (dx << format) + (dy * pitch)];

    uint32_t xdir = 1, pitchstep = pitch;

    if (y < dy) {
        pitchstep = -pitch;
        sptr += ((h - 1) * pitch);
        dptr += ((h - 1) * pitch);
    }
    if (x < dx) {
        xdir = 0;
    }

    for (int ys = 0; ys < h; ys++) {
        if (xdir)
            memcpy(dptr, sptr, w << format);
        else
            memmove(dptr, sptr, w << format);
        sptr += pitchstep;
        dptr += pitchstep;
    }
}
