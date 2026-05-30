/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include "Rr_Platform.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_PLATFORM
#include "Rr_LogMacro.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Utility.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <windowsx.h>
#define COBJMACROS
#include <dxgi1_6.h>

#include <vulkan/vulkan_win32.h>

/* TODO: Add wheel events! */

#define RR_WIN32_FULLSCREEN_EXSTYLE (WS_EX_APPWINDOW | WS_EX_ACCEPTFILES)
#define RR_WIN32_FULLSCREEN_STYLE \
    (WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN)
#define RR_WIN32_WINDOWED_EXSTYLE \
    (WS_EX_APPWINDOW | WS_EX_WINDOWEDGE | WS_EX_ACCEPTFILES)
#define RR_WIN32_WINDOWED_STYLE \
    (WS_VISIBLE | WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN)

static struct
{
    bool Initialized;

    HINSTANCE Instance;

    HWND Window;
    RECT WindowedRect;

    HCURSOR Cursors[RR_CURSOR_TYPE_COUNT];

    uint32_t HighSurrogate;

    IDXGIFactory *DXGIFactory;

    HMODULE VulkanModule;
    PFN_vkGetInstanceProcAddr VkGetInstanceProcAddr;
    PFN_vkCreateWin32SurfaceKHR VkCreateWin32SurfaceKHR;
} gWin32;

static LPCWSTR const RR_WIN32_CLASS_NAME = L"Rr.Win32.Class";

