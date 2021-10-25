// SPDX-License-Identifier: MIT

#include "config_file/config_file.h"
#include "platforms/amiga/pistorm-dev/pistorm-dev-enums.h"
#include "emulator.h"
#include "rtg.h"

#include "raylib/raylib.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "interface/vmcs_host/vc_tvservice.h"

#define RTG_INIT_ERR(a) { printf(a); *data->running = 0; }

//#define DEBUG_RAYLIB_RTG

#ifdef DEBUG_RAYLIB_RTG
#define DEBUG printf
#else
#define DEBUG(...)
#endif

uint8_t busy = 0, rtg_on = 0, rtg_initialized = 0, emulator_exiting = 0, rtg_output_in_vblank = 0, rtg_dpms = 0, shutdown = 0;
extern uint8_t *rtg_mem, display_enabled;
extern uint32_t framebuffer_addr;
extern uint32_t framebuffer_addr_adj;

extern uint16_t rtg_display_width, rtg_display_height;
extern uint16_t rtg_display_format;
extern uint16_t rtg_pitch, rtg_total_rows;
extern uint16_t rtg_offset_x, rtg_offset_y;

uint32_t cur_rtg_frame = 0;

static pthread_t thread_id;
static uint8_t mouse_cursor_enabled = 0, cursor_image_updated = 0;
static uint8_t clut_cursor_enabled = 0, clut_image_updated = 0;
static uint8_t updating_screen = 0, debug_palette = 0, show_fps = 0, palette_updated = 0;
static uint8_t mouse_cursor_w = 16, mouse_cursor_h = 16;
static int16_t mouse_cursor_x = 0, mouse_cursor_y = 0;
static int16_t mouse_cursor_x_adj = 0, mouse_cursor_y_adj = 0;

static int32_t pi_screen_width = 1280, pi_screen_height = 720;

struct rtg_shared_data {
    uint16_t *width, *height;
    uint16_t *format, *pitch;
    uint16_t *offset_x, *offset_y;
    volatile uint8_t *memory;
    uint32_t *addr;
    uint8_t *running;
};

float scale_x = 1.0f, scale_y = 1.0f;
static Rectangle srcrect, dstscale;
static Vector2 origin;
static uint8_t scale_mode = PIGFX_SCALE_FULL_ASPECT;
static uint8_t filter_mode = 0;

struct rtg_shared_data rtg_share_data;
static uint32_t palette[256];
static uint32_t cursor_palette[256];

uint32_t cursor_data[256 * 256];
uint32_t clut_cursor_texture_data[256 * 256];

void rtg_update_screen() {}

uint32_t rtg_to_raylib[RTGFMT_NUM] = {
    PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
    PIXELFORMAT_UNCOMPRESSED_R5G6B5,
    PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
    PIXELFORMAT_UNCOMPRESSED_R5G5B5A1,
};

uint32_t rtg_pixel_size[RTGFMT_NUM] = {
    1,
    2,
    4,
    2,
};

