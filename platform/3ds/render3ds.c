/**
 *  Filename:   render3ds.c
 *  Author:     Adrian Padin (padin.adrian@gmail.com)
 *  Date:       2023-04-01
 * 
 *  Functions for rendering on the 3DS screen.
 */

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

static int size = 0;
static uint32 mallocCounter = 0;
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

/**
 * Setup to draw to the buffer.
 */
void N3DS_Renderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
    if (size == 0) {
        size = width * height;
    }

    // Allocate a buffer to copy pixel data into if necessary.
    if (size > g_screen_buffer_size) {
        mallocCounter++;
        g_screen_buffer_size = size;
        free(g_screen_buffer);
        g_screen_buffer = malloc(size * 4);
    }

    g_draw_width = width;
    g_draw_height = height;
    *pixels = g_screen_buffer;
    *pitch = width * 4;
}

/**
 * After drawing to the buffer.
 */
void N3DS_Renderer_EndDraw() {
    // The 3DS top screen is 400x240 pixels.
    // The "width" of the 3DS screen is actually the height.
    const int MAX_WIDTH = 240;
    uint32* frameBuffer = (uint32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
    uint32* pixelBuffer = (uint32*)g_screen_buffer;
    uint32 pixel;

    // Copy pixel data to the 3DS framebuffer.
    for (int y = 0; y < g_draw_height; ++y) {
        for (int x = 0; x < g_draw_width; ++x) {

            // Convert pixel format from ARGB -> RGBA
            pixel = pixelBuffer[x + (y * g_draw_width)];
            pixel = (
                ((pixel & 0xFF000000) >> 24 ) |
                ((pixel & 0x00FF0000) << 8  ) |
                ((pixel & 0x0000FF00) << 8  ) |
                ((pixel & 0x000000FF) << 8  )
            );

            // Don't change this part!
            // The picture needs to be rotated and flipped because
            // the 3DS screen is sideways.
            frameBuffer[(MAX_WIDTH - y) + (x * MAX_WIDTH)] = pixel;
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