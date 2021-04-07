#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "config_file/config_file.h"
#ifndef FAKESTORM
#include "gpio/ps_protocol.h"
#endif
#include "rtg.h"

extern uint32_t rtg_address[8];
extern uint32_t rtg_address_adj[8];
extern uint8_t *rtg_mem; // FIXME
extern uint16_t rtg_display_format;
extern uint16_t rtg_user[8];
extern uint16_t rtg_x[8], rtg_y[8];

extern uint8_t realtime_graphics_debug;

void rtg_fillrect_solid(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format) {
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (x << format) + (y * pitch)];
    switch(format) {
        case RTGFMT_8BIT: {
            for (int xs = 0; xs < w; xs++) {
                dptr[xs] = color & 0xFF;
            }
            break;
        }
        case RTGFMT_RBG565: {
            color = htobe16((color & 0xFFFF));
            uint16_t *ptr = (uint16_t *)dptr;
            for (int xs = 0; xs < w; xs++) {
                ptr[xs] = color;
            }
            break;
        }
        case RTGFMT_RGB32: {
            color = htobe32(color);
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

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask) {
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (x << format) + (y * pitch)];

    for (int ys = 0; ys < h; ys++) {
        for (int xs = 0; xs < w; xs++) {
            SET_RTG_PIXEL_MASK(&dptr[xs], (color & 0xFF), format);
        }
        dptr += pitch;
    }
}

void rtg_invertrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask) {
    if (mask) {}
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (x << format) + (y * pitch)];
    for (int ys = 0; ys < h; ys++) {
        switch(format) {
            case RTGFMT_8BIT: {
                for (int xs = 0; xs < w; xs++) {
                    dptr[xs] ^= mask;
                }
                break;
            }
            case RTGFMT_RBG565: {
                for (int xs = 0; xs < w; xs++) {
                    ((uint16_t *)dptr)[xs] = ~((uint16_t *)dptr)[xs];
                }
                break;
            }
            case RTGFMT_RGB32: {
                for (int xs = 0; xs < w; xs++) {
                    ((uint32_t *)dptr)[xs] = ~((uint32_t *)dptr)[xs];
                }
                break;
            }
        }
        dptr += pitch;
    }
}

void rtg_blitrect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask) {
    if (mask) {}
    uint8_t *sptr = &rtg_mem[rtg_address_adj[0] + (x << format) + (y * pitch)];
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (dx << format) + (dy * pitch)];

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
        if (xdir) {
            for (int xs = 0; xs < w; xs++) {
                SET_RTG_PIXEL_MASK(&dptr[xs], sptr[xs], format);
            }
        }
        else {
            for (int xs = w - 1; xs >= x; xs--) {
                SET_RTG_PIXEL_MASK(&dptr[xs], sptr[xs], format);
            }
        }
        sptr += pitchstep;
        dptr += pitchstep;
    }
}

void rtg_blitrect_solid(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format) {
    uint8_t *sptr = &rtg_mem[rtg_address_adj[0] + (x << format) + (y * pitch)];
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (dx << format) + (dy * pitch)];

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

