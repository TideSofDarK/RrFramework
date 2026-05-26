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

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Rr_Platform.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_PLATFORM
#include "Rr_LogMacro.h"

#include <Rr/Rr_App.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <assert.h>
#include <stdio.h>

static struct Rr_Platform_SDL
{
    bool Initialized;
    SDL_Window *Window;
    bool WindowScaled;
    Rr_Vec2 WindowScale;
    Rr_IntVec2 WindowedOffset;
    Rr_IntVec2 WindowedExtent;
    Rr_Vec2 MousePositionDelta;
    bool RelativeMouseMode;
    Rr_Vec2 RelativeMouseModePosition;
} gPlatform;

bool Rr_InitPlatform(Rr_AppConfig *Config)
{
    assert(!gPlatform.Initialized);

    RR_LOG_INFO("Using SDL3");

#if defined(__linux__)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (!SDL_Vulkan_LoadLibrary(NULL))
    {
        RR_LOG_ERROR("%s", SDL_GetError());
    }

    SDL_WindowFlags SDLWindowFlags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (RR_HAS_BIT(Config->WindowFlags, RR_WINDOW_FLAGS_RESIZE_BIT))
    {
        SDLWindowFlags |= SDL_WINDOW_RESIZABLE;
    }
    gPlatform.Window = SDL_CreateWindow(Config->Title, 0, 0, SDLWindowFlags);
    if (!gPlatform.Window)
    {
        RR_LOG_ERROR("%s", SDL_GetError());
    }
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_StartTextInput(gPlatform.Window);
    Rr_IntVec2 WindowSize = Rr_GetDisplaySize();
    WindowSize.X = (int32_t)((float)WindowSize.X * RR_WINDOWED_RATIO);
    WindowSize.Y = (int32_t)((float)WindowSize.Y * RR_WINDOWED_RATIO);
#ifdef __APPLE__
    SDL_Rect UsableBounds;
    SDL_GetDisplayUsableBounds(
        SDL_GetDisplayForWindow(gPlatform->Window),
        &UsableBounds);
    WindowSize.X = (int32_t)((float)UsableBounds.w * RR_WINDOWED_RATIO);
    WindowSize.Y = (int32_t)((float)UsableBounds.h * RR_WINDOWED_RATIO);
#endif
    SDL_SetWindowSize(gPlatform.Window, WindowSize.X, WindowSize.Y);
    SDL_SetWindowPosition(
        gPlatform.Window,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED);

    char DoubleClickTimeString[32];
    sprintf(DoubleClickTimeString, "%d", RR_DOUBLE_CLICK_TIME_MS);
    SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, DoubleClickTimeString);

    gPlatform.Initialized = true;

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gPlatform.Initialized);

    SDL_DestroyWindow(gPlatform.Window);
    SDL_Quit();

    RR_ZERO(gPlatform);
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return SDL_Vulkan_GetVkGetInstanceProcAddr();
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    return SDL_Vulkan_GetInstanceExtensions(Count);
}

bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface)
{
    return SDL_Vulkan_CreateSurface(
        gPlatform.Window,
        (VkInstance)Instance,
        NULL,
        (VkSurfaceKHR *)Surface);
}

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return SDL_GetKeyboardState(NULL)[Scancode];
}

static inline Rr_Vec2 Rr_SDLConvertMousePosition(Rr_Vec2 Scaled)
{
    Rr_IntVec2 WindowSize;
    SDL_GetWindowSize(gPlatform.Window, &WindowSize.X, &WindowSize.Y);

    Rr_IntVec2 WindowSizeInPixels;
    SDL_GetWindowSizeInPixels(
        gPlatform.Window,
        &WindowSizeInPixels.X,
        &WindowSizeInPixels.Y);

    Scaled.X /= (float)WindowSize.X;
    Scaled.Y /= (float)WindowSize.Y;

    Scaled.X *= (float)WindowSizeInPixels.X;
    Scaled.Y *= (float)WindowSizeInPixels.Y;

    return Scaled;
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    Rr_Vec2 MousePosition;
    SDL_GetMouseState(&MousePosition.X, &MousePosition.Y);

    return Rr_SDLConvertMousePosition(MousePosition);
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    return gPlatform.MousePositionDelta;
}

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    return SDL_GetMouseState(NULL, NULL);
}

void Rr_NewPlatformFrame(void)
{
    gPlatform.MousePositionDelta = Rr_V2F(0.0f);
}

