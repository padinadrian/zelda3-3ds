// Stubs for unused SDL functions.

#include "SDL.h"

void SDL_GL_SetSwapInterval(int) {
    // Nothing
}

void SDL_GL_SetAttribute(int, int) {
    // Nothing
}

uint64_t SDL_GetPerformanceFrequency() {
    // TODO: No clue what this should be
    return 1000;
}