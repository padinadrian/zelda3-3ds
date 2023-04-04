/**
 *  Filename:   main3ds.c
 *  Author:     Adrian Padin (padinadrian@gmail.com)
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


/* ===== Forwards ===== */

void ShaderInit();
// static bool LoadRom(const char *filename);
static void LoadLinkGraphics();
// static void RenderNumber(uint8 *dst, size_t pitch, int n, bool big);
static void HandleInput(int keyCode, int modCode, bool pressed);
static void HandleCommand(uint32 j, bool pressed);
static int RemapSdlButton(int button);
static void HandleGamepadInput(int button, bool pressed);
static void HandleGamepadAxisInput(int gamepad_id, int axis, int value);
static void OpenOneGamepad(int i);
static void HandleVolumeAdjustment(int volume_adjustment);
static void LoadAssets();
static void SwitchDirectory();

// Render functions


/* ===== Enumerations ===== */

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
    //   if (g_display_perf)
        // RenderNumber(pixel_buffer + pitch * render_scale, pitch, g_curr_fps, render_scale == 4);
    g_renderer_funcs.EndDraw();
}




// State for sdl renderer
// static SDL_Renderer *g_renderer;
// static SDL_Texture *g_texture;
// static SDL_Rect g_sdl_renderer_rect;

// static bool SdlRenderer_Init(SDL_Window *window) {

//   if (g_config.shader)
//     fprintf(stderr, "Warning: Shaders are supported only with the OpenGL backend\n");

//   SDL_Renderer *renderer = SDL_CreateRenderer(g_window, -1,
//                                               g_config.output_method == kOutputMethod_SDLSoftware ? SDL_RENDERER_SOFTWARE :
//                                               SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
//   if (renderer == NULL) {
//     printf("Failed to create renderer: %s\n", SDL_GetError());
//     return false;
//   }
//   SDL_RendererInfo renderer_info;
//   SDL_GetRendererInfo(renderer, &renderer_info);
//   if (kDebugFlag) {
//     printf("Supported texture formats:");
//     for (int i = 0; i < renderer_info.num_texture_formats; i++)
//       printf(" %s", SDL_GetPixelFormatName(renderer_info.texture_formats[i]));
//     printf("\n");
//   }
//   g_renderer = renderer;
//   if (!g_config.ignore_aspect_ratio)
//     SDL_RenderSetLogicalSize(renderer, g_snes_width, g_snes_height);
//   if (g_config.linear_filtering)
//     SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");

//   int tex_mult = (g_ppu_render_flags & kPpuRenderFlags_4x4Mode7) ? 4 : 1;
//   g_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
//                                 g_snes_width * tex_mult, g_snes_height * tex_mult);
//   if (g_texture == NULL) {
//     printf("Failed to create texture: %s\n", SDL_GetError());
//     return false;
//   }
//   return true;
// }

// static void SdlRenderer_Destroy() {
//   SDL_DestroyTexture(g_texture);
//   SDL_DestroyRenderer(g_renderer);
// }

// static void SdlRenderer_BeginDraw(int width, int height, uint8 **pixels, int *pitch) {
//   g_sdl_renderer_rect.w = width;
//   g_sdl_renderer_rect.h = height;
//   if (SDL_LockTexture(g_texture, &g_sdl_renderer_rect, (void **)pixels, pitch) != 0) {
//     printf("Failed to lock texture: %s\n", SDL_GetError());
//     return;
//   }
// }

// static void SdlRenderer_EndDraw() {

// //  uint64 before = SDL_GetPerformanceCounter();
//   SDL_UnlockTexture(g_texture);
// //  uint64 after = SDL_GetPerformanceCounter();
// //  float v = (double)(after - before) / SDL_GetPerformanceFrequency();
// //  printf("%f ms\n", v * 1000);
//   SDL_RenderClear(g_renderer);
//   SDL_RenderCopy(g_renderer, g_texture, &g_sdl_renderer_rect, NULL);
//   SDL_RenderPresent(g_renderer); // vsyncs to 60 FPS?
// }

// static const struct RendererFuncs kSdlRendererFuncs  = {
//   &SdlRenderer_Init,
//   &SdlRenderer_Destroy,
//   &SdlRenderer_BeginDraw,
//   &SdlRenderer_EndDraw,
// };