static inline Rr_Scancode Rr_Win32KeyToScancode(WPARAM WParam, LPARAM LParam)
{
    static const Rr_Scancode Mapping[RR_SCANCODE_COUNT] = {
        [0x1E] = RR_SCANCODE_A,
        [0x30] = RR_SCANCODE_B,
        [0x2E] = RR_SCANCODE_C,
        [0x20] = RR_SCANCODE_D,
        [0x12] = RR_SCANCODE_E,
        [0x21] = RR_SCANCODE_F,
        [0x22] = RR_SCANCODE_G,
        [0x23] = RR_SCANCODE_H,
        [0x17] = RR_SCANCODE_I,
        [0x24] = RR_SCANCODE_J,
        [0x25] = RR_SCANCODE_K,
        [0x26] = RR_SCANCODE_L,
        [0x32] = RR_SCANCODE_M,
        [0x31] = RR_SCANCODE_N,
        [0x18] = RR_SCANCODE_O,
        [0x19] = RR_SCANCODE_P,
        [0x10] = RR_SCANCODE_Q,
        [0x13] = RR_SCANCODE_R,
        [0x1F] = RR_SCANCODE_S,
        [0x14] = RR_SCANCODE_T,
        [0x16] = RR_SCANCODE_U,
        [0x2F] = RR_SCANCODE_V,
        [0x11] = RR_SCANCODE_W,
        [0x2D] = RR_SCANCODE_X,
        [0x15] = RR_SCANCODE_Y,
        [0x2C] = RR_SCANCODE_Z,
        [0x02] = RR_SCANCODE_1,
        [0x03] = RR_SCANCODE_2,
        [0x04] = RR_SCANCODE_3,
        [0x05] = RR_SCANCODE_4,
        [0x06] = RR_SCANCODE_5,
        [0x07] = RR_SCANCODE_6,
        [0x08] = RR_SCANCODE_7,
        [0x09] = RR_SCANCODE_8,
        [0x0A] = RR_SCANCODE_9,
        [0x0B] = RR_SCANCODE_0,
        [0x1C] = RR_SCANCODE_RETURN,
        [0x01] = RR_SCANCODE_ESCAPE,
        [0x0E] = RR_SCANCODE_BACKSPACE,
        [0x0F] = RR_SCANCODE_TAB,
        [0x39] = RR_SCANCODE_SPACE,
        [0x0C] = RR_SCANCODE_MINUS,
        [0x0D] = RR_SCANCODE_EQUALS,
        [0x1A] = RR_SCANCODE_LEFT_BRACKET,
        [0x1B] = RR_SCANCODE_RIGHT_BRACKET,
        [0x2B] = RR_SCANCODE_BACKSLASH,
        [0x27] = RR_SCANCODE_SEMICOLON,
        [0x28] = RR_SCANCODE_APOSTROPHE,
        [0x29] = RR_SCANCODE_GRAVE_TILDE,
        [0x33] = RR_SCANCODE_COMMA,
        [0x34] = RR_SCANCODE_PERIOD,
        [0x35] = RR_SCANCODE_SLASH,
        [0x3A] = RR_SCANCODE_CAPS_LOCK,
        [0x3B] = RR_SCANCODE_F1,
        [0x3C] = RR_SCANCODE_F2,
        [0x3D] = RR_SCANCODE_F3,
        [0x3E] = RR_SCANCODE_F4,
        [0x3F] = RR_SCANCODE_F5,
        [0x40] = RR_SCANCODE_F6,
        [0x41] = RR_SCANCODE_F7,
        [0x42] = RR_SCANCODE_F8,
        [0x43] = RR_SCANCODE_F9,
        [0x44] = RR_SCANCODE_F10,
        [0x57] = RR_SCANCODE_F11,
        [0x58] = RR_SCANCODE_F12,
        [0x54] = RR_SCANCODE_PRINT_SCREEN, /* NOTE: Duplicated! */
        [0x46] = RR_SCANCODE_SCROLL_LOCK,
        [0x45] = RR_SCANCODE_PAUSE,
        [0x37] = RR_SCANCODE_KP_MULTIPLY,
        [0x4A] = RR_SCANCODE_KP_MINUS,
        [0x4E] = RR_SCANCODE_KP_PLUS,
        [0x4F] = RR_SCANCODE_KP_1,
        [0x50] = RR_SCANCODE_KP_2,
        [0x51] = RR_SCANCODE_KP_3,
        [0x4B] = RR_SCANCODE_KP_4,
        [0x4C] = RR_SCANCODE_KP_5,
        [0x4D] = RR_SCANCODE_KP_6,
        [0x47] = RR_SCANCODE_KP_7,
        [0x48] = RR_SCANCODE_KP_8,
        [0x49] = RR_SCANCODE_KP_9,
        [0x52] = RR_SCANCODE_KP_0,
        [0x53] = RR_SCANCODE_KP_PERIOD_DELETE,
        [0x56] = RR_SCANCODE_ISO_BACKSLASH,
        [0x64] = RR_SCANCODE_F13,
        [0x65] = RR_SCANCODE_F14,
        [0x66] = RR_SCANCODE_F15,
        [0x67] = RR_SCANCODE_F16,
        [0x68] = RR_SCANCODE_F17,
        [0x69] = RR_SCANCODE_F18,
        [0x6A] = RR_SCANCODE_F19,
        [0x6B] = RR_SCANCODE_F20,
        [0x6C] = RR_SCANCODE_F21,
        [0x6D] = RR_SCANCODE_F22,
        [0x6E] = RR_SCANCODE_F23,
        [0x76] = RR_SCANCODE_F24,
        [0x5D] = RR_SCANCODE_MENU, /* NOTE: Duplicated! */
        [0x1D] = RR_SCANCODE_LCTRL,
        [0x2A] = RR_SCANCODE_LSHIFT,
        [0x38] = RR_SCANCODE_LALT,
        [0x36] = RR_SCANCODE_RSHIFT,
    };

    static const Rr_Scancode MappingExtended[RR_SCANCODE_COUNT] = {
        [0x37] = RR_SCANCODE_PRINT_SCREEN, /* NOTE: Duplicated! */
        [0x52] = RR_SCANCODE_INSERT,
        [0x47] = RR_SCANCODE_HOME,
        [0x49] = RR_SCANCODE_PAGE_UP,
        [0x53] = RR_SCANCODE_DELETE,
        [0x4F] = RR_SCANCODE_END,
        [0x51] = RR_SCANCODE_PAGE_DOWN,
        [0x4D] = RR_SCANCODE_RIGHT,
        [0x4B] = RR_SCANCODE_LEFT,
        [0x48] = RR_SCANCODE_UP,
        [0x45] = RR_SCANCODE_NUMLOCK_CLEAR,
        [0x35] = RR_SCANCODE_KP_DIVIDE,
        [0x50] = RR_SCANCODE_DOWN,
        [0x1C] = RR_SCANCODE_KP_ENTER,
        [0x5D] = RR_SCANCODE_MENU, /* NOTE: Duplicated! */
        [0x5B] = RR_SCANCODE_LSUPER,
        [0x1D] = RR_SCANCODE_RCTRL,
        [0x38] = RR_SCANCODE_RALT,
        [0x5C] = RR_SCANCODE_RSUPER,
    };

    UINT Win32Scancode = (LParam & 0x00ff0000) >> 16;
    int Extended = (LParam & 0x01000000) != 0;

    if (Extended)
    {
        return MappingExtended[Win32Scancode];
    }

    return Mapping[Win32Scancode];
}

