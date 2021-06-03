// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <endian.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "irtg_structs.h"
#include "config_file/config_file.h"
#include "gpio/ps_protocol.h"
#include "rtg.h"

#include "m68k.h"

uint8_t rtg_u8[4];
uint16_t rtg_x[8], rtg_y[8];
uint16_t rtg_user[8];
uint16_t rtg_format;
uint32_t rtg_address[8];
uint32_t rtg_address_adj[8];
uint32_t rtg_rgb[8];

static uint8_t display_enabled = 0xFF;

uint16_t rtg_display_width, rtg_display_height;
uint16_t rtg_display_format;
uint16_t rtg_pitch, rtg_total_rows;
uint16_t rtg_offset_x, rtg_offset_y;

uint8_t *rtg_mem; // FIXME

uint32_t framebuffer_addr = 0;
uint32_t framebuffer_addr_adj = 0;

static void handle_rtg_command(uint32_t cmd);
static void handle_irtg_command(uint32_t cmd);

uint8_t realtime_graphics_debug = 0;
extern int cpu_emulation_running;
extern struct emulator_config *cfg;
extern uint8_t rtg_on;

//#define DEBUG_RTG

#ifdef DEBUG_RTG
/*static const char *op_type_names[OP_TYPE_NUM] = {
    "BYTE",
    "WORD",
    "LONGWORD",
    "MEM",
};*/

/*static const char *rtg_format_names[RTGFMT_NUM] = {
    "8BPP CLUT",
    "16BPP RGB (565)",
    "32BPP RGB (RGBA)",
    "15BPP RGB (555)",
};*/
#define DEBUG printf
#else
#define DEBUG(...)
#endif

int init_rtg_data(struct emulator_config *cfg_) {
    rtg_mem = calloc(1, 40 * SIZE_MEGA);
    if (!rtg_mem) {
        printf("Failed to allocate RTG video memory.\n");
        return 0;
    }

    m68k_add_ram_range(PIGFX_RTG_BASE + PIGFX_REG_SIZE, 32 * SIZE_MEGA - PIGFX_REG_SIZE, rtg_mem);
    add_mapping(cfg_, MAPTYPE_RAM_NOALLOC, PIGFX_RTG_BASE + PIGFX_REG_SIZE, 40 * SIZE_MEGA - PIGFX_REG_SIZE, -1, (char *)rtg_mem, "rtg_mem", 0);
    return 1;
}

void shutdown_rtg() {
    printf("[RTG] Shutting down RTG.\n");
    if (rtg_on) {
        rtg_on = 0;
    }
    if (rtg_mem) {
        
        free(rtg_mem);
        rtg_mem = NULL;
    }
}

unsigned int rtg_get_fb() {
    return PIGFX_RTG_BASE + PIGFX_REG_SIZE + framebuffer_addr_adj;
}

unsigned int rtg_read(uint32_t address, uint8_t mode) {
    //printf("%s read from RTG: %.8X\n", op_type_names[mode], address);
    if (address == RTG_COMMAND) {
        return 0xFFCF;
    }
    if (address >= PIGFX_REG_SIZE) {
        if (rtg_mem && (address - PIGFX_REG_SIZE) < PIGFX_UPPER) {
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
        if (rtg_mem && (address - PIGFX_REG_SIZE) < PIGFX_UPPER) {
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
    } else if (address == RTG_DEBUGME) {
        printf("RTG DEBUGME WRITE: %d.\n", value);
    } else {
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
                    case IRTG_COMMAND:
                        handle_irtg_command(value);
                        break;
                }
                break;
            case OP_TYPE_LONGWORD:
                switch (address) {
                    case RTG_ADDR1:
                        rtg_address[0] = value;
                        rtg_address_adj[0] = value - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
                        break;
                    case RTG_ADDR2:
                        rtg_address[1] = value;
                        rtg_address_adj[1] = value - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
                        break;
                    CHKREG(RTG_ADDR3, rtg_address[2]);
                    CHKREG(RTG_ADDR4, rtg_address[3]);
                    CHKREG(RTG_RGB1, rtg_rgb[0]);
                    CHKREG(RTG_RGB2, rtg_rgb[1]);
                }
                break;
        }
    }

    return;
}