// void OpenGLRenderer_Create(struct RendererFuncs *funcs);

// #undef main
// int main(int argc, char** argv) {
//   argc--, argv++;
//   const char *config_file = NULL;
//   if (argc >= 2 && strcmp(argv[0], "--config") == 0) {
//     config_file = argv[1];
//     argc -= 2, argv += 2;
//   } else {
//     SwitchDirectory();
//   }
//   ParseConfigFile(config_file);
//   LoadAssets();
//   LoadLinkGraphics();

//   ZeldaInitialize();
//   g_zenv.ppu->extraLeftRight = UintMin(g_config.extended_aspect_ratio, kPpuExtraLeftRight);
//   g_snes_width = (g_config.extended_aspect_ratio * 2 + 256);
//   g_snes_height = (g_config.extend_y ? 240 : 224);


//   // Delay actually setting those features in ram until any snapshots finish playing.
//   g_wanted_zelda_features = g_config.features0;

//   g_ppu_render_flags = g_config.new_renderer * kPpuRenderFlags_NewRenderer |
//                        g_config.enhanced_mode7 * kPpuRenderFlags_4x4Mode7 |
//                        g_config.extend_y * kPpuRenderFlags_Height240 |
//                        g_config.no_sprite_limits * kPpuRenderFlags_NoSpriteLimits;
//   ZeldaEnableMsu(g_config.enable_msu);

// //   if (g_config.fullscreen == 1)
// //     g_win_flags ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
// //   else if (g_config.fullscreen == 2)
// //     g_win_flags ^= SDL_WINDOW_FULLSCREEN;

//   // Window scale (1=100%, 2=200%, 3=300%, etc.)
//   g_current_window_scale = (g_config.window_scale == 0) ? 2 : IntMin(g_config.window_scale, kMaxWindowScale);

//   // audio_freq: Use common sampling rates (see user config file. values higher than 48000 are not supported.)
//   if (g_config.audio_freq < 11025 || g_config.audio_freq > 48000)
//     g_config.audio_freq = kDefaultFreq;

//   // Currently, the SPC/DSP implementation only supports up to stereo.
//   if (g_config.audio_channels < 1 || g_config.audio_channels > 2)
//     g_config.audio_channels = kDefaultChannels;

//   // audio_samples: power of 2
//   if (g_config.audio_samples <= 0 || ((g_config.audio_samples & (g_config.audio_samples - 1)) != 0))
//     g_config.audio_samples = kDefaultSamples;

//   // set up SDL
// //   if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
// //     printf("Failed to init SDL: %s\n", SDL_GetError());
// //     return 1;
// //   }

//   bool custom_size  = g_config.window_width != 0 && g_config.window_height != 0;
//   int window_width  = custom_size ? g_config.window_width  : g_current_window_scale * g_snes_width;
//   int window_height = custom_size ? g_config.window_height : g_current_window_scale * g_snes_height;

// //   if (g_config.output_method == kOutputMethod_OpenGL) {
// //     g_win_flags |= SDL_WINDOW_OPENGL;
// //     OpenGLRenderer_Create(&g_renderer_funcs);
// //   } else {
// //     g_renderer_funcs = kSdlRendererFuncs;
// //   }
//     g_renderer_funcs

// //   SDL_Window* window = SDL_CreateWindow(kWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, g_win_flags);
// //   if(window == NULL) {
// //     printf("Failed to create window: %s\n", SDL_GetError());
// //     return 1;
// //   }
// //   g_window = window;
// //   SDL_SetWindowHitTest(window, HitTestCallback, NULL);

//   if (!g_renderer_funcs.Initialize(NULL))
//     return 1;

// //   SDL_AudioDeviceID device = 0;
// //   SDL_AudioSpec want = { 0 }, have;
// //   g_audio_mutex = SDL_CreateMutex();
// //   if (!g_audio_mutex) Die("No mutex");