static inline WCHAR *Rr_UTF8ToWin32(char const *CString, Rr_Arena *Arena)
{
    size_t Length =
        (size_t)MultiByteToWideChar(CP_UTF8, 0, CString, -1, NULL, 0);
    if (!Length)
    {
        return NULL;
    }

    WCHAR *Win32String = RR_ALLOC_NO_ZERO(Length, Arena);

    if (!MultiByteToWideChar(CP_UTF8, 0, CString, -1, Win32String, (int)Length))
    {
        return NULL;
    }

    return Win32String;
}

static inline char *Rr_Win32ToUTF8(LPCWCH CString, Rr_Arena *Arena)
{
    size_t Length = (size_t)
        WideCharToMultiByte(CP_UTF8, 0, CString, -1, NULL, 0, NULL, NULL);
    if (!Length)
    {
        return NULL;
    }

    char *String = RR_ALLOC_NO_ZERO(Length, Arena);

    if (!WideCharToMultiByte(
            CP_UTF8,
            0,
            CString,
            -1,
            String,
            (int)Length,
            NULL,
            NULL))
    {
        return NULL;
    }

    return String;
}

static inline HMONITOR Rr_GetWin32Monitor(DWORD Flags)
{
    return MonitorFromWindow(gWin32.Window, Flags);
}

static inline void Rr_GetWin32DeviceMode(DEVMODEW *DeviceMode, DWORD Flags)
{
    HMONITOR Monitor = Rr_GetWin32Monitor(Flags);

    MONITORINFOEXW MonitorInfo = { 0 };
    MonitorInfo.cbSize = sizeof(MONITORINFOEXW);

    GetMonitorInfoW(Monitor, (LPMONITORINFO)&MonitorInfo);

    RR_ZERO_PTR(DeviceMode);
    DeviceMode->dmSize = sizeof(*DeviceMode);
    DeviceMode->dmDriverExtra = 0;
    EnumDisplaySettingsW(
        MonitorInfo.szDevice,
        ENUM_CURRENT_SETTINGS,
        DeviceMode);
}

static inline void Rr_UpdateMouseState(WPARAM WParam)
{
    Rr_MouseButtonFlags MouseState = 0;

    if (WParam & MK_LBUTTON)
    {
        MouseState |= RR_MOUSE_BUTTON_LEFT_BIT;
    }

    if (WParam & MK_MBUTTON)
    {
        MouseState |= RR_MOUSE_BUTTON_MIDDLE_BIT;
    }

    if (WParam & MK_RBUTTON)
    {
        MouseState |= RR_MOUSE_BUTTON_RIGHT_BIT;
    }

    if (WParam & MK_XBUTTON1)
    {
        MouseState |= RR_MOUSE_BUTTON_X1_BIT;
    }

    if (WParam & MK_XBUTTON2)
    {
        MouseState |= RR_MOUSE_BUTTON_X2_BIT;
    }

    if (MouseState)
    {
        SetCapture(gWin32.Window);
    }
    else
    {
        ReleaseCapture();
    }

    gPlatform.MouseState = MouseState;
}

#define RR_LPARAM_TO_VEC2(LParam) \
    Rr_V2((float)GET_X_LPARAM(LParam), (float)GET_Y_LPARAM(LParam))