void rtg_blitrect_nomask_complete(uint16_t sx, uint16_t sy, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t srcpitch, uint16_t dstpitch, uint32_t src_addr, uint32_t dst_addr, uint16_t format, uint8_t minterm) {
    if (minterm) {}
    uint8_t *sptr = &rtg_mem[src_addr - (PIGFX_RTG_BASE + PIGFX_REG_SIZE) + (sx << format) + (sy * srcpitch)];
    uint8_t *dptr = &rtg_mem[dst_addr - (PIGFX_RTG_BASE + PIGFX_REG_SIZE) + (dx << format) + (dy * dstpitch)];

    uint32_t xdir = 1, src_pitchstep = srcpitch, dst_pitchstep = dstpitch;
    uint8_t draw_mode = minterm;
    uint32_t mask = 0xFF;

    if (src_addr == dst_addr) {
        if (sy < dy) {
            src_pitchstep = -srcpitch;
            sptr += ((h - 1) * srcpitch);
            dst_pitchstep = -dstpitch;
            dptr += ((h - 1) * dstpitch);
        }
        if (sx < dx) {
            xdir = 0;
        }
    }

    if (format == RTGFMT_RBG565)
        mask = 0xFFFF;
    if (format == RTGFMT_RGB32)
        mask = 0xFFFFFFFF;

    if (minterm == MINTERM_SRC) {
        for (int ys = 0; ys < h; ys++) {
            if (xdir)
                memcpy(dptr, sptr, w << format);
            else
                memmove(dptr, sptr, w << format);
            sptr += src_pitchstep;
            dptr += dst_pitchstep;
        }
    }
    else {
        for (int ys = 0; ys < h; ys++) {
            if (xdir) {
                for (int xs = 0; xs < w; xs++) {
                    switch (format) {
                        case RTGFMT_8BIT:
                            HANDLE_MINTERM_PIXEL(sptr[xs], dptr[xs], format);
                            break;
                        case RTGFMT_RBG565:
                            HANDLE_MINTERM_PIXEL(((uint16_t *)sptr)[xs], ((uint16_t *)dptr)[xs], format);
                            break;
                        case RTGFMT_RGB32:
                            HANDLE_MINTERM_PIXEL(((uint32_t *)sptr)[xs], ((uint32_t *)dptr)[xs], format);
                            break;
                    }
                }
            }
            else {
                for (int xs = w - 1; xs >= sx; xs--) {
                    switch (format) {
                        case RTGFMT_8BIT:
                            HANDLE_MINTERM_PIXEL(sptr[xs], dptr[xs], format);
                            break;
                        case RTGFMT_RBG565:
                            HANDLE_MINTERM_PIXEL(((uint16_t *)sptr)[xs], ((uint16_t *)dptr)[xs], format);
                            break;
                        case RTGFMT_RGB32:
                            HANDLE_MINTERM_PIXEL(((uint32_t *)sptr)[xs], ((uint32_t *)dptr)[xs], format);
                            break;
                    }
                }
            }
            sptr += src_pitchstep;
            dptr += src_pitchstep;
        }
    }
}

extern struct emulator_config *cfg;

