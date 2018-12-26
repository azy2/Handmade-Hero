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
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
           (len(input->controllers[0].buttons)));

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

    for (int controllerIndex = 0; controllerIndex < len(input->controllers); ++controllerIndex) {
        GameControllerInput *controller = getController(input, controllerIndex);
        if (controller->isAnalog) {
            gameState->blueOffset += (i32)(4.0f * controller->stickAverageX);
            gameState->toneHz = 256 + (i32)(128.0f * controller->stickAverageY);
        } else {
            if (controller->moveLeft.endedDown) {
                gameState->blueOffset -= 1;
            }
            if (controller->moveRight.endedDown) {
                gameState->blueOffset += 1;
            }
            if (controller->moveUp.endedDown) {
                gameState->toneHz += 1;
            }
            if (controller->moveDown.endedDown) {
                gameState->toneHz -= 1;
            }
        }

        if (controller->actionDown.endedDown) {
            gameState->greenOffset += 1;
        }
    }

    gameOutputSound(soundBuffer, gameState->toneHz);
    renderWeirdGradient(buffer, gameState->blueOffset, gameState->greenOffset);
}