//     // TODO: Audio
// //   if (g_config.enable_audio) {
// //     want.freq = g_config.audio_freq;
// //     want.format = AUDIO_S16;
// //     want.channels = g_config.audio_channels;
// //     want.samples = g_config.audio_samples;
// //     want.callback = &AudioCallback;
// //     device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
// //     if (device == 0) {
// //       printf("Failed to open audio device: %s\n", SDL_GetError());
// //       return 1;
// //     }
// //     g_audio_channels = have.channels;
// //     g_frames_per_block = (534 * have.freq) / 32000;
// //     g_audiobuffer = malloc(g_frames_per_block * have.channels * sizeof(int16));
// //   }

// //   if (argc >= 1 && !g_run_without_emu)
// //     LoadRom(argv[0]);

//     mkdir("saves", 0755);

//     ZeldaReadSram();

//     // TODO: init gamepad
// //   for (int i = 0; i < SDL_NumJoysticks(); i++)
//     // OpenOneGamepad(i);

//   bool running = true;
// //   SDL_Event event;
// //   uint32 lastTick = SDL_GetTicks();
//     uint32 lastTick = 0;
//     uint32 curTick = 0;
//     uint32 frameCtr = 0;
//     bool audiopaused = true;

// //   if (g_config.autosave)
//     // HandleCommand(kKeys_Load + 0, true);

//   while(running) {
//     while(SDL_PollEvent(&event)) {
//       switch(event.type) {
//       case SDL_CONTROLLERDEVICEADDED:
//         OpenOneGamepad(event.cdevice.which);
//         break;
//       case SDL_CONTROLLERAXISMOTION:
//         HandleGamepadAxisInput(event.caxis.which, event.caxis.axis, event.caxis.value);
//         break;
//       case SDL_CONTROLLERBUTTONDOWN:
//       case SDL_CONTROLLERBUTTONUP: {
//         int b = RemapSdlButton(event.cbutton.button);
//         if (b >= 0)
//           HandleGamepadInput(b, event.type == SDL_CONTROLLERBUTTONDOWN);
//         break;
//       }
//       case SDL_MOUSEWHEEL:
//         if (SDL_GetModState() & KMOD_CTRL && event.wheel.y != 0)
//           ChangeWindowScale(event.wheel.y > 0 ? 1 : -1);
//         break;
//       case SDL_MOUSEBUTTONDOWN:
//         if (event.button.button == SDL_BUTTON_LEFT && event.button.state == SDL_PRESSED && event.button.clicks == 2) {
//           if ((g_win_flags & SDL_WINDOW_FULLSCREEN_DESKTOP) == 0 && (g_win_flags & SDL_WINDOW_FULLSCREEN) == 0 && SDL_GetModState() & KMOD_SHIFT) {
//             g_win_flags ^= SDL_WINDOW_BORDERLESS;
//             SDL_SetWindowBordered(g_window, (g_win_flags & SDL_WINDOW_BORDERLESS) == 0);
//           }
//         }
//         break;
//       case SDL_KEYDOWN:
//         HandleInput(event.key.keysym.sym, event.key.keysym.mod, true);
//         break;
//       case SDL_KEYUP:
//         HandleInput(event.key.keysym.sym, event.key.keysym.mod, false);
//         break;
//       case SDL_QUIT:
//         running = false;
//         break;
//       }
//     }

//     if (g_paused != audiopaused) {
//       audiopaused = g_paused;
//       if (device)
//         SDL_PauseAudioDevice(device, audiopaused);
//     }

//     if (g_paused) {
//       SDL_Delay(16);
//       continue;
//     }

//     // Clear gamepad inputs when joypad directional inputs to avoid wonkiness
//     int inputs = g_input1_state;
//     if (g_input1_state & 0xf0)
//       g_gamepad_buttons = 0;
//     inputs |= g_gamepad_buttons;

//     SDL_LockMutex(g_audio_mutex);
//     bool is_replay = ZeldaRunFrame(inputs);
//     SDL_UnlockMutex(g_audio_mutex);

//     frameCtr++;

//     if ((g_turbo ^ (is_replay & g_replay_turbo)) && (frameCtr & (g_turbo ? 0xf : 0x7f)) != 0) {
//       continue;
//     }

//     DrawPpuFrameWithPerf();

//     // if (g_config.display_perf_title) {
//     //   char title[60];
//     //   snprintf(title, sizeof(title), "%s | FPS: %d", kWindowTitle, g_curr_fps);
//     //   SDL_SetWindowTitle(g_window, title);
//     // }

