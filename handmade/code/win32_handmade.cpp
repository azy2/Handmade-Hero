#undef UNICODE
#include "defines.h"
#include <Windows.h>
#include <dsound.h>
// TODO: Implement sine ourselves.
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <xinput.h>

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
global x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
global x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

local void Win32_LoadXInput() {
    HMODULE XInputLibrary = LoadLibrary("xinput1_4.dll");
    if (!XInputLibrary) {
        XInputLibrary = LoadLibrary("xinput1_3.dll");
    }
    if (XInputLibrary) {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

global LPDIRECTSOUNDBUFFER globalSecondaryBuffer;

local void Win32_InitDSound(HWND Window, i32 samplesPerSecond, i32 bufferSize) {
    // NOTE: Load the library
    HMODULE DSoundLibrary = LoadLibrary("dsound.dll");
    if (DSoundLibrary) {
        direct_sound_create *DirectSoundCreate =
            (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) {
            WAVEFORMATEX waveformat = {};
            waveformat.wFormatTag = WAVE_FORMAT_PCM;
            waveformat.nChannels = 2;
            waveformat.nSamplesPerSec = samplesPerSecond;
            waveformat.wBitsPerSample = 16;
            waveformat.nBlockAlign = (waveformat.nChannels * waveformat.wBitsPerSample) / 8;
            waveformat.nAvgBytesPerSec = waveformat.nSamplesPerSec * waveformat.nBlockAlign;
            waveformat.cbSize = 0;

            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) {
                DSBUFFERDESC bufferDescription = {};
                bufferDescription.dwSize = sizeof(bufferDescription);
                bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // NOTE: "Create" a primary buffer
                // TODO: DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER primaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0))) {
                    HRESULT error = primaryBuffer->SetFormat(&waveformat);
                    if (SUCCEEDED(error)) {
                        OutputDebugString("Primary buffer format was set.\n");
                    } else {
                        // TODO: Diagnostic
                    }
                } else {
                    // TODO: Diagnostic
                }
            } else {
                // TODO: Diagnostic
            }

            DSBUFFERDESC bufferDescription = {};
            bufferDescription.dwSize = sizeof(bufferDescription);
            bufferDescription.dwFlags = 0;
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveformat;
            HRESULT error = DirectSound->CreateSoundBuffer(&bufferDescription, &globalSecondaryBuffer, 0);
            if (SUCCEEDED(error)) {
                OutputDebugString("Secondary buffer created successfully.\n");
            }
        } else {
            // TODO: Diagnostic
        }
    } else {
        // TODO: Diagnostic
    }
}

void odprintf(const char *format, ...) {
    char buf[4096], *p = buf;
    va_list args;
    int n;

    va_start(args, format);
    n = _vsnprintf(p, sizeof(buf) - 3, format, args); // buf - 3 is room for CR/LF/NULL
    va_end(args);

    p += (n < 0) ? sizeof(buf - 3) : n;

    while (p > buf && isspace(p[-1])) {
        *--p = '\0';
    }

    *p++ = '\r';
    *p++ = '\n';
    *p++ = '\0';

    OutputDebugString(buf);
}

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

// TODO: This is a global for now.
global bool running = true;
global Win32_OffscreenBuffer globalBackBuffer;

local Win32_WindowDimension Win32_GetWindowDimension(const HWND Window) {
    Win32_WindowDimension result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    result.width = ClientRect.right - ClientRect.left;
    result.height = ClientRect.bottom - ClientRect.top;

    return result;
}

local void RenderWeirdGradient(Win32_OffscreenBuffer &buffer, const i32 blueOffset, const i32 greenOffset) {
    u8 *row = (u8 *)buffer.memory;
    for (i32 y = 0; y < buffer.height; ++y) {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < buffer.width; ++x) {
            u8 blue = (u8)(x + blueOffset);
            u8 green = (u8)(y + greenOffset);
            *pixel++ = blue | (green << 8);
        }
        row += buffer.pitch;
    }
}

