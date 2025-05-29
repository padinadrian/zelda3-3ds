#include <3ds.h>
#include <stdint.h>

#include "snes/ppu.h"

#include "types.h"
#include "variables.h"

#include "zelda_rtl.h"
#include "zelda_cpu_infra.h"

#include "config.h"
#include "assets.h"
#include "load_gfx.h"
#include "util.h"
#include "audio.h"

#define WIDTH 240
#define HEIGHT 400
#define BUFFER_SIZE (WIDTH * HEIGHT)

#define COLOR_RED   0xFF0000FF
#define COLOR_GREEN 0x00FF00FF
#define COLOR_BLUE  0x0000FFFF

enum {
    STATE_RED_TO_GREEN = 0,
    STATE_GREEN_TO_BLUE = 1,
    STATE_BLUE_TO_RED = 2,
};

// Static data
const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

// Forwards
void fill_buffer(u32* buffer, u32 color);
void Die(const char *error);
static void LoadAssets();
static void LoadLinkGraphics();


int main(int argc, char** argv)
{
    // Initialize services
    // gfxInitDefault();
    romfsInit();

    // Initialize frame buffers
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
    gfxSetDoubleBuffering(GFX_TOP, 1);

    //  Initialize console on bottom window
    consoleInit(GFX_BOTTOM, NULL);
    printf("Starting up.\n");

    printf("Loading assets...\n");
    LoadAssets();
    printf("Assets loaded.\n");

    printf("Loading link graphics...\n");
    LoadLinkGraphics();
    printf("Link graphics loaded.\n");

    printf("Initializing Zelda...\n");
    ZeldaInitialize();
    printf("Zelda initialized.\n");

    printf("\x1b[30;16HPress Start to exit.");

    uint32_t red = 0xFF, green = 0, blue = 0;
    uint32_t state = STATE_RED_TO_GREEN;

    // Main loop
    while(aptMainLoop()) {
        gfxSwapBuffers();
        gfxFlushBuffers();
        gspWaitForVBlank();

        hidScanInput();
        uint32_t pressed = hidKeysDown();
        if (pressed & KEY_START) {
            break; // break in order to return to hbmenu
        }

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

    }

    // Close program
    gfxExit();
    return 0;
}

// Functions

void fill_buffer(u32* buffer, u32 color) {
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        buffer[i] = color;
    }
}

// Halt the program if we run into an error
void Die(const char *error) {
    fprintf(stderr, "Error: %s\n", error);
    while(aptMainLoop()) {
        hidScanInput();
        uint32_t pressed = hidKeysDown();
        if (pressed & KEY_START) {
            break; // break in order to return to hbmenu
        }
    }
    exit(1);
}

static void LoadAssets() {
    size_t length = 0;
    uint8 *data = ReadWholeFile("romfs:/zelda3_assets.dat", &length);
    if (!data) {
        size_t bps_length, bps_src_length;
        uint8 *bps, *bps_src;
        bps = ReadWholeFile("zelda3_assets.bps", &bps_length);
        if (!bps) {
            Die("Failed to read zelda3_assets.dat. Please see the README for information about how you get this file.");
        }
        bps_src = ReadWholeFile("zelda3.sfc", &bps_src_length);
        if (!bps_src) {
            Die("Missing file: zelda3.sfc");
        }
        data = ApplyBps(bps_src, bps_src_length, bps, bps_length, &length);
        if (!data) {
            Die("Unable to apply zelda3_assets.bps. Please make sure you got the right version of 'zelda3.sfc'");
        }
    }

    static const char kAssetsSig[] = { kAssets_Sig };

    if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
        memcmp(data, kAssetsSig, 48) != 0 ||
        *(uint32*)(data + 80) != kNumberOfAssets
    ) {
        Die("Invalid assets file");
    }

    uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(data + 84);

    for (size_t i = 0; i < kNumberOfAssets; i++) {
        uint32 size = *(uint32 *)(data + 88 + i * 4);
        offset = (offset + 3) & ~3;
        if ((uint64)offset + size > length) {
            Die("Assets file corruption");
        }
        g_asset_sizes[i] = size;
        g_asset_ptrs[i] = data + offset;
        offset += size;
    }

    if (g_config.features0 & kFeatures0_DimFlashes) { // patch dungeon floor palettes
        kPalette_DungBgMain[0x484] = 0x70;
        kPalette_DungBgMain[0x485] = 0x95;
        kPalette_DungBgMain[0x486] = 0x57;
    }
}

static bool ParseLinkGraphics(uint8 *file, size_t length) {
    if (length < 27 || memcmp(file, "ZSPR", 4) != 0) {
        return false;
    }
    uint32 pixel_offs = DWORD(file[9]);
    uint32 pixel_length = WORD(file[13]);
    uint32 palette_offs = DWORD(file[15]);
    uint32 palette_length = WORD(file[19]);
    if ((uint64)pixel_offs + pixel_length > length ||
        (uint64)palette_offs + palette_length > length ||
        pixel_length != 0x7000
    ) {
        return false;
    }
    if (kPalette_ArmorAndGloves_SIZE != 150 || kLinkGraphics_SIZE != 0x7000) {
        Die("ParseLinkGraphics: Invalid asset sizes");
    }
    memcpy(kLinkGraphics, file + pixel_offs, 0x7000);
    if (palette_length >= 120) {
        memcpy(kPalette_ArmorAndGloves, file + palette_offs, 120);
    }
    if (palette_length >= 124) {
        memcpy(kGlovesColor, file + palette_offs + 120, 4);
    }
    return true;
}

static void LoadLinkGraphics() {
    if (g_config.link_graphics) {
        fprintf(stderr, "Loading Link Graphics: %s\n", g_config.link_graphics);
        size_t length = 0;
        uint8 *file = ReadWholeFile(g_config.link_graphics, &length);
        if (file == NULL || !ParseLinkGraphics(file, length)) {
            Die("Unable to load file");
        }
        free(file);
    }
}