//     // if vsync isn't working, delay manually
//     // curTick = SDL_GetTicks();

// //     if (!g_config.disable_frame_delay) {
// //       static const uint8 delays[3] = { 17, 17, 16 }; // 60 fps
// //       lastTick += delays[frameCtr % 3];

// //       if (lastTick > curTick) {
// //         uint32 delta = lastTick - curTick;
// //         if (delta > 500) {
// //           lastTick = curTick - 500;
// //           delta = 500;
// //         }
// // //        printf("Sleeping %d\n", delta);
// //         SDL_Delay(delta);
// //       } else if (curTick - lastTick > 500) {
// //         lastTick = curTick;
// //       }
// //     }
// //   }

//     // TODO
//     if (g_config.autosave) {
//         HandleCommand(kKeys_Save + 0, true);
//     }

//     // // clean sdl
//     // if (g_config.enable_audio) {
//     //     SDL_PauseAudioDevice(device, 1);
//     //     SDL_CloseAudioDevice(device);
//     // }

// //   SDL_DestroyMutex(g_audio_mutex);
//     free(g_audiobuffer);

//     g_renderer_funcs.Destroy();

// //   SDL_DestroyWindow(window);
// //   SDL_Quit();
//   //SaveConfigFile();
//   return 0;
// }

// static void RenderDigit(uint8 *dst, size_t pitch, int digit, uint32 color, bool big) {
//   static const uint8 kFont[] = {
//     0x1c, 0x36, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x36, 0x1c,
//     0x18, 0x1c, 0x1e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7e,
//     0x3e, 0x63, 0x60, 0x30, 0x18, 0x0c, 0x06, 0x03, 0x63, 0x7f,
//     0x3e, 0x63, 0x60, 0x60, 0x3c, 0x60, 0x60, 0x60, 0x63, 0x3e,
//     0x30, 0x38, 0x3c, 0x36, 0x33, 0x7f, 0x30, 0x30, 0x30, 0x78,
//     0x7f, 0x03, 0x03, 0x03, 0x3f, 0x60, 0x60, 0x60, 0x63, 0x3e,
//     0x1c, 0x06, 0x03, 0x03, 0x3f, 0x63, 0x63, 0x63, 0x63, 0x3e,
//     0x7f, 0x63, 0x60, 0x60, 0x30, 0x18, 0x0c, 0x0c, 0x0c, 0x0c,
//     0x3e, 0x63, 0x63, 0x63, 0x3e, 0x63, 0x63, 0x63, 0x63, 0x3e,
//     0x3e, 0x63, 0x63, 0x63, 0x7e, 0x60, 0x60, 0x60, 0x30, 0x1e,
//   };
//   const uint8 *p = kFont + digit * 10;
//   if (!big) {
//     for (int y = 0; y < 10; y++, dst += pitch) {
//       int v = *p++;
//       for (int x = 0; v; x++, v >>= 1) {
//         if (v & 1)
//           ((uint32 *)dst)[x] = color;
//       }
//     }
//   } else {
//     for (int y = 0; y < 10; y++, dst += pitch * 2) {
//       int v = *p++;
//       for (int x = 0; v; x++, v >>= 1) {
//         if (v & 1) {
//           ((uint32 *)dst)[x * 2 + 1] = ((uint32 *)dst)[x * 2] = color;
//           ((uint32 *)(dst+pitch))[x * 2 + 1] = ((uint32 *)(dst + pitch))[x * 2] = color;
//         }
//       }
//     }
//   }
// }

// static void RenderNumber(uint8 *dst, size_t pitch, int n, bool big) {
//   char buf[32], *s;
//   int i;
//   sprintf(buf, "%d", n);
//   for (s = buf, i = 2 * 4; *s; s++, i += 8 * 4)
//     RenderDigit(dst + ((pitch + i + 4) << big), pitch, *s - '0', 0x404040, big);
//   for (s = buf, i = 2 * 4; *s; s++, i += 8 * 4)
//     RenderDigit(dst + (i << big), pitch, *s - '0', 0xffffff, big);
// }

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
    //   g_win_flags ^= SDL_WINDOW_FULLSCREEN_DESKTOP;
    //   SDL_SetWindowFullscreen(g_window, g_win_flags & SDL_WINDOW_FULLSCREEN_DESKTOP);
      g_cursor = !g_cursor;
    //   SDL_ShowCursor(g_cursor);
      break;
    case kKeys_Reset:
      ZeldaReset(true);
      break;
    case kKeys_Pause: g_paused = !g_paused; break;
    case kKeys_PauseDimmed:
      g_paused = !g_paused;
      // SDL_RenderPresent may not be called more than once per frame.
      // Seems to work on Windows still. Temporary measure until it's fixed.
