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

#include "Rr_Platform.h"

#include <Rr/Rr_App.h>

#include "Rr_Log.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <assert.h>
#include <stdio.h>

bool Rr_InitPlatformLibrary(Rr_AppConfig *Config)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    if (!SDL_Vulkan_LoadLibrary(NULL))
    {
        RR_LOG("%s", SDL_GetError());
    }

    SDL_WindowFlags SDLWindowFlags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (RR_HAS_BIT(Config->WindowFlags, RR_WINDOW_FLAGS_RESIZE_BIT))
    {
        SDLWindowFlags |= SDL_WINDOW_RESIZABLE;
    }
    Rr_Arena *Arena = Rr_CreateDefaultArena();
    gPlatform = RR_ALLOC_TYPE(Arena, Rr_Platform);
    gPlatform->Arena = Arena;
    gPlatform->EventScratch =
        (Rr_Scratch){ .Arena = Arena, .Position = Arena->Position };
    gPlatform->Window = SDL_CreateWindow(Config->Title, 0, 0, SDLWindowFlags);
    if (!gPlatform->Window)
    {
        RR_LOG("%s", SDL_GetError());
    }
    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_StartTextInput(gPlatform->Window);
    Rr_IntVec2 WindowSize = Rr_GetDisplaySize();
    WindowSize.X = (int32_t)((float)WindowSize.X * RR_WINDOWED_RATIO);
    WindowSize.Y = (int32_t)((float)WindowSize.Y * RR_WINDOWED_RATIO);
    SDL_SetWindowSize(gPlatform->Window, WindowSize.X, WindowSize.Y);
    SDL_SetWindowPosition(
        gPlatform->Window,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED);

    char DoubleClickTimeString[32];
    sprintf(DoubleClickTimeString, "%d", RR_DOUBLE_CLICK_TIME_MS);
    SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, DoubleClickTimeString);

    RR_LOG("Using SDL platform library");

    return true;
}

bool Rr_CleanupPlatformLibrary(void)
{
    SDL_DestroyWindow(gPlatform->Window);

    SDL_Quit();

    return true;
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return SDL_Vulkan_GetVkGetInstanceProcAddr();
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    return SDL_Vulkan_GetInstanceExtensions(Count);
}

bool Rr_CreateVulkanSurface(void *Instance, void **Surface)
{
    return SDL_Vulkan_CreateSurface(
        gPlatform->Window,
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
    SDL_GetWindowSize(gPlatform->Window, &WindowSize.X, &WindowSize.Y);

    Rr_IntVec2 WindowSizeInPixels;
    SDL_GetWindowSizeInPixels(
        gPlatform->Window,
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

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    return SDL_GetMouseState(NULL, NULL);
}

bool Rr_PollPlatformEvent(Rr_Event *Event)
{
    static SDL_Event SDLEvent;
    SDL_PollEvent(&SDLEvent);

    switch (SDLEvent.type)
    {
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
        {
            return false;
        }
        case SDL_EVENT_TEXT_INPUT:
        {
            Event->Type = RR_EVENT_TYPE_TEXT_INPUT;
            Event->Text.Text = SDLEvent.text.text;
            return true;
        }
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
        {
            Event->Type = SDLEvent.type == SDL_EVENT_KEY_DOWN
                              ? RR_EVENT_TYPE_KEY_DOWN
                              : RR_EVENT_TYPE_KEY_UP;
            Event->Key.Down = SDLEvent.key.down;
            Event->Key.Scancode = (Rr_Scancode)SDLEvent.key.scancode;
            Event->Key.Keymod = 0;
            if ((SDLEvent.key.mod & SDL_KMOD_LCTRL) ||
                (SDLEvent.key.mod & SDL_KMOD_RCTRL) ||
                (SDLEvent.key.mod & SDL_KMOD_CTRL))
            {
                Event->Key.Keymod |= RR_KEYMOD_CTRL;
            }
            if ((SDLEvent.key.mod & SDL_KMOD_LSHIFT) ||
                (SDLEvent.key.mod & SDL_KMOD_RSHIFT) ||
                (SDLEvent.key.mod & SDL_KMOD_SHIFT))
            {
                Event->Key.Keymod |= RR_KEYMOD_SHIFT;
            }
            if ((SDLEvent.key.mod & SDL_KMOD_LALT) ||
                (SDLEvent.key.mod & SDL_KMOD_RALT) ||
                (SDLEvent.key.mod & SDL_KMOD_ALT))
            {
                Event->Key.Keymod |= RR_KEYMOD_ALT;
            }
            return true;
        }
        case SDL_EVENT_MOUSE_MOTION:
        {
            Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
            Event->MouseMotion.Position = Rr_SDLConvertMousePosition(
                (Rr_Vec2){ SDLEvent.motion.x, SDLEvent.motion.y });
            return true;
        }
        case SDL_EVENT_MOUSE_WHEEL:
        {
            Event->Type = RR_EVENT_TYPE_MOUSE_WHEEL;
            Event->Wheel.Position = Rr_SDLConvertMousePosition(
                (Rr_Vec2){ SDLEvent.wheel.mouse_x, SDLEvent.wheel.mouse_y });
            Event->Wheel.Amount =
                (Rr_Vec2){ SDLEvent.wheel.x, SDLEvent.wheel.y };
            return true;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP:
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        {
            Event->Type = SDLEvent.type == SDL_EVENT_MOUSE_BUTTON_UP
                              ? RR_EVENT_TYPE_MOUSE_BUTTON_UP
                              : RR_EVENT_TYPE_MOUSE_BUTTON_DOWN;
            Event->MouseButton.Position =
                (Rr_Vec2){ SDLEvent.button.x, SDLEvent.button.y };
            if (SDLEvent.button.button == SDL_BUTTON_LEFT)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_LEFT;
            }
            else if (SDLEvent.button.button == SDL_BUTTON_RIGHT)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_RIGHT;
            }
            else if (SDLEvent.button.button == SDL_BUTTON_MIDDLE)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_MIDDLE;
            }
            else if (SDLEvent.button.button == SDL_BUTTON_X1)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_X1;
            }
            else if (SDLEvent.button.button == SDL_BUTTON_X2)
            {
                Event->MouseButton.Button = RR_MOUSE_BUTTON_X2;
            }
            Event->MouseButton.Clicks = SDLEvent.button.clicks;
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
        default:
            return false;
    }
}

