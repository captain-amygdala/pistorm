// SPDX-License-Identifier: MIT

#define PIGFX_RTG_BASE     0x70000000
#define PIGFX_REG_SIZE     0x00010000
#define PIGFX_RTG_SIZE     0x02000000
#define PIGFX_SCRATCH_SIZE 0x00800000
#define PIGFX_SCRATCH_AREA 0x72010000
#define PIGFX_UPPER        0x72810000

#define CARD_OFFSET 0

#include "rtg_enums.h"

void rtg_write(uint32_t address, uint32_t value, uint8_t mode);
unsigned int rtg_read(uint32_t address, uint8_t mode);
void rtg_set_clut_entry(uint8_t index, uint32_t xrgb);
void rtg_init_display();
void rtg_shutdown_display();
void rtg_enable_mouse_cursor();

unsigned int rtg_get_fb();
void rtg_set_mouse_cursor_pos(int16_t x, int16_t y);
void rtg_set_cursor_clut_entry(uint8_t r, uint8_t g, uint8_t b, uint8_t idx);
void rtg_set_mouse_cursor_image(uint8_t *src, uint8_t w, uint8_t h);

void rtg_show_fps(uint8_t enable);
void rtg_palette_debug(uint8_t enable);

int init_rtg_data(struct emulator_config *cfg);
void shutdown_rtg();

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_fillrect_solid(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format);
void rtg_invertrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_blitrect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_blitrect_solid(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format);
void rtg_blitrect_nomask_complete(uint16_t sx, uint16_t sy, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t srcpitch, uint16_t dstpitch, uint32_t src_addr, uint32_t dst_addr, uint16_t format, uint8_t minterm);
void rtg_blittemplate(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t t_pitch, uint16_t format, uint16_t offset_x, uint8_t mask, uint8_t draw_mode);
void rtg_blitpattern(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t format, uint16_t offset_x, uint16_t offset_y, uint8_t mask, uint8_t draw_mode, uint8_t loop_rows);
void rtg_drawline_solid(int16_t x1_, int16_t y1_, int16_t x2_, int16_t y2_, uint16_t len, uint32_t fgcol, uint16_t pitch, uint16_t format);
void rtg_drawline (int16_t x1_, int16_t y1_, int16_t x2_, int16_t y2_, uint16_t len, uint16_t pattern, uint16_t pattern_offset, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t format, uint8_t mask, uint8_t draw_mode);

void rtg_p2c (int16_t sx, int16_t sy, int16_t dx, int16_t dy, int16_t w, int16_t h, uint8_t draw_mode, uint8_t planes, uint8_t mask, uint8_t layer_mask, uint16_t src_line_pitch, uint8_t *bmp_data_src);
void rtg_p2d (int16_t sx, int16_t sy, int16_t dx, int16_t dy, int16_t w, int16_t h, uint8_t draw_mode, uint8_t planes, uint8_t mask, uint8_t layer_mask, uint16_t src_line_pitch, uint8_t *bmp_data_src);

#define PATTERN_LOOPX \
    if (sptr)   { cur_byte = sptr[tmpl_x]; } \
    else        { cur_byte = m68k_read_memory_8(src_addr + tmpl_x); } \
    if (invert) { cur_byte ^= 0xFF; } \
    tmpl_x ^= 0x01;

#define PATTERN_LOOPY \
	sptr += 2 ; \
    src_addr += 2; \
	if ((ys + offset_y + 1) % loop_rows == 0) { \
		if (sptr) sptr = sptr_base; \
        src_addr = src_addr_base; \
    } \
	tmpl_x = (offset_x / 8) % 2; \
	cur_bit = base_bit; \
	dptr += pitch;

#define TEMPLATE_LOOPX \
    if (sptr)   { cur_byte = sptr[tmpl_x]; } \
    else        { cur_byte = m68k_read_memory_8(src_addr + tmpl_x); } \
    if (invert) { cur_byte ^= 0xFF; } \
    tmpl_x++;

#define TEMPLATE_LOOPY \
    if (sptr) sptr += t_pitch; \
    src_addr += t_pitch; \
    dptr += pitch; \
    tmpl_x = offset_x / 8; \
    cur_bit = base_bit;