void rtg_blittemplate(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t t_pitch, uint16_t format, uint16_t offset_x, uint8_t mask, uint8_t draw_mode) {
    if (mask) {}

    uint8_t *dptr = &rtg_mem[rtg_address_adj[1] + (x << format) + (y * pitch)];
    uint8_t *sptr = NULL;
    uint8_t cur_bit = 0, base_bit = 0, cur_byte = 0;
    uint8_t invert = (draw_mode & DRAWMODE_INVERSVID);
    uint16_t tmpl_x = 0;

    draw_mode &= 0x03;

	tmpl_x = offset_x / 8;
    cur_bit = base_bit = (0x80 >> (offset_x % 8));

    if (realtime_graphics_debug) {
        printf("DEBUG: BlitTemplate - %d, %d (%dx%d)\n", x, y, w, h);
        printf("Src: %.8X (%.8X)\n", src_addr, rtg_address_adj[0]);
        printf("Dest: %.8X (%.8X)\n", rtg_address[1], rtg_address_adj[1]);
        printf("pitch: %d t_pitch: %d format: %d\n", pitch, t_pitch, format);
        printf("offset_x: %d mask: %.2X draw_mode: %d\n", offset_x, mask, draw_mode);
    }

    uint32_t fg_color[3] = {
        (fgcol & 0xFF),
        htobe16((fgcol & 0xFFFF)),
        htobe32(fgcol),
    };
    uint32_t bg_color[3] = {
        (bgcol & 0xFF),
        htobe16((bgcol & 0xFFFF)),
        htobe32(bgcol),
    };

    if (src_addr >= (PIGFX_RTG_BASE + PIGFX_REG_SIZE)) {
        sptr = &rtg_mem[src_addr - (PIGFX_RTG_BASE + PIGFX_REG_SIZE)];
        if (realtime_graphics_debug) {
            printf("Origin: %.8X\n", rtg_address[2]);
            printf("Grabbing data from RTG memory.\nData:\n");
            for (int i = 0; i < h; i++) {
                for (int j = 0; j < t_pitch; j++) {
                    printf("%.2X", sptr[j + (i * t_pitch)]);
                }
                printf("\n");
            }
#ifndef FAKESTORM
            printf("Data available at origin:\n");
            for (int i = 0; i < h; i++) {
                for (int j = 0; j < w; j++) {
                    printf("%.2X", read8(rtg_address[2] + j + (i * t_pitch)));
                }
                printf("\n");
            }
#endif
        }
    }
    else {
        int i = get_mapped_item_by_address(cfg, src_addr);
        if (i != -1) {
            sptr = &cfg->map_data[i][src_addr - cfg->map_offset[i]];
            if (realtime_graphics_debug) {
                printf("Grabbing data from maping %d - offset %.8X\nData:\n", i, src_addr - cfg->map_offset[i]);
                for (int i = 0; i < h; i++) {
                    for (int j = 0; j < t_pitch; j++) {
                        printf("%.2X", sptr[j + (i * t_pitch)]);
                    }
                    printf("\n");
                }
            }
        }
        else {
            printf("BlitTemplate: Failed to find mapped range for address %.8X\n", src_addr);
            return;
        }
    }

    switch (draw_mode) {
        case DRAWMODE_JAM1:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        if (mask == 0xFF || format != RTGFMT_8BIT) {
                            SET_RTG_PIXELS(&dptr[xs << format], fg_color[format], format);
                        }
                        else {
                            SET_RTG_PIXELS_MASK(&dptr[xs], fg_color[format], format);
                        }
                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (cur_byte & cur_bit) {
                                if (mask == 0xFF || format != RTGFMT_8BIT) {
                                    SET_RTG_PIXEL(&dptr[xs << format], fg_color[format], format);
                                }
                                else {
                                    SET_RTG_PIXEL_MASK(&dptr[xs], fg_color[format], format);
                                }
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    TEMPLATE_LOOPX;
                }
                TEMPLATE_LOOPY;
            }
            return;
        case DRAWMODE_JAM2:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        if (mask == 0xFF || format != RTGFMT_8BIT) {
                            SET_RTG_PIXELS2_COND(&dptr[xs << format], fg_color[format], bg_color[format], format);
                        }
                        else {
                            SET_RTG_PIXELS2_COND_MASK(&dptr[xs << format], fg_color[format], bg_color[format], format);
                        }

                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (mask == 0xFF || format != RTGFMT_8BIT) {
                                SET_RTG_PIXEL(&dptr[xs << format], (cur_byte & cur_bit) ? fg_color[format] : bg_color[format], format);
                            }
                            else {
                                SET_RTG_PIXEL_MASK(&dptr[xs << format], (cur_byte & cur_bit) ? fg_color[format] : bg_color[format], format);
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    TEMPLATE_LOOPX;
                }
                TEMPLATE_LOOPY;
            }
            return;
        case DRAWMODE_COMPLEMENT:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        INVERT_RTG_PIXELS(&dptr[xs << format], format)
                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (cur_byte & cur_bit) {
                                INVERT_RTG_PIXEL(&dptr[xs << format], format)
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    TEMPLATE_LOOPX;
                }
                TEMPLATE_LOOPY;
            }
            return;
    }
}

