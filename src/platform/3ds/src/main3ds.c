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

#include "SDL.h"
#include "render.h"

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

// Forwards
void fill_buffer(u32* buffer, u32 color);
void Die(const char *error);
static void LoadAssets();
static void LoadLinkGraphics();
static void DrawPpuFrameWithPerf();
static void RenderNumber(uint8 *dst, size_t pitch, int n, bool big);

// Static data
const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

static uint8 g_paused, g_turbo, g_replay_turbo = true, g_cursor = true;
static uint8 g_current_window_scale;
static uint8 g_gamepad_buttons;
static int g_input1_state;
static bool g_display_perf;
static int g_curr_fps;
static int g_ppu_render_flags = 0;
static int g_snes_width, g_snes_height;
static struct RendererFuncs g_renderer_funcs;
static uint32 g_gamepad_modifiers;
static uint16 g_gamepad_last_cmd[kGamepadBtn_Count];

enum {
  kDefaultFullscreen = 0,
  kMaxWindowScale = 10,
  kDefaultFreq = 44100,
  kDefaultChannels = 2,
  kDefaultSamples = 2048,
};

static const char kWindowTitle[] = "The Legend of Zelda: A Link to the Past";

static const struct RendererFuncs renderFuncs3ds  = {
    &RendererInitialize_3ds,
    &RendererDestroy_3ds,
    &RendererBeginDraw_3ds,
    &RendererEndDraw_3ds,
};

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

    // Configuration
    printf("Loading configuration...\n");
    g_zenv.ppu->extraLeftRight = UintMin(g_config.extended_aspect_ratio, kPpuExtraLeftRight);
    g_snes_width = (g_config.extended_aspect_ratio * 2 + 256);
    g_snes_height = (g_config.extend_y ? 240 : 224);

    // Delay actually setting those features in ram until any snapshots finish playing.
    g_wanted_zelda_features = g_config.features0;

    g_ppu_render_flags = g_config.new_renderer * kPpuRenderFlags_NewRenderer |
                        g_config.enhanced_mode7 * kPpuRenderFlags_4x4Mode7 |
                        g_config.extend_y * kPpuRenderFlags_Height240 |
                        g_config.no_sprite_limits * kPpuRenderFlags_NoSpriteLimits;
    // ZeldaEnableMsu(g_config.enable_msu);
    ZeldaSetLanguage(g_config.language);
    printf("Config loaded.\n");

    // TODO: Audio setup

    // TODO: Renderer setup
    g_renderer_funcs = renderFuncs3ds;

    printf("Reading SRAM...\n");
    ZeldaReadSram();
    printf("SRAM read.\n");

    bool running = true;
    uint32 curTick = 0;
    uint32 frameCtr = 0;
    bool audiopaused = true;

    printf("Entering main loop...\n");

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
        fill_buffer((uint32_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0), color);

        // TODO: Get inputs
        int inputs = g_input1_state;
        if (g_input1_state & 0xf0) {
            g_gamepad_buttons = 0;
        }
        inputs |= g_gamepad_buttons;

        // LockMutex(g_audio_mutex)
        bool is_replay = ZeldaRunFrame(inputs);
        // UnlockMutex(g_audio_mutex)

        frameCtr++;

        if ((g_turbo ^ (is_replay & g_replay_turbo)) && (frameCtr & (g_turbo ? 0xf : 0x7f)) != 0) {
            continue;
        }

        DrawPpuFrameWithPerf();
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

MemBlk FindInAssetArray(int asset, int idx) {
    return FindIndexInMemblk((MemBlk) { g_asset_ptrs[asset], g_asset_sizes[asset] }, idx);
}

static void DrawPpuFrameWithPerf() {
    int render_scale = PpuGetCurrentRenderScale(g_zenv.ppu, g_ppu_render_flags);
    uint8 *pixel_buffer = 0;
    int pitch = 0;

    g_renderer_funcs.BeginDraw(g_snes_width * render_scale,
                                g_snes_height * render_scale,
                                &pixel_buffer, &pitch);
    if (g_display_perf || g_config.display_perf_title) {
        static float history[64], average;
        static int history_pos;
        uint64 before = osGetTime();
        ZeldaDrawPpuFrame(pixel_buffer, pitch, g_ppu_render_flags);
        uint64 after = osGetTime();
        float v = (double)SDL_GetPerformanceFrequency() / (after - before);
        average += v - history[history_pos];
        history[history_pos] = v;
        history_pos = (history_pos + 1) & 63;
        g_curr_fps = average * (1.0f / 64);
    } else {
        ZeldaDrawPpuFrame(pixel_buffer, pitch, g_ppu_render_flags);
    }
    // if (g_display_perf) {
    //     RenderNumber(pixel_buffer + pitch * render_scale, pitch, g_curr_fps, render_scale == 4);
    // }
    g_renderer_funcs.EndDraw();
}

static void RenderDigit(uint8 *dst, size_t pitch, int digit, uint32 color, bool big) {
    static const uint8 kFont[] = {
        0x1c, 0x36, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x1c,
        0x18, 0x1c, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e,
        0x3e, 0x63, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x63, 0x7f,
        0x3e, 0x63, 0x60, 0x60, 0x3c, 0x60, 0x60, 0x60, 0x63, 0x3e,
        0x30, 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x30, 0x30, 0x78,
        0x7f, 0x03, 0x03, 0x03, 0x3f, 0x60, 0x60, 0x60, 0x63, 0x3e,
        0x1c, 0x06, 0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3e,
        0x7f, 0x63, 0x60, 0x60, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c,
        0x3e, 0x63, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x3e,
        0x3e, 0x63, 0x63, 0x63, 0x7e, 0x60, 0x60, 0x60, 0x30, 0x1e,
    };
    const uint8 *p = kFont + digit * 10;
    if (!big) {
        for (int y = 0; y < 10; y++, dst += pitch) {
            int v = *p++;
            for (int x = 0; v; x++, v >>= 1) {
                if (v & 1) {
                    ((uint32 *)dst)[x] = color;
                }
            }
        }
    } else {
        for (int y = 0; y < 10; y++, dst += pitch * 2) {
            int v = *p++;
            for (int x = 0; v; x++, v >>= 1) {
                if (v & 1) {
                    ((uint32 *)dst)[x * 2 + 1] = ((uint32 *)dst)[x * 2] = color;
                    ((uint32 *)(dst+pitch))[x * 2 + 1] = ((uint32 *)(dst + pitch))[x * 2] = color;
                }
            }
        }
    }
}

static void RenderNumber(uint8 *dst, size_t pitch, int n, bool big) {
    char buf[32], *s;
    int i;
    sprintf(buf, "%d", n);
    for (s = buf, i = 2 * 4; *s; s++, i += 8 * 4) {
        RenderDigit(dst + ((pitch + i + 4) << big), pitch, *s - '0', 0x404040, big);
    }
    for (s = buf, i = 2 * 4; *s; s++, i += 8 * 4) {
        RenderDigit(dst + (i << big), pitch, *s - '0', 0xffffff, big);
    }
}

void ZeldaApuLock() {
    // SDL_LockMutex(g_audio_mutex);
}

void ZeldaApuUnlock() {
    // SDL_UnlockMutex(g_audio_mutex);
}