#define gdebug(a) if (realtime_graphics_debug) { printf(a); m68k_end_timeslice(); cpu_emulation_running = 0; }
#define M68KR(a) m68k_get_reg(NULL, a)
#define RGBF_D7 rgbf_to_rtg[M68KR(M68K_REG_D7)]
#define CMD_PITCH be16toh(r->BytesPerRow)

static struct P96RenderInfo *r;
static struct P96BoardInfo *b;
static struct P96Line *ln;
static uint8_t cmd_mask;

static void handle_irtg_command(uint32_t cmd) {
    b = (struct P96BoardInfo *)get_mapped_data_pointer_by_address(cfg, M68KR(M68K_REG_A0));
    r = (struct P96RenderInfo *)get_mapped_data_pointer_by_address(cfg, M68KR(M68K_REG_A1));

    switch (cmd) {
        case RTGCMD_SETPAN: {
            // A0: struct BoardInfo *b, A1: UBYTE *addr, D0 UWORD width, D1: WORD x_offset, D2: WORD y_offset, D7: RGBFTYPE format
#ifdef DEBUG_RTG
            if (realtime_graphics_debug) {
                printf("iSetPanning begin\n");
                printf("IRTGCmd SetPanning\n");
                printf("IRTGCmd x: %d y: %d w: %d (%d)\n", M68KR(M68K_REG_D1), M68KR(M68K_REG_D2), M68KR(M68K_REG_D0) << RGBF_D7, M68KR(M68K_REG_D0));
                printf("BoardInfo: %.8X Addr: %.8X\n", M68KR(M68K_REG_A0), M68KR(M68K_REG_A1));
                printf("BoardInfo Xoffs: %d Yoffs: %d\n", be16toh(b->XOffset), be16toh(b->YOffset));
            }
#endif
            if (!b) break;

            b->XOffset = (int16_t)htobe16(M68KR(M68K_REG_D1));
            b->YOffset = (int16_t)htobe16(M68KR(M68K_REG_D2));

            rtg_offset_x = M68KR(M68K_REG_D1);
            rtg_offset_y = M68KR(M68K_REG_D2);
            rtg_pitch = (M68KR(M68K_REG_D0) << RGBF_D7);
            framebuffer_addr = M68KR(M68K_REG_A1) - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
            framebuffer_addr_adj = framebuffer_addr + (rtg_offset_x << RGBF_D7) + (rtg_offset_y * rtg_pitch);

#ifdef DEBUG_RTG
            if (realtime_graphics_debug) {
                printf("RTG OffsetX/Y: %d/%d\n", rtg_offset_x, rtg_offset_y);
                printf("RTG Pitch: %d\n", rtg_pitch);
                printf("RTG FBAddr/Adj: %.8X (%.8X)/%.8X\n", framebuffer_addr, M68KR(M68K_REG_A1), framebuffer_addr_adj);
                printf("iSetPanning End\n");
            }
#endif

            break;
        }
        case RTGCMD_DRAWLINE: {
            // A0: struct BoardInfo *b, A1: RenderInfo *r A2: struct Line *l, D0: UBYTE mask, D7: RGBFTYPE format
            gdebug("iDrawLine begin\n");
            ln = (struct P96Line *)get_mapped_data_pointer_by_address(cfg, M68KR(M68K_REG_A2));

            if (!ln || !r) break;

            cmd_mask = (uint8_t)M68KR(M68K_REG_D0);
            rtg_address_adj[0] = be32toh(r->_p_Memory) - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);

            if (cmd_mask == 0xFF && be16toh(ln->LinePtrn) == 0xFFFF) {
                rtg_drawline_solid(be16toh(ln->X), be16toh(ln->Y), be16toh(ln->dX), be16toh(ln->dY),
                                   be16toh(ln->Length), be32toh(ln->FgPen), CMD_PITCH, RGBF_D7);
            } else {
                rtg_drawline(be16toh(ln->X), be16toh(ln->Y), be16toh(ln->dX), be16toh(ln->dY),
                             be16toh(ln->Length), be16toh(ln->LinePtrn), be16toh(ln->PatternShift),
                             be32toh(ln->FgPen), be32toh(ln->BgPen), CMD_PITCH,
                             RGBF_D7, cmd_mask, ln->DrawMode);
            }
            gdebug("iDrawLine end\n");
            break;
        }
        case RTGCMD_FILLRECT: {
            // A0: BoardInfo *b, A1: RenderInfo *r
            // D0 WORD x, D1: WORD y, D2: WORD w, D3: WORD h
            // D4: ULONG color, D5: UBYTE mask, D7: RGBFTYPE format
            gdebug("iFillRect begin\n");
#ifdef DEBUG_RTG
            if (realtime_graphics_debug) {
                DEBUG("X1/X2: %d/%d-> X2/Y2: %d/%d\n", (int16_t)M68KR(M68K_REG_D0), (int16_t)M68KR(M68K_REG_D1), (int16_t)M68KR(M68K_REG_D2), (int16_t)M68KR(M68K_REG_D3));
                DEBUG("R: %.8X B: %.8X\n", M68KR(M68K_REG_A0), M68KR(M68K_REG_A1));
            }
#endif

            if (!b || !r) break;

            cmd_mask = (uint8_t)M68KR(M68K_REG_D5);
            rtg_address_adj[0] = be32toh(r->_p_Memory) - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);

            if (cmd_mask == 0xFF) {
                rtg_fillrect_solid((int16_t)M68KR(M68K_REG_D0), (int16_t)M68KR(M68K_REG_D1), (int16_t)M68KR(M68K_REG_D2), (int16_t)M68KR(M68K_REG_D3),
                                   M68KR(M68K_REG_D4), CMD_PITCH, RGBF_D7);
            } else {
                rtg_fillrect((int16_t)M68KR(M68K_REG_D0), (int16_t)M68KR(M68K_REG_D1), (int16_t)M68KR(M68K_REG_D2), (int16_t)M68KR(M68K_REG_D3),
                             M68KR(M68K_REG_D4), CMD_PITCH, RGBF_D7, cmd_mask);
            }
            gdebug("iFillRect end\n");
            break;
        }
        case RTGCMD_INVERTRECT: {
            // A0: BoardInfo *b, A1: RenderInfo *r
            // D0 WORD x, D1: WORD y, D2: WORD w, D3: WORD h
            // D4: UBYTE mask, D7: RGBFTYPE format
            gdebug("iInvertRect begin\n");
            if (!b || !r) break;

            cmd_mask = (uint8_t)M68KR(M68K_REG_D4);
            rtg_address_adj[0] = be32toh(r->_p_Memory) - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);

            rtg_invertrect((int16_t)M68KR(M68K_REG_D0), (int16_t)M68KR(M68K_REG_D1), (int16_t)M68KR(M68K_REG_D2), (int16_t)M68KR(M68K_REG_D3), CMD_PITCH, RGBF_D7, cmd_mask);
            gdebug("iInvertRect end\n");
            break;
        }
        case RTGCMD_BLITRECT: {
            // A0: BoardInfo *b, A1: RenderInfo *r)
            // D0: WORD x, D1: WORD y, D2: WORD dx, D3: WORD dy, D4: WORD w, D5: WORD h,
            // D6: UBYTE mask, D7: RGBFTYPE format
            gdebug("iBlitRect begin\n");

            cmd_mask = (uint8_t)M68KR(M68K_REG_D6);
            rtg_address_adj[0] = be32toh(r->_p_Memory) - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);

            if (cmd_mask == 0xFF) {
                rtg_blitrect_solid(M68KR(M68K_REG_D0), M68KR(M68K_REG_D1), M68KR(M68K_REG_D2), M68KR(M68K_REG_D3), M68KR(M68K_REG_D4), M68KR(M68K_REG_D5), CMD_PITCH, RGBF_D7);
            } else {
                rtg_blitrect(M68KR(M68K_REG_D0), M68KR(M68K_REG_D1), M68KR(M68K_REG_D2), M68KR(M68K_REG_D3), M68KR(M68K_REG_D4), M68KR(M68K_REG_D5), CMD_PITCH, RGBF_D7, cmd_mask);
            }

            gdebug("iBlitRect end\n");
            break;
        }
        case RTGCMD_BLITRECT_NOMASK_COMPLETE: {
            // A0: BoardInfo *b, A1: RenderInfo *rs, A2: RenderInfo *rt,
            // D0: WORD x, D1: WORD y, D2: WORD dx, D3: WORD dy, D4: WORD w, D5: WORD h,
            // D6: UBYTE minterm, D7: RGBFTYPE format
            gdebug("iBlitRectNoMaskComplete begin\n");

            uint8_t minterm = (uint8_t)M68KR(M68K_REG_D6);
            struct P96RenderInfo *rt = (struct P96RenderInfo *)get_mapped_data_pointer_by_address(cfg, M68KR(M68K_REG_A2));

            uint32_t src_addr = be32toh(r->_p_Memory);
            uint32_t dst_addr = be32toh(rt->_p_Memory);

            rtg_blitrect_nomask_complete(M68KR(M68K_REG_D0), M68KR(M68K_REG_D1), M68KR(M68K_REG_D2), M68KR(M68K_REG_D3), M68KR(M68K_REG_D4), M68KR(M68K_REG_D5),
                                         CMD_PITCH, be16toh(rt->BytesPerRow), src_addr, dst_addr, RGBF_D7, minterm);

            gdebug("iBlitRectNoMaskComplete end\n");
            break;
        }
        case RTGCMD_BLITTEMPLATE: {
            // A0: BoardInfo *b, A1: RenderInfo *r, A2 Template *t
            // D0: WORD x, D1: WORD y, D2: WORD w, D3: WORD h
            // D4: UBYTE mask, D7: RGBFTYPE format
            if (!r)
                break;

            uint16_t t_pitch = 0, x_offset = 0;
            uint32_t src_addr = M68KR(M68K_REG_A2);
            uint32_t fgcol = 0, bgcol = 0;
            uint8_t draw_mode = 0;

            struct P96Template *t = (struct P96Template *)get_mapped_data_pointer_by_address(cfg, M68KR(M68K_REG_A2));
            if (t) {
                t_pitch = be16toh(t->BytesPerRow);
                fgcol = be32toh(t->FgPen);
                bgcol = be32toh(t->BgPen);
                x_offset = be16toh(t->XOffset);
                draw_mode = t->DrawMode;
                src_addr = be32toh(t->_p_Memory);
            } else {
                t_pitch = be16toh(ps_read_16(src_addr + (uint32_t)&t->BytesPerRow));
                fgcol = be32toh(ps_read_32(src_addr + (uint32_t)&t->FgPen));
                bgcol = be32toh(ps_read_32(src_addr + (uint32_t)&t->BgPen));
                x_offset = be16toh(ps_read_16(src_addr + (uint32_t)&t->XOffset));
                draw_mode = ps_read_8(src_addr + (uint32_t)&t->DrawMode);
                src_addr = be32toh(ps_read_32(src_addr + (uint32_t)&t->_p_Memory));
            }

            cmd_mask = (uint8_t)M68KR(M68K_REG_D4);
            rtg_address[1] = be32toh(r->_p_Memory);
            rtg_address_adj[1] = rtg_address[1] - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);

            rtg_blittemplate(M68KR(M68K_REG_D0), M68KR(M68K_REG_D1), M68KR(M68K_REG_D2), M68KR(M68K_REG_D3), src_addr, fgcol, bgcol, CMD_PITCH, t_pitch, RGBF_D7, x_offset, cmd_mask, draw_mode);
            break;
        }
        default:
            printf("[!!!IRTG] Unnkonw/unhandled iRTG command %d.\n", cmd);
            break;
    }
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
            //printf("Set RTG mode:\n");
            //printf("%dx%d pixels\n", rtg_display_width, rtg_display_height);
            //printf("Pixel format: %s\n", rtg_format_names[rtg_display_format]);
            break;
        case RTGCMD_SETPAN:
            //printf("Command: SetPan.\n");
            rtg_offset_x = rtg_x[1];
            rtg_offset_y = rtg_y[1];
            rtg_pitch = (rtg_x[0] << rtg_display_format);
            framebuffer_addr = rtg_address[0] - (PIGFX_RTG_BASE + PIGFX_REG_SIZE);
            framebuffer_addr_adj = framebuffer_addr + (rtg_offset_x << rtg_display_format) + (rtg_offset_y * rtg_pitch);
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
            if (rtg_u8[0] == 0xFF || rtg_format != RTGFMT_8BIT) {
                rtg_fillrect_solid(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_rgb[0], rtg_x[2], rtg_format);
                gdebug("FillRect Solid\n");
            }
            else {
                rtg_fillrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_rgb[0], rtg_x[2], rtg_format, rtg_u8[0]);
                gdebug("FillRect Masked\n");
            }
            break;
        case RTGCMD_INVERTRECT:
            rtg_invertrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_format, rtg_u8[0]);
            gdebug("InvertRect\n");
            break;
        case RTGCMD_BLITRECT:
            if (rtg_u8[0] == 0xFF || rtg_format != RTGFMT_8BIT) {
                rtg_blitrect_solid(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_x[3], rtg_format);
                gdebug("BlitRect Solid\n");
            }
            else {
                rtg_blitrect(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_x[3], rtg_format, rtg_u8[0]);
                gdebug("BlitRect Masked\n");
            }
            break;
        case RTGCMD_BLITRECT_NOMASK_COMPLETE:
            rtg_blitrect_nomask_complete(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_x[3], rtg_x[4], rtg_address[0], rtg_address[1], rtg_format, rtg_u8[0]);
            gdebug("BlitRectNoMaskComplete\n");
            break;
        case RTGCMD_BLITPATTERN:
            rtg_blitpattern(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_address[0], rtg_rgb[0], rtg_rgb[1], rtg_x[3], rtg_format, rtg_x[2], rtg_y[2], rtg_u8[0], rtg_u8[1], rtg_u8[2]);
            gdebug("BlitPattern\n");
            return;
        case RTGCMD_BLITTEMPLATE:
            rtg_blittemplate(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_address[0], rtg_rgb[0], rtg_rgb[1], rtg_x[3], rtg_x[4], rtg_format, rtg_x[2], rtg_u8[0], rtg_u8[1]);
            gdebug("BlitTemplate\n");
            break;
        case RTGCMD_DRAWLINE:
            if (rtg_u8[0] == 0xFF && rtg_y[2] == 0xFFFF)
                rtg_drawline_solid(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_rgb[0], rtg_x[3], rtg_format);
            else
                rtg_drawline(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_x[4], rtg_rgb[0], rtg_rgb[1], rtg_x[3], rtg_format, rtg_u8[0], rtg_u8[1]);
            gdebug("DrawLine\n");
            break;
        case RTGCMD_P2C:
            rtg_p2c(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_u8[1], rtg_u8[2], rtg_u8[0], (rtg_user[0] >> 0x8), rtg_x[4], (uint8_t *)&rtg_mem[rtg_address_adj[1]]);
            gdebug("Planar2Chunky\n");
            break;
        case RTGCMD_P2D:
            rtg_p2d(rtg_x[0], rtg_y[0], rtg_x[1], rtg_y[1], rtg_x[2], rtg_y[2], rtg_u8[1], rtg_u8[2], rtg_u8[0], (rtg_user[0] >> 0x8), rtg_x[4], (uint8_t *)&rtg_mem[rtg_address_adj[1]]);
            gdebug("Planar2Direct\n");
            break;
        case RTGCMD_SETSPRITE:
            rtg_enable_mouse_cursor();
            gdebug("SetSprite\n");
            break;
        case RTGCMD_SETSPRITECOLOR:
            rtg_set_cursor_clut_entry(rtg_u8[0], rtg_u8[1], rtg_u8[2], rtg_u8[3]);
            gdebug("SetSpriteColor\n");
            break;
        case RTGCMD_SETSPRITEPOS:
            rtg_set_mouse_cursor_pos((int16_t)rtg_x[0], (int16_t)rtg_y[0]);
            gdebug("SetSpritePos\n");
            break;
        case RTGCMD_SETSPRITEIMAGE:
            rtg_set_mouse_cursor_image(&rtg_mem[rtg_address_adj[1]], rtg_u8[0], rtg_u8[1]);
            gdebug("SetSpriteImage\n");
            break;
        case RTGCMD_DEBUGME:
            printf ("[RTG] DebugMe!\n");
            break;
        default:
            printf("[!!!RTG] Unknown/unhandled RTG command %d ($%.4X)\n", cmd, cmd);
            break;
    }
}
