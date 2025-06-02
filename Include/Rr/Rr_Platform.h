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

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Rr_PlatformInfo Rr_PlatformInfo;
struct Rr_PlatformInfo
{
    int PageSize;
    int AllocationGranularity;
};

extern bool Rr_InitPlatform(void);

extern Rr_PlatformInfo *Rr_GetPlatformInfo(void);

extern void *Rr_ReserveMemory(size_t Size);

extern void Rr_ReleaseMemory(void *Data, size_t Size);

extern bool Rr_CommitMemory(void *Data, size_t Size);

extern void Rr_DecommitMemory(void *Data, size_t Size);

typedef struct Rr_AtomicInt Rr_AtomicInt;
struct Rr_AtomicInt
{
    int Value;
};

extern int Rr_GetAtomicInt(Rr_AtomicInt *AtomicInt);

extern int Rr_SetAtomicInt(Rr_AtomicInt *AtomicInt, int Value);

extern int Rr_AddAtomicInt(Rr_AtomicInt *AtomicInt, int Value);

#define RR_INCREMENT_ATOMIC_INT(AtomicInt) (Rr_AddAtomicInt((AtomicInt), 1))

#define RR_DECREMENT_ATOMIC_INT(AtomicInt) \
    (Rr_AddAtomicInt((AtomicInt), -1) == 1)

typedef int Rr_Spinlock;

extern bool Rr_TryLockSpinlock(Rr_Spinlock *SpinLock);

extern void Rr_LockSpinlock(Rr_Spinlock *SpinLock);

extern void Rr_UnlockSpinlock(Rr_Spinlock *SpinLock);

typedef void *Rr_Window;

typedef enum
{
    RR_WINDOW_FLAGS_RESIZE_BIT = (1 << 0),
} Rr_WindowFlagsBits;
typedef uint32_t Rr_WindowFlags;

extern Rr_IntVec2 Rr_GetWindowSize(void);

extern void Rr_SetWindowTitle(const char *Title);

extern Rr_IntVec2 Rr_GetDisplaySize(void);

extern void Rr_SetWindowSize(Rr_IntVec2 Size);

typedef enum Rr_CursorType
{
    RR_UI_CURSOR_TYPE_NORMAL,
    RR_UI_CURSOR_TYPE_TEXT,
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
    Rr_Vec2 Delta;
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