void rtg_scale_output(uint16_t width, uint16_t height) {
    static uint8_t fit_to_screen = 1, center = 1;
    srcrect.x = srcrect.y = 0;
    srcrect.width = width;
    srcrect.height = height;
    if (scale_mode != PIGFX_SCALE_CUSTOM && scale_mode != PIGFX_SCALE_CUSTOM_RECT) {
        dstscale.x = dstscale.y = 0;
        dstscale.width = width;
        dstscale.height = height;
        mouse_cursor_x_adj = 0;
        mouse_cursor_y_adj = 0;
    }

    if (dstscale.width == 0.0f || dstscale.height == 0.0f) {
        dstscale.width = 128.0f;
        dstscale.height = 128.0f;
    }
    if (srcrect.width == 0.0f || srcrect.height == 0.0f) {
        dstscale.width = 128.0f;
        dstscale.height = 128.0f;
    }

    switch (scale_mode) {
        case PIGFX_SCALE_INTEGER_MAX:
        default:
            if (dstscale.height * 2 <= pi_screen_height) {
                if (width == 320) {
                    if (pi_screen_height == 720) {
                        dstscale.width = 960;
                        dstscale.height = 720;
                    } else if (pi_screen_height == 1080) {
                        dstscale.width = 1440;
                        dstscale.height = 1080;
                    } else if (pi_screen_height == 1200) {
                        dstscale.width = 1600;
                        dstscale.height = 1200;
                    }
                }
                while (dstscale.height + height <= pi_screen_height) {
                    dstscale.height += height;
                    dstscale.width += width;
                }
            }
            break;
        case PIGFX_SCALE_FULL_ASPECT:
            dstscale.height = (float)pi_screen_height;
            dstscale.width = srcrect.width * (dstscale.height / srcrect.height);
            break;
        case PIGFX_SCALE_FULL_43:
            dstscale.height = (float)pi_screen_height;
            dstscale.width = ((float)pi_screen_height / 3.0f) * 4.0f;
            break;
        case PIGFX_SCALE_FULL_169:
            dstscale.height = (float)pi_screen_height;
            dstscale.width = ((float)pi_screen_height / 9.0f) * 16.0f;
            break;
        case PIGFX_SCALE_FULL:
            dstscale.height = pi_screen_height;
            dstscale.width = pi_screen_width;
            break;
        case PIGFX_SCALE_NONE:
            dstscale.width = srcrect.width;
            dstscale.height = srcrect.height;
            fit_to_screen = 0;
            break;
        case PIGFX_SCALE_CUSTOM:
        case PIGFX_SCALE_CUSTOM_RECT:
            fit_to_screen = 0;
            center = 0;
            break;
    }

    if (fit_to_screen) {
        if (dstscale.width > pi_screen_width || dstscale.height > pi_screen_height) {
            if (dstscale.height > pi_screen_height) {
                DEBUG("[H > SH]\n");
                DEBUG("Adjusted width from %d to", (int)dstscale.width);
                dstscale.width = dstscale.width * ((float)pi_screen_height / srcrect.height);
                DEBUG("%d.\n", (int)dstscale.width);
                DEBUG("Adjusted height from %d to", (int)dstscale.height);
                dstscale.height = pi_screen_height;
                DEBUG("%d.\n", (int)dstscale.height);
            }
            if (dstscale.width > pi_screen_width) {
                // First scaling attempt failed, do not double adjust, re-adjust
                dstscale.width = width;
                dstscale.height = height;
                DEBUG("[W > SW]\n");
                DEBUG("Adjusted height from %d to", (int)dstscale.height);
                dstscale.height = dstscale.height * ((float)pi_screen_width / srcrect.width);
                DEBUG("%d.\n", (int)dstscale.height);
                DEBUG("Adjusted width from %d to", (int)dstscale.width);
                dstscale.width = pi_screen_width;
                DEBUG("%d.\n", (int)dstscale.width);
            }
        }
    }

    scale_x = dstscale.width / srcrect.width;
    scale_y = dstscale.height / srcrect.height;

    if (center) {
        origin.x = (dstscale.width - pi_screen_width) * 0.5;
        origin.y = (dstscale.height - pi_screen_height) * 0.5;
    }
}

