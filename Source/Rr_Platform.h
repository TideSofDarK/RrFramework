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

#pragma once

#include <Rr/Rr_Platform.h>

#define RR_WINDOWED_RATIO 0.85f

struct Rr_AppConfig;

#define RR_HIVE_TYPE               Rr_Event
#define RR_HIVE_TYPE_NAME          Event
#define RR_HIVE_PREFIX             Rr_
#define RR_HIVE_MIN_BLOCK_CAPACITY 32
#include "Rr_Hive.h"

typedef struct Rr_Platform Rr_Platform;
struct Rr_Platform
{
    Rr_CursorType CursorType;
    Rr_MouseButtonFlags MouseState;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MousePositionDelta;
    bool RelativeMouseMode;
    Rr_IntVec2 RelativeMouseRestorePosition;

    Rr_KeymodFlags Keymod;
    uint32_t PressedKeyCount;
    bool PressedKeys[RR_SCANCODE_COUNT];

    Rr_EventHive EventHive;
};

extern Rr_Platform gPlatform;

/* Platform-Specific Functions */

extern bool Rr_InitPlatform(struct Rr_AppConfig *Config);

extern void Rr_CleanupPlatform(void);

extern void (*Rr_GetVkGetInstanceProcAddr(void))(void);

extern const char *const *Rr_GetVulkanExtensions(uint32_t *Count);

extern bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface);

extern void Rr_ProcessPlatformEvents(Rr_Arena *Arena);

extern void Rr_ShowWindow(void);

extern double Rr_GetDisplayRefreshRate(void);

extern Rr_Vec2 Rr_QueryPlatformMousePosition(void);

/* Common Functions */

extern void Rr_BeginPlatformEvents(void);

extern void Rr_EndPlatformEvents(void);

extern void Rr_ReleaseAllInput(void);

/* Events */

extern void Rr_AddQuitRequestedEvent(void);

extern void Rr_AddSwapchainCreatedEvent(void);

extern void Rr_AddKeyEvent(Rr_Scancode Scancode, bool Down);

extern void Rr_AddMouseMotionEvent(Rr_Vec2 Position);

extern void Rr_AddMouseWheelEvent(Rr_Vec2 Position, Rr_Vec2 Amount);

extern void Rr_AddMouseButtonEvent(
    bool Down,
    Rr_Vec2 Position,
    Rr_MouseButton Button);

extern void Rr_AddTextInputEventString(char const *CString, size_t Length);

extern void Rr_AddTextInputEvent(uint32_t Codepoint, Rr_Arena *Arena);

extern void Rr_AddDropFileEvent(char const *Path);

extern void Rr_AddFocusEvent(bool HasFocus);
