
/* ===== Includes ===== */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
// #include <unistd.h>

#include <3ds.h>

#include "util.h"


/* ===== Data ===== */

typedef uint8_t uint8;
static uint8 *g_screen_buffer = NULL;
static size_t g_screen_buffer_size = 0;
static int g_draw_width = 0, g_draw_height = 0;

/* ===== Functions ===== */

/**
 * Initialize the renderer.
 */
bool N3DS_Renderer_Init(SDL_Window* window) {
    return true;
}

/**
 * Destroy the renderer.
 */
void N3DS_Renderer_Destroy() {
    // Nothing
}

static int size = 0;
static uint32 mallocCounter = 0;

/**
 * Setup to draw to the buffer.
 */
void N3DS_Renderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
    if (size == 0) {
        size = width * height;
    }

    if (size > g_screen_buffer_size) {
        mallocCounter++;
        printf("\x1b[5;1H malloc:    %lu\x1b[K", mallocCounter);
        g_screen_buffer_size = size;
        free(g_screen_buffer);
        g_screen_buffer = malloc(size * 4);
    }

    g_draw_width = width;
    g_draw_height = height;
    *pixels = g_screen_buffer;
    *pitch = width * 4;
}


// static void fill_buffer(u32* buffer, u32 color) {
//     for (int y = 0; y < HEIGHT; y++) {
//         for (int x = 0; x < WIDTH; x++) {
//             buffer[x + (y * WIDTH)] = color;
//         }
//     }
// }

/**
 * After drawing to the buffer.
 */
void N3DS_Renderer_EndDraw() {
    // TODO
    
    const int MAX_WIDTH = 240;
    const int MAX_HEIGHT = 400;
    uint32* frameBuffer = (uint32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
    uint32* pixelBuffer = (uint32*)g_screen_buffer;

    uint32 pixel;

    // This works!
    for (int y = 0; y < g_draw_height; ++y) {
        for (int x = 0; x < g_draw_width; ++x) {
            pixel = pixelBuffer[x + (y * g_draw_width)];

            pixel = (
                ((pixel & 0xFF000000) >> 24 ) |
                ((pixel & 0x00FF0000) << 8  ) |
                ((pixel & 0x0000FF00) << 8  ) |
                ((pixel & 0x000000FF) << 8  )
            );

            // Don't change this part!
            frameBuffer[(MAX_WIDTH - y) + ((x) * MAX_WIDTH)] = pixel;
        }
    }
}

/**
 * Initialize renderer funcs.
 */
void N3DS_Renderer_Create(struct RendererFuncs *funcs) {
    funcs->Initialize = &N3DS_Renderer_Init;
    funcs->Destroy = &N3DS_Renderer_Destroy;
    funcs->BeginDraw = &N3DS_Renderer_BeginDraw;
    funcs->EndDraw = &N3DS_Renderer_EndDraw;
}