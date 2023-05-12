/**
 *  Filename:   main3ds.c
 *  Author:     Adrian Padin (padin.adrian@gmail.com)
 *  Date:       2023-03-30
 *  
 *  Starting point for 3ds port.
 */


/* ===== Includes ===== */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <3ds.h>
#include <citro2d.h>

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

#include "profiling.h"

/* ===== Forwards ===== */

// static bool LoadRom(const char *filename);
static void LoadLinkGraphics();
static void HandleInput(int keyCode, int modCode, bool pressed);
static void HandleCommand(uint32 j, bool pressed);
static void HandleGamepadInput(int button, bool pressed);
static void HandleVolumeAdjustment(int volume_adjustment);
static void LoadAssets();
static void SwitchDirectory();


/* ===== Enumerations ===== */

// Config options
enum {
    kDefaultFullscreen = 0,
    kMaxWindowScale = 10,
    kDefaultFreq = 44100,
    kDefaultChannels = 2,
    kDefaultSamples = 2048,
};


/* ===== Data ===== */

static bool g_run_without_emu = 1;

static const char kWindowTitle[] = "The Legend of Zelda: A Link to the Past";

// static uint32 g_win_flags = SDL_WINDOW_RESIZABLE;
static uint32 g_win_flags = 0;
static SDL_Window *g_window;

static uint8 g_paused, g_turbo, g_replay_turbo = true, g_cursor = true;
static uint8 g_current_window_scale;
static uint8 g_gamepad_buttons;
static int g_input1_state;
static bool g_display_perf;
static int g_curr_fps;
static int g_ppu_render_flags = 0;
static int g_snes_width, g_snes_height;
// static int g_sdl_audio_mixer_volume = SDL_MIX_MAXVOLUME;
static int g_sdl_audio_mixer_volume = 0;
static struct RendererFuncs g_renderer_funcs;
static uint32 g_gamepad_modifiers;
static uint16 g_gamepad_last_cmd[kGamepadBtn_Count];

static uint8 *g_audiobuffer, *g_audiobuffer_cur, *g_audiobuffer_end;
static int g_frames_per_block;
static uint8 g_audio_channels;


/* ===== Functions ===== */

/**
 * Immediately kills the program and returns an error code.
 */
void NORETURN Die(const char *error) {
    fprintf(stderr, "Error: %s\n", error);
    exit(1);
}

/**
 * Draw a single frame of the game.
*/
static void DrawPpuFrameWithPerf() {
    int render_scale = PpuGetCurrentRenderScale(g_zenv.ppu, g_ppu_render_flags);
    uint8 *pixel_buffer = 0;
    int pitch = 0;

    g_renderer_funcs.BeginDraw(
        g_snes_width * render_scale,
        g_snes_height * render_scale,
        &pixel_buffer,
        &pitch
    );

    if (g_display_perf || g_config.display_perf_title) {
        static float history[64], average;
        static int history_pos;
        // uint64 before = SDL_GetPerformanceCounter();
        ZeldaDrawPpuFrame(pixel_buffer, pitch, g_ppu_render_flags);
        // uint64 after = SDL_GetPerformanceCounter();
        // float v = (double)SDL_GetPerformanceFrequency() / (after - before);
        float v = 0;
        average += v - history[history_pos];
        history[history_pos] = v;
        history_pos = (history_pos + 1) & 63;
        g_curr_fps = average * (1.0f / 64);
    } else {
        ZeldaDrawPpuFrame(pixel_buffer, pitch, g_ppu_render_flags);
    }
    g_renderer_funcs.EndDraw();
}

static void HandleCommand_Locked(uint32 j, bool pressed);

