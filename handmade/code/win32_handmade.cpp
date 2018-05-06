#undef UNICODE
#include <Windows.h>

// TODO: This is a global for now.
static bool Running = true;

LRESULT CALLBACK WindowProc(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam) {
    LRESULT Result = 0;

    switch (Msg) {
        case WM_SIZE: {
            OutputDebugString("WM_SIZE\n");
        } break;

        case WM_DESTROY: {
            // TODO: Handle this with a message to the user?
            Running = false;
            OutputDebugString("WM_DESTROY\n");
        } break;

        case WM_CLOSE: {
            // TODO: Handle this as an error - recreate window?
            Running = false;
            OutputDebugString("WM_CLOSE\n");
        } break;

        case WM_ACTIVATEAPP: {
            OutputDebugString("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT: {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            PatBlt(DeviceContext,
                   Paint.rcPaint.left,
                   Paint.rcPaint.top,
                   Paint.rcPaint.right - Paint.rcPaint.left,
                   Paint.rcPaint.bottom - Paint.rcPaint.top,
                   WHITENESS);
            EndPaint(Window, &Paint);
        } break;

        default: {
            Result = DefWindowProc(Window, Msg, WParam, LParam);
        } break;
    }

    return Result;
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int ShowCode) {
    WNDCLASS WindowClass = {};

    // TODO: Check if HREDRAW/VREDRAW/OWNDC still matter
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = WindowProc;
    WindowClass.hInstance = Instance;
    //   WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) {
        HWND WindowHandle = CreateWindowEx(
            0, WindowClass.lpszClassName,
            "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT, CW_USEDEFAULT,
            CW_USEDEFAULT, CW_USEDEFAULT,
            0, 0, Instance, 0);
            
            if (WindowHandle) {
                Running = true;
                MSG Msg;

                while (Running) {
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