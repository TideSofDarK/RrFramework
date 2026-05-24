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
#include "Rr_App.h"
#include "Rr_LogMacro.h"
#include "Rr_Vulkan.h"

#include <xcb/xcb.h>

static struct Rr_Platform_XCB
{
    bool Initialized;
    void *Window;
    bool WindowScaled;
    Rr_Vec2 WindowScale;
    Rr_IntVec2 WindowedOffset;
    Rr_IntVec2 WindowedExtent;
    Rr_Vec2 LastMousePosition;
    Rr_Vec2 MousePositionDelta;
    Rr_Scratch EventScratch;
    Rr_Arena *Arena;
} gPlatform;

Rr_Platform *Rr_GetPlatform(void)
{
    return (void *)&gPlatform;
}

Rr_Window *Rr_CreateWindow(Rr_CreateWindowInfo const *CreateWindowInfo)
{
    return NULL;
}

/* bool Rr_InitPlatform(Rr_AppConfig *Config) */
/* { */
/*     Rr_Platform *Platform = Rr_GetPlatform(); */
/*     assert(!Platform->Initialized); */

/*     RR_LOG_INFO("Using SDL"); */

/* #if defined(__linux__) */
/*     SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11"); */
/* #endif */

/*     SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL); */
/*     SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS); */

/*     if (!SDL_Vulkan_LoadLibrary(NULL)) */
/*     { */
/*         RR_LOG_ERROR("%s", SDL_GetError()); */
/*     } */

/*     SDL_WindowFlags SDLWindowFlags = */
/*         SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN |
 * SDL_WINDOW_HIGH_PIXEL_DENSITY; */
/*     if (RR_HAS_BIT(Config->WindowFlags, RR_WINDOW_FLAGS_RESIZE_BIT)) */
/*     { */
/*         SDLWindowFlags |= SDL_WINDOW_RESIZABLE; */
/*     } */
/*     Rr_Arena *Arena = Rr_CreateDefaultArena(); */
/*     Platform->Arena = Arena; */
/*     Platform->EventScratch = */
/*         (Rr_Scratch){ .Arena = Arena, .Position = Arena->Position }; */
/*     Platform->Window = SDL_CreateWindow(Config->Title, 0, 0, SDLWindowFlags);
 */
/*     if (!Platform->Window) */
/*     { */
/*         RR_LOG_ERROR("%s", SDL_GetError()); */
/*     } */
/*     SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true); */
/*     SDL_StartTextInput(Platform->Window); */
/*     Rr_IntVec2 WindowSize = Rr_GetDisplaySize(); */
/*     WindowSize.X = (int32_t)((float)WindowSize.X * RR_WINDOWED_RATIO); */
/*     WindowSize.Y = (int32_t)((float)WindowSize.Y * RR_WINDOWED_RATIO); */
/* #ifdef __APPLE__ */
/*     SDL_Rect UsableBounds; */
/*     SDL_GetDisplayUsableBounds( */
/*         SDL_GetDisplayForWindow(gPlatform->Window), */
/*         &UsableBounds); */
/*     WindowSize.X = (int32_t)((float)UsableBounds.w * RR_WINDOWED_RATIO); */
/*     WindowSize.Y = (int32_t)((float)UsableBounds.h * RR_WINDOWED_RATIO); */
/* #endif */
/*     SDL_SetWindowSize(Platform->Window, WindowSize.X, WindowSize.Y); */
/*     SDL_SetWindowPosition( */
/*         Platform->Window, */
/*         SDL_WINDOWPOS_CENTERED, */
/*         SDL_WINDOWPOS_CENTERED); */

/*     char DoubleClickTimeString[32]; */
/*     sprintf(DoubleClickTimeString, "%d", RR_DOUBLE_CLICK_TIME_MS); */
/*     SDL_SetHint(SDL_HINT_MOUSE_DOUBLE_CLICK_TIME, DoubleClickTimeString); */

/*     Platform->Initialized = true; */

/*     return true; */
/* } */