bool Rr_PollPlatformEvent(Rr_Event *Event, Rr_Arena *Arena)
{
    static SDL_Event SDLEvent;

    while (SDL_PollEvent(&SDLEvent))
    {
        switch (SDLEvent.type)
        {
            case SDL_EVENT_TEXT_INPUT:
            {
                size_t Length = strlen(SDLEvent.text.text);
                char *Buffer = RR_ALLOC_NO_ZERO(Length + 1, Arena);
                memcpy(Buffer, SDLEvent.text.text, Length + 1);

                Event->Type = RR_EVENT_TYPE_TEXT_INPUT;
                Event->Text.Length = Length;
                Event->Text.CString = Buffer;

                return true;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if (SDLEvent.key.repeat)
                {
                    Event->Type = RR_EVENT_TYPE_KEY_REPEAT;
                }
                else
                {
                    Event->Type = SDLEvent.type == SDL_EVENT_KEY_DOWN
                                      ? RR_EVENT_TYPE_KEY_DOWN
                                      : RR_EVENT_TYPE_KEY_UP;
                }
                Event->Key.Down = Event->Type != RR_EVENT_TYPE_KEY_UP;
                Event->Key.Scancode = (Rr_Scancode)SDLEvent.key.scancode;
                Event->Key.Keymod = 0;
                if (SDLEvent.key.mod & SDL_KMOD_LCTRL ||
                    SDLEvent.key.mod & SDL_KMOD_RCTRL ||
                    SDLEvent.key.mod & SDL_KMOD_CTRL)
                {
                    Event->Key.Keymod |= RR_KEYMOD_CTRL;
                }
                if (SDLEvent.key.mod & SDL_KMOD_LSHIFT ||
                    SDLEvent.key.mod & SDL_KMOD_RSHIFT ||
                    SDLEvent.key.mod & SDL_KMOD_SHIFT)
                {
                    Event->Key.Keymod |= RR_KEYMOD_SHIFT;
                }
                if (SDLEvent.key.mod & SDL_KMOD_LALT ||
                    SDLEvent.key.mod & SDL_KMOD_RALT ||
                    SDLEvent.key.mod & SDL_KMOD_ALT)
                {
                    Event->Key.Keymod |= RR_KEYMOD_ALT;
                }
                if (SDLEvent.key.mod & SDL_KMOD_LGUI ||
                    SDLEvent.key.mod & SDL_KMOD_RGUI ||
                    SDLEvent.key.mod & SDL_KMOD_GUI)
                {
                    Event->Key.Keymod |= RR_KEYMOD_GUI;
                }

                return true;
            }
            case SDL_EVENT_MOUSE_MOTION:
            {
                Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
                Event->MouseMotion.Position = Rr_SDLConvertMousePosition(
                    (Rr_Vec2){ SDLEvent.motion.x, SDLEvent.motion.y });

                gPlatform.MousePositionDelta = Rr_AddV2(
                    gPlatform.MousePositionDelta,
                    (Rr_Vec2){ SDLEvent.motion.xrel, SDLEvent.motion.yrel });

                return true;
            }
            case SDL_EVENT_MOUSE_WHEEL:
            {
                Event->Type = RR_EVENT_TYPE_MOUSE_WHEEL;
                Event->Wheel.Position = Rr_SDLConvertMousePosition((
                    Rr_Vec2){ SDLEvent.wheel.mouse_x, SDLEvent.wheel.mouse_y });
                Event->Wheel.Amount =
                    (Rr_Vec2){ SDLEvent.wheel.x, SDLEvent.wheel.y };

                return true;
            }
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                Rr_SetMouseButtonEvent(
                    SDLEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
                    Rr_V2(SDLEvent.button.x, SDLEvent.button.y),
                    SDLEvent.button.button - 1,
                    Event);

                return true;
            }
            case SDL_EVENT_DROP_FILE:
            {
                Event->Type = RR_EVENT_TYPE_DROP_FILE;
                Event->DropFile.Path = SDLEvent.drop.data;

                return true;
            }
            case SDL_EVENT_QUIT:
            {
                Event->Type = RR_EVENT_TYPE_QUIT;

                return true;
            }
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            {
                Event->Type = RR_EVENT_TYPE_FOCUS;
                Event->Focus.Focused = true;

                return true;
            }
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            {
                Event->Type = RR_EVENT_TYPE_FOCUS;
                Event->Focus.Focused = false;

                return true;
            }
            default:
                break;
        }
    }

    return false;
}

