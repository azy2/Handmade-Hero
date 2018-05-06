#undef UNICODE
#include "defines.h"
#include <Windows.h>

// TODO: This is a global for now.
global bool running = true;
global BITMAPINFO BitmapInfo;
global void *BitmapMemory;
global i32 BitmapWidth;
global i32 BitmapHeight;

local void RenderWeirdGradient(i32 xOffset, i32 yOffset) {
    i32 pitch = BitmapWidth * 4;
    u8 *row = (u8 *)BitmapMemory;
    for (i32 y = 0; y < BitmapHeight; ++y) {
        u32 *pixel = (u32 *)row;
        for (i32 x = 0; x < BitmapWidth; ++x) {
            u8 blue = x + xOffset;
            u8 green = y + yOffset;
            *pixel++ = blue | (green << 8);
        }
        row += pitch;
    }
}

local void Win32_ResizeDIBSection(i32 width, i32 height) {
    // TODO: Bulletproof this. Maybe don't free first, free after, then free
    // first if that fails

    if (BitmapMemory) {
        VirtualFree(BitmapMemory, 0, MEM_RELEASE);
    }

    BitmapHeight = height;
    BitmapWidth = width;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = width;
    // NOTE: Making this negative makes the origin the top-left.
    BitmapInfo.bmiHeader.biHeight = -height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    BitmapMemory =
        VirtualAlloc(0, 4 * width * height, MEM_COMMIT, PAGE_READWRITE);
}

local void Win32_UpdateWindow(HDC DeviceContext, RECT *WindowRect, i32 x, i32 y,
                              i32 width, i32 height) {
    i32 WindowWidth = WindowRect->right - WindowRect->left;
    i32 WindowHeight = WindowRect->bottom - WindowRect->top;
    StretchDIBits(DeviceContext, 0, 0, BitmapWidth, BitmapHeight, 0, 0,
                  WindowWidth, WindowHeight, BitmapMemory, &BitmapInfo,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK WindowProc(HWND Window, UINT Msg, WPARAM WParam,
                            LPARAM LParam) {
    LRESULT Result = 0;

    switch (Msg) {
    case WM_SIZE: {
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        i32 width = ClientRect.right - ClientRect.left;
        i32 height = ClientRect.bottom - ClientRect.top;
        Win32_ResizeDIBSection(width, height);
        OutputDebugString("WM_SIZE\n");
    } break;

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

    case WM_PAINT: {
        PAINTSTRUCT Paint;
        HDC DeviceContext = BeginPaint(Window, &Paint);
        RECT ClientRect;
        GetClientRect(Window, &ClientRect);
        Win32_UpdateWindow(DeviceContext, &ClientRect, Paint.rcPaint.left,
                           Paint.rcPaint.top,
                           Paint.rcPaint.right - Paint.rcPaint.left,
                           Paint.rcPaint.bottom - Paint.rcPaint.top);
        EndPaint(Window, &Paint);
    } break;

    default: { Result = DefWindowProc(Window, Msg, WParam, LParam); } break;
    }

    return Result;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine,
                     int ShowCode) {
    WNDCLASS WindowClass = {};

    // TODO: Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    //   WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowEx(
            0, WindowClass.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);

        if (Window) {
            running = true;
            MSG Msg;
            i32 xOffset = 0;
            i32 yOffset = 0;

            while (running) {
                while (PeekMessage(&Msg, 0, 0, 0, PM_REMOVE)) {
                    if (Msg.message == WM_QUIT) {
                        running = false;
                    }
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                }
                RenderWeirdGradient(xOffset++, yOffset++);
                HDC DeviceContext = GetDC(Window);
                RECT ClientRect;
                GetClientRect(Window, &ClientRect);
                i32 WindowWidth = ClientRect.right - ClientRect.left;
                i32 WindowHeight = ClientRect.bottom - ClientRect.top;
                Win32_UpdateWindow(DeviceContext, &ClientRect, 0, 0,
                                   WindowWidth, WindowHeight);
                ReleaseDC(Window, DeviceContext);
            }
        } else {
            // TODO: Logging
        }
    } else {
        // TODO: Logging
    }

    return 0;
}