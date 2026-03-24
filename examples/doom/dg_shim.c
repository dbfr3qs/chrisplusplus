// SDL2 platform shim for doomgeneric
// Implements the 6 required DG_* functions + a dg_start() wrapper for chrisplusplus

#include "doomkeys.h"
#include "doomgeneric.h"

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <SDL.h>

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

#define KEYQUEUE_SIZE 16

static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned char convertToDoomKey(unsigned int key) {
    switch (key) {
        case SDLK_RETURN:    return KEY_ENTER;
        case SDLK_ESCAPE:    return KEY_ESCAPE;
        case SDLK_LEFT:      return KEY_LEFTARROW;
        case SDLK_RIGHT:     return KEY_RIGHTARROW;
        case SDLK_UP:        return KEY_UPARROW;
        case SDLK_DOWN:      return KEY_DOWNARROW;
        case SDLK_LCTRL:
        case SDLK_RCTRL:
        case SDLK_z:         return KEY_FIRE;
        case SDLK_SPACE:     return KEY_USE;
        case SDLK_LSHIFT:
        case SDLK_RSHIFT:    return KEY_RSHIFT;
        case SDLK_LALT:
        case SDLK_RALT:      return KEY_LALT;
        case SDLK_F2:        return KEY_F2;
        case SDLK_F3:        return KEY_F3;
        case SDLK_F4:        return KEY_F4;
        case SDLK_F5:        return KEY_F5;
        case SDLK_F6:        return KEY_F6;
        case SDLK_F7:        return KEY_F7;
        case SDLK_F8:        return KEY_F8;
        case SDLK_F9:        return KEY_F9;
        case SDLK_F10:       return KEY_F10;
        case SDLK_F11:       return KEY_F11;
        case SDLK_EQUALS:
        case SDLK_PLUS:      return KEY_EQUALS;
        case SDLK_MINUS:     return KEY_MINUS;
        default:             return tolower(key);
    }
}

static void addKeyToQueue(int pressed, unsigned int keyCode) {
    unsigned char key = convertToDoomKey(keyCode);
    unsigned short keyData = (pressed << 8) | key;
    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

static void handleKeyInput(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            puts("Quit requested");
            atexit(SDL_Quit);
            exit(1);
        }
        if (e.type == SDL_KEYDOWN) {
            // Cmd+F toggles fullscreen on macOS
            if (e.key.keysym.sym == SDLK_f && (e.key.keysym.mod & KMOD_GUI)) {
                Uint32 flags = SDL_GetWindowFlags(window);
                if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
                    SDL_SetWindowFullscreen(window, 0);
                } else {
                    SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                }
                continue;
            }
            addKeyToQueue(1, e.key.keysym.sym);
        } else if (e.type == SDL_KEYUP) {
            addKeyToQueue(0, e.key.keysym.sym);
        }
    }
}

void DG_Init(void) {
    window = SDL_CreateWindow("DOOM",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              DOOMGENERIC_RESX,
                              DOOMGENERIC_RESY,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_RenderClear(renderer);
    SDL_RenderPresent(renderer);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB888,
                                SDL_TEXTUREACCESS_TARGET,
                                DOOMGENERIC_RESX, DOOMGENERIC_RESY);
}

void DG_DrawFrame(void) {
    SDL_UpdateTexture(texture, NULL, DG_ScreenBuffer,
                      DOOMGENERIC_RESX * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
    handleKeyInput();
}

void DG_SleepMs(uint32_t ms) {
    SDL_Delay(ms);
}

uint32_t DG_GetTicksMs(void) {
    return SDL_GetTicks();
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
        return 0;
    }
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

void DG_SetWindowTitle(const char* title) {
    if (window != NULL) {
        SDL_SetWindowTitle(window, title);
    }
}

// Wrapper for chrisplusplus: starts doom with a default WAD path
// chrisplusplus calls this from main()
void dg_start(const char* wad_path) {
    char* argv[3];
    argv[0] = "doom";
    argv[1] = "-iwad";
    argv[2] = (char*)wad_path;
    doomgeneric_Create(3, argv);

    while (1) {
        doomgeneric_Tick();
    }
}