#define INVERT_RTG_PIXELS(dest, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            if (cur_byte & 0x80) (dest)[0] ^= mask; \
            if (cur_byte & 0x40) (dest)[1] ^= mask; \
            if (cur_byte & 0x20) (dest)[2] ^= mask; \
            if (cur_byte & 0x10) (dest)[3] ^= mask; \
            if (cur_byte & 0x08) (dest)[4] ^= mask; \
            if (cur_byte & 0x04) (dest)[5] ^= mask; \
            if (cur_byte & 0x02) (dest)[6] ^= mask; \
            if (cur_byte & 0x01) (dest)[7] ^= mask; \
            break; \
        case RTGFMT_RBG565: \
            if (cur_byte & 0x80) ((uint16_t *)dest)[0] = ~((uint16_t *)dest)[0]; \
            if (cur_byte & 0x40) ((uint16_t *)dest)[1] = ~((uint16_t *)dest)[1]; \
            if (cur_byte & 0x20) ((uint16_t *)dest)[2] = ~((uint16_t *)dest)[2]; \
            if (cur_byte & 0x10) ((uint16_t *)dest)[3] = ~((uint16_t *)dest)[3]; \
            if (cur_byte & 0x08) ((uint16_t *)dest)[4] = ~((uint16_t *)dest)[4]; \
            if (cur_byte & 0x04) ((uint16_t *)dest)[5] = ~((uint16_t *)dest)[5]; \
            if (cur_byte & 0x02) ((uint16_t *)dest)[6] = ~((uint16_t *)dest)[6]; \
            if (cur_byte & 0x01) ((uint16_t *)dest)[7] = ~((uint16_t *)dest)[7]; \
            break; \
        case RTGFMT_RGB32: \
            if (cur_byte & 0x80) ((uint32_t *)dest)[0] = ~((uint32_t *)dest)[0]; \
            if (cur_byte & 0x40) ((uint32_t *)dest)[1] = ~((uint32_t *)dest)[1]; \
            if (cur_byte & 0x20) ((uint32_t *)dest)[2] = ~((uint32_t *)dest)[2]; \
            if (cur_byte & 0x10) ((uint32_t *)dest)[3] = ~((uint32_t *)dest)[3]; \
            if (cur_byte & 0x08) ((uint32_t *)dest)[4] = ~((uint32_t *)dest)[4]; \
            if (cur_byte & 0x04) ((uint32_t *)dest)[5] = ~((uint32_t *)dest)[5]; \
            if (cur_byte & 0x02) ((uint32_t *)dest)[6] = ~((uint32_t *)dest)[6]; \
            if (cur_byte & 0x01) ((uint32_t *)dest)[7] = ~((uint32_t *)dest)[7]; \
            break; \
    }

#define SET_RTG_PIXELS_MASK(dest, src, format) \
    if (cur_byte & 0x80) (dest)[0] = src ^ ((dest)[0] & ~mask); \
    if (cur_byte & 0x40) (dest)[1] = src ^ ((dest)[1] & ~mask); \
    if (cur_byte & 0x20) (dest)[2] = src ^ ((dest)[2] & ~mask); \
    if (cur_byte & 0x10) (dest)[3] = src ^ ((dest)[3] & ~mask); \
    if (cur_byte & 0x08) (dest)[4] = src ^ ((dest)[4] & ~mask); \
    if (cur_byte & 0x04) (dest)[5] = src ^ ((dest)[5] & ~mask); \
    if (cur_byte & 0x02) (dest)[6] = src ^ ((dest)[6] & ~mask); \
    if (cur_byte & 0x01) (dest)[7] = src ^ ((dest)[7] & ~mask); \

#define SET_RTG_PIXELS2_COND_MASK(dest, src, src2, format) \
    (dest)[0] = (cur_byte & 0x80) ? src : src2 ^ ((dest)[0] & ~mask); \
    (dest)[1] = (cur_byte & 0x40) ? src : src2 ^ ((dest)[1] & ~mask); \
    (dest)[2] = (cur_byte & 0x20) ? src : src2 ^ ((dest)[2] & ~mask); \
    (dest)[3] = (cur_byte & 0x10) ? src : src2 ^ ((dest)[3] & ~mask); \
    (dest)[4] = (cur_byte & 0x08) ? src : src2 ^ ((dest)[4] & ~mask); \
    (dest)[5] = (cur_byte & 0x04) ? src : src2 ^ ((dest)[5] & ~mask); \
    (dest)[6] = (cur_byte & 0x02) ? src : src2 ^ ((dest)[6] & ~mask); \
    (dest)[7] = (cur_byte & 0x01) ? src : src2 ^ ((dest)[7] & ~mask); \


