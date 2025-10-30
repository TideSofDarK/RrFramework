/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PlatformInfo Rr_PlatformInfo;
struct Rr_PlatformInfo
{
    int PageSize;
    int AllocationGranularity;
    uint64_t PerformanceFrequency;
};

extern Rr_PlatformInfo *Rr_GetPlatformInfo(void);

extern uint64_t Rr_GetPerformanceCounter(void);

extern uint64_t Rr_GetPerformanceFrequency(void);

extern void *Rr_ReserveMemory(size_t Size);

extern void Rr_ReleaseMemory(void *Data, size_t Size);

extern bool Rr_CommitMemory(void *Data, size_t Size);

extern void Rr_DecommitMemory(void *Data, size_t Size);

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
    RR_SCANCODE_COUNT = 512,
} Rr_Scancode;

typedef enum Rr_KeymodFlagsBits
{
    RR_KEYMOD_CTRL = (1 << 0),
    RR_KEYMOD_SHIFT = (1 << 1),
    RR_KEYMOD_ALT = (1 << 2),
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

extern bool Rr_IsScancodePressed(Rr_Scancode Scancode);

extern Rr_Vec2 Rr_GetMousePosition(void);

extern Rr_Vec2 Rr_GetMousePositionDelta(void);

extern Rr_MouseButtonFlags Rr_GetMouseState(void);

typedef struct Rr_Platform Rr_Platform;

typedef enum
{
    RR_WINDOW_FLAGS_RESIZE_BIT = (1 << 0),
} Rr_WindowFlagsBits;
typedef uint32_t Rr_WindowFlags;

extern void Rr_ToggleWindowFullscreen(void);

extern Rr_IntVec2 Rr_GetWindowSize(void);

extern void Rr_SetWindowTitle(const char *Title);

extern Rr_IntVec2 Rr_GetDisplaySize(void);

extern float Rr_GetWindowContentsScale(void);

extern void Rr_SetWindowSize(Rr_IntVec2 Size);

extern void Rr_SetRelativeMouseMode(bool Relative);

typedef enum Rr_CursorType
{
    RR_CURSOR_TYPE_NORMAL,
    RR_CURSOR_TYPE_RESIZE_NWSE,
    RR_CURSOR_TYPE_TEXT,
} Rr_CursorType;

extern void Rr_SetCursor(Rr_CursorType Type);

extern void Rr_SetClipboardText(const char *CString);

extern const char *Rr_GetClipboardText(void);

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
    const char *Text;
};

typedef struct Rr_DropFileEvent Rr_DropFileEvent;
struct Rr_DropFileEvent
{
    const char *Path;
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
    };
};

#ifdef __cplusplus
}
#endif