static void HandleCommand(uint32 j, bool pressed) {
  if (j <= kKeys_Controls_Last) {
    static const uint8 kKbdRemap[] = { 0, 4, 5, 6, 7, 2, 3, 8, 0, 9, 1, 10, 11 };
    if (pressed)
      g_input1_state |= 1 << kKbdRemap[j];
    else
      g_input1_state &= ~(1 << kKbdRemap[j]);
    return;
  }

  if (j == kKeys_Turbo) {
    g_turbo = pressed;
    return;
  }

  // Everything that might access audio state
  // (like SaveLoad and Reset) must have the lock.
//   SDL_LockMutex(g_audio_mutex);
  HandleCommand_Locked(j, pressed);
//   SDL_UnlockMutex(g_audio_mutex);
}

void ZeldaApuLock() {
//   SDL_LockMutex(g_audio_mutex);
}

void ZeldaApuUnlock() {
//   SDL_UnlockMutex(g_audio_mutex);
}


static void HandleCommand_Locked(uint32 j, bool pressed) {
  if (!pressed)
    return;
  if (j <= kKeys_Load_Last) {
    SaveLoadSlot(kSaveLoad_Load, j - kKeys_Load);
  } else if (j <= kKeys_Save_Last) {
    SaveLoadSlot(kSaveLoad_Save, j - kKeys_Save);
  } else if (j <= kKeys_Replay_Last) {
    SaveLoadSlot(kSaveLoad_Replay, j - kKeys_Replay);
  } else if (j <= kKeys_LoadRef_Last) {
    SaveLoadSlot(kSaveLoad_Load, 256 + j - kKeys_LoadRef);
  } else if (j <= kKeys_ReplayRef_Last) {
    SaveLoadSlot(kSaveLoad_Replay, 256 + j - kKeys_ReplayRef);
  } else {
    switch (j) {
    case kKeys_CheatLife: PatchCommand('w'); break;
    case kKeys_CheatEquipment: PatchCommand('W'); break;
    case kKeys_CheatKeys: PatchCommand('o'); break;
    case kKeys_CheatWalkThroughWalls: PatchCommand('E'); break;
    case kKeys_ClearKeyLog: PatchCommand('k'); break;
    case kKeys_StopReplay: PatchCommand('l'); break;
    case kKeys_Fullscreen:
      g_cursor = !g_cursor;
      break;
    case kKeys_Reset:
      ZeldaReset(true);
      break;
    case kKeys_Pause: g_paused = !g_paused; break;
    case kKeys_PauseDimmed:
      g_paused = !g_paused;
      // SDL_RenderPresent may not be called more than once per frame.
      break;
    case kKeys_ReplayTurbo: g_replay_turbo = !g_replay_turbo; break;
    case kKeys_DisplayPerf: g_display_perf ^= 1; break;
    case kKeys_VolumeUp:
    case kKeys_VolumeDown: HandleVolumeAdjustment(j == kKeys_VolumeUp ? 1 : -1); break;
    default: assert(0);
    }
  }
}

static void HandleGamepadInput(int button, bool pressed) {
  if (!!(g_gamepad_modifiers & (1 << button)) == pressed)
    return;
  g_gamepad_modifiers ^= 1 << button;
  if (pressed)
    g_gamepad_last_cmd[button] = FindCmdForGamepadButton(button, g_gamepad_modifiers);
  if (g_gamepad_last_cmd[button] != 0)
    HandleCommand(g_gamepad_last_cmd[button], pressed);
}

static void HandleVolumeAdjustment(int volume_adjustment) {
#if SYSTEM_VOLUME_MIXER_AVAILABLE
  int current_volume = GetApplicationVolume();
  int new_volume = IntMin(IntMax(0, current_volume + volume_adjustment * 5), 100);
  SetApplicationVolume(new_volume);
  printf("[System Volume]=%i\n", new_volume);
#else
//   g_sdl_audio_mixer_volume = IntMin(IntMax(0, g_sdl_audio_mixer_volume + volume_adjustment * (SDL_MIX_MAXVOLUME >> 4)), SDL_MIX_MAXVOLUME);
//   printf("[SDL mixer volume]=%i\n", g_sdl_audio_mixer_volume);
#endif
}