void rtg_blitpattern(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t format, uint16_t offset_x, uint16_t offset_y, uint8_t mask, uint8_t draw_mode, uint8_t loop_rows) {
    if (mask) {}

    uint8_t *dptr = &rtg_mem[rtg_address_adj[1] + (x << format) + (y * pitch)];
    uint8_t *sptr = NULL, *sptr_base = NULL;
    uint8_t cur_bit = 0, base_bit = 0, cur_byte = 0;
    uint8_t invert = (draw_mode & DRAWMODE_INVERSVID);
    uint16_t tmpl_x = 0;

    draw_mode &= 0x03;

	tmpl_x = (offset_x / 8) % 2;
    cur_bit = base_bit = (0x80 >> (offset_x % 8));

    uint32_t fg_color[3] = {
        (fgcol & 0xFF),
        htobe16((fgcol & 0xFFFF)),
        htobe32(fgcol),
    };
    uint32_t bg_color[3] = {
        (bgcol & 0xFF),
        htobe16((bgcol & 0xFFFF)),
        htobe32(bgcol),
    };


    if (src_addr >= (PIGFX_RTG_BASE + PIGFX_REG_SIZE))
        sptr = &rtg_mem[src_addr - (PIGFX_RTG_BASE + PIGFX_REG_SIZE)];
    else {
        int i = get_mapped_item_by_address(cfg, src_addr);
        if (i != -1) {
            sptr = &cfg->map_data[i][src_addr - cfg->map_offset[i]];
        }
        else {
            printf("BlitPattern: Failed to find mapped range for address %.8X\n", src_addr);
            return;
        }
    }

    sptr_base = sptr;
    sptr += (offset_y % loop_rows) * 2;

    switch (draw_mode) {
        case DRAWMODE_JAM1:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        if (mask == 0xFF || format != RTGFMT_8BIT) {
                            SET_RTG_PIXELS(&dptr[xs << format], fg_color[format], format);
                        }
                        else {
                            SET_RTG_PIXELS_MASK(&dptr[xs], fg_color[format], format);
                        }
                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (cur_byte & cur_bit) {
                                if (mask == 0xFF || format != RTGFMT_8BIT) {
                                    SET_RTG_PIXEL(&dptr[xs << format], fg_color[format], format);
                                }
                                else {
                                    SET_RTG_PIXEL_MASK(&dptr[xs], fg_color[format], format);
                                }
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    PATTERN_LOOPX;
                }
                PATTERN_LOOPY;
            }
            return;
        case DRAWMODE_JAM2:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        if (mask == 0xFF || format != RTGFMT_8BIT) {
                            SET_RTG_PIXELS2_COND(&dptr[xs << format], fg_color[format], bg_color[format], format);
                        }
                        else {
                            SET_RTG_PIXELS2_COND_MASK(&dptr[xs << format], fg_color[format], bg_color[format], format);
                        }

                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (mask == 0xFF || format != RTGFMT_8BIT) {
                                SET_RTG_PIXEL(&dptr[xs << format], (cur_byte & cur_bit) ? fg_color[format] : bg_color[format], format);
                            }
                            else {
                                SET_RTG_PIXEL_MASK(&dptr[xs << format], (cur_byte & cur_bit) ? fg_color[format] : bg_color[format], format);
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    PATTERN_LOOPX;
                }
                PATTERN_LOOPY;
            }
            return;
        case DRAWMODE_COMPLEMENT:
            for (uint16_t ys = 0; ys < h; ys++) {
                cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x];

                for (int xs = 0; xs < w; xs++) {
                    if (w >= 8 && cur_bit == 0x80 && xs < w - 8) {
                        INVERT_RTG_PIXELS(&dptr[xs << format], format)
                        xs += 7;
                    }
                    else {
                        while (cur_bit > 0 && xs < w) {
                            if (cur_byte & cur_bit) {
                                INVERT_RTG_PIXEL(&dptr[xs << format], format)
                            }
                            xs++;
                            cur_bit >>= 1;
                        }
                        xs--;
                        cur_bit = 0x80;
                    }
                    PATTERN_LOOPX;
                }
                PATTERN_LOOPY;
            }
            return;
    }
}