#define SET_RTG_PIXELS(dest, src, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            if (cur_byte & 0x80) (dest)[0] = src; \
            if (cur_byte & 0x40) (dest)[1] = src; \
            if (cur_byte & 0x20) (dest)[2] = src; \
            if (cur_byte & 0x10) (dest)[3] = src; \
            if (cur_byte & 0x08) (dest)[4] = src; \
            if (cur_byte & 0x04) (dest)[5] = src; \
            if (cur_byte & 0x02) (dest)[6] = src; \
            if (cur_byte & 0x01) (dest)[7] = src; \
            break; \
        case RTGFMT_RBG565: \
            if (cur_byte & 0x80) ((uint16_t *)dest)[0] = src; \
            if (cur_byte & 0x40) ((uint16_t *)dest)[1] = src; \
            if (cur_byte & 0x20) ((uint16_t *)dest)[2] = src; \
            if (cur_byte & 0x10) ((uint16_t *)dest)[3] = src; \
            if (cur_byte & 0x08) ((uint16_t *)dest)[4] = src; \
            if (cur_byte & 0x04) ((uint16_t *)dest)[5] = src; \
            if (cur_byte & 0x02) ((uint16_t *)dest)[6] = src; \
            if (cur_byte & 0x01) ((uint16_t *)dest)[7] = src; \
            break; \
        case RTGFMT_RGB32: \
            if (cur_byte & 0x80) ((uint32_t *)dest)[0] = src; \
            if (cur_byte & 0x40) ((uint32_t *)dest)[1] = src; \
            if (cur_byte & 0x20) ((uint32_t *)dest)[2] = src; \
            if (cur_byte & 0x10) ((uint32_t *)dest)[3] = src; \
            if (cur_byte & 0x08) ((uint32_t *)dest)[4] = src; \
            if (cur_byte & 0x04) ((uint32_t *)dest)[5] = src; \
            if (cur_byte & 0x02) ((uint32_t *)dest)[6] = src; \
            if (cur_byte & 0x01) ((uint32_t *)dest)[7] = src; \
            break; \
    }

#define SET_RTG_PIXELS2_COND(dest, src, src2, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            (dest)[0] = (cur_byte & 0x80) ? src : src2; \
            (dest)[1] = (cur_byte & 0x40) ? src : src2; \
            (dest)[2] = (cur_byte & 0x20) ? src : src2; \
            (dest)[3] = (cur_byte & 0x10) ? src : src2; \
            (dest)[4] = (cur_byte & 0x08) ? src : src2; \
            (dest)[5] = (cur_byte & 0x04) ? src : src2; \
            (dest)[6] = (cur_byte & 0x02) ? src : src2; \
            (dest)[7] = (cur_byte & 0x01) ? src : src2; \
            break; \
        case RTGFMT_RBG565: \
            ((uint16_t *)dest)[0] = (cur_byte & 0x80) ? src : src2; \
            ((uint16_t *)dest)[1] = (cur_byte & 0x40) ? src : src2; \
            ((uint16_t *)dest)[2] = (cur_byte & 0x20) ? src : src2; \
            ((uint16_t *)dest)[3] = (cur_byte & 0x10) ? src : src2; \
            ((uint16_t *)dest)[4] = (cur_byte & 0x08) ? src : src2; \
            ((uint16_t *)dest)[5] = (cur_byte & 0x04) ? src : src2; \
            ((uint16_t *)dest)[6] = (cur_byte & 0x02) ? src : src2; \
            ((uint16_t *)dest)[7] = (cur_byte & 0x01) ? src : src2; \
            break; \
        case RTGFMT_RGB32: \
            ((uint32_t *)dest)[0] = (cur_byte & 0x80) ? src : src2; \
            ((uint32_t *)dest)[1] = (cur_byte & 0x40) ? src : src2; \
            ((uint32_t *)dest)[2] = (cur_byte & 0x20) ? src : src2; \
            ((uint32_t *)dest)[3] = (cur_byte & 0x10) ? src : src2; \
            ((uint32_t *)dest)[4] = (cur_byte & 0x08) ? src : src2; \
            ((uint32_t *)dest)[5] = (cur_byte & 0x04) ? src : src2; \
            ((uint32_t *)dest)[6] = (cur_byte & 0x02) ? src : src2; \
            ((uint32_t *)dest)[7] = (cur_byte & 0x01) ? src : src2; \
            break; \
    }



#define SET_RTG_PIXEL(dest, src, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            *(dest) = src; \
            break; \
        case RTGFMT_RBG565: \
            *((uint16_t *)dest) = src; \
            break; \
        case RTGFMT_RGB32: \
            *((uint32_t *)dest) = src; \
            break; \
    }