void *rtgThread(void *args) {

    printf("RTG thread running\n");
    fflush(stdout);

    int reinit = 0, old_filter_mode = -1, force_filter_mode = 0;
    rtg_on = 1;

    volatile uint32_t *indexed_buf = NULL;

    rtg_share_data.format = &rtg_display_format;
    rtg_share_data.width = &rtg_display_width;
    rtg_share_data.height = &rtg_display_height;
    rtg_share_data.pitch = &rtg_pitch;
    rtg_share_data.offset_x = &rtg_offset_x;
    rtg_share_data.offset_y = &rtg_offset_y;
    rtg_share_data.memory = rtg_mem;
    rtg_share_data.running = &rtg_on;
    rtg_share_data.addr = &framebuffer_addr_adj;
    struct rtg_shared_data *data = &rtg_share_data;

    uint16_t width = rtg_display_width;
    uint16_t height = rtg_display_height;
    uint16_t format = rtg_display_format;
    uint16_t pitch = rtg_pitch;

    Texture raylib_texture, raylib_cursor_texture;
    Texture raylib_clut_texture;
    Image raylib_fb, raylib_cursor, raylib_clut;

    InitWindow(0, 0, "Pistorm RTG");
    HideCursor();
    SetTargetFPS(60);

    pi_screen_width = GetScreenWidth();
    pi_screen_height = GetScreenHeight();

	Color bef = { 0, 64, 128, 255 };

    Shader clut_shader = LoadShader(NULL, "platforms/amiga/rtg/clut.shader");
    Shader swizzle_shader = LoadShader(NULL, "platforms/amiga/rtg/argbswizzle.shader");
    int clut_loc = GetShaderLocation(clut_shader, "texture1");

    raylib_clut.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    raylib_clut.width = 256;
    raylib_clut.height = 1;
    raylib_clut.mipmaps = 1;
    raylib_clut.data = palette;

    raylib_clut_texture = LoadTextureFromImage(raylib_clut);
    SetTextureWrap(raylib_clut_texture, TEXTURE_WRAP_CLAMP);

    raylib_cursor.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    raylib_cursor.width = 256;
    raylib_cursor.height = 256;
    raylib_cursor.mipmaps = 1;
    raylib_cursor.data = cursor_data;
    raylib_cursor_texture = LoadTextureFromImage(raylib_cursor);

reinit_raylib:;
    if (reinit) {
        printf("Reinitializing raylib...\n");
        width = rtg_display_width;
        height = rtg_display_height;
        format = rtg_display_format;
        pitch = rtg_pitch;
        if (indexed_buf) {
            free(indexed_buf);
            indexed_buf = NULL;
        }
        UnloadTexture(raylib_texture);
        old_filter_mode = -1;
        reinit = 0;
    }

    printf("Creating %dx%d raylib window...\n", width, height);

    printf("Setting up raylib framebuffer image.\n");
    raylib_fb.format = rtg_to_raylib[format];

    switch (format) {
        case RTGFMT_RBG565:
            raylib_fb.width = width;
            indexed_buf = calloc(1, width * height * 2);
            break;
        default:
            raylib_fb.width = pitch / rtg_pixel_size[format];
            break;
    }
    raylib_fb.height = height;
	raylib_fb.mipmaps = 1;
	raylib_fb.data = &data->memory[*data->addr];
    printf("Width: %d\nPitch: %d\nBPP: %d\n", raylib_fb.width, pitch, rtg_pixel_size[format]);

    raylib_texture = LoadTextureFromImage(raylib_fb);

    printf("Loaded framebuffer texture.\n");

    rtg_scale_output(width, height);

    force_filter_mode = 0;

    while (1) {
        if (rtg_on) {
            if (old_filter_mode != filter_mode) {
                old_filter_mode = filter_mode;
                SetTextureFilter(raylib_texture, filter_mode);
                SetTextureFilter(raylib_cursor_texture, filter_mode);
            }
            /* If we are not in 16bit mode then don't use any filtering - otherwise force_filter_mode to no smoothing */
            if (force_filter_mode == 0) {
                if (format != RTGFMT_RBG565 && filter_mode != 0) {
                    printf("Turning Smooth filtering off - display mode not 16bit\n");
                    force_filter_mode = 1;
                    old_filter_mode = filter_mode;
                    SetTextureFilter(raylib_texture, 0);
                    SetTextureFilter(raylib_cursor_texture, 0);
            	}
            } else {
                if (format == RTGFMT_RBG565) {
                    printf("Turning Smooth filtering back on - display mode is 16bit\n");
		    force_filter_mode = 0;
                    old_filter_mode = -1;
                }
            }
            BeginDrawing();
            rtg_output_in_vblank = 0;
            updating_screen = 1;

            switch (format) {
                case RTGFMT_8BIT:
                    BeginShaderMode(clut_shader);
                    SetShaderValueTexture(clut_shader, clut_loc, raylib_clut_texture);
                    break;
                case RTGFMT_RGB32:
                    BeginShaderMode(swizzle_shader);
                    break;
            }
            
            DrawTexturePro(raylib_texture, srcrect, dstscale, origin, 0.0f, RAYWHITE);

            switch (format) {
                case RTGFMT_8BIT:
                case RTGFMT_RGB32:
                    EndShaderMode();
                    break;
            }

            if (mouse_cursor_enabled || clut_cursor_enabled) {
                float mc_x = mouse_cursor_x - rtg_offset_x + mouse_cursor_x_adj;
                float mc_y = mouse_cursor_y - rtg_offset_y + mouse_cursor_y_adj;
                Rectangle cursor_srcrect = { 0, 0, mouse_cursor_w, mouse_cursor_h };
                Rectangle dstrect = { mc_x * scale_x, mc_y * scale_y, (float)mouse_cursor_w * scale_x, (float)mouse_cursor_h * scale_y };
                DrawTexturePro(raylib_cursor_texture, cursor_srcrect, dstrect, origin, 0.0f, RAYWHITE);
            }

            if (debug_palette) {
                if (format == RTGFMT_8BIT) {
                    Rectangle srcrect = { 0, 0, 256, 1 };
                    Rectangle dstrect = { 0, 0, 1024, 8 };
                    DrawTexturePro(raylib_clut_texture, srcrect, dstrect, origin, 0.0f, RAYWHITE);
                }
            }

            if (show_fps) {
                DrawFPS(pi_screen_width - 128, 0);
            }

            EndDrawing();
            rtg_output_in_vblank = 1;
            cur_rtg_frame++;
            if (format == RTGFMT_RBG565) {
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        ((uint16_t *)indexed_buf)[x + (y * width)] = be16toh(((uint16_t *)data->memory)[(*data->addr / 2) + x + (y * (pitch / 2))]);
                    }
                }
                UpdateTexture(raylib_texture, indexed_buf);
            }
            else {
                UpdateTexture(raylib_texture, &data->memory[*data->addr]);
            }
            if (cursor_image_updated) {
                if (clut_cursor_enabled)
                    UpdateTexture(raylib_cursor_texture, clut_cursor_texture_data);
                else
                    UpdateTexture(raylib_cursor_texture, cursor_data);
                cursor_image_updated = 0;
            }
            if (palette_updated) {
                UpdateTexture(raylib_clut_texture, palette);
                palette_updated = 0;
            }
            updating_screen = 0;
        } else {
            BeginDrawing();
            ClearBackground(bef);
            DrawText("RTG is currently sleeping.", 16, 16, 12, RAYWHITE);
            EndDrawing();
        }
        if (pitch != *data->pitch || height != *data->height || width != *data->width || format != *data->format) {
            printf("Reinitializing due to something change.\n");
            reinit = 1;
            goto shutdown_raylib;
        }
        if (emulator_exiting) {
            goto shutdown_raylib;
        }
        if (shutdown) {
            break;
        }
    }

    shutdown = 0;
    rtg_initialized = 0;
    printf("RTG thread shut down.\n");

