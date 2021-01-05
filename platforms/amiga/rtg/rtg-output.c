#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <pthread.h>

#define RTG_INIT_ERR(a) { printf(a); *data->running = 0; }

uint8_t busy = 0, rtg_on = 0;
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

void rtg_update_screen() {}

void *rtgThread(void *args) {

    printf("RTG thread running\n");
    fflush(stdout);

    int err;
    rtg_on = 1;

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

    printf("Initializing SDL2...\n");
    if (SDL_Init(0) < 0) {
        printf("Failed to initialize SDL2.\n");
    }

    printf("Initializing SDL2 Video...\n");
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Failed to initialize SDL2 Video..\n");
    }

    printf("Creating %dx%d SDL2 window...\n", *data->width, *data->height);
    win = SDL_CreateWindow("Pistorm RTG", 0, 0, *data->width, *data->height, 0);
    if (!win) {
        RTG_INIT_ERR("Failed create SDL2 window.\n");
    }
    else {
        printf("Created %dx%d window.\n", *data->width, *data->height);
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
    img = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_TARGET, *data->width, *data->height);
    if (!img) {
        RTG_INIT_ERR("Failed create SDL2 texture.\n");
    }
    else {
        printf("Created %dx%d texture.\n", *data->width, *data->height);
    }

    while (1) {
        if (renderer && win && img) {
            SDL_UpdateTexture(img, NULL, &data->memory[*data->addr + (*data->offset_x << *data->format) + (*data->offset_y * *data->pitch)], *data->pitch);
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, img, NULL, NULL);
            SDL_RenderPresent(renderer);
            usleep(16667); //ghetto 60hz
        }
        else
            break;
    }

    printf("RTG thread shut down.\n");

    return args;
}

void rtg_set_clut_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {

}

void rtg_init_display() {
    int err;
    rtg_on = 1;

    err = pthread_create(&thread_id, NULL, &rtgThread, (void *)&rtg_share_data);
    if (err != 0) {
        rtg_on = 0;
        printf("can't create RTG thread :[%s]", strerror(err));
    }
    else {
        printf("RTG Thread created successfully\n");
        printf("RTG display enabled.\n");
    }
}

void rtg_shutdown_display() {
    printf("RTG display disabled.\n");
    rtg_on = 0;

    if (img) SDL_DestroyTexture(img);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (win) SDL_DestroyWindow(win);
}
