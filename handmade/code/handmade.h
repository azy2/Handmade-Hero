#pragma once

#include "defines.h"

/*
    NOTE:

    HANDMADE_INTERNAL:
    0 - Build for public release
    1 - Build for developer only

    HANDMADE_SLOW:
    0 - No slow code allowed!
    1 - Slow code welcome.
*/

#if HANDMADE_SLOW
#define Assert(exp)                                                                                                    \
    if (!(exp)) {                                                                                                      \
        *(int *)0 = 0;                                                                                                 \
    }
#else
#define Assert(exp)
#endif

template <class T> constexpr T Kilobytes(T v) {
    return v * 1024;
}

template <class T> constexpr T Megabytes(T v) {
    return Kilobytes(v) * 1024;
}

template <class T> constexpr T Gigabytes(T v) {
    return Megabytes(v) * 1024;
}

template <class T> constexpr T Terabytes(T v) {
    return Gigabytes(v) * 1024;
}

#define len(array) (sizeof(array) / sizeof((array)[0]))
// template <class T> constexpr int len(T array) {
//     Assert(array != nullptr);
//     return sizeof(array) / sizeof(array[0]);
// }

constexpr u32 safeTruncateU64(u64 value) {
    Assert(value <= 0xFFFFFFFF);
    return (u32)value;
}

#if HANDMADE_INTERNAL
/* IMPORTANT:

    These are NOT for doing anything in the shipping game - they are
    blocking and the write doesn't protect against lost data!
*/

struct DebugReadFileResult {
    u32 size;
    void *contents;
};

local DebugReadFileResult DEBUGPlatform_readEntireFile(char *filename);
local void DEBUGPlatform_freeFileMemory(void *memory);
local bool32 DEBUGPlatform_writeEntireFile(char *filename, u32 memorySize, void *memory);
#endif

struct GameOffscreenBuffer {
    // NOTE: Pixels are always 32-bits wide, Memory order: BB GG RR XX
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
};

struct GameSoundOutputBuffer {
    i32 samplesPerSecond;
    i32 sampleCount;
    i16 *samples;
};

struct GameButtonState {
    i32 halfTransitionCount;
    bool32 endedDown;
};

struct GameControllerInput {
    bool32 isConnected;
    bool32 isAnalog;
    float stickAverageX;
    float stickAverageY;

    union {
        GameButtonState buttons[12];
        struct {
            GameButtonState moveUp;
            GameButtonState moveDown;
            GameButtonState moveLeft;
            GameButtonState moveRight;

            GameButtonState actionUp;
            GameButtonState actionDown;
            GameButtonState actionLeft;
            GameButtonState actionRight;

            GameButtonState leftShoulder;
            GameButtonState rightShoulder;

            GameButtonState back;
            GameButtonState start;

            // NOTE: All buttons must be added above this line

            GameButtonState terminator;
        };
    };
};

struct GameInput {
    // TODO: Insert clock values here.
    GameControllerInput controllers[5];
};

inline GameControllerInput *getController(GameInput *input, u32 controllerIndex) {
    Assert(controllerIndex < (u32)len(input->controllers));

    return &input->controllers[controllerIndex];
}

struct GameMemory {
    bool32 isInitialized;

    u64 permanentStorageSize;
    void *permanentStorage; // NOTE: REQUIRED to be cleared to zero at startup

    u64 transientStorageSize;
    void *transientStorage; // NOTE:: REQUIRED to be cleared to zero at startup
};

local void game_updateAndRender(GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer,
                                GameSoundOutputBuffer *soundBuffer);

struct GameState {
    i32 toneHz;
    i32 greenOffset;
    i32 blueOffset;
};