static LRESULT CALLBACK
Rr_Win32WindowProcedure(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    switch (Message)
    {
        case WM_DROPFILES:
        {
            Rr_Scratch Scratch = Rr_GetScratch(NULL);

            Rr_Arena *Arena = (void *)GetWindowLongPtrW(Window, GWLP_USERDATA);

            HDROP Drop = (HDROP)WParam;
            UINT Count = DragQueryFileW(Drop, 0xFFFFFFFF, NULL, 0);
            for (UINT Index = 0; Index < Count; ++Index)
            {
                UINT Length = DragQueryFileW(Drop, Index, NULL, 0) + 1;
                LPWSTR String =
                    RR_ALLOC_NO_ZERO(sizeof(WCHAR) * Length, Scratch.Arena);
                if (DragQueryFileW(Drop, Index, String, Length))
                {
                    Rr_SetDropFileEvent(Rr_Win32ToUTF8(String, Arena), NULL);
                }
            }
            DragFinish(Drop);

            Rr_DestroyScratch(Scratch);

            return 0;
        }
        case WM_SETCURSOR:
        {
            if (LOWORD(LParam) == HTCLIENT)
            {
                if (gPlatform.RelativeMouseMode)
                {
                    SetCursor(NULL);
                }
                else
                {
                    SetCursor(gWin32.Cursors[gPlatform.CursorType]);
                }

                return 0;
            }
        }
        break;
        case WM_ACTIVATE:
        {
            if (LOWORD(WParam) == WA_INACTIVE)
            {
                Rr_SetFocusEvent(false, NULL);
            }
            else
            {
                Rr_SetFocusEvent(true, NULL);
            }
        }
        break;
        case WM_SETFOCUS:
        {
            Rr_SetFocusEvent(true, NULL);

            return 0;
        }
        case WM_KILLFOCUS:
        {
            Rr_SetFocusEvent(false, NULL);

            return 0;
        }
        case WM_CHAR:
        case WM_SYSCHAR:
        {
            if (IS_HIGH_SURROGATE(WParam))
            {
                gWin32.HighSurrogate = (uint32_t)WParam;
            }

            uint32_t Codepoint;
            if (IS_SURROGATE_PAIR(gWin32.HighSurrogate, WParam))
            {
                Codepoint =
                    ((gWin32.HighSurrogate - HIGH_SURROGATE_START) << 10) +
                    ((uint32_t)WParam - LOW_SURROGATE_START) + 0x10000;
            }
            else
            {
                Codepoint = (uint32_t)WParam;
            }

            gWin32.HighSurrogate = 0;

            Rr_Arena *Arena = (void *)GetWindowLongPtrW(Window, GWLP_USERDATA);
            Rr_SetTextInputEvent(Codepoint, NULL, Arena);

            return 0;
        }
        case WM_UNICHAR:
        {
            if (WParam == UNICODE_NOCHAR)
            {
                return TRUE;
            }

            Rr_Arena *Arena = (void *)GetWindowLongPtrW(Window, GWLP_USERDATA);
            Rr_SetTextInputEvent((uint32_t)WParam, NULL, Arena);

            return 0;
        }
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            bool Down = (HIWORD(LParam) & KF_UP) == 0;

            Rr_SetKeyEvent(Rr_Win32KeyToScancode(WParam, LParam), Down, NULL);

            return 0;
        }
        case WM_MOUSEMOVE:
        {
            Rr_UpdateMouseState(WParam);

            if (gPlatform.RelativeMouseMode)
            {
                POINT Point;
                GetCursorPos(&Point);

                RECT Rect;
                GetWindowRect(gWin32.Window, &Rect);
                POINT Center = {
                    (Rect.right + Rect.left) / 2,
                    (Rect.top + Rect.bottom) / 2,
                };

                Rr_IntVec2 Delta =
                    Rr_IntV2(Point.x - Center.x, Point.y - Center.y);

                if (Delta.X == 0 && Delta.Y == 0)
                {
                    /* NOTE: Skip event that comes from SetCursorPos call. */

                    break;
                }

                SetCursorPos(Center.x, Center.y);

                gPlatform.MousePosition =
                    Rr_AddV2(gPlatform.MousePosition, Rr_CastV2(Delta));
            }
            else
            {
                gPlatform.MousePosition = RR_LPARAM_TO_VEC2(LParam);
            }

            Rr_Event *Event = Rr_AddEvent();
            Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
            Event->MouseMotion.Position = gPlatform.MousePosition;

            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
        {
            Rr_UpdateMouseState(WParam);

            Rr_MouseButton Button = RR_MOUSE_BUTTON_LEFT;
            bool Down = false;
            if (Message == WM_XBUTTONDOWN || Message == WM_XBUTTONUP)
            {
                Down = Message == WM_XBUTTONDOWN;
                int XButton = GET_XBUTTON_WPARAM(WParam);
                if (XButton == XBUTTON1)
                {
                    Button = RR_MOUSE_BUTTON_X1;
                }
                else if (XButton == XBUTTON2)
                {
                    Button = RR_MOUSE_BUTTON_X2;
                }
                else
                {
                    break;
                }
            }
            else
            {
                switch (Message)
                {
                    case WM_LBUTTONDOWN:
                    case WM_LBUTTONUP:
                    {
                        Button = RR_MOUSE_BUTTON_LEFT;
                        Down = Message == WM_LBUTTONDOWN;
                    }
                    break;
                    case WM_MBUTTONDOWN:
                    case WM_MBUTTONUP:
                    {
                        Button = RR_MOUSE_BUTTON_MIDDLE;
                        Down = Message == WM_MBUTTONDOWN;
                    }
                    break;
                    case WM_RBUTTONDOWN:
                    case WM_RBUTTONUP:
                    {
                        Button = RR_MOUSE_BUTTON_RIGHT;
                        Down = Message == WM_RBUTTONDOWN;
                    }
                    break;
                    default:
                        break;
                }
            }

            Rr_Event *Event = Rr_AddEvent();
            Rr_SetMouseButtonEvent(
                Down,
                RR_LPARAM_TO_VEC2(LParam),
                Button,
                Event);

            if (gPlatform.RelativeMouseMode)
            {
                POINT Point;
                GetCursorPos(&Point);

                RECT Rect;
                GetWindowRect(gWin32.Window, &Rect);
                POINT Center = {
                    (Rect.right + Rect.left) / 2,
                    (Rect.top + Rect.bottom) / 2,
                };

                Rr_IntVec2 Delta =
                    Rr_IntV2(Point.x - Center.x, Point.y - Center.y);

                if (Delta.X == 0 && Delta.Y == 0)
                {
                    /* NOTE: Skip event that comes from SetCursorPos call. */

                    break;
                }

                SetCursorPos(Center.x, Center.y);

                gPlatform.MousePosition =
                    Rr_AddV2(gPlatform.MousePosition, Rr_CastV2(Delta));
            }
            else
            {
                gPlatform.MousePosition = RR_LPARAM_TO_VEC2(LParam);
            }

            return 0;
        }
        case WM_DESTROY:
        case WM_QUIT:
        case WM_CLOSE:
        {
            Rr_Event *Event = Rr_AddEvent();
            Event->Type = RR_EVENT_TYPE_QUIT;

            return 0;
        }
        default:
        {
            return DefWindowProcW(Window, Message, WParam, LParam);
        }
    }

    return DefWindowProcW(Window, Message, WParam, LParam);
}

