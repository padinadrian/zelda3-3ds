#include <3ds.h>
#include <stdint.h>

#define WIDTH 240
#define HEIGHT 400

#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE  0x0000FFFF

enum {
    STATE_RED_TO_GREEN = 0,
    STATE_GREEN_TO_BLUE = 1,
    STATE_BLUE_TO_RED = 2,
};

void fill_buffer(u32* buffer, u32 color) {
    int x, y;
    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            buffer[x + (y * WIDTH)] = color;
        }
    }
}

int main() {
    // Initialize frame buffers
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
    gfxSetDoubleBuffering(GFX_TOP, 1);

    uint32_t red = 0xFF, green = 0, blue = 0;

    uint32_t state = STATE_RED_TO_GREEN;

    // Main loop
    while(aptMainLoop()) {
        hidScanInput();

        switch (state) {
            case STATE_RED_TO_GREEN: {
                red -= 1;
                green += 1;
                if (red == 0) {
                    state = STATE_GREEN_TO_BLUE;
                }
                break;
            }
            case STATE_GREEN_TO_BLUE: {
                green -= 1;
                blue += 1;
                if (green == 0) {
                    state = STATE_BLUE_TO_RED;
                }
                break;
            }
            case STATE_BLUE_TO_RED: {
                blue -= 1;
                red += 1;
                if (blue == 0) {
                    state = STATE_RED_TO_GREEN;
                }
                break;
            }
        }

        uint32_t color = 0xFF | (red << 24) | (green << 16) | (blue << 8);

        // Fill top screen with red
        fill_buffer((uint32_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0), color);

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    // Close program
    gfxExit();
    return 0;
}