void Rr_ShowWindow(void)
{
    SDL_ShowWindow(gPlatform->Window);
}

bool Rr_IsWindowMinimized(void)
{
    return SDL_GetWindowFlags(gPlatform->Window) & SDL_WINDOW_MINIMIZED;
}

bool Rr_IsWindowFullscreen(void)
{
    return (SDL_GetWindowFlags(gPlatform->Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    SDL_SetWindowFullscreen(gPlatform->Window, Fullscreen);
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    SDL_SetWindowRelativeMouseMode(gPlatform->Window, Relative);
}

float Rr_GetDisplayRefreshRate(void)
{
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gPlatform->Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);
    return Mode->refresh_rate;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    Rr_IntVec2 Size;
    SDL_GetWindowSizeInPixels(gPlatform->Window, &Size.X, &Size.Y);
    return Size;
}

void Rr_SetWindowTitle(const char *Title)
{
    SDL_SetWindowTitle(gPlatform->Window, Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    assert(gPlatform && gPlatform->Window);
    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(gPlatform->Window);
    float Scale = SDL_GetWindowPixelDensity(gPlatform->Window);
    SDL_Rect Rect;
    SDL_GetDisplayBounds(DisplayID, &Rect);
    return (Rr_IntVec2){
        (int32_t)(Scale * (float)Rect.w),
        (int32_t)(Scale * (float)Rect.h),
    };
}

float Rr_GetWindowContentsScale(void)
{
    float Density = SDL_GetWindowPixelDensity(gPlatform->Window);
    return Density;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    float Scale = SDL_GetWindowDisplayScale(gPlatform->Window);
    SDL_SetWindowSize(
        gPlatform->Window,
        (int32_t)((float)Size.Width / Scale),
        (int32_t)((float)Size.Height / Scale));
}

void Rr_SetCursor(Rr_CursorType Type)
{
    switch (Type)
    {
        case RR_CURSOR_TYPE_NORMAL:
        {
            static SDL_Cursor *SDLCursor;
            if (SDLCursor == NULL)
            {
                SDLCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
            }
            SDL_SetCursor(SDLCursor);
            return;
        }
        case RR_CURSOR_TYPE_RESIZE_EW:
        {
            static SDL_Cursor *SDLCursor;
            if (SDLCursor == NULL)
            {
                SDLCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
            }
            SDL_SetCursor(SDLCursor);
            return;
        }
        case RR_CURSOR_TYPE_RESIZE_NWSE:
        {
            static SDL_Cursor *SDLCursor;
            if (SDLCursor == NULL)
            {
                SDLCursor =
                    SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
            }
            SDL_SetCursor(SDLCursor);
            return;
        }
        case RR_CURSOR_TYPE_TEXT:
        {
            static SDL_Cursor *SDLCursor;
            if (SDLCursor == NULL)
            {
                SDLCursor = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
            }
            SDL_SetCursor(SDLCursor);
            return;
        }
        default:
            return;
    }
}

void Rr_SetClipboardText(const char *CString)
{
    SDL_SetClipboardText(CString);
}

const char *Rr_GetClipboardText(void)
{
    return SDL_GetClipboardText();
}