// static bool LoadRom(const char *filename) {
//   size_t length = 0;
//   uint8 *file = ReadWholeFile(filename, &length);
//   if(!file) Die("Failed to read file");
//   bool result = EmuInitialize(file, length);
//   free(file);
//   return result;
// }

static bool ParseLinkGraphics(uint8 *file, size_t length) {
  if (length < 27 || memcmp(file, "ZSPR", 4) != 0)
    return false;
  uint32 pixel_offs = DWORD(file[9]);
  uint32 pixel_length = WORD(file[13]);
  uint32 palette_offs = DWORD(file[15]);
  uint32 palette_length = WORD(file[19]);
  if ((uint64)pixel_offs + pixel_length > length ||
      (uint64)palette_offs + palette_length > length ||
      pixel_length != 0x7000)
    return false;
  if (kPalette_ArmorAndGloves_SIZE != 150 || kLinkGraphics_SIZE != 0x7000)
    Die("ParseLinkGraphics: Invalid asset sizes");
  memcpy(kLinkGraphics, file + pixel_offs, 0x7000);
  if (palette_length >= 120)
    memcpy(kPalette_ArmorAndGloves, file + palette_offs, 120);
  if (palette_length >= 124)
    memcpy(kGlovesColor, file + palette_offs + 120, 4);
  return true;
}

static void LoadLinkGraphics() {
  if (g_config.link_graphics) {
    fprintf(stderr, "Loading Link Graphics: %s\n", g_config.link_graphics);
    size_t length = 0;
    uint8 *file = ReadWholeFile(g_config.link_graphics, &length);
    if (file == NULL || !ParseLinkGraphics(file, length))
      Die("Unable to load file");
    free(file);
  }
}


const uint8 *g_asset_ptrs[kNumberOfAssets];
uint32 g_asset_sizes[kNumberOfAssets];

static void LoadAssets() {
    size_t length = 0;
    uint8 *data = ReadWholeFile("romfs:/tables/zelda3_assets.dat", &length);
    if (!data) {
        data = ReadWholeFile("romfs:/zelda3_assets.dat", &length);
    }
    if (!data) {
        Die("Failed to read zelda3_assets.dat. Please see the README for information about how you get this file.");
    }

  static const char kAssetsSig[] = { kAssets_Sig };

  if (length < 16 + 32 + 32 + 8 + kNumberOfAssets * 4 ||
      memcmp(data, kAssetsSig, 48) != 0 ||
      *(uint32*)(data + 80) != kNumberOfAssets)
    Die("Invalid assets file");

  uint32 offset = 88 + kNumberOfAssets * 4 + *(uint32 *)(data + 84);

  for (size_t i = 0; i < kNumberOfAssets; i++) {
    uint32 size = *(uint32 *)(data + 88 + i * 4);
    offset = (offset + 3) & ~3;
    if ((uint64)offset + size > length)
      Die("Assets file corruption");
    g_asset_sizes[i] = size;
    g_asset_ptrs[i] = data + offset;
    offset += size;
  }
}

// Go some steps up and find zelda3.ini
static void SwitchDirectory() {
  char buf[4096];
  if (!getcwd(buf, sizeof(buf) - 32))
    return;
  size_t pos = strlen(buf);

  for (int step = 0; pos != 0 && step < 3; step++) {
    memcpy(buf + pos, "/zelda3.ini", 12);
    FILE *f = fopen(buf, "rb");
    if (f) {
      fclose(f);
      buf[pos] = 0;
      if (step != 0) {
        printf("Found zelda3.ini in %s\n", buf);
        int err = chdir(buf);
        (void)err;
      }
      return;
    }
    pos--;
    while (pos != 0 && buf[pos] != '/' && buf[pos] != '\\')
      pos--;
  }
}


/* ===== 3DS Input Functions ===== */

/** 
 *  Convert 3DS button to internal gamepad command.
 */
Remap3dsButton(u32 button) {
    switch (button) {
        case KEY_A: return kGamepadBtn_A;
        case KEY_B: return kGamepadBtn_B;
        case KEY_X: return kGamepadBtn_X;
        case KEY_Y: return kGamepadBtn_Y;
        case KEY_L: return kGamepadBtn_L1;
        case KEY_R: return kGamepadBtn_R1;
        case KEY_SELECT: return kGamepadBtn_Guide;
        case KEY_START: return kGamepadBtn_Start;
        case KEY_UP:
        case KEY_DUP: return kGamepadBtn_DpadUp;
        case KEY_DOWN:
        case KEY_DDOWN: return kGamepadBtn_DpadDown;
        case KEY_LEFT:
        case KEY_DLEFT: return kGamepadBtn_DpadLeft;
        case KEY_RIGHT:
        case KEY_DRIGHT: return kGamepadBtn_DpadRight;
        default: return -1;
    }
}


/* ===== MAIN ===== */

#define WIDTH 240
#define HEIGHT 400

static void fill_buffer(u32* buffer, u32 color) {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            buffer[x + (y * WIDTH)] = color;
        }
    }
}

int main3ds(int args, char** argv) {
    // Init libs
    romfsInit();

    // Initialize graphics
    gfxInitDefault();
    gfxInit(GSP_RGBA8_OES, GSP_RGBA8_OES, false);
    gfxSetDoubleBuffering(GFX_TOP, true);
    consoleInit(GFX_BOTTOM, NULL);

    printf("\x1b[1;1HStarting up... \x1b[K");

    // Initialize the game
    ParseConfigFile(NULL);
    printf("\x1b[1;1H LoadAssets \x1b[K");
    LoadAssets();
    printf("\x1b[1;1H LoadLinkGraphics \x1b[K");
    LoadLinkGraphics();
    printf("\x1b[1;1H ZeldaInitialize \x1b[K");
    ZeldaInitialize();

    g_zenv.ppu->extraLeftRight = UintMin(g_config.extended_aspect_ratio, kPpuExtraLeftRight);
    g_snes_width = (g_config.extended_aspect_ratio * 2 + 256);
    g_snes_height = (g_config.extend_y ? 240 : 224);

    // Delay actually setting those features in ram until any snapshots finish playing.
    g_wanted_zelda_features = g_config.features0;

    g_config.new_renderer = 1;

    g_ppu_render_flags = g_config.new_renderer * kPpuRenderFlags_NewRenderer |
                         g_config.enhanced_mode7 * kPpuRenderFlags_4x4Mode7 |
                         g_config.extend_y * kPpuRenderFlags_Height240 |
                         g_config.no_sprite_limits * kPpuRenderFlags_NoSpriteLimits;

    printf("\x1b[1;1H ZeldaEnableMsu \x1b[K");
    ZeldaEnableMsu(g_config.enable_msu);

    // // I don't know if I need these yet but I'm playing it safe.
    g_current_window_scale = 1;
    g_config.audio_samples = kDefaultSamples;
    g_config.audio_channels = kDefaultChannels;
    bool custom_size = false;
    int window_width  = custom_size ? g_config.window_width  : g_current_window_scale * g_snes_width;
    int window_height = custom_size ? g_config.window_height : g_current_window_scale * g_snes_height;

    printf("\x1b[1;1H N3DS_Renderer_Create \x1b[K");
    N3DS_Renderer_Create(&g_renderer_funcs);

    printf("\x1b[1;1H ZeldaReadSram \x1b[K");
    ZeldaReadSram();

    bool running = true;
    uint32 lastTick = 0;
    uint32 curTick = 0;
    uint32 frameCtr = 0;
    bool audiopaused = true;

    u64 currentTime = 0;
    u64 previousTime = 0;
    u64 before = 0;
    u64 after = 0;
    u32 color = 0;

    // Debugging - fill screen pink to show unused portions.
    // fill_buffer((u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0), 0xFF0088FF);

    // Profiling
    TickCounter counter;
    osTickCounterStart(&counter);

    // 0 will be ZeldaRunFrame
    Array* zeldaRunFrameArray = &(profilingArrays[0]);
    ArrayInitialize(zeldaRunFrameArray, 20);

    ArrayInitialize(&(profilingArrays[1]), 100);
    ArrayInitialize(&(profilingArrays[2]), 100);
    ArrayInitialize(&(profilingArrays[3]), 100);
    ArrayInitialize(&(profilingArrays[4]), 100);
    ArrayInitialize(&(profilingArrays[5]), 100);
    ArrayInitialize(&(profilingArrays[6]), 100);
    ArrayInitialize(&(profilingArrays[7]), 100);

    while(aptMainLoop()) {

        // Handle input
        hidScanInput();

        // Handle buttons being pressed
        u32 kDown = hidKeysDown();
        for (u32 bitmask = 1; bitmask; bitmask <<= 1) {
            u32 buttonPressed = (kDown & bitmask);
            if (buttonPressed) {
                int remappedButton = Remap3dsButton(buttonPressed);
                HandleGamepadInput(remappedButton, true);
            }
        }

        // Handle buttons being released
        u32 kUp = hidKeysUp();
        for (u32 bitmask = 1; bitmask; bitmask <<= 1) {
            u32 buttonReleased = (kUp & bitmask);
            if (buttonReleased) {
                int remappedButton = Remap3dsButton(buttonReleased);
                HandleGamepadInput(remappedButton, false);
            }
        }

        // Clear gamepad inputs when joypad directional inputs to avoid wonkiness
        int inputs = g_input1_state;

        // TODO: Audio mutex
        // SDL_LockMutex(g_audio_mutex);
        osTickCounterUpdate(&counter);
        bool is_replay = ZeldaRunFrame(inputs);
        osTickCounterUpdate(&counter);
        ArrayPush(zeldaRunFrameArray, osTickCounterRead(&counter));
        // SDL_UnlockMutex(g_audio_mutex);

        frameCtr++;

        // TODO: Debugging - only draw every other frame
        if ((frameCtr % 2) == 0) {
            osTickCounterUpdate(&counter);
            DrawPpuFrameWithPerf();
            osTickCounterUpdate(&counter);
        }

        // Debugging
        if ((frameCtr % 10) == 0) {
            printf("\x1b[1;1H frameCtr:         %lu\x1b[K", frameCtr);

            double average = ArrayAverage(zeldaRunFrameArray);
            printf("\x1b[2;1H ZeldaRunFrame:        %f\x1b[K", average);

            printf("\x1b[3;1H DrawPpuFrame:         %f\x1b[K", osTickCounterRead(&counter));

            average = ArrayAverage(profilingArrays + 1);
            printf("\x1b[4;1H ppuDrawWholeLine:     %f\x1b[K", average);
            
            average = ArrayAverage(profilingArrays + 2);
            printf("\x1b[5;1H PpuDrawSprites:       %f\x1b[K", average);
            
            average = ArrayAverage(profilingArrays + 3);
            printf("\x1b[6;1H DrawBackground_4bpp:  %f\x1b[K", average);

            average = ArrayAverage(profilingArrays + 4);
            printf("\x1b[7;1H DrawBackground_4bpp:  %f\x1b[K", average);
            
            average = ArrayAverage(profilingArrays + 5);
            printf("\x1b[8;1H DrawBackground_2bpp:  %f\x1b[K", average);
            
            average = ArrayAverage(profilingArrays + 6);
            printf("\x1b[9;1H DrawBackground_mode7: %f\x1b[K", average);

            average = ArrayAverage(profilingArrays + 7);
            printf("\x1b[9;1H everythingElse:       %f\x1b[K", average);

            // Clear profiling arrays
            for (size_t i = 0; i < 8; ++i) {
                ArrayClear(profilingArrays + i);
            }
        }

        // Cleanup buffers and wait for vblank
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    // free(g_audiobuffer);

    ArrayDestroy(zeldaRunFrameArray);

    g_renderer_funcs.Destroy();
    gfxExit();
    return 0;
}

