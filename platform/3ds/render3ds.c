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

typedef uint8_t uint8;

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
 * Anything that needs to be done before drawing to the screen.
 */
void N3DS_Renderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
    *pixels = gfxGetFramebuffer(GFX_TOP,GFX_LEFT, 0, 0);
}

/**
 * Anything that needs to be done after drawing to the screen.
 */
void N3DS_Renderer_EndDraw() {
    // TODO
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