local void Win32_ResizeDIBSection(Win32_OffscreenBuffer &buffer, const i32 width, const i32 height) {
    // TODO: Bulletproof this. Maybe don't free first, free after, then free
    // first if that fails

    if (buffer.memory) {
        VirtualFree(buffer.memory, 0, MEM_RELEASE);
    }

    buffer.width = width;
    buffer.height = height;

    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = width;
    // NOTE: Making this negative makes the origin the top-left.
    buffer.info.bmiHeader.biHeight = -height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    buffer.memory = VirtualAlloc(0, 4 * width * height, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    buffer.pitch = width * 4;
}

local void Win32_DisplayBufferInWindow(const HDC DeviceContext, const i32 windowWidth, const i32 windowHeight,
                                       const Win32_OffscreenBuffer &buffer) {
    // TODO: Aspect ratio correction
    // TODO: Play with stretch modes
    StretchDIBits(DeviceContext, 0, 0, windowWidth, windowHeight, 0, 0, buffer.width, buffer.height, buffer.memory,
                  &buffer.info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32_MainWindowCallback(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Msg) {
    case WM_DESTROY: {
        // TODO: Handle this with a message to the user?
        running = false;
        OutputDebugString("WM_DESTROY\n");
    } break;

    case WM_CLOSE: {
        // TODO: Handle this as an error - recreate window?
        running = false;
        OutputDebugString("WM_CLOSE\n");
    } break;

    case WM_ACTIVATEAPP: {
        OutputDebugString("WM_ACTIVATEAPP\n");
    } break;

    case WM_SIZE: {
        Win32_WindowDimension win_dimension = Win32_GetWindowDimension(Window);
        Win32_ResizeDIBSection(globalBackBuffer, win_dimension.width, win_dimension.height);
    } break;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);

        Win32_WindowDimension win_dimension = Win32_GetWindowDimension(Window);
        Win32_DisplayBufferInWindow(DeviceContext, win_dimension.width, win_dimension.height, globalBackBuffer);

        EndPaint(Window, &Paint);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        WPARAM VKCode = WParam;
        bool wasDown = ((LParam & (1 << 30)) != 0);
        bool isDown = ((LParam & (1 << 31)) == 0);

        bool32 altDown = LParam & (1 << 29);

        if (wasDown != isDown) {
            if (VKCode == VK_OEM_COMMA) {

            } else if (VKCode == 'A') {

            } else if (VKCode == 'O') {

            } else if (VKCode == 'E') {
            } else if (VKCode == VK_OEM_7) { // '/"
            } else if (VKCode == VK_OEM_PERIOD) {

            } else if (VKCode == VK_UP) {

            } else if (VKCode == VK_LEFT) {

            } else if (VKCode == VK_DOWN) {

            } else if (VKCode == VK_RIGHT) {

            } else if (VKCode == VK_ESCAPE) {

            } else if (VKCode == VK_SPACE) {
            } else if (VKCode == VK_F4 && altDown) {
                running = false;
            }
        }
    } break;

    default: { Result = DefWindowProc(Window, Msg, WParam, LParam); } break;
    }

    return Result;
}

struct Win32_SoundOutput {
    i32 samplesPerSecond;
    i32 Hz;
    i16 toneVolume;
    u32 runningSampleIndex;
    i32 wavePeriod;
    i32 bytesPerSample;
    i32 secondaryBufferSize;
    i32 latencySampleCount;
};

void Win32_FillSoundBuffer(Win32_SoundOutput &soundOutput, DWORD byteToLock, DWORD bytesToWrite) {
    // TODO: More strenuous test!
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;

    if (SUCCEEDED(
            globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
        // TODO: assert that region1/2Size is valid
        i16 *sampleOut = (i16 *)region1;
        DWORD region1SampleCount = region1Size / soundOutput.bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex, ++soundOutput.runningSampleIndex) {
            float t = 2.0f * PI32 * (float)soundOutput.runningSampleIndex / (float)soundOutput.wavePeriod;
            float sineValue = sinf(t);
            i16 sampleValue = (i16)(sineValue * soundOutput.toneVolume);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;
        }
        sampleOut = (i16 *)region2;
        DWORD region2SampleCount = region2Size / soundOutput.bytesPerSample;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex, ++soundOutput.runningSampleIndex) {
            float t = 2.0f * PI32 * (float)soundOutput.runningSampleIndex / (float)soundOutput.wavePeriod;
            float sineValue = sinf(t);
            i16 sampleValue = (i16)(sineValue * soundOutput.toneVolume);
            *sampleOut++ = sampleValue;
            *sampleOut++ = sampleValue;
        }

        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCode) {

    Win32_LoadXInput();

    WNDCLASS WindowClass = {};

    Win32_ResizeDIBSection(globalBackBuffer, 1920, 1080);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32_MainWindowCallback;
    WindowClass.hInstance = Instance;
    //   WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);

        if (Window) {
            // NOTE: Since we specified CS_OWNDC, we can just get one device
            // context and use it forever because we are not sharing it with
            // anyone
            HDC DeviceContext = GetDC(Window);

            Win32_SoundOutput soundOutput = {};
            soundOutput.samplesPerSecond = 48000;
            soundOutput.Hz = 421;
            soundOutput.toneVolume = 6000;
            soundOutput.runningSampleIndex = 0;
            soundOutput.wavePeriod = soundOutput.samplesPerSecond / soundOutput.Hz;
            soundOutput.bytesPerSample = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

            Win32_InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            Win32_FillSoundBuffer(soundOutput, 0, soundOutput.secondaryBufferSize);
            globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            running = true;
            i32 xOffset = 0;
            i32 yOffset = 0;

            i64 lastCycleCount = __rdtsc();

            LARGE_INTEGER liperfFrequency;
            QueryPerformanceFrequency(&liperfFrequency);
            i64 perfFrequency = liperfFrequency.QuadPart;

            LARGE_INTEGER lastCounter;
            QueryPerformanceCounter(&lastCounter);

            while (running) {
                MSG Msg;
                while (PeekMessage(&Msg, 0, 0, 0, PM_REMOVE)) {
                    if (Msg.message == WM_QUIT) {
                        running = false;
                    }
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                }

                // TODO: Should we poll this more frequently
                // TODO: XInputGetState stalls for unconnected gamepads. Only poll pads we know are connected. Use
                // HID interrupts to keep track of pads connected.
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex) {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS) {
                        // NOTE: This controller is plugged in
                        // TODO: See if ControllerState.dwPacketNumber
                        // increments too rapidly
                        XINPUT_GAMEPAD *pad = &ControllerState.Gamepad;

                        bool Up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                        bool Down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                        bool Left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                        bool Right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
                        bool Start = pad->wButtons & XINPUT_GAMEPAD_START;
                        bool Back = pad->wButtons & XINPUT_GAMEPAD_BACK;
                        bool LeftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
                        bool RightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
                        bool AButton = pad->wButtons & XINPUT_GAMEPAD_A;
                        bool BButton = pad->wButtons & XINPUT_GAMEPAD_B;
                        bool XButton = pad->wButtons & XINPUT_GAMEPAD_X;
                        bool YButton = pad->wButtons & XINPUT_GAMEPAD_Y;

                        i16 StickX = pad->sThumbLX;
                        i16 StackY = pad->sThumbLY;

                        if (AButton) {
                            xOffset++;
                        }
                    } else {
                        // NOTE: This controller is not available
                    }
                }

                // XINPUT_VIBRATION Vibration;
                // Vibration.wLeftMotorSpeed = 1000;
                // Vibration.wRightMotorSpeed = 1000;
                // XInputSetState(0, &Vibration);

                RenderWeirdGradient(globalBackBuffer, xOffset, yOffset);

                // NOTE: DirectSound output test
                DWORD playCursor;
                DWORD writeCursor;
                if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
                    DWORD byteToLock =
                        (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                    DWORD targetCursor = ((playCursor + (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
                                          soundOutput.secondaryBufferSize);

                    DWORD bytesToWrite = 0;
                    // TODO: Change this to using a lower latency offset from the playcursor
                    // when we actually start having sound effects.
                    if (byteToLock > targetCursor) {
                        bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                        bytesToWrite += targetCursor;
                    } else {
                        bytesToWrite = targetCursor - byteToLock;
                    }

                    Win32_FillSoundBuffer(soundOutput, byteToLock, bytesToWrite);
                }

                Win32_WindowDimension win_dimension = Win32_GetWindowDimension(Window);
                Win32_DisplayBufferInWindow(DeviceContext, win_dimension.width, win_dimension.height, globalBackBuffer);

                i64 endCycleCount = __rdtsc();

                LARGE_INTEGER endCounter;
                QueryPerformanceCounter(&endCounter);
                i64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;

                float msPerFrame = ((1000.0f * counterElapsed) / perfFrequency);
                float fps = ((float)perfFrequency / counterElapsed);
                float megaCyclesPerFrame = ((endCycleCount - lastCycleCount) / (1000.0f * 1000.0f));

                odprintf("%.02fms/f | %.02ff/s | %.02fMc/f", msPerFrame, fps, megaCyclesPerFrame);

                lastCounter = endCounter;
                lastCycleCount = endCycleCount;
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}