#define SET_RTG_PIXEL_MASK(dest, src, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            *(dest) = src ^ (*(dest) & ~mask); \
            break; \
        case RTGFMT_RBG565: \
            *((uint16_t *)dest) = src; \
            break; \
        case RTGFMT_RGB32: \
            *((uint32_t *)dest) = src; \
            break; \
    }

#define INVERT_RTG_PIXEL(dest, format) \
    switch (format) { \
        case RTGFMT_8BIT: \
            *(dest) ^= mask; \
            break; \
        case RTGFMT_RBG565: \
            *((uint16_t *)dest) = ~*((uint16_t *)dest); \
            break; \
        case RTGFMT_RGB32: \
            *((uint32_t *)dest) = ~*((uint32_t *)dest); \
            break; \
    }

#define HANDLE_MINTERM_PIXEL(s, d, f) \
      switch(draw_mode) {\
            case MINTERM_NOR: \
                  s &= ~(d); \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_ONLYDST: \
                  d = d & ~(s); break; \
            case MINTERM_NOTSRC: \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_ONLYSRC: \
                  s &= (d ^ 0xFF); \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_INVERT: \
                  d ^= 0xFF; break; \
            case MINTERM_EOR: \
                  d ^= s; break; \
            case MINTERM_NAND: \
                  s = ~(d & ~(s)) & mask; \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_AND: \
                  s &= d; \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_NEOR: \
                  d ^= (s & mask); break; \
            case MINTERM_DST: /* This one does nothing. */ \
                  return; break; \
            case MINTERM_NOTONLYSRC: \
                  d |= (s & mask); break; \
            case MINTERM_SRC: \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_NOTONLYDST: \
                  s = ~(d & s) & mask; \
            SET_RTG_PIXEL_MASK(&d, s, f); break; \
            case MINTERM_OR: \
                  d |= (s & mask); break; \
      }


#define DECODE_PLANAR_PIXEL(a) \
	switch (planes) { \
		case 8: if (layer_mask & 0x80 && bmp_data[(plane_size * 7) + cur_byte] & cur_bit) a |= 0x80; \
        /* Fallthrough */ \
		case 7: if (layer_mask & 0x40 && bmp_data[(plane_size * 6) + cur_byte] & cur_bit) a |= 0x40; \
        /* Fallthrough */ \
		case 6: if (layer_mask & 0x20 && bmp_data[(plane_size * 5) + cur_byte] & cur_bit) a |= 0x20; \
        /* Fallthrough */ \
		case 5: if (layer_mask & 0x10 && bmp_data[(plane_size * 4) + cur_byte] & cur_bit) a |= 0x10; \
        /* Fallthrough */ \
		case 4: if (layer_mask & 0x08 && bmp_data[(plane_size * 3) + cur_byte] & cur_bit) a |= 0x08; \
        /* Fallthrough */ \
		case 3: if (layer_mask & 0x04 && bmp_data[(plane_size * 2) + cur_byte] & cur_bit) a |= 0x04; \
        /* Fallthrough */ \
		case 2: if (layer_mask & 0x02 && bmp_data[plane_size + cur_byte] & cur_bit) a |= 0x02; \
        /* Fallthrough */ \
		case 1: if (layer_mask & 0x01 && bmp_data[cur_byte] & cur_bit) a |= 0x01; \
			break; \
	}

#define DECODE_INVERTED_PLANAR_PIXEL(a) \
	switch (planes) { \
		case 8: if (layer_mask & 0x80 && (bmp_data[(plane_size * 7) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x80; \
        /* Fallthrough */ \
		case 7: if (layer_mask & 0x40 && (bmp_data[(plane_size * 6) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x40; \
        /* Fallthrough */ \
		case 6: if (layer_mask & 0x20 && (bmp_data[(plane_size * 5) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x20; \
        /* Fallthrough */ \
		case 5: if (layer_mask & 0x10 && (bmp_data[(plane_size * 4) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x10; \
        /* Fallthrough */ \
		case 4: if (layer_mask & 0x08 && (bmp_data[(plane_size * 3) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x08; \
        /* Fallthrough */ \
		case 3: if (layer_mask & 0x04 && (bmp_data[(plane_size * 2) + cur_byte] ^ 0xFF) & cur_bit) a |= 0x04; \
        /* Fallthrough */ \
		case 2: if (layer_mask & 0x02 && (bmp_data[plane_size + cur_byte] ^ 0xFF) & cur_bit) a |= 0x02; \
        /* Fallthrough */ \
		case 1: if (layer_mask & 0x01 && (bmp_data[cur_byte] ^ 0xFF) & cur_bit) a |= 0x01; \
			break; \
	}
