#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include "rtg.h"

#define RTG_INIT_ERR(a) { printf(a); *data->running = 0; }

uint8_t busy = 0, rtg_on = 0, rtg_initialized = 0;
extern uint8_t *rtg_mem;
extern uint32_t framebuffer_addr;

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

SDL_Window *win = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *img = NULL;

struct rtg_shared_data rtg_share_data;
static uint32_t palette[256];

void rtg_update_screen() {}

uint32_t rtg_to_sdl2[RTGFMT_NUM] = {
    SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_RGB565,
    SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_RGB555,
};

void *rtgThread(void *args) {

    printf("RTG thread running\n");
    fflush(stdout);

    int reinit = 0;
    rtg_on = 1;

    uint32_t *indexed_buf;

    rtg_share_data.format = &rtg_display_format;
    rtg_share_data.width = &rtg_display_width;
    rtg_share_data.height = &rtg_display_height;
    rtg_share_data.pitch = &rtg_pitch;
    rtg_share_data.offset_x = &rtg_offset_x;
    rtg_share_data.offset_y = &rtg_offset_y;
    rtg_share_data.memory = rtg_mem;
    rtg_share_data.running = &rtg_on;
    rtg_share_data.addr = &framebuffer_addr;
    struct rtg_shared_data *data = &rtg_share_data;

    uint16_t width = rtg_display_width;
    uint16_t height = rtg_display_height;
    uint16_t format = rtg_display_format;
    uint16_t pitch = rtg_pitch;

    printf("Initializing SDL2...\n");
    if (SDL_Init(0) < 0) {
        printf("Failed to initialize SDL2.\n");
    }

    printf("Initializing SDL2 Video...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to initialize SDL2 Video..\n");
    }

reinit_sdl:;
    if (reinit) {
        printf("Reinitializing SDL2...\n");
        width = rtg_display_width;
        height = rtg_display_height;
        format = rtg_display_format;
        pitch = rtg_pitch;
        if (indexed_buf) {
            free(indexed_buf);
            indexed_buf = NULL;
        }
        reinit = 0;
    }

    printf("Creating %dx%d SDL2 window...\n", width, height);
    win = SDL_CreateWindow("Pistorm RTG", 0, 0, width, height, 0);
    if (!win) {
        RTG_INIT_ERR("Failed create SDL2 window.\n");
    }
    else {
        printf("Created %dx%d window.\n", width, height);
    }

    printf("Creating SDL2 renderer...\n");
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        RTG_INIT_ERR("Failed create SDL2 renderer.\n");
    }
    else {
        printf("Created SDL2 renderer.\n");
    }

    printf("Creating SDL2 texture...\n");
    img = SDL_CreateTexture(renderer, rtg_to_sdl2[format], SDL_TEXTUREACCESS_TARGET, width, height);
    if (!img) {
        RTG_INIT_ERR("Failed create SDL2 texture.\n");
    }
    else {
        printf("Created %dx%d texture.\n", width, height);
    }

    if (format == RTGFMT_8BIT) {
        indexed_buf = calloc(1, width * height * 4);
        pitch = width * 4;
    }

    while (1) {
        if (renderer && win && img) {
            SDL_RenderClear(renderer);
            if (*data->running) {
                SDL_UpdateTexture(img, NULL, (format != RTGFMT_8BIT) ? &data->memory[*data->addr] : (uint8_t *)indexed_buf, pitch);
                SDL_RenderCopy(renderer, img, NULL, NULL);
            }
            SDL_RenderPresent(renderer);
            usleep(16667); //ghetto 60hz
            if (height != *data->height || width != *data->width || format != *data->format) {
                printf("Reinitializing due to something change.\n");
                reinit = 1;
                goto shutdown_sdl;
            }
            if (format == RTGFMT_8BIT) {
                for (int y = 0; y < height; y++) {
                    for (int x = 0; x < width; x++) {
                        indexed_buf[x + (y * width)] = palette[data->memory[*data->addr + x + (y * width)]];
                    }
                }
            }
        }
        else
            break;
    }

    rtg_initialized = 0;
    printf("RTG thread shut down.\n");

shutdown_sdl:;
    if (img) SDL_DestroyTexture(img);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win) SDL_DestroyWindow(win);

    win = NULL;
    img = NULL;
    renderer = NULL;

    if (reinit)
        goto reinit_sdl;

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return args;
}

void rtg_set_clut_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    //int r = (int)((float)rtg_u8[1] / 255.0f * 31.0f);
    //int g = (int)((float)rtg_u8[2] / 255.0f * 63.0f);
    //int b = (int)((float)rtg_u8[3] / 255.0f * 31.0f);
    palette[index] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
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
            printf("RTG Thread created successfully\n");
        }
    }
    printf("RTG display enabled.\n");
}

void rtg_shutdown_display() {
    printf("RTG display disabled.\n");
    rtg_on = 0;
}