bool Rr_InitPlatform(Rr_AppConfig *Config)
{
    assert(!gWin32.Initialized);

    RR_LOG_INFO("Using Win32");

    SetProcessDPIAware();

    HINSTANCE Instance = GetModuleHandleW(NULL);
    gWin32.Instance = Instance;

    WNDCLASSEXW WindowClassEx = {
        .cbSize = sizeof(WNDCLASSEX),
        .lpfnWndProc = Rr_Win32WindowProcedure,
        .hInstance = Instance,
        .hIcon = LoadIcon(NULL, IDI_APPLICATION),
        .lpszClassName = RR_WIN32_CLASS_NAME,
        .hIconSm = LoadIcon(NULL, IDI_WINLOGO),
    };
    RegisterClassExW(&WindowClassEx);

    DEVMODEW DeviceMode;
    Rr_GetWin32DeviceMode(&DeviceMode, MONITOR_DEFAULTTOPRIMARY);

    Rr_IntVec2 WindowedSize = {
        (int32_t)((float)DeviceMode.dmPelsWidth * RR_WINDOWED_RATIO),
        (int32_t)((float)DeviceMode.dmPelsHeight * RR_WINDOWED_RATIO),
    };
    RECT WindowedRect = {
        ((int32_t)DeviceMode.dmPelsWidth - WindowedSize.X) / 2,
        ((int32_t)DeviceMode.dmPelsHeight - WindowedSize.Y) / 2,
        0,
        0,
    };
    WindowedRect.right = WindowedRect.left + WindowedSize.X;
    WindowedRect.bottom = WindowedRect.top + WindowedSize.Y;
    AdjustWindowRect(&WindowedRect, WS_OVERLAPPEDWINDOW, FALSE);
    gWin32.WindowedRect = WindowedRect;
    UINT ExStyle;
    UINT Style;
    Rr_IntVec2 WindowSize;
    Rr_IntVec2 WindowPosition;
    if (RR_HAS_BIT(Config->WindowFlags, RR_WINDOW_FLAGS_FULLSCREEN_BIT))
    {
        ExStyle = RR_WIN32_FULLSCREEN_EXSTYLE;
        Style = RR_WIN32_FULLSCREEN_STYLE;
        WindowSize = Rr_IntV2(
            (int32_t)DeviceMode.dmPelsWidth,
            (int32_t)DeviceMode.dmPelsHeight);
        WindowPosition = Rr_IntV2I(0);
        gPlatform.Fullscreen = true;
    }
    else
    {
        ExStyle = RR_WIN32_WINDOWED_EXSTYLE;
        Style = RR_WIN32_WINDOWED_STYLE;
        WindowSize = Rr_IntV2(
            WindowedRect.right - WindowedRect.left,
            WindowedRect.bottom - WindowedRect.top);
        WindowPosition = Rr_IntV2(WindowedRect.left, WindowedRect.top);
    }
    Rr_Scratch Scratch = Rr_GetScratch(NULL);
    WCHAR *WindowName = Rr_UTF8ToWin32(Config->Title, Scratch.Arena);
    HWND Window = CreateWindowExW(
        ExStyle,
        WindowClassEx.lpszClassName,
        WindowName,
        Style,
        WindowPosition.X,
        WindowPosition.Y,
        WindowSize.X,
        WindowSize.Y,
        NULL,
        NULL,
        Instance,
        &gWin32);
    Rr_DestroyScratch(Scratch);
    if (!Window)
    {
        return false;
    }
    gWin32.Window = Window;

    gWin32.Cursors[RR_CURSOR_TYPE_NORMAL] = LoadCursor(NULL, IDC_ARROW);
    gWin32.Cursors[RR_CURSOR_TYPE_RESIZE_EW] = LoadCursor(NULL, IDC_SIZEWE);
    gWin32.Cursors[RR_CURSOR_TYPE_RESIZE_NS] = LoadCursor(NULL, IDC_SIZENS);
    gWin32.Cursors[RR_CURSOR_TYPE_RESIZE_NWSE] = LoadCursor(NULL, IDC_SIZENWSE);
    gWin32.Cursors[RR_CURSOR_TYPE_RESIZE_NESW] = LoadCursor(NULL, IDC_SIZENESW);
    gWin32.Cursors[RR_CURSOR_TYPE_RESIZE_ALL] = LoadCursor(NULL, IDC_SIZEALL);
    gWin32.Cursors[RR_CURSOR_TYPE_TEXT] = LoadCursor(NULL, IDC_IBEAM);
    SetCursor(gWin32.Cursors[gPlatform.CursorType]);

    GUID DXGIGUID = { 0x7b7166ec,
                      0x21c7,
                      0x44ae,
                      { 0xb2, 0x1a, 0xc9, 0xae, 0x32, 0x1a, 0xe3, 0x69 } };
    IDXGIFactory *DXGIFactory = NULL;
    CreateDXGIFactory(&DXGIGUID, (void **)&DXGIFactory);
    if (!DXGIFactory)
    {
        return false;
    }
    gWin32.DXGIFactory = DXGIFactory;
    IDXGIFactory_MakeWindowAssociation(DXGIFactory, Window, 0);

    HMODULE VulkanModule = LoadLibraryW(L"vulkan-1.dll");
    if (!VulkanModule)
    {
        return false;
    }
    gWin32.VulkanModule = VulkanModule;
    gWin32.VkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)(void (*)(
        void))GetProcAddress(VulkanModule, "vkGetInstanceProcAddr");
    if (!gWin32.VkGetInstanceProcAddr)
    {
        return false;
    }
    gWin32.VkCreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)(void (*)(
        void))GetProcAddress(VulkanModule, "vkCreateWin32SurfaceKHR");
    if (!gWin32.VkCreateWin32SurfaceKHR)
    {
        return false;
    }

    gWin32.Initialized = true;

    Rr_DestroyScratch(Scratch);

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gWin32.Initialized);

    UnregisterClassW(RR_WIN32_CLASS_NAME, GetModuleHandleW(NULL));

    IDXGIFactory_Release(gWin32.DXGIFactory);

    FreeLibrary(gWin32.VulkanModule);

    RR_ZERO(gWin32);
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return (void (*)(void))gWin32.VkGetInstanceProcAddr;
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    static char const *Extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME,
                                        VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

    if (Count)
    {
        *Count = RR_ARRAY_COUNT(Extensions);
    }

    return Extensions;
}

bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface)
{
    VkWin32SurfaceCreateInfoKHR Info = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = gWin32.Instance,
        .hwnd = gWin32.Window,
    };

    return gWin32.VkCreateWin32SurfaceKHR(
               (VkInstance)Instance,
               &Info,
               NULL,
               (VkSurfaceKHR *)Surface) == VK_SUCCESS;
}

void Rr_NewPlatformFrame(void)
{
}

void Rr_ProcessPlatformEvents(Rr_Arena *Arena)
{
    Rr_Vec2 LastMousePosition = gPlatform.MousePosition;
    gPlatform.MousePositionDelta = Rr_V2F(0.0f);

    SetWindowLongPtrW(gWin32.Window, GWLP_USERDATA, (LONG_PTR)Arena);

    MSG Message;
    if (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&Message);
        DispatchMessage(&Message);
    }

    SetWindowLongPtrW(gWin32.Window, GWLP_USERDATA, (LONG_PTR)NULL);

    gPlatform.MousePositionDelta =
        Rr_SubV2(gPlatform.MousePosition, LastMousePosition);

    /* HACK: Windows doesn't send us WM_KEYUP for second shift. */

    if (gPlatform.PressedKeys[RR_SCANCODE_LSHIFT] &&
        !(GetKeyState(VK_LSHIFT) & KF_UP))
    {
        Rr_AddKeyEvent(RR_SCANCODE_LSHIFT, false);
    }
    if (gPlatform.PressedKeys[RR_SCANCODE_RSHIFT] &&
        !(GetKeyState(VK_RSHIFT) & KF_UP))
    {
        Rr_AddKeyEvent(RR_SCANCODE_RSHIFT, false);
    }

    return false;
}