#ifdef _WIN32
      if (g_paused) {
        // SDL_SetRenderDrawBlendMode(g_renderer, SDL_BLENDMODE_BLEND);
        // SDL_SetRenderDrawColor(g_renderer, 0, 0, 0, 159);
        // SDL_RenderFillRect(g_renderer, NULL);
        // SDL_RenderPresent(g_renderer);
      }
#endif
      break;
    case kKeys_ReplayTurbo: g_replay_turbo = !g_replay_turbo; break;
    case kKeys_WindowBigger: ChangeWindowScale(1); break;
    case kKeys_WindowSmaller: ChangeWindowScale(-1); break;
    case kKeys_DisplayPerf: g_display_perf ^= 1; break;
    case kKeys_ToggleRenderer: g_ppu_render_flags ^= kPpuRenderFlags_NewRenderer; break;
    case kKeys_VolumeUp:
    case kKeys_VolumeDown: HandleVolumeAdjustment(j == kKeys_VolumeUp ? 1 : -1); break;
    default: assert(0);
    }
  }
}

static void HandleInput(int keyCode, int keyMod, bool pressed) {
//   int j = FindCmdForSdlKey(keyCode, keyMod);
    int j= 0;
  if (j != 0)
    HandleCommand(j, pressed);
}

static void OpenOneGamepad(int i) {
//   if (SDL_IsGameController(i)) {
    // SDL_GameController *controller = SDL_GameControllerOpen(i);
    // if (!controller)
    //   fprintf(stderr, "Could not open gamepad %d: %s\n", i, SDL_GetError());
//   }
}

static int RemapSdlButton(int button) {
//   switch (button) {
//   case SDL_CONTROLLER_BUTTON_A: return kGamepadBtn_A;
//   case SDL_CONTROLLER_BUTTON_B: return kGamepadBtn_B;
//   case SDL_CONTROLLER_BUTTON_X: return kGamepadBtn_X;
//   case SDL_CONTROLLER_BUTTON_Y: return kGamepadBtn_Y;
//   case SDL_CONTROLLER_BUTTON_BACK: return kGamepadBtn_Back;
//   case SDL_CONTROLLER_BUTTON_GUIDE: return kGamepadBtn_Guide;
//   case SDL_CONTROLLER_BUTTON_START: return kGamepadBtn_Start;
//   case SDL_CONTROLLER_BUTTON_LEFTSTICK: return kGamepadBtn_L3;
//   case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return kGamepadBtn_R3;
//   case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return kGamepadBtn_L1;
//   case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return kGamepadBtn_R1;
//   case SDL_CONTROLLER_BUTTON_DPAD_UP: return kGamepadBtn_DpadUp;
//   case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return kGamepadBtn_DpadDown;
//   case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return kGamepadBtn_DpadLeft;
//   case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return kGamepadBtn_DpadRight;
//   default: return -1;
//   }
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

// Approximates atan2(y, x) normalized to the [0,4) range
// with a maximum error of 0.1620 degrees
// normalized_atan(x) ~ (b x + x^2) / (1 + 2 b x + x^2)
static float ApproximateAtan2(float y, float x) {
  uint32 sign_mask = 0x80000000;
  float b = 0.596227f;
  // Extract the sign bits
  uint32 ux_s = sign_mask & *(uint32 *)&x;
  uint32 uy_s = sign_mask & *(uint32 *)&y;
  // Determine the quadrant offset
  float q = (float)((~ux_s & uy_s) >> 29 | ux_s >> 30);
  // Calculate the arctangent in the first quadrant
  float bxy_a = b * x * y;
  if (bxy_a < 0.0f) bxy_a = -bxy_a;  // avoid fabs
  float num = bxy_a + y * y;
  float atan_1q = num / (x * x + bxy_a + num + 0.000001f);
  // Translate it to the proper quadrant
  uint32_t uatan_2q = (ux_s ^ uy_s) | *(uint32 *)&atan_1q;
  return q + *(float *)&uatan_2q;
}

static void HandleGamepadAxisInput(int gamepad_id, int axis, int value) {
//   static int last_gamepad_id, last_x, last_y;
//   if (axis == SDL_CONTROLLER_AXIS_LEFTX || axis == SDL_CONTROLLER_AXIS_LEFTY) {
//     // ignore other gamepads unless they have a big input
//     if (last_gamepad_id != gamepad_id) {
//       if (value > -16000 && value < 16000)
//         return;
//       last_gamepad_id = gamepad_id;
//       last_x = last_y = 0;
//     }
//     *(axis == SDL_CONTROLLER_AXIS_LEFTX ? &last_x : &last_y) = value;
//     int buttons = 0;
//     if (last_x * last_x + last_y * last_y >= 10000 * 10000) {
//       // in the non deadzone part, divide the circle into eight 45 degree
//       // segments rotated by 22.5 degrees that control which direction to move.
//       // todo: do this without floats?
//       static const uint8 kSegmentToButtons[8] = {
//         1 << 4,           // 0 = up
//         1 << 4 | 1 << 7,  // 1 = up, right
//         1 << 7,           // 2 = right
//         1 << 7 | 1 << 5,  // 3 = right, down
//         1 << 5,           // 4 = down
//         1 << 5 | 1 << 6,  // 5 = down, left
//         1 << 6,           // 6 = left
//         1 << 6 | 1 << 4,  // 7 = left, up
//       };
//       uint8 angle = (uint8)(int)(ApproximateAtan2(last_y, last_x) * 64.0f + 0.5f);
//       buttons = kSegmentToButtons[(uint8)(angle + 16 + 64) >> 5];
//     }
//     g_gamepad_buttons = buttons;
//   } else if ((axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT)) {
//     if (value < 12000 || value >= 16000)  // hysteresis
//       HandleGamepadInput(axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? kGamepadBtn_L2 : kGamepadBtn_R2, value >= 12000);
//   }
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
    gfxSetDoubleBuffering(GFX_TOP, false);
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
    fill_buffer((u32*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0), 0xFF0088FF);
    // fill_buffer((u32*)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,0,0), 0xFF0088FF);

    while(aptMainLoop()) {

        // Handle input
        hidScanInput();

        // fill top screen with red
        currentTime = osGetTime();
        if ((currentTime / 1000) % 2) {
            color = 0xFF0000FF;
        }
        else {
            color = 0x00FFFF00;
        }
        // fill_buffer((u32*)gfxGetFramebuffer(GFX_TOP,GFX_LEFT,0,0), color);

        // Clear gamepad inputs when joypad directional inputs to avoid wonkiness
        // int inputs = g_input1_state;
        // if (g_input1_state & 0xf0) {
        //     g_gamepad_buttons = 0;
        // }
        // inputs |= g_gamepad_buttons;
        int inputs = 0;

        // TODO: Audio mutex
        // SDL_LockMutex(g_audio_mutex);
        before = osGetTime();
        bool is_replay = ZeldaRunFrame(inputs);
        after = osGetTime();
        // SDL_UnlockMutex(g_audio_mutex);

        frameCtr++;

        // Debugging
        if ((frameCtr % 10) == 0) {
            printf("\x1b[1;1H frameCtr:         %lu\x1b[K", frameCtr);
            printf("\x1b[2;1H ZeldaRunFrame:    %lu\x1b[K", after - before);
        }

        // TODO: Debugging - only draw every tenth frame
        if ((frameCtr % 10) == 0) {
            before = osGetTime();
            DrawPpuFrameWithPerf();
            after = osGetTime();
        }

        // Debugging
        if ((frameCtr % 10) == 0) {
            printf("\x1b[3;1H DrawPpuFrame:     %lu\x1b[K", after - before);
        }

        // Cleanup buffers and wait for vblank
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    // free(g_audiobuffer);

    g_renderer_funcs.Destroy();
    gfxExit();
    return 0;
}