void rtg_drawline_solid(int16_t x1_, int16_t y1_, int16_t x2_, int16_t y2_, uint16_t len, uint32_t fgcol, uint16_t pitch, uint16_t format) {
	int16_t x1 = x1_, y1 = y1_;
	int16_t x2 = x1_ + x2_, y2 = y1 + y2_;

    uint32_t fg_color[3] = {
        (fgcol & 0xFF),
        htobe16((fgcol & 0xFFFF)),
        htobe32(fgcol),
    };

    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (y1 * pitch)];

	int32_t line_step = pitch;
	int8_t x_step = 1;

	int16_t dx, dy, dx_abs, dy_abs, ix, iy, x = x1;

	if (x2 < x1)
		x_step = -1;
	if (y2 < y1)
		line_step = -pitch;

	dx = x2 - x1;
	dy = y2 - y1;
	dx_abs = abs(dx);
	dy_abs = abs(dy);
	ix = dy_abs >> 1;
	iy = dx_abs >> 1;

    SET_RTG_PIXEL(&dptr[x << format], fg_color[format], format);

	if (dx_abs >= dy_abs) {
		if (!len) len = dx_abs;
		for (uint16_t i = 0; i < len; i++) {
			iy += dy_abs;
			if (iy >= dx_abs) {
				iy -= dx_abs;
				dptr += line_step;
			}
			x += x_step;

            SET_RTG_PIXEL(&dptr[x << format], fg_color[format], format);
		}
	}
	else {
		if (!len) len = dy_abs;
		for (uint16_t i = 0; i < len; i++) {
			ix += dx_abs;
			if (ix >= dy_abs) {
				ix -= dy_abs;
				x += x_step;
			}
			dptr += line_step;

			SET_RTG_PIXEL(&dptr[x << format], fg_color[format], format);
		}
	}
}

#define DRAW_LINE_PIXEL \
    if (pattern & cur_bit) { \
        if (invert) { INVERT_RTG_PIXEL(&dptr[x << format], format) } \
        else { \
            if (mask == 0xFF || format != RTGFMT_8BIT) { SET_RTG_PIXEL(&dptr[x << format], fg_color[format], format); } \
            else { SET_RTG_PIXEL_MASK(&dptr[x << format], fg_color[format], format); } \
        } \
    } \
    else if (draw_mode == DRAWMODE_JAM2) { \
        if (invert) { INVERT_RTG_PIXEL(&dptr[x << format], format) } \
        else { \
            if (mask == 0xFF || format != RTGFMT_8BIT) { SET_RTG_PIXEL(&dptr[x << format], bg_color[format], format); } \
            else { SET_RTG_PIXEL_MASK(&dptr[x << format], bg_color[format], format); } \
        } \
    } \
    if ((cur_bit >>= 1) == 0) \
            cur_bit = 0x8000;

void rtg_drawline (int16_t x1_, int16_t y1_, int16_t x2_, int16_t y2_, uint16_t len, uint16_t pattern, uint16_t pattern_offset, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t format, uint8_t mask, uint8_t draw_mode) {
    if (pattern_offset) {}

 	int16_t x1 = x1_, y1 = y1_;
	int16_t x2 = x1_ + x2_, y2 = y1 + y2_;
    uint16_t cur_bit = 0x8000;
    //uint32_t color_mask = 0xFFFF0000;
    uint8_t invert = 0;

    uint32_t fg_color[3] = {
        (fgcol & 0xFF),
        htobe16((fgcol & 0xFFFF)),
        htobe32(fgcol),
    };
    uint32_t bg_color[3] = {
        (bgcol & 0xFF),
        htobe16((bgcol & 0xFFFF)),
        htobe32(bgcol),
    };

    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (y1 * pitch)];

	int32_t line_step = pitch;
	int8_t x_step = 1;

	int16_t dx, dy, dx_abs, dy_abs, ix, iy, x = x1;

	if (x2 < x1)
		x_step = -1;
	if (y2 < y1)
		line_step = -pitch;

	dx = x2 - x1;
	dy = y2 - y1;
	dx_abs = abs(dx);
	dy_abs = abs(dy);
	ix = dy_abs >> 1;
	iy = dx_abs >> 1;

    if (draw_mode & DRAWMODE_INVERSVID)
        pattern = ~pattern;
    if (draw_mode & DRAWMODE_COMPLEMENT) {
        invert = 1;
    }
    draw_mode &= 0x01;

    DRAW_LINE_PIXEL;

	if (dx_abs >= dy_abs) {
		if (!len) len = dx_abs;
		for (uint16_t i = 0; i < len; i++) {
			iy += dy_abs;
			if (iy >= dx_abs) {
				iy -= dx_abs;
				dptr += line_step;
			}
			x += x_step;

            DRAW_LINE_PIXEL;
		}
	}
	else {
		if (!len) len = dy_abs;
		for (uint16_t i = 0; i < len; i++) {
			ix += dx_abs;
			if (ix >= dy_abs) {
				ix -= dy_abs;
				x += x_step;
			}
			dptr += line_step;

			DRAW_LINE_PIXEL;
		}
	}
}

