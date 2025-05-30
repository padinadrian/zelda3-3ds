// Implementations for required SDL functions.

#include "SDL.h"

/** Set the swap interval for the current OpenGL context. */
void SDL_GL_SetSwapInterval(int) {
    // Nothing
}

/** Set an OpenGL window attribute before window creation. */
void SDL_GL_SetAttribute(int, int) {
    // Nothing
}

/** Returns a platform-specific count per second. */
uint64_t SDL_GetPerformanceFrequency() {
    // TODO: No clue what this should be.
    // Copied this value from the PSP implementation.
    return 1000;
}

/** Get a key code from a human-readable name. */
SDL_Keycode SDL_GetKeyFromName(const char *name) {
    SDL_Keycode sdl_keycode;
    return sdl_keycode;
}
