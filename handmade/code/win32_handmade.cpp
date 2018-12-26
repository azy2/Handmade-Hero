#undef UNICODE

#include "handmade.cpp"
#include "handmade.h"

#include <stdio.h>

#include <Windows.h>
#include <dsound.h>
#include <malloc.h>
#include <stdarg.h>
#include <xinput.h>

#include "win32_handmade.h"

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
    n = _vsnprintf_s(p, sizeof(buf) - 3, sizeof(buf) - 3, format, args); // buf - 3 is room for CR/LF/NULL
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

local void Win32_DisplayBufferInWindow(const Win32_OffscreenBuffer *buffer, const HDC DeviceContext,
                                       const i32 windowWidth, const i32 windowHeight) {
    // TODO: Aspect ratio correction
    // TODO: Play with stretch modes
    StretchDIBits(DeviceContext, 0, 0, windowWidth, windowHeight, 0, 0, buffer->width, buffer->height, buffer->memory,
                  &buffer->info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32_MainWindowCallback(HWND Window, UINT msg, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (msg) {
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
        Win32_DisplayBufferInWindow(&globalBackBuffer, DeviceContext, win_dimension.width, win_dimension.height);

        EndPaint(Window, &Paint);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
        Assert(!"Keyboard event through non-standard channel");
    } break;

    default: { Result = DefWindowProc(Window, msg, WParam, LParam); } break;
    }

    return Result;
}

void Win32_ClearBuffer(Win32_SoundOutput *soundOutput) {
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;
    if (SUCCEEDED(globalSecondaryBuffer->Lock(0, soundOutput->secondaryBufferSize, &region1, &region1Size, &region2,
                                              &region2Size, 0))) {
        // TODO(casey): assert that region1Size/region2Size is valid
        u8 *destSample = (u8 *)region1;
        for (DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex) {
            *destSample++ = 0;
        }

        destSample = (u8 *)region2;
        for (DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex) {
            *destSample++ = 0;
        }

        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

void Win32_FillSoundBuffer(Win32_SoundOutput *soundOutput, DWORD byteToLock, DWORD bytesToWrite,
                           GameSoundOutputBuffer *sourceBuffer) {
    // TODO: More strenuous test!
    void *region1;
    DWORD region1Size;
    void *region2;
    DWORD region2Size;

    if (SUCCEEDED(
            globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
        // TODO: assert that region1/2Size is valid
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        i16 *destSample = (i16 *)region1;
        i16 *sourceSample = sourceBuffer->samples;
        for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }

        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        destSample = (i16 *)region2;
        for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }

        globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
    }
}

local void Win32_processXInputDigitalButton(DWORD XInputButtonState, GameButtonState *oldState, DWORD buttonBit,
                                            GameButtonState *newState) {
    newState->endedDown = ((XInputButtonState & buttonBit) == buttonBit);
    newState->halfTransitionCount += (oldState->endedDown != newState->endedDown) ? 1 : 0;
}

local void Win32_processKeyboardMessage(GameButtonState *newState, bool isDown) {
    newState->endedDown = isDown;
    ++newState->halfTransitionCount;
}

void Win32_processPendingMessages(GameControllerInput *keyboardController) {
    MSG msg;
    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
        switch (msg.message) {
        case WM_QUIT: {
            running = false;
        } break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
            WPARAM VKCode = msg.wParam;
            bool wasDown = ((msg.lParam & (1 << 30)) != 0);
            bool isDown = ((msg.lParam & (1 << 31)) == 0);

            bool32 altDown = msg.lParam & (1 << 29);

            if (wasDown != isDown) {
                if (VKCode == VK_OEM_COMMA) {

                } else if (VKCode == 'A') {

                } else if (VKCode == 'O') {

                } else if (VKCode == 'E') {
                } else if (VKCode == VK_OEM_7) { // '/"
                    Win32_processKeyboardMessage(&keyboardController->leftShoulder, isDown);
                } else if (VKCode == VK_OEM_PERIOD) {
                    Win32_processKeyboardMessage(&keyboardController->rightShoulder, isDown);
                } else if (VKCode == VK_UP) {
                    Win32_processKeyboardMessage(&keyboardController->up, isDown);
                } else if (VKCode == VK_LEFT) {
                    Win32_processKeyboardMessage(&keyboardController->left, isDown);
                } else if (VKCode == VK_DOWN) {
                    Win32_processKeyboardMessage(&keyboardController->down, isDown);
                } else if (VKCode == VK_RIGHT) {
                    Win32_processKeyboardMessage(&keyboardController->right, isDown);
                } else if (VKCode == VK_ESCAPE) {
                    running = false;
                } else if (VKCode == VK_SPACE) {
                } else if (VKCode == VK_F4 && altDown) {
                    running = false;
                }
            }
            break;
        }
        default: {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } break;
        }
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCode) {

    LARGE_INTEGER liperfFrequency;
    QueryPerformanceFrequency(&liperfFrequency);
    i64 perfFrequency = liperfFrequency.QuadPart;

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
            soundOutput.runningSampleIndex = 0;
            soundOutput.bytesPerSample = sizeof(i16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount = soundOutput.samplesPerSecond / 15;

            Win32_InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize);
            Win32_ClearBuffer(&soundOutput);
            globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            running = true;

            i16 *samples =
                (i16 *)VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID baseAddress = (LPVOID)Terabytes(2LL);
#else
            LPVOID baseAddress = 0;
#endif

            GameMemory gameMemory = {};
            gameMemory.permanentStorageSize = Megabytes(64);
            gameMemory.transientStorageSize = Gigabytes(4);

            u64 totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
            gameMemory.permanentStorage =
                VirtualAlloc(baseAddress, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.transientStorage = ((u8 *)gameMemory.permanentStorage + gameMemory.permanentStorageSize);

            if (samples && gameMemory.permanentStorage && gameMemory.transientStorage) {
                GameInput input[2] = {};
                GameInput *newInput = &input[0];
                GameInput *oldInput = &input[1];

                i64 lastCycleCount = __rdtsc();
                LARGE_INTEGER lastCounter;
                QueryPerformanceCounter(&lastCounter);

                while (running) {
                    GameControllerInput *keyboardController = &newInput->controllers[0];
                    GameControllerInput zeroController = {};
                    *keyboardController = zeroController;

                    Win32_processPendingMessages(keyboardController);

                    // TODO: Should we poll this more frequently
                    // TODO: XInputGetState stalls for unconnected gamepads. Only poll pads we know are connected.
                    // Use HID interrupts to keep track of pads connected.
                    DWORD maxControllerCount = XUSER_MAX_COUNT;
                    if (maxControllerCount > (DWORD)(len(newInput->controllers))) {
                        maxControllerCount = len(newInput->controllers);
                    }

                    for (DWORD controllerIndex = 0; controllerIndex < maxControllerCount; ++controllerIndex) {
                        GameControllerInput *oldController = &oldInput->controllers[controllerIndex];
                        GameControllerInput *newController = &newInput->controllers[controllerIndex];

                        XINPUT_STATE ControllerState;
                        if (XInputGetState(controllerIndex, &ControllerState) == ERROR_SUCCESS) {
                            // NOTE: This controller is plugged in
                            // TODO: See if ControllerState.dwPacketNumber
                            // increments too rapidly
                            XINPUT_GAMEPAD *pad = &ControllerState.Gamepad;

                            bool32 up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
                            bool32 down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
                            bool32 left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
                            bool32 right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;

                            newController->isAnalog = true;
                            newController->startX = oldController->endX;
                            newController->startY = oldController->endY;

                            // TODO: Dead zone processing!!
                            // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE
                            // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE

                            // TODO: min/max fun
                            // TODO: collapse to single function
                            float x = (float)pad->sThumbLX / ((pad->sThumbLX < 0) ? 32768.0f : 32767.0f);
                            newController->minX = newController->maxX = newController->endX = x;

                            float y = (float)pad->sThumbLY / ((pad->sThumbLY < 0) ? 32768.0f : 32767.0f);
                            newController->minY = newController->maxY = newController->endY = y;

                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->down, XINPUT_GAMEPAD_A,
                                                             &newController->down);
                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->right, XINPUT_GAMEPAD_B,
                                                             &newController->right);
                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->left, XINPUT_GAMEPAD_X,
                                                             &newController->left);
                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->up, XINPUT_GAMEPAD_Y,
                                                             &newController->up);
                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->leftShoulder,
                                                             XINPUT_GAMEPAD_LEFT_SHOULDER,
                                                             &newController->leftShoulder);
                            Win32_processXInputDigitalButton(pad->wButtons, &oldController->rightShoulder,
                                                             XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                                             &newController->rightShoulder);
                        } else {
                            // NOTE: This controller is not available
                        }
                    }

                    // XINPUT_VIBRATION Vibration;
                    // Vibration.wLeftMotorSpeed = 1000;
                    // Vibration.wRightMotorSpeed = 1000;
                    // XInputSetState(0, &Vibration);

                    // NOTE: DirectSound output test
                    DWORD byteToLock = 0;
                    DWORD targetCursor = 0;
                    DWORD bytesToWrite = 0;
                    DWORD playCursor = 0;
                    DWORD writeCursor = 0;
                    bool32 soundIsValid = false;
                    if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))) {
                        byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) %
                                     soundOutput.secondaryBufferSize;
                        targetCursor = ((playCursor + (soundOutput.latencySampleCount * soundOutput.bytesPerSample)) %
                                        soundOutput.secondaryBufferSize);

                        // TODO: Change this to using a lower latency offset from the playcursor
                        // when we actually start having sound effects.
                        if (byteToLock > targetCursor) {
                            bytesToWrite = soundOutput.secondaryBufferSize - byteToLock;
                            bytesToWrite += targetCursor;
                        } else {
                            bytesToWrite = targetCursor - byteToLock;
                        }

                        soundIsValid = true;
                    }

                    GameSoundOutputBuffer soundBuffer = {};
                    soundBuffer.samplesPerSecond = soundOutput.samplesPerSecond;
                    soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.samples = samples;

                    GameOffscreenBuffer buffer = {};
                    buffer.memory = globalBackBuffer.memory;
                    buffer.width = globalBackBuffer.width;
                    buffer.height = globalBackBuffer.height;
                    buffer.pitch = globalBackBuffer.pitch;
                    game_updateAndRender(&gameMemory, newInput, &buffer, &soundBuffer);

                    if (soundIsValid) {
                        Win32_FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                    }

                    Win32_WindowDimension dimension = Win32_GetWindowDimension(Window);
                    Win32_DisplayBufferInWindow(&globalBackBuffer, DeviceContext, dimension.width, dimension.height);

                    i64 endCycleCount = __rdtsc();

                    LARGE_INTEGER endCounter;
                    QueryPerformanceCounter(&endCounter);
                    i64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;

                    float msPerFrame = ((1000.0f * counterElapsed) / perfFrequency);
                    float fps = ((float)perfFrequency / counterElapsed);
                    float megaCyclesPerFrame = ((endCycleCount - lastCycleCount) / (1000.0f * 1000.0f));

#if HANDMADE_INTERNAL
                    odprintf("%.02fms/f | %.02ff/s | %.02fMc/f", msPerFrame, fps, megaCyclesPerFrame);
#endif

                    lastCounter = endCounter;
                    lastCycleCount = endCycleCount;

                    GameInput *temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;
                }
            }

        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }
    return 0;
}

DebugReadFileResult DEBUGPlatform_readEntireFile(char *filename) {
    DebugReadFileResult result = {};

    HANDLE fileHandle = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER fileSize;
        if (GetFileSizeEx(fileHandle, &fileSize)) {
            u32 fileSize32 = safeTruncateU64(fileSize.QuadPart);
            result.contents = VirtualAlloc(0, fileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (result.contents) {
                DWORD bytesRead;
                if (ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) && fileSize32 == bytesRead) {
                    // NOTE: File read successfully
                    result.size = fileSize32;
                } else {
                    // TODO: Logging
                    DEBUGPlatform_freeFileMemory(result.contents);
                    result.contents = 0;
                }
            } else {
                // TODO: Logging
            }
        } else {
            // TODO: Logging
        }

        CloseHandle(fileHandle);
    } else {
        // TODO: Logging
    }

    return result;
}

void DEBUGPlatform_freeFileMemory(void *memory) {
    if (memory) {
        VirtualFree(memory, 0, MEM_RELEASE);
    }
}

bool32 DEBUGPlatform_writeEntireFile(char *filename, u32 memorySize, void *memory) {
    bool32 result = false;

    HANDLE fileHandle = CreateFile(filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (fileHandle != INVALID_HANDLE_VALUE) {
        DWORD bytesWritten;
        if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0)) {
            result = (bytesWritten == memorySize);
        } else {
            // TODO: Logging
        }

        CloseHandle(fileHandle);
    } else {
        // TODO: Logging
    }

    return result;
}