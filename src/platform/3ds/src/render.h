#ifndef ZELDA3_3DS_RENDER_H_
#define ZELDA3_3DS_RENDER_H_


#include <stdint.h>

typedef struct SDL_Window SDL_Window;

/** Initialize the renderer. */
bool RendererInitialize_3ds(SDL_Window* window);

/** Destroy the renderer. */
void RendererDestroy_3ds();

/** Start the process of drawing the frame. */
void RendererBeginDraw_3ds(int width, int height, uint8_t **pixels, int *pitch);

/** Finish the process of drawing the frame. */
void RendererEndDraw_3ds();


#endif  // ZELDA3_3DS_RENDER_H_