void Rr_ShowWindow(void)
{
    ShowWindow(gWin32.Window, SW_SHOW);
}

bool Rr_IsWindowMinimized(void)
{
    return IsIconic(gWin32.Window);
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    UINT ExStyle;
    UINT Style;
    if (Fullscreen)
    {
        ExStyle = RR_WIN32_FULLSCREEN_EXSTYLE;
        Style = RR_WIN32_FULLSCREEN_STYLE;
    }
    else
    {
        ExStyle = RR_WIN32_WINDOWED_EXSTYLE;
        Style = RR_WIN32_WINDOWED_STYLE;
    }

    SetWindowLongPtr(gWin32.Window, GWL_EXSTYLE, ExStyle);
    SetWindowLongPtr(gWin32.Window, GWL_STYLE, Style);

    if (Fullscreen)
    {
        GetWindowRect(gWin32.Window, &gWin32.WindowedRect);
        Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();
        SetWindowPos(
            gWin32.Window,
            HWND_TOP,
            0,
            0,
            DisplaySize.Width,
            DisplaySize.Height,
            SWP_FRAMECHANGED);
    }
    else
    {
        SetWindowPos(
            gWin32.Window,
            HWND_TOP,
            gWin32.WindowedRect.left,
            gWin32.WindowedRect.top,
            gWin32.WindowedRect.right - gWin32.WindowedRect.left,
            gWin32.WindowedRect.bottom - gWin32.WindowedRect.top,
            SWP_FRAMECHANGED);
    }

    gPlatform.Fullscreen = Fullscreen;
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    if (Relative == gPlatform.RelativeMouseMode)
    {
        return;
    }

#if !defined(HID_USAGE_PAGE_GENERIC)
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif
#if !defined(HID_USAGE_GENERIC_MOUSE)
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

    if (Relative)
    {
        POINT Point;
        GetCursorPos(&Point);
        ScreenToClient(gWin32.Window, &Point);

        gPlatform.RelativeMouseRestorePosition = Rr_IntV2(Point.x, Point.y);

        RECT Rect;
        GetWindowRect(gWin32.Window, &Rect);
        POINT Center = {
            (Rect.right + Rect.left) / 2,
            (Rect.top + Rect.bottom) / 2,
        };

        SetCursorPos(Center.x, Center.y);
        SetCursor(NULL);

        // RECT Rect;
        // GetClientRect(gPlatform.Window, &Rect);
        // ClientToScreen(gPlatform.Window, (POINT *)&Rect.left);
        // ClientToScreen(gPlatform.Window, (POINT *)&Rect.right);
        // ClipCursor(&Rect);
    }
    else
    {
        POINT Point = {
            gPlatform.RelativeMouseRestorePosition.X,
            gPlatform.RelativeMouseRestorePosition.Y,
        };
        ClientToScreen(gWin32.Window, &Point);
        SetCursorPos(Point.x, Point.y);

        gPlatform.MousePosition =
            Rr_CastV2(gPlatform.RelativeMouseRestorePosition);

        // ClipCursor(NULL);
    }

    gPlatform.RelativeMouseMode = Relative;
}

