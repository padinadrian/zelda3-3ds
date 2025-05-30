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
    // This value is the number of clock ticks per second.
    // Since ticks are measured in milliseconds, this value is millis per second.
    return 1000;
}

/** Get a key code from a human-readable name. */
SDL_Keycode SDL_GetKeyFromName(const char *name) {
    // TODO: Copy implementation from SDL source.
    SDL_Keycode sdl_keycode;
    return sdl_keycode;
}