shutdown_raylib:;

    if (reinit)
        goto reinit_raylib;

    if (indexed_buf)
        free(indexed_buf);

    UnloadTexture(raylib_texture);
    UnloadShader(clut_shader);

    CloseWindow();

    return args;
}

void rtg_set_clut_entry(uint8_t index, uint32_t xrgb) {
    //palette[index] = xrgb;
    unsigned char *src = (unsigned char *)&xrgb;
    unsigned char *dst = (unsigned char *)&palette[index];
    palette_updated = 1;
    dst[0] = src[2];
    dst[1] = src[1];
    dst[2] = src[0];
    dst[3] = 0xFF;
}

void rtg_init_display() {
    int err;
    rtg_on = 1;

    if (!rtg_initialized) {
        if (rtg_dpms) {
            vc_tv_hdmi_power_on_preferred();
        }
        err = pthread_create(&thread_id, NULL, &rtgThread, (void *)&rtg_share_data);
        if (err != 0) {
            rtg_on = 0;
            display_enabled = 0xFF;
            printf("can't create RTG thread :[%s]", strerror(err));
        }
        else {
            rtg_initialized = 1;
            pthread_setname_np(thread_id, "pistorm: rtg");
            printf("RTG Thread created successfully\n");
        }
    }
    printf("RTG display enabled.\n");
}

void rtg_shutdown_display() {
    printf("RTG display disabled.\n");
    if (rtg_dpms) {
        shutdown = 1;
        vc_tv_power_off();
        pthread_join(thread_id, NULL);
    }

    rtg_on = 0;
    display_enabled = 0xFF;
}

void rtg_show_clut_cursor(uint8_t show) {
    if (clut_cursor_enabled != show) {
        while (rtg_on && !updating_screen)
            usleep(0);
        cursor_image_updated = 1;
    }
    clut_cursor_enabled = show;
}

void rtg_set_clut_cursor(uint8_t *bmp, uint32_t *pal, int16_t offs_x, int16_t offs_y, uint16_t w, uint16_t h, uint8_t mask_color) {
    if (bmp == NULL) {
        memset(clut_cursor_texture_data, 0x00, (256 * 256) * sizeof(uint32_t));
        cursor_image_updated = 1;
    }
    if (bmp != NULL && pal != NULL) {
        memset(clut_cursor_texture_data, 0x00, (256 * 256) * sizeof(uint32_t));
        mouse_cursor_w = w;
        mouse_cursor_h = h;
        mouse_cursor_x_adj = -offs_x;
        mouse_cursor_y_adj = -offs_y;
        for (uint8_t y = 0; y < mouse_cursor_h; y++) {
            for (uint8_t x = 0; x < mouse_cursor_w; x++) {
                uint8_t clut_index = bmp[x + (y * w)];
                if (bmp[x + (y * w)] != mask_color) {
                    uint32_t *col = (uint32_t *)((uint8_t *)pal + (clut_index *3));
                    clut_cursor_texture_data[x + (y * 256)] = (*col | 0xFF000000);
                }
            }
        }
        while (rtg_on && !updating_screen)
            usleep(0);
        cursor_image_updated = 1;
    }
}

void rtg_enable_mouse_cursor(uint8_t enable) {
    mouse_cursor_enabled = enable;
}

