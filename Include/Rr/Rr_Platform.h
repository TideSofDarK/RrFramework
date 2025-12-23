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

#ifndef RR_PLATFORM_H
#define RR_PLATFORM_H

#include <Rr/Rr_Math.h>

typedef struct Rr_PlatformInfo Rr_PlatformInfo;
struct Rr_PlatformInfo
{
    int PageSize;
    int AllocationGranularity;
    uint64_t PerformanceFrequency;
};

RR_EXTERN Rr_PlatformInfo *Rr_GetPlatformInfo(void);

RR_EXTERN uint64_t Rr_GetPerformanceCounter(void);

RR_EXTERN uint64_t Rr_GetPerformanceFrequency(void);

RR_EXTERN void *Rr_ReserveMemory(size_t Size);

RR_EXTERN void Rr_ReleaseMemory(void *Data, size_t Size);

RR_EXTERN bool Rr_CommitMemory(void *Data, size_t Size);

RR_EXTERN void Rr_DecommitMemory(void *Data, size_t Size);

/* NOTE: These come from SDL3. */
typedef enum Rr_Scancode
{
    RR_SCANCODE_UNKNOWN = 0,
    RR_SCANCODE_A = 4,
    RR_SCANCODE_B = 5,
    RR_SCANCODE_C = 6,
    RR_SCANCODE_D = 7,
    RR_SCANCODE_E = 8,
    RR_SCANCODE_F = 9,
    RR_SCANCODE_G = 10,
    RR_SCANCODE_H = 11,
    RR_SCANCODE_I = 12,
    RR_SCANCODE_J = 13,
    RR_SCANCODE_K = 14,
    RR_SCANCODE_L = 15,
    RR_SCANCODE_M = 16,
    RR_SCANCODE_N = 17,
    RR_SCANCODE_O = 18,
    RR_SCANCODE_P = 19,
    RR_SCANCODE_Q = 20,
    RR_SCANCODE_R = 21,
    RR_SCANCODE_S = 22,
    RR_SCANCODE_T = 23,
    RR_SCANCODE_U = 24,
    RR_SCANCODE_V = 25,
    RR_SCANCODE_W = 26,
    RR_SCANCODE_X = 27,
    RR_SCANCODE_Y = 28,
    RR_SCANCODE_Z = 29,
    RR_SCANCODE_1 = 30,
    RR_SCANCODE_2 = 31,
    RR_SCANCODE_3 = 32,
    RR_SCANCODE_4 = 33,
    RR_SCANCODE_5 = 34,
    RR_SCANCODE_6 = 35,
    RR_SCANCODE_7 = 36,
    RR_SCANCODE_8 = 37,
    RR_SCANCODE_9 = 38,
    RR_SCANCODE_0 = 39,
    RR_SCANCODE_RETURN = 40,
    RR_SCANCODE_ESCAPE = 41,
    RR_SCANCODE_BACKSPACE = 42,
    RR_SCANCODE_TAB = 43,
    RR_SCANCODE_SPACE = 44,
    RR_SCANCODE_CAPSLOCK = 57,
    RR_SCANCODE_F1 = 58,
    RR_SCANCODE_F2 = 59,
    RR_SCANCODE_F3 = 60,
    RR_SCANCODE_F4 = 61,
    RR_SCANCODE_F5 = 62,
    RR_SCANCODE_F6 = 63,
    RR_SCANCODE_F7 = 64,
    RR_SCANCODE_F8 = 65,
    RR_SCANCODE_F9 = 66,
    RR_SCANCODE_F10 = 67,
    RR_SCANCODE_F11 = 68,
    RR_SCANCODE_F12 = 69,
    RR_SCANCODE_HOME = 74,
    RR_SCANCODE_PAGEUP = 75,
    RR_SCANCODE_DELETE = 76,
    RR_SCANCODE_END = 77,
    RR_SCANCODE_PAGEDOWN = 78,
    RR_SCANCODE_RIGHT = 79,
    RR_SCANCODE_LEFT = 80,
    RR_SCANCODE_DOWN = 81,
    RR_SCANCODE_UP = 82,
    RR_SCANCODE_NUMLOCKCLEAR = 83,
    RR_SCANCODE_KP_DIVIDE = 84,
    RR_SCANCODE_KP_MULTIPLY = 85,
    RR_SCANCODE_KP_MINUS = 86,
    RR_SCANCODE_KP_PLUS = 87,
    RR_SCANCODE_KP_ENTER = 88,
    RR_SCANCODE_KP_1 = 89,
    RR_SCANCODE_KP_2 = 90,
    RR_SCANCODE_KP_3 = 91,
    RR_SCANCODE_KP_4 = 92,
    RR_SCANCODE_KP_5 = 93,
    RR_SCANCODE_KP_6 = 94,
    RR_SCANCODE_KP_7 = 95,
    RR_SCANCODE_KP_8 = 96,
    RR_SCANCODE_KP_9 = 97,
    RR_SCANCODE_KP_0 = 98,
    RR_SCANCODE_KP_PERIOD = 99,
    RR_SCANCODE_LCTRL = 224,
    RR_SCANCODE_LSHIFT = 225,
    RR_SCANCODE_LALT = 226,
    RR_SCANCODE_LSUPER = 227,
    RR_SCANCODE_RCTRL = 228,
    RR_SCANCODE_RSHIFT = 229,
    RR_SCANCODE_RALT = 230,
    RR_SCANCODE_RSUPER = 231,
    RR_SCANCODE_COUNT = 512,
} Rr_Scancode;

typedef enum Rr_KeymodFlagsBits
{
    RR_KEYMOD_CTRL = (1 << 0),
    RR_KEYMOD_SHIFT = (1 << 1),
    RR_KEYMOD_ALT = (1 << 2),
    RR_KEYMOD_GUI = (1 << 3),
} Rr_KeymodFlagsBits;
typedef uint16_t Rr_KeymodFlags;

typedef enum Rr_MouseButton
{
    RR_MOUSE_BUTTON_LEFT = 0,
    RR_MOUSE_BUTTON_MIDDLE = 1,
    RR_MOUSE_BUTTON_RIGHT = 2,
    RR_MOUSE_BUTTON_X1 = 3,
    RR_MOUSE_BUTTON_X2 = 4
} Rr_MouseButton;

typedef enum Rr_MouseButtonFlagsBits
{
    RR_MOUSE_BUTTON_LEFT_BIT = (1 << 0),
    RR_MOUSE_BUTTON_MIDDLE_BIT = (1 << 1),
    RR_MOUSE_BUTTON_RIGHT_BIT = (1 << 2),
    RR_MOUSE_BUTTON_X1_BIT = (1 << 3),
    RR_MOUSE_BUTTON_X2_BIT = (1 << 4),
} Rr_MouseButtonFlagsBits;
typedef uint32_t Rr_MouseButtonFlags;

#define RR_DOUBLE_CLICK_TIME_MS (200)

RR_EXTERN bool Rr_IsScancodePressed(Rr_Scancode Scancode);

RR_EXTERN Rr_Vec2 Rr_GetMousePosition(void);

RR_EXTERN Rr_Vec2 Rr_GetMousePositionDelta(void);

RR_EXTERN Rr_MouseButtonFlags Rr_GetMouseState(void);

typedef struct Rr_Platform Rr_Platform;

typedef enum
{
    RR_WINDOW_FLAGS_RESIZE_BIT = (1 << 0),
    RR_WINDOW_FLAGS_FULLSCREEN_BIT = (1 << 1),
} Rr_WindowFlagsBits;
typedef uint32_t Rr_WindowFlags;

RR_EXTERN void Rr_ToggleWindowFullscreen(void);

RR_EXTERN Rr_IntVec2 Rr_GetWindowSize(void);

RR_EXTERN void Rr_SetWindowTitle(const char *Title);

RR_EXTERN Rr_IntVec2 Rr_GetDisplaySize(void);

RR_EXTERN float Rr_GetWindowContentsScale(void);

RR_EXTERN void Rr_SetWindowSize(Rr_IntVec2 Size);

RR_EXTERN void Rr_SetRelativeMouseMode(bool Relative);

typedef enum Rr_CursorType
{
    RR_CURSOR_TYPE_NORMAL,
    RR_CURSOR_TYPE_RESIZE_EW,
    RR_CURSOR_TYPE_RESIZE_NWSE,
    RR_CURSOR_TYPE_TEXT,
} Rr_CursorType;

RR_EXTERN void Rr_SetCursor(Rr_CursorType Type);

RR_EXTERN void Rr_SetClipboardText(const char *CString);

RR_EXTERN const char *Rr_GetClipboardText(void);

typedef enum Rr_EventType
{
    RR_EVENT_TYPE_SWAPCHAIN_CREATED,
    RR_EVENT_TYPE_KEY_DOWN,
    RR_EVENT_TYPE_KEY_UP,
    RR_EVENT_TYPE_MOUSE_MOTION,
    RR_EVENT_TYPE_MOUSE_WHEEL,
    RR_EVENT_TYPE_MOUSE_BUTTON_DOWN,
    RR_EVENT_TYPE_MOUSE_BUTTON_UP,
    RR_EVENT_TYPE_TEXT_INPUT,
    RR_EVENT_TYPE_DROP_FILE,
    RR_EVENT_TYPE_QUIT,
    RR_EVENT_TYPE_FOCUS,
} Rr_EventType;

typedef struct Rr_KeyEvent Rr_KeyEvent;
struct Rr_KeyEvent
{
    Rr_Scancode Scancode;
    Rr_KeymodFlags Keymod;
    bool Down;
};

typedef struct Rr_MouseMotionEvent Rr_MouseMotionEvent;
struct Rr_MouseMotionEvent
{
    Rr_Vec2 Position;
};

typedef struct Rr_MouseButtonEvent Rr_MouseButtonEvent;
struct Rr_MouseButtonEvent
{
    Rr_Vec2 Position;
    uint8_t Button;
    uint8_t Clicks;
};

typedef struct Rr_MouseWheelEvent Rr_MouseWheelEvent;
struct Rr_MouseWheelEvent
{
    Rr_Vec2 Position;
    Rr_Vec2 Amount;
};

typedef struct Rr_TextInputEvent Rr_TextInputEvent;
struct Rr_TextInputEvent
{
    const char *CString;
};

typedef struct Rr_DropFileEvent Rr_DropFileEvent;
struct Rr_DropFileEvent
{
    const char *Path;
};

typedef struct Rr_FocusEvent Rr_FocusEvent;
struct Rr_FocusEvent
{
    bool Focused;
};

typedef struct Rr_Event Rr_Event;
struct Rr_Event
{
    Rr_EventType Type;
    union
    {
        Rr_MouseMotionEvent MouseMotion;
        Rr_MouseWheelEvent Wheel;
        Rr_MouseButtonEvent MouseButton;
        Rr_KeyEvent Key;
        Rr_TextInputEvent Text;
        Rr_DropFileEvent DropFile;
        Rr_FocusEvent Focus;
    };
};

#endif