void rtg_p2c (int16_t sx, int16_t sy, int16_t dx, int16_t dy, int16_t w, int16_t h, uint8_t draw_mode, uint8_t planes, uint8_t mask, uint8_t layer_mask, uint16_t src_line_pitch, uint8_t *bmp_data_src) {
    uint16_t pitch = rtg_x[3];
    uint8_t *dptr = &rtg_mem[rtg_address_adj[0] + (dy * pitch)];

	uint8_t cur_bit, base_bit, base_byte;
	uint16_t cur_byte = 0, u8_fg = 0;
    //uint32_t color_mask = 0xFFFFFFFF;

	uint32_t plane_size = src_line_pitch * h;
	uint8_t *bmp_data = bmp_data_src;

	cur_bit = base_bit = (0x80 >> (sx % 8));
	cur_byte = base_byte = ((sx / 8) % src_line_pitch);

    if (realtime_graphics_debug) {
        printf("P2C: %d,%d - %d,%d (%dx%d) %d, %.2X\n", sx, sy, dx, dy, w, h, planes, layer_mask);
        printf("Mask: %.2X Minterm: %.2X\n", mask, draw_mode);
        printf("Pitch: %d Src Pitch: %d (!!!: %.4X)\n", pitch, src_line_pitch, rtg_user[0]);
        printf("Curbyte: %d Curbit: %d\n", cur_byte, cur_bit);
        printf("Plane size: %d Total size: %d (%X)\n", plane_size, plane_size * planes, plane_size * planes);
        printf("Source: %.8X - %.8X\n", rtg_address[1], rtg_address_adj[1]);
        printf("Target: %.8X - %.8X\n", rtg_address[0], rtg_address_adj[0]);
        fflush(stdout);

        printf("Grabbing data from RTG memory.\nData:\n");
        for (int i = 0; i < h; i++) {
            for (int k = 0; k < planes; k++) {
                for (int j = 0; j < src_line_pitch; j++) {
                    printf("%.2X", bmp_data_src[j + (i * src_line_pitch) + (plane_size * k)]);
                }
                printf("  ");
            }
            printf("\n");
        }
    }

	for (int16_t line_y = 0; line_y < h; line_y++) {
		for (int16_t x = dx; x < dx + w; x++) {
			u8_fg = 0;
			if (draw_mode & 0x01) {
				DECODE_INVERTED_PLANAR_PIXEL(u8_fg)
            }
			else {
				DECODE_PLANAR_PIXEL(u8_fg)
            }

			if (mask == 0xFF && (draw_mode == MINTERM_SRC || draw_mode == MINTERM_NOTSRC)) {
				dptr[x] = u8_fg;
				goto skip;
			}

            HANDLE_MINTERM_PIXEL(u8_fg, dptr[x], rtg_display_format);

			skip:;
			if ((cur_bit >>= 1) == 0) {
				cur_bit = 0x80;
				cur_byte++;
				cur_byte %= src_line_pitch;
			}

		}
		dptr += pitch;
		if ((line_y + sy + 1) % h)
			bmp_data += src_line_pitch;
		else
			bmp_data = bmp_data_src;
		cur_bit = base_bit;
		cur_byte = base_byte;
	}
}
