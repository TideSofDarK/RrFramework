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

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <Rr/Rr_App.h>
#include <Rr/Rr_System.h>
#include <Rr/Rr_Thread.h>
#include <Rr/Rr_Utility.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <assert.h>
#include <stdio.h>

static struct
{
    bool Initialized;

    SDL_Window *Window;
} gSDL;

static inline Rr_Vec2 Rr_SDLConvertMousePosition(Rr_Vec2 Scaled)
{
    Rr_IntVec2 WindowSize;
    SDL_GetWindowSize(gSDL.Window, &WindowSize.X, &WindowSize.Y);

    Rr_IntVec2 WindowSizeInPixels;
    SDL_GetWindowSizeInPixels(
        gSDL.Window,
        &WindowSizeInPixels.X,
        &WindowSizeInPixels.Y);

    Scaled.X /= (float)WindowSize.X;
    Scaled.Y /= (float)WindowSize.Y;

    Scaled.X *= (float)WindowSizeInPixels.X;
    Scaled.Y *= (float)WindowSizeInPixels.Y;

    return Scaled;
}

bool Rr_InitPlatform(Rr_Config *Config)
{
    assert(!gSDL.Initialized);

    RR_LOG_INFO("Using SDL3");

#if defined(__linux__)
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
#endif

    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (!SDL_Vulkan_LoadLibrary(NULL))
    {
        RR_LOG_ERROR("%s", SDL_GetError());

        return false;
    }

    SDL_WindowFlags SDLWindowFlags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Config->WindowFlags & RR_WINDOW_FLAGS_RESIZE_BIT)
    {
        SDLWindowFlags |= SDL_WINDOW_RESIZABLE;
    }
    if (Config->WindowFlags & RR_WINDOW_FLAGS_FULLSCREEN_BIT)
    {
        SDLWindowFlags |= SDL_WINDOW_FULLSCREEN;
    }
    SDL_DisplayID PrimaryDisplayID = SDL_GetPrimaryDisplay();
    SDL_Rect PrimaryDisplayBounds;
    SDL_GetDisplayBounds(PrimaryDisplayID, &PrimaryDisplayBounds);
    Rr_IntVec2 WindowSize = {
        (int32_t)((float)PrimaryDisplayBounds.w * RR_WINDOWED_RATIO),
        (int32_t)((float)PrimaryDisplayBounds.h * RR_WINDOWED_RATIO),
    };
    gSDL.Window = SDL_CreateWindow(
        Config->WindowTitle,
        WindowSize.X,
        WindowSize.Y,
        SDLWindowFlags);
    if (!gSDL.Window)
    {
        RR_LOG_ERROR("%s", SDL_GetError());

        return false;
    }
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_StartTextInput(gSDL.Window);
    /* SDL_SetWindowSize(gSDL.Window, WindowSize.X, WindowSize.Y); */
    SDL_SetWindowPosition(
        gSDL.Window,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED);

    char DoubleClickTimeString[32];
    sprintf(DoubleClickTimeString, "%d", RR_DOUBLE_CLICK_TIME_MS);
    SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, DoubleClickTimeString);

    gSDL.Initialized = true;

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gSDL.Initialized);

    SDL_DestroyWindow(gSDL.Window);

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
        gSDL.Window,
        (VkInstance)Instance,
        NULL,
        (VkSurfaceKHR *)Surface);
}

