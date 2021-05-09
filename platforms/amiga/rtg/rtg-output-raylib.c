// SPDX-License-Identifier: MIT

#include "config_file/config_file.h"
#include "emulator.h"
#include "rtg.h"

#include "raylib/raylib.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//#define DEBUG_RAYLIB_RTG

#define RTG_INIT_ERR(a) { printf(a); *data->running = 0; }

uint8_t busy = 0, rtg_on = 0, rtg_initialized = 0;
extern uint8_t *rtg_mem;
extern uint32_t framebuffer_addr;
extern uint32_t framebuffer_addr_adj;

extern uint16_t rtg_display_width, rtg_display_height;
extern uint16_t rtg_display_format;
extern uint16_t rtg_pitch, rtg_total_rows;
extern uint16_t rtg_offset_x, rtg_offset_y;

static pthread_t thread_id;

struct rtg_shared_data {
    uint16_t *width, *height;
    uint16_t *format, *pitch;
    uint16_t *offset_x, *offset_y;
    uint8_t *memory;
    uint32_t *addr;
    uint8_t *running;
};

struct rtg_shared_data rtg_share_data;
static uint32_t palette[256];
static uint32_t cursor_palette[256];

extern uint8_t cursor_data[256 * 256];

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

void *rtgThread(void *args) {

    printf("RTG thread running\n");
    fflush(stdout);

    int reinit = 0;
    rtg_on = 1;

    uint32_t *indexed_buf = NULL;

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
    Texture raylib_clut_texture, raylib_cursor_clut_texture;
    Image raylib_fb, raylib_cursor, raylib_clut, raylib_cursor_clut;

    InitWindow(GetScreenWidth(), GetScreenHeight(), "Pistorm RTG");
    HideCursor();
    SetTargetFPS(60);

	Color bef = { 0, 64, 128, 255 };

    Shader clut_shader = LoadShader(NULL, "platforms/amiga/rtg/clut.shader");
    Shader swizzle_shader = LoadShader(NULL, "platforms/amiga/rtg/argbswizzle.shader");
    int clut_loc = GetShaderLocation(clut_shader, "texture1");

    raylib_clut.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    raylib_clut.width = 256;
    raylib_clut.height = 1;
    raylib_clut.mipmaps = 1;
    raylib_clut.data = palette;

    raylib_cursor_clut.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    raylib_cursor_clut.width = 256;
    raylib_cursor_clut.height = 1;
    raylib_cursor_clut.mipmaps = 1;
    raylib_cursor_clut.data = cursor_palette;

    raylib_clut_texture = LoadTextureFromImage(raylib_clut);
    raylib_cursor_clut_texture = LoadTextureFromImage(raylib_cursor_clut);

    raylib_cursor.format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
    raylib_cursor.width = 256;
    raylib_cursor.height = 256;
    raylib_cursor.mipmaps = 1;
    raylib_cursor.data = cursor_data;
    raylib_cursor_texture = LoadTextureFromImage(raylib_cursor);

    Rectangle srchax, dsthax;
    Vector2 originhax;

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

    srchax.x = srchax.y = 0;
    srchax.width = width;
    srchax.height = height;
    dsthax.x = dsthax.y = 0;
    if (GetScreenHeight() == 720) {
        dsthax.width = 960;
        dsthax.height = 720;
    } else if (GetScreenHeight() == 1080) {
        dsthax.width = 1440;
        dsthax.height = 1080;
    }
    originhax.x = 0.0f;
    originhax.y = 0.0f;

    while (1) {
        if (rtg_on) {
            BeginDrawing();

            switch (format) {
                case RTGFMT_8BIT:
                    UpdateTexture(raylib_clut_texture, palette);
                    BeginShaderMode(clut_shader);
                    SetShaderValueTexture(clut_shader, clut_loc, raylib_clut_texture);
                    break;
                case RTGFMT_RGB32:
                    BeginShaderMode(swizzle_shader);
                    break;
            }
            
            if (width == 320) {
                DrawTexturePro(raylib_texture, srchax, dsthax, originhax, 0.0f, RAYWHITE);
            }
            else {
                Rectangle srcrect = { 0, 0, width, height };
                DrawTexturePro(raylib_texture, srcrect, srcrect, originhax, 0.0f, RAYWHITE);
                //DrawTexture(raylib_texture, 0, 0, RAYWHITE);
            }

            switch (format) {
                case RTGFMT_8BIT:
                case RTGFMT_RGB32:
                    EndShaderMode();
                    break;
            }
#ifdef DEBUG_RAYLIB_RTG
            if (format == RTGFMT_8BIT) {
                Rectangle srcrect = { 0, 0, 256, 1 };
                Rectangle dstrect = { 0, 0, 1024, 8 };
                //DrawTexture(raylib_clut_texture, 0, 0, RAYWHITE);
                DrawTexturePro(raylib_clut_texture, srcrect, dstrect, originhax, 0.0f, RAYWHITE);
            }
#endif

            DrawFPS(width - 200, 0);
            EndDrawing();
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
        /*if (!rtg_on) {
            goto shutdown_raylib;
        }*/
    }

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
    dst[0] = src[2];
    dst[1] = src[1];
    dst[2] = src[0];
    dst[3] = 0xFF;
}

void rtg_init_display() {
    int err;
    rtg_on = 1;

    if (!rtg_initialized) {
        err = pthread_create(&thread_id, NULL, &rtgThread, (void *)&rtg_share_data);
        if (err != 0) {
            rtg_on = 0;
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
    rtg_on = 0;
}
