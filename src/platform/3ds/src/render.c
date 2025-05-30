// Render the frame using libctru

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "render.h"

#define WINDOW_WIDTH 256
#define WINDOW_HEIGHT 240
#define BYTES_PER_PIXEL 4

#define PIXEL_BUFFER_SIZE (WINDOW_WIDTH * WINDOW_HEIGHT)

static uint32_t g_pixel_buffer[PIXEL_BUFFER_SIZE];
static int g_width, g_height;

bool RendererInitialize_3ds(SDL_Window* window) {
    // TODO
    return NULL;
}

void RendererDestroy_3ds() {
    // TODO
}

void RendererBeginDraw_3ds(int width, int height, uint8_t **pixels, int *pitch) {
    static bool first_time = true;
    if (first_time) {
        printf("width: %d, height: %d, size: %d\n", width, height, PIXEL_BUFFER_SIZE);
        first_time = false;
        for (size_t i = 0; i < PIXEL_BUFFER_SIZE; ++i) {
            g_pixel_buffer[i] = 0xFF;
        }
    }

    // Initialize the pixel buffer
    *pixels = (uint8_t*)g_pixel_buffer;
    g_width = width;
    g_height = height;
    *pitch = width * 4;
}

void RendererEndDraw_3ds() {
    uint32_t* frame_buffer = (uint32_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
    frame_buffer += (240 * 72) - 1;

    int num_pixels = g_width * g_height;
    uint32_t pixel = 0;
    for (int x = 0; x < g_width; ++x) {
        for (int y = 0; y < g_height; ++y) {
            int pi = (y * g_width) + x;
            int bi = (x * g_height) + (g_height - y);

            pixel = g_pixel_buffer[pi];
            frame_buffer[bi] = (pixel >> 24) | (pixel << 8);   // Convert ARGB to RGBA
        }
    }
}