void Rr_ProcessPlatformEvents(Rr_Arena *Arena)
{
    SDL_Event SDLEvent;
    while (SDL_PollEvent(&SDLEvent))
    {
        switch (SDLEvent.type)
        {
            case SDL_EVENT_TEXT_INPUT:
            {
                size_t Length = strlen(SDLEvent.text.text);
                char *Buffer = Rr_AllocNoZero(Length + 1, Arena);
                memcpy(Buffer, SDLEvent.text.text, Length + 1);
                Rr_AddTextInputEventString(Buffer, Length);
            }
            break;
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                Rr_AddKeyEvent(
                    (Rr_Scancode)SDLEvent.key.scancode,
                    SDLEvent.key.down);
            }
            break;
            case SDL_EVENT_MOUSE_MOTION:
            {
                gPlatform.MouseState =
                    (Rr_MouseButtonFlags)SDL_GetMouseState(NULL, NULL);

                Rr_Vec2 Position =
                    (Rr_Vec2){ SDLEvent.motion.x, SDLEvent.motion.y };
                Position = Rr_SDLConvertMousePosition(Position);

                if (gPlatform.RelativeMouseMode)
                {
                    Rr_Vec2 Delta =
                        (Rr_Vec2){ SDLEvent.motion.xrel, SDLEvent.motion.yrel };
                    Delta = Rr_SDLConvertMousePosition(Delta);

                    if ((int)Delta.X == 0 && (int)Delta.Y == 0)
                    {
                        break;
                    }

                    gPlatform.MousePosition =
                        Rr_AddV2(gPlatform.MousePosition, Delta);
                    gPlatform.MousePositionDelta =
                        Rr_AddV2(gPlatform.MousePositionDelta, Delta);
                }
                else
                {
                    gPlatform.MousePosition = Position;
                }

                Rr_AddMouseMotionEvent(gPlatform.MousePosition);
            }
            break;
            case SDL_EVENT_MOUSE_WHEEL:
            {
                Rr_AddMouseWheelEvent(
                    gPlatform.MousePosition,
                    (Rr_Vec2){ SDLEvent.wheel.x, SDLEvent.wheel.y });
            }
            break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            {
                gPlatform.MouseState =
                    (Rr_MouseButtonFlags)SDL_GetMouseState(NULL, NULL);

                Rr_AddMouseButtonEvent(
                    SDLEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN,
                    gPlatform.MousePosition,
                    SDLEvent.button.button - 1);
            }
            break;
            case SDL_EVENT_DROP_FILE:
            {
                Rr_AddDropFileEvent(Rr_AllocCopy(
                    SDLEvent.drop.data,
                    strlen(SDLEvent.drop.data) + 1,
                    Arena));
            }
            break;
            case SDL_EVENT_QUIT:
            {
                Rr_AddQuitRequestedEvent();
            }
            break;
            case SDL_EVENT_WINDOW_FOCUS_GAINED:
            {
                Rr_AddFocusEvent(true);
            }
            break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
            {
                Rr_AddFocusEvent(false);
            }
            break;
            default:
            {
            }
            break;
        }
    }
}

void Rr_ShowWindow(void)
{
    SDL_ShowWindow(gSDL.Window);
}

bool Rr_IsWindowMinimized(void)
{
    return SDL_GetWindowFlags(gSDL.Window) & SDL_WINDOW_MINIMIZED;
}

bool Rr_IsWindowFullscreen(void)
{
    return SDL_GetWindowFlags(gSDL.Window) & SDL_WINDOW_FULLSCREEN;
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    SDL_SetWindowFullscreen(gSDL.Window, Fullscreen);
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    if (Relative == gPlatform.RelativeMouseMode)
    {
        return;
    }

    if (Relative)
    {
        SDL_SetWindowRelativeMouseMode(gSDL.Window, true);

        SDL_Rect Clip = {
            (int)gPlatform.MousePosition.X,
            (int)gPlatform.MousePosition.Y,
            2,
            2,
        };
        SDL_SetWindowMouseRect(gSDL.Window, &Clip);

        gPlatform.RelativeMouseMode = true;
    }
    else
    {
        SDL_SetWindowMouseRect(gSDL.Window, NULL);

        SDL_SetWindowRelativeMouseMode(gSDL.Window, false);

        gPlatform.RelativeMouseMode = false;
    }
}

double Rr_GetDisplayRefreshRate(void)
{
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gSDL.Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);

    return (double)Mode->refresh_rate;
}

Rr_Vec2 Rr_QueryPlatformMousePosition(void)
{
    Rr_Vec2 Position;
    SDL_GetMouseState(&Position.X, &Position.Y);
    Position = Rr_SDLConvertMousePosition(Position);

    return Position;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    Rr_IntVec2 Size;
    SDL_GetWindowSizeInPixels(gSDL.Window, &Size.X, &Size.Y);

    return Size;
}

void Rr_SetWindowTitle(const char *Title)
{
    SDL_SetWindowTitle(gSDL.Window, Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gSDL.Window);
    float Scale = SDL_GetWindowPixelDensity(gSDL.Window);
    SDL_Rect Rect;
    SDL_GetDisplayBounds(DisplayID, &Rect);

    return (Rr_IntVec2){
        (int32_t)(Scale * (float)Rect.w),
        (int32_t)(Scale * (float)Rect.h),
    };
}

float Rr_GetDisplayScale(void)
{
    return SDL_GetWindowDisplayScale(gSDL.Window);
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    float Scale = SDL_GetWindowDisplayScale(gSDL.Window);
    SDL_SetWindowSize(
        gSDL.Window,
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

char const *Rr_GetClipboardText(Rr_Arena *Arena)
{
    if (!Arena)
    {
        return NULL;
    }

    char const *SDLClipboard = SDL_GetClipboardText();

    size_t Length = strlen(SDLClipboard);
    if (!Length)
    {
        return NULL;
    }

    return Rr_AllocCopy(SDLClipboard, Length + 1, Arena);
}