void rtg_set_mouse_cursor_pos(int16_t x, int16_t y) {
    mouse_cursor_x = x;
    mouse_cursor_y = y;
}

static uint8_t clut_cursor_data[256*256];

void update_mouse_cursor(uint8_t *src) {
    if (src != NULL) {
        memset(clut_cursor_data, 0x00, 256*256);
        uint8_t cur_bit = 0x80;
        uint8_t line_pitch = (mouse_cursor_w / 8) * 2;

        for (uint8_t y = 0; y < mouse_cursor_h; y++) {
            for (uint8_t x = 0; x < mouse_cursor_w; x++) {
                if (src[(x / 8) + (line_pitch * y)] & cur_bit)
                    clut_cursor_data[x + (y * 256)] |= 0x01;
                if (src[(x / 8) + (line_pitch * y) + (mouse_cursor_w / 8)] & cur_bit)
                    clut_cursor_data[x + (y * 256)] |= 0x02;
                cur_bit >>= 1;
                if (cur_bit == 0x00)
                    cur_bit = 0x80;
            }
            cur_bit = 0x80;
        }
    }

    for (int y = 0; y < mouse_cursor_h; y++) {
        for (int x = 0; x < mouse_cursor_w; x++) {
            cursor_data[x + (y * 256)] = cursor_palette[clut_cursor_data[x + (y * 256)]];
        }
    }

    while (rtg_on && !updating_screen)
        usleep(0);
    cursor_image_updated = 1;
}

void rtg_set_cursor_clut_entry(uint8_t r, uint8_t g, uint8_t b, uint8_t idx) {
    uint32_t color = 0;
    unsigned char *dst = (unsigned char *)&color;

    dst[0] = r;
    dst[1] = g;
    dst[2] = b;
    dst[3] = 0xFF;
    if (cursor_palette[idx + 1] != color) {
        cursor_palette[0] = 0;
        cursor_palette[idx + 1] = color;
        update_mouse_cursor(NULL);
    }
}

static uint8_t old_mouse_w, old_mouse_h;
static uint8_t old_mouse_data[256];

void rtg_set_mouse_cursor_image(uint8_t *src, uint8_t w, uint8_t h) {
    uint8_t new_cursor_data = 0;

    mouse_cursor_w = w;
    mouse_cursor_h = h;

    if (memcmp(src, old_mouse_data, (w / 8 * h)) != 0)
        new_cursor_data = 1;

    if (old_mouse_w != w || old_mouse_h != h || new_cursor_data) {
        old_mouse_w = w;
        old_mouse_h = h;
        update_mouse_cursor(src);
    }
}

void rtg_show_fps(uint8_t enable) {
    show_fps = (enable != 0);
}

void rtg_palette_debug(uint8_t enable) {
    debug_palette = (enable != 0);
}

void rtg_set_scale_mode(uint16_t _scale_mode) {
    switch (_scale_mode) {
        case PIGFX_SCALE_INTEGER_MAX:
        case PIGFX_SCALE_FULL_ASPECT:
        case PIGFX_SCALE_FULL_43:
        case PIGFX_SCALE_FULL_169:
        case PIGFX_SCALE_FULL:
        case PIGFX_SCALE_NONE:
            scale_mode = _scale_mode;
            rtg_scale_output(rtg_display_width, rtg_display_height);
            break;
        case PIGFX_SCALE_CUSTOM:
        case PIGFX_SCALE_CUSTOM_RECT:
            printf("[!!!RTG] Tried to set RTG scale mode to custom or custom rect using the wrong function. Ignored.\n");
            break;
    }
}

uint16_t rtg_get_scale_mode() {
    return scale_mode;
}

void rtg_set_scale_rect(uint16_t _scale_mode, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    scale_mode = _scale_mode;

    origin.x = 0.0f;
    origin.y = 0.0f;
    mouse_cursor_x_adj = x1;
    mouse_cursor_y_adj = y1;
    dstscale.x = (float)x1;
    dstscale.y = (float)y1;

    switch (scale_mode) {
        case PIGFX_SCALE_CUSTOM_RECT:
            dstscale.width = (float)x2;
            dstscale.height = (float)y2;
            break;
        case PIGFX_SCALE_CUSTOM:
            dstscale.width = (float)x2 - (float)x1;
            dstscale.height = (float)y2 - (float)y1;
            break;
    }

    rtg_scale_output(rtg_display_width, rtg_display_height);
}

void rtg_set_scale_filter(uint16_t _filter_mode) {
    filter_mode = _filter_mode;
}

uint16_t rtg_get_scale_filter() {
    return filter_mode;
} 