double Rr_GetDisplayRefreshRate(void)
{
    DEVMODEW DevMode;
    Rr_GetWin32DeviceMode(&DevMode, MONITOR_DEFAULTTONEAREST);

    UINT Numerator = DevMode.dmDisplayFrequency;

    double Result = 0.0;

    UINT AdapterIndex = 0;
    IDXGIAdapter *Adapter = NULL;
    while (Result == 0.0 && IDXGIFactory_EnumAdapters(
                                gWin32.DXGIFactory,
                                AdapterIndex++,
                                &Adapter) == S_OK)
    {
        UINT OutputIndex = 0;
        IDXGIOutput *Output = NULL;
        while (Result == 0.0 &&
               IDXGIAdapter_EnumOutputs(Adapter, OutputIndex++, &Output) ==
                   S_OK)
        {
            DXGI_OUTPUT_DESC Desc;
            IDXGIOutput_GetDesc(Output, &Desc);
            if (wcscmp(Desc.DeviceName, DevMode.dmDeviceName) == 0)
            {
                DXGI_MODE_DESC ClosestMatch = { 0 };
                DXGI_MODE_DESC ModeToMatch = {
                    .Width = DevMode.dmPelsWidth,
                    .Height = DevMode.dmPelsHeight,
                    .RefreshRate.Numerator = Numerator,
                    .RefreshRate.Denominator = 1,
                    .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
                };

                if (IDXGIOutput_FindClosestMatchingMode(
                        Output,
                        &ModeToMatch,
                        &ClosestMatch,
                        NULL) == S_OK)
                {
                    Result = (double)ClosestMatch.RefreshRate.Numerator /
                             (double)ClosestMatch.RefreshRate.Denominator;
                }
            }

            IDXGIOutput_Release(Output);
        }
        IDXGIAdapter_Release(Adapter);
    }

    if (Result == 0.0)
    {
        Result = (double)Numerator;
    }

    return Result;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    RECT WindowRect;
    GetClientRect(gWin32.Window, &WindowRect);

    return Rr_IntV2(
        WindowRect.right - WindowRect.left,
        WindowRect.bottom - WindowRect.top);
}

void Rr_SetWindowTitle(const char *Title)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    SetWindowTextW(gWin32.Window, Rr_UTF8ToWin32(Title, Scratch.Arena));

    Rr_DestroyScratch(Scratch);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    DEVMODEW DevMode;
    Rr_GetWin32DeviceMode(&DevMode, MONITOR_DEFAULTTONEAREST);

    return Rr_IntV2(
        (int32_t)DevMode.dmPelsWidth,
        (int32_t)DevMode.dmPelsHeight);
}

float Rr_GetWindowContentsScale(void)
{
    HDC DeviceContext = GetDC(gWin32.Window);
    LONG DensityPerInch = GetDeviceCaps(DeviceContext, LOGPIXELSY);
    float Scale = (float)DensityPerInch / 96.0f;
    ReleaseDC(gWin32.Window, DeviceContext);

    return Scale;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    RECT Rect = {
        .right = Size.X,
        .bottom = Size.Y,
    };
    AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(
        gWin32.Window,
        NULL,
        0,
        0,
        Rect.right - Rect.left,
        Rect.bottom - Rect.top,
        SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOCOPYBITS | SWP_NOZORDER);
}

void Rr_SetCursor(Rr_CursorType Type)
{
    if (gPlatform.CursorType == Type)
    {
        return;
    }

    gPlatform.CursorType = Type;
}

void Rr_SetClipboardText(const char *CString)
{
    if (!CString)
    {
        return;
    }

    size_t Length = strlen(CString);
    if (!Length)
    {
        return;
    }

    if (!OpenClipboard(gWin32.Window))
    {
        return;
    }

    HGLOBAL Handle = GlobalAlloc(GMEM_MOVEABLE, Length + 1);
    if (!Handle)
    {
        CloseClipboard();

        return;
    }
    char *Copy = GlobalLock(Handle);
    if (!Copy)
    {
        CloseClipboard();

        return;
    }
    memcpy(Copy, CString, Length);
    Copy[Length] = '\0';
    GlobalUnlock(Copy);

    EmptyClipboard();
    SetClipboardData(CF_TEXT, Copy);
    CloseClipboard();
}

char const *Rr_GetClipboardText(Rr_Arena *Arena)
{
    if (!Arena)
    {
        return NULL;
    }

    if (!IsClipboardFormatAvailable(CF_TEXT))
    {
        return NULL;
    }

    if (!OpenClipboard(gWin32.Window))
    {
        return NULL;
    }

    HGLOBAL Global = GetClipboardData(CF_TEXT);
    if (!Global)
    {
        CloseClipboard();

        return NULL;
    }

    char const *Text = GlobalLock(Global);
    if (!Text)
    {
        CloseClipboard();

        return NULL;
    }

    size_t Length = strlen(Text);
    if (Length == 0)
    {
        GlobalUnlock(Global);

        CloseClipboard();

        return NULL;
    }

    char *NewAllocation = RR_ALLOC_NO_ZERO(Length + 1, Arena);
    ;
    memcpy(NewAllocation, Text, Length);
    NewAllocation[Length] = '\0';

    GlobalUnlock(Global);

    CloseClipboard();

    return NewAllocation;
}
