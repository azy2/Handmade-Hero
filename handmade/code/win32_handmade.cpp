#undef UNICODE
#include "defines.h"
#include <Windows.h>

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

Win32_WindowDimension Win32_GetWindowDimension(HWND Window) {
    Win32_WindowDimension result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    result.width = ClientRect.right - ClientRect.left;
    result.height = ClientRect.bottom - ClientRect.top;

    return result;
}

local void RenderWeirdGradient(Win32_OffscreenBuffer &buffer, i32 blueOffset,
                               i32 greenOffset) {
    u8 *row = (u8 *)buffer.memory;
    for (i32 y = 0; y < buffer.height; ++y) {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < buffer.width; ++x) {
            u8 blue = x + blueOffset;
            u8 green = y + greenOffset;
            *pixel++ = blue | (green << 8);
        }
        row += buffer.pitch;
    }
}

local void Win32_ResizeDIBSection(Win32_OffscreenBuffer &buffer, i32 width,
                                  i32 height) {
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

    buffer.memory =
        VirtualAlloc(0, 4 * width * height, MEM_COMMIT, PAGE_READWRITE);
    buffer.pitch = width * 4;
}

local void Win32_DisplayBufferInWindow(HDC DeviceContext, i32 windowWidth,
                                       i32 windowHeight,
                                       const Win32_OffscreenBuffer &buffer) {
    // TODO: Aspect ratio correction
    // TODO: Play with stretch modes
    StretchDIBits(DeviceContext, 0, 0, windowWidth, windowHeight, 0, 0,
                  buffer.width, buffer.height, buffer.memory, &buffer.info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32_MainWindowCallback(HWND Window, UINT Msg, WPARAM WParam,
                                          LPARAM LParam) {
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
        Win32_ResizeDIBSection(globalBackBuffer, win_dimension.width,
                               win_dimension.height);
    } break;

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);

        Win32_WindowDimension win_dimension = Win32_GetWindowDimension(Window);
        Win32_DisplayBufferInWindow(DeviceContext, win_dimension.width,
                                    win_dimension.height, globalBackBuffer);

        EndPaint(Window, &Paint);
    } break;

    default: { Result = DefWindowProc(Window, Msg, WParam, LParam); } break;
    }

    return Result;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine,
                     int ShowCode) {
    WNDCLASS WindowClass = {};

    Win32_ResizeDIBSection(globalBackBuffer, 1920, 1080);

    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32_MainWindowCallback;
    WindowClass.hInstance = Instance;
    //   WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(
            0, WindowClass.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);

        if (Window) {
            // NOTE: Since we specified CS_OWNDC, we can just get one device
            // context and use it forever because we are not sharing it with
            // anyone
            HDC DeviceContext = GetDC(Window);

            running = true;
            i32 xOffset = 0;
            i32 yOffset = 0;

            while (running) {
                MSG Msg;
                while (PeekMessage(&Msg, 0, 0, 0, PM_REMOVE)) {
                    if (Msg.message == WM_QUIT) {
                        running = false;
                    }
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                }
                RenderWeirdGradient(globalBackBuffer, xOffset++, yOffset++);

                Win32_WindowDimension win_dimension =
                    Win32_GetWindowDimension(Window);
                Win32_DisplayBufferInWindow(DeviceContext, win_dimension.width,
                                            win_dimension.height,
                                            globalBackBuffer);
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}