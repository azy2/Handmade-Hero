#undef UNICODE
#include "defines.h"
#include <Windows.h>

// TODO: This is a global for now.
global bool running = true;
global BITMAPINFO BitmapInfo;
global void *BitmapMemory;
global HBITMAP BitmapHandle;
global HDC BitmapDeviceContext;

local void Win32_ResizeDIBSection(i32 width, i32 height) {
    // TODO: Bulletproof this. Maybe don't free first, free after, then free
    // first if that fails

    if (BitmapHandle) {
        DeleteObject(BitmapHandle);
    }
    if (!BitmapDeviceContext) {
        // TODO: Should we recreate thes under certain special circumstances?
        BitmapDeviceContext = CreateCompatibleDC(0);
    }

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = width;
    BitmapInfo.bmiHeader.biHeight = height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    BitmapHandle = CreateDIBSection(BitmapDeviceContext, &BitmapInfo,
                                    DIB_RGB_COLORS, &BitmapMemory, 0, 0);
}

local void Win32_UpdateWindow(HDC DeviceContext, i32 x, i32 y, i32 width,
                              i32 height) {
    StretchDIBits(DeviceContext, x, y, width, height, x, y, width, height,
                  BitmapMemory, &BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
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
        Win32_UpdateWindow(DeviceContext, Paint.rcPaint.left, Paint.rcPaint.top,
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
        HWND WindowHandle = CreateWindowEx(
            0, WindowClass.lpszClassName, "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);

        if (WindowHandle) {
            running = true;
            MSG Msg;

            while (running) {
                BOOL MsgResult = GetMessage(&Msg, 0, 0, 0);
                if (MsgResult > 0) {
                    TranslateMessage(&Msg);
                    DispatchMessage(&Msg);
                } else {
                    break;
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