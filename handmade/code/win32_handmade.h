#pragma once
#include "defines.h"

struct Win32_OffscreenBuffer {
    BITMAPINFO info;
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
};

struct Win32_WindowDimension {
    i32 width;
    i32 height;
};

struct Win32_SoundOutput {
    i32 samplesPerSecond;
    u32 runningSampleIndex;
    i32 bytesPerSample;
    i32 secondaryBufferSize;
    float tSine;
    i32 latencySampleCount;
};