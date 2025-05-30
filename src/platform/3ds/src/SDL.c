// Stubs for unused SDL functions.

#include "SDL.h"

void SDL_GL_SetSwapInterval(int) {
    // Nothing
}

void SDL_GL_SetAttribute(int, int) {
    // Nothing
}

uint64_t SDL_GetPerformanceFrequency() {
    // TODO: No clue what this should be.
    // Copied this value from the PSP implementation.
    return 1000;
}

SDL_Keycode SDL_GetKeyFromName(const char *name) {
    SDL_Keycode sdl_keycode;
    return sdl_keycode;
}