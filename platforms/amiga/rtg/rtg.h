#define PIGFX_RTG_BASE 0x70000000
#define PIGFX_RTG_SIZE 0x02000000

#define PIGFX_REG_SIZE 0x00010000

#define CARD_OFFSET 0

#include "rtg_driver_amiga/rtg_enums.h"

void rtg_write(uint32_t address, uint32_t value, uint8_t mode);
unsigned int rtg_read(uint32_t address, uint8_t mode);
void rtg_set_clut_entry(uint8_t index, uint32_t xrgb);
void rtg_init_display();
void rtg_shutdown_display();

void rtg_fillrect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_blitrect(uint16_t x, uint16_t y, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t pitch, uint16_t format, uint8_t mask);
void rtg_blitrect_nomask_complete(uint16_t sx, uint16_t sy, uint16_t dx, uint16_t dy, uint16_t w, uint16_t h, uint16_t srcpitch, uint16_t dstpitch, uint32_t src_addr, uint32_t dst_addr, uint16_t format, uint8_t minterm);
void rtg_blittemplate(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t t_pitch, uint16_t format, uint16_t offset_x, uint8_t mask, uint8_t draw_mode);
void rtg_blitpattern(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t src_addr, uint32_t fgcol, uint32_t bgcol, uint16_t pitch, uint16_t format, uint16_t offset_x, uint16_t offset_y, uint8_t mask, uint8_t draw_mode, uint8_t loop_rows);

#define PATTERN_LOOPX \
    tmpl_x ^= 0x01; \
    cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x]; \

#define PATTERN_LOOPY \
	sptr += 2 ; \
	if ((ys + offset_y + 1) % loop_rows == 0) \
		sptr = sptr_base; \
	tmpl_x = (offset_x / 8) % 2; \
	cur_bit = base_bit; \
	dptr += pitch;

#define TEMPLATE_LOOPX \
    tmpl_x++; \
    cur_byte = (invert) ? sptr[tmpl_x] ^ 0xFF : sptr[tmpl_x]; \

#define TEMPLATE_LOOPY \
    sptr += t_pitch; \
    dptr += pitch; \
    tmpl_x = offset_x / 8; \
    cur_bit = base_bit;

#define SET_RTG_PIXELS(a, b, c, d) \
    switch (c) { \
        case RTGFMT_8BIT: \
            if (cur_byte & 0x80) a[b + 0] = d[c]; \
            if (cur_byte & 0x40) a[b + 1] = d[c]; \
            if (cur_byte & 0x20) a[b + 2] = d[c]; \
            if (cur_byte & 0x10) a[b + 3] = d[c]; \
            if (cur_byte & 0x08) a[b + 4] = d[c]; \
            if (cur_byte & 0x04) a[b + 5] = d[c]; \
            if (cur_byte & 0x02) a[b + 6] = d[c]; \
            if (cur_byte & 0x01) a[b + 7] = d[c]; \
            break; \
        case RTGFMT_RBG565: \
            if (cur_byte & 0x80) ((uint16_t *)a)[b + 0] = d[c]; \
            if (cur_byte & 0x40) ((uint16_t *)a)[b + 1] = d[c]; \
            if (cur_byte & 0x20) ((uint16_t *)a)[b + 2] = d[c]; \
            if (cur_byte & 0x10) ((uint16_t *)a)[b + 3] = d[c]; \
            if (cur_byte & 0x08) ((uint16_t *)a)[b + 4] = d[c]; \
            if (cur_byte & 0x04) ((uint16_t *)a)[b + 5] = d[c]; \
            if (cur_byte & 0x02) ((uint16_t *)a)[b + 6] = d[c]; \
            if (cur_byte & 0x01) ((uint16_t *)a)[b + 7] = d[c]; \
            break; \
        case RTGFMT_RGB32: \
            if (cur_byte & 0x80) ((uint32_t *)a)[b + 0] = d[c]; \
            if (cur_byte & 0x40) ((uint32_t *)a)[b + 1] = d[c]; \
            if (cur_byte & 0x20) ((uint32_t *)a)[b + 2] = d[c]; \
            if (cur_byte & 0x10) ((uint32_t *)a)[b + 3] = d[c]; \
            if (cur_byte & 0x08) ((uint32_t *)a)[b + 4] = d[c]; \
            if (cur_byte & 0x04) ((uint32_t *)a)[b + 5] = d[c]; \
            if (cur_byte & 0x02) ((uint32_t *)a)[b + 6] = d[c]; \
            if (cur_byte & 0x01) ((uint32_t *)a)[b + 7] = d[c]; \
            break; \
    }

#define SET_RTG_PIXELS_COND(a, b, c, d, e) \
    switch (c) { \
        case RTGFMT_8BIT: \
            a[b + 0] = (cur_byte & 0x80) ? d[c] : e[c]; \
            a[b + 1] = (cur_byte & 0x40) ? d[c] : e[c]; \
            a[b + 2] = (cur_byte & 0x20) ? d[c] : e[c]; \
            a[b + 3] = (cur_byte & 0x10) ? d[c] : e[c]; \
            a[b + 4] = (cur_byte & 0x08) ? d[c] : e[c]; \
            a[b + 5] = (cur_byte & 0x04) ? d[c] : e[c]; \
            a[b + 6] = (cur_byte & 0x02) ? d[c] : e[c]; \
            a[b + 7] = (cur_byte & 0x01) ? d[c] : e[c]; \
            break; \
        case RTGFMT_RBG565: \
            ((uint16_t *)a)[b + 0] = (cur_byte & 0x80) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 1] = (cur_byte & 0x40) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 2] = (cur_byte & 0x20) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 3] = (cur_byte & 0x10) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 4] = (cur_byte & 0x08) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 5] = (cur_byte & 0x04) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 6] = (cur_byte & 0x02) ? d[c] : e[c]; \
            ((uint16_t *)a)[b + 7] = (cur_byte & 0x01) ? d[c] : e[c]; \
            break; \
        case RTGFMT_RGB32: \
            ((uint32_t *)a)[b + 0] = (cur_byte & 0x80) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 1] = (cur_byte & 0x40) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 2] = (cur_byte & 0x20) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 3] = (cur_byte & 0x10) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 4] = (cur_byte & 0x08) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 5] = (cur_byte & 0x04) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 6] = (cur_byte & 0x02) ? d[c] : e[c]; \
            ((uint32_t *)a)[b + 7] = (cur_byte & 0x01) ? d[c] : e[c]; \
            break; \
    }

#define INVERT_RTG_PIXELS(a, b, c) \
    switch (c) { \
        case RTGFMT_8BIT: \
            if (cur_byte & 0x80) a[b + 0] = ~a[b + 0]; \
            if (cur_byte & 0x40) a[b + 1] = ~a[b + 1]; \
            if (cur_byte & 0x20) a[b + 2] = ~a[b + 2]; \
            if (cur_byte & 0x10) a[b + 3] = ~a[b + 3]; \
            if (cur_byte & 0x08) a[b + 4] = ~a[b + 4]; \
            if (cur_byte & 0x04) a[b + 5] = ~a[b + 5]; \
            if (cur_byte & 0x02) a[b + 6] = ~a[b + 6]; \
            if (cur_byte & 0x01) a[b + 7] = ~a[b + 7]; \
            break; \
        case RTGFMT_RBG565: \
            if (cur_byte & 0x80) ((uint16_t *)a)[b + 0] = ~((uint16_t *)a)[b + 0]; \
            if (cur_byte & 0x40) ((uint16_t *)a)[b + 1] = ~((uint16_t *)a)[b + 1]; \
            if (cur_byte & 0x20) ((uint16_t *)a)[b + 2] = ~((uint16_t *)a)[b + 2]; \
            if (cur_byte & 0x10) ((uint16_t *)a)[b + 3] = ~((uint16_t *)a)[b + 3]; \
            if (cur_byte & 0x08) ((uint16_t *)a)[b + 4] = ~((uint16_t *)a)[b + 4]; \
            if (cur_byte & 0x04) ((uint16_t *)a)[b + 5] = ~((uint16_t *)a)[b + 5]; \
            if (cur_byte & 0x02) ((uint16_t *)a)[b + 6] = ~((uint16_t *)a)[b + 6]; \
            if (cur_byte & 0x01) ((uint16_t *)a)[b + 7] = ~((uint16_t *)a)[b + 7]; \
            break; \
        case RTGFMT_RGB32: \
            if (cur_byte & 0x80) ((uint32_t *)a)[b + 0] = ~((uint32_t *)a)[b + 0]; \
            if (cur_byte & 0x40) ((uint32_t *)a)[b + 1] = ~((uint32_t *)a)[b + 1]; \
            if (cur_byte & 0x20) ((uint32_t *)a)[b + 2] = ~((uint32_t *)a)[b + 2]; \
            if (cur_byte & 0x10) ((uint32_t *)a)[b + 3] = ~((uint32_t *)a)[b + 3]; \
            if (cur_byte & 0x08) ((uint32_t *)a)[b + 4] = ~((uint32_t *)a)[b + 4]; \
            if (cur_byte & 0x04) ((uint32_t *)a)[b + 5] = ~((uint32_t *)a)[b + 5]; \
            if (cur_byte & 0x02) ((uint32_t *)a)[b + 6] = ~((uint32_t *)a)[b + 6]; \
            if (cur_byte & 0x01) ((uint32_t *)a)[b + 7] = ~((uint32_t *)a)[b + 7]; \
            break; \
    }

#define SET_RTG_PIXEL(a, b, c, d) \
    switch (c) { \
        case RTGFMT_8BIT: \
            a[b] = d[c]; \
            break; \
        case RTGFMT_RBG565: \
            ((uint16_t *)a)[b] = d[c]; \
            break; \
        case RTGFMT_RGB32: \
            ((uint32_t *)a)[b] = d[c]; \
            break; \
    }

#define INVERT_RTG_PIXEL(a, b, c) \
    switch (c) { \
        case RTGFMT_8BIT: \
            a[b] = ~a[c]; \
            break; \
        case RTGFMT_RBG565: \
            ((uint16_t *)a)[b] = ~((uint16_t *)a)[b]; \
            break; \
        case RTGFMT_RGB32: \
            ((uint32_t *)a)[b] = ~((uint32_t *)a)[b]; \
            break; \
    }
