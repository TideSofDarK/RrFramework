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

#include <xcb/xcb.h>

static struct Rr_Platform_NULL
{
    bool Initialized;
} gPlatform;

bool Rr_InitPlatform(Rr_AppConfig *Config)
{
    gPlatform.Initialized = true;

    return true;
}

void Rr_CleanupPlatform(void)
{
    assert(gPlatform.Initialized);

    RR_ZERO(gPlatform);
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return NULL;
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    return NULL;
}

bool Rr_CreateVulkanSurface(void *Instance, void **Surface)
{
    return false;
}

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return false;
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    return Rr_V2F(0.0f);
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    return Rr_V2F(0.0f);
}

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    return 0;
}

void Rr_NewPlatformFrame(void)
{
}

bool Rr_PollPlatformEvent(Rr_Event *Event, Rr_Arena *Arena)
{
    return false;
}

void Rr_ShowWindow(void)
{
}

bool Rr_IsWindowMinimized(void)
{
    return false;
}

bool Rr_IsWindowFullscreen(void)
{
    return false;
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
}

void Rr_SetRelativeMouseMode(bool Relative)
{
}

float Rr_GetDisplayRefreshRate(void)
{
    return 0.0f;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    return Rr_IntV2I(0);
}

void Rr_SetWindowTitle(const char *Title)
{
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    return Rr_IntV2I(0);
}

float Rr_GetWindowContentsScale(void)
{
    return 1.0f;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
}

void Rr_SetCursor(Rr_CursorType Type)
{
}

void Rr_SetClipboardText(const char *CString)
{
}

const char *Rr_GetClipboardText(void)
{
    return NULL;
}