void Rr_ShowWindow(void)
{
    SDL_ShowWindow(gPlatform.Window);
}

bool Rr_IsWindowMinimized(void)
{
    return SDL_GetWindowFlags(gPlatform.Window) & SDL_WINDOW_MINIMIZED;
}

bool Rr_IsWindowFullscreen(void)
{
    return (SDL_GetWindowFlags(gPlatform.Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    SDL_SetWindowFullscreen(gPlatform.Window, Fullscreen);
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    if (Relative == gPlatform.RelativeMouseMode)
    {
        return;
    }

    gPlatform.RelativeMouseMode = Relative;

    if (Relative)
    {
        /* SDL_CaptureMouse(true); */
        /* SDL_SetWindowMouseGrab(gPlatform.Window, true); */
        SDL_GetMouseState(
            &gPlatform.RelativeMouseModePosition.X,
            &gPlatform.RelativeMouseModePosition.Y);
        SDL_Rect Rect = {
            .x = (int)gPlatform.RelativeMouseModePosition.X,
            .y = (int)gPlatform.RelativeMouseModePosition.Y,
            .w = 1,
            .h = 1,
        };
        SDL_SetWindowMouseRect(gPlatform.Window, &Rect);
        SDL_SetWindowRelativeMouseMode(gPlatform.Window, true);
    }
    else
    {
        /* SDL_CaptureMouse(false); */
        /* SDL_SetWindowMouseGrab(gPlatform.Window, false); */
        SDL_SetWindowMouseRect(gPlatform.Window, NULL);
        SDL_SetWindowRelativeMouseMode(gPlatform.Window, false);
    }
}

float Rr_GetDisplayRefreshRate(void)
{
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gPlatform.Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);
    return Mode->refresh_rate;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    Rr_IntVec2 Size;
    SDL_GetWindowSizeInPixels(gPlatform.Window, &Size.X, &Size.Y);
    return Size;
}

void Rr_SetWindowTitle(const char *Title)
{
    SDL_SetWindowTitle(gPlatform.Window, Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gPlatform.Window);
    float Scale = SDL_GetWindowPixelDensity(gPlatform.Window);
    SDL_Rect Rect;
    SDL_GetDisplayBounds(DisplayID, &Rect);
    return (Rr_IntVec2){
        (int32_t)(Scale * (float)Rect.w),
        (int32_t)(Scale * (float)Rect.h),
    };
}

float Rr_GetWindowContentsScale(void)
{
    return SDL_GetWindowDisplayScale(gPlatform.Window);
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    float Scale = SDL_GetWindowDisplayScale(gPlatform.Window);
    SDL_SetWindowSize(
        gPlatform.Window,
        (int32_t)((float)Size.Width / Scale),
        (int32_t)((float)Size.Height / Scale));
}

void Rr_SetCursor(Rr_CursorType Type)
{
    if (Type >= RR_CURSOR_TYPE_COUNT)
    {
        return;
    }

    static SDL_Cursor *SDLCursors[RR_CURSOR_TYPE_COUNT] = { 0 };
    static SDL_SystemCursor const ToSDLCursor[RR_CURSOR_TYPE_COUNT] = {
        [RR_CURSOR_TYPE_NORMAL] = SDL_SYSTEM_CURSOR_DEFAULT,
        [RR_CURSOR_TYPE_RESIZE_EW] = SDL_SYSTEM_CURSOR_EW_RESIZE,
        [RR_CURSOR_TYPE_RESIZE_NS] = SDL_SYSTEM_CURSOR_NS_RESIZE,
        [RR_CURSOR_TYPE_RESIZE_NWSE] = SDL_SYSTEM_CURSOR_NWSE_RESIZE,
        [RR_CURSOR_TYPE_RESIZE_NESW] = SDL_SYSTEM_CURSOR_NESW_RESIZE,
        [RR_CURSOR_TYPE_RESIZE_ALL] = SDL_SYSTEM_CURSOR_MOVE,
        [RR_CURSOR_TYPE_TEXT] = SDL_SYSTEM_CURSOR_TEXT,
    };

    if (SDLCursors[Type] == NULL)
    {
        SDLCursors[Type] = SDL_CreateSystemCursor(ToSDLCursor[Type]);
    }
    SDL_SetCursor(SDLCursors[Type]);
}

void Rr_SetClipboardText(const char *CString)
{
    SDL_SetClipboardText(CString);
}

const char *Rr_GetClipboardText(void)
{
    return SDL_GetClipboardText();
}
