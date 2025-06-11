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

#include <Rr/Rr_Platform.h>

#include <Rr/Rr_Memory.h>

#include <stdatomic.h>

struct Rr_Window
{
    void *Handle;
    bool WindowedFullscreen;
    Rr_IntVec2 WindowedOffset;
    Rr_IntVec2 WindowedExtent;
    Rr_Vec2 LastMousePosition;
    Rr_Vec2 MousePositionDelta;
    Rr_Scratch EventScratch;
    Rr_Arena *Arena;
};

struct Rr_AppConfig;

extern bool Rr_InitPlatform(void);

extern bool Rr_InitPlatformLibrary(struct Rr_AppConfig *Config);

extern bool Rr_CleanupPlatformLibrary(void);

extern bool Rr_InitWindow(struct Rr_AppConfig *Config);

extern void Rr_CleanupWindow(void);

extern void (*Rr_GetVkGetInstanceProcAddr(void))(void);

extern const char *const *Rr_GetVulkanExtensions(uint32_t *Count);

extern bool Rr_CreateVulkanSurface(void *Instance, void **Surface);

typedef atomic_bool Rr_Spinlock;

extern void Rr_LockSpinlock(Rr_Spinlock *Spinlock);

extern bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock);

extern void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock);

extern void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes);

extern void Rr_AlignedFree(void *Ptr);

extern bool Rr_PollPlatformEvent(Rr_Event *Event);

extern Rr_IntVec2 Rr_GetDefaultWindowSize(void);

extern void Rr_ShowWindow(void);

extern bool Rr_IsWindowMinimized(void);

extern bool Rr_IsWindowFullscreen(void);

extern void Rr_SetWindowFullscreen(bool Fullscreen);

extern float Rr_GetDisplayRefreshRate(void);

extern Rr_Window *gWindow;
