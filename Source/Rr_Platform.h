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

#include <Rr/Rr_Arena.h>

#define RR_WINDOWED_RATIO 0.85f

struct Rr_AppConfig;

typedef struct Rr_Platform Rr_Platform;

extern bool Rr_InitPlatform(struct Rr_AppConfig *Config);

extern void Rr_CleanupPlatform(void);

extern void (*Rr_GetVkGetInstanceProcAddr(void))(void);

extern const char *const *Rr_GetVulkanExtensions(uint32_t *Count);

extern bool Rr_CreateVulkanSurface(uint64_t Instance, uint64_t *Surface);

extern void Rr_NewPlatformFrame(void);

extern bool Rr_PollPlatformEvent(Rr_Event *Event, Rr_Arena *Arena);

extern void Rr_ShowWindow(void);

extern float Rr_GetDisplayRefreshRate(void);
