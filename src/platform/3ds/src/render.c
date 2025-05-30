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

/**
 * Initialize the renderer.
 */
bool RendererInitialize_3ds(SDL_Window* window) {
    // Nothing
    return NULL;
}

/**
 * Destroy the renderer.
 */
void RendererDestroy_3ds() {
    // Nothing
}

/**
 * Start the process of drawing the frame.
 *
 * Initialize the frame buffer and height/width of the window.
 */
void RendererBeginDraw_3ds(int width, int height, uint8_t **pixels, int *pitch) {
    // Debug: print the width and height the first time the frame is drawn.
    static bool first_time = true;
    if (first_time) {
        printf("width: %d, height: %d, size: %d\n", width, height, PIXEL_BUFFER_SIZE);
        first_time = false;
    }

    // Initialize the pixel buffer
    *pixels = (uint8_t*)g_pixel_buffer;
    g_width = width;
    g_height = height;
    *pitch = width * BYTES_PER_PIXEL;
}

/**
 * Finish the process of drawing the frame.
 *
 * Translate and copy the rendered frame to the frame buffer.
 */
void RendererEndDraw_3ds() {
    uint32_t* frame_buffer = (uint32_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);

    // Shift frame buffer 72 rows to center the image in the screen.
    // Default screen width is 256 pixels, 3DS top screen is 400 pixels
    // 400 - 256 = 144, 144 / 2 = 72
    // TODO: For widescreen this will need to be dynamic
    frame_buffer += ((g_height * 72) - 1);

    uint32_t pixel = 0, pbi = 0, fbi = 0;
    for (uint32_t x = 0; x < g_width; ++x) {
        for (uint32_t y = 0; y < g_height; ++y) {
            pbi = (y * g_width) + x;
            fbi = (x * g_height) + (g_height - y);

            pixel = g_pixel_buffer[pbi];
            frame_buffer[fbi] = (pixel >> 24) | (pixel << 8);   // Convert ARGB to RGBA
        }
    }
}
