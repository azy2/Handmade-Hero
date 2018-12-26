#include "handmade.h"
#include <cmath>

local void gameOutputSound(GameSoundOutputBuffer *soundBuffer, const i32 toneHz) {
    persist float tSine;
    const i16 toneVolume = 3000;
    const i32 wavePeriod = soundBuffer->samplesPerSecond / toneHz;

    i16 *sampleOut = soundBuffer->samples;
    for (i32 sampleIndex = 0; sampleIndex < soundBuffer->sampleCount; ++sampleIndex) {
        float sineValue = sinf(tSine);
        i16 sampleValue = (i16)(sineValue * toneVolume);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;

        tSine += 2.0f * PI32 * 1.0f / (float)wavePeriod;
    }
}

local void renderWeirdGradient(GameOffscreenBuffer *buffer, i32 blueOffset, i32 greenOffset) {
    u8 *row = (u8 *)buffer->memory;

    for (i32 y = 0; y < buffer->height; ++y) {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < buffer->width; ++x) {
            u8 blue = (u8)(x + blueOffset);
            u8 green = (u8)(y + greenOffset);

            *pixel++ = (green << 8) | blue;
        }

        row += buffer->pitch;
    }
}

local void game_updateAndRender(GameMemory *memory, GameInput *input, GameOffscreenBuffer *buffer,
                                GameSoundOutputBuffer *soundBuffer) {
    Assert(sizeof(GameState) <= memory->permanentStorageSize);

    GameState *gameState = (GameState *)memory->permanentStorage;
    if (!memory->isInitialized) {
        char *filename = __FILE__;

        DebugReadFileResult file = DEBUGPlatform_readEntireFile(filename);
        if (file.contents) {
            DEBUGPlatform_writeEntireFile("test.out", file.size, file.contents);
            DEBUGPlatform_freeFileMemory(file.contents);
        }

        gameState->toneHz = 256;

        // TODO: This may be more appropriate to do in the platform layer.
        memory->isInitialized = true;
    }

    GameControllerInput *input0 = &input->controllers[0];
    if (input0->isAnalog) {
        // NOTE: Use analog movement tuning
        gameState->blueOffset += (i32)(4.0f * input0->endX);
        gameState->toneHz += (i32)(128.0f * input0->endY);
    } else {
        // NOTE: Use digital movement tuning
    }

    if (input0->down.endedDown) {
        gameState->greenOffset += 1;
    }

    gameOutputSound(soundBuffer, gameState->toneHz);
    renderWeirdGradient(buffer, gameState->blueOffset, gameState->greenOffset);
}