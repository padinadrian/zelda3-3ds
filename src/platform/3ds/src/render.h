#ifndef ZELDA3_3DS_RENDER_H_
#define ZELDA3_3DS_RENDER_H_

#include <stdint.h>

typedef struct SDL_Window SDL_Window;

// Forwards
bool RendererInitialize_3ds(SDL_Window* window);
void RendererDestroy_3ds();
void RendererBeginDraw_3ds(int width, int height, uint8_t **pixels, int *pitch);
void RendererEndDraw_3ds();

#endif  // ZELDA3_3DS_RENDER_H_
