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

#if defined(_MSC_VER) && !defined(__clang__) && !defined(__INTEL_COMPILER)
#define RR_THREAD_LOCAL __declspec(thread)
#else
#define RR_THREAD_LOCAL __thread
#endif

#define RR_WINDOWED_RATIO 0.85f

struct Rr_Platform
{
    void *Window;
    bool WindowScaled;
    bool Wayland;
    Rr_Vec2 WindowScale;
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

extern void (*Rr_GetVkGetInstanceProcAddr(void))(void);

extern const char *const *Rr_GetVulkanExtensions(uint32_t *Count);

extern bool Rr_CreateVulkanSurface(void *Instance, void **Surface);

extern void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes);

extern void Rr_AlignedFree(void *Ptr);

extern bool Rr_PollPlatformEvent(Rr_Event *Event);

extern void Rr_ShowWindow(void);

extern bool Rr_IsWindowMinimized(void);

extern bool Rr_IsWindowFullscreen(void);

extern void Rr_SetWindowFullscreen(bool Fullscreen);

extern float Rr_GetDisplayRefreshRate(void);

typedef struct Rr_AtomicInt
{
    int Value;
} Rr_AtomicInt;

extern int Rr_LoadAtomicRelaxed(Rr_AtomicInt *AtomicInt);

extern int Rr_ExchangeAtomicAcquire(Rr_AtomicInt *AtomicInt, int Value);

extern void Rr_StoreAtomicRelease(Rr_AtomicInt *AtomicInt, int Value);

extern void Rr_StoreAtomicRelaxed(Rr_AtomicInt *AtomicInt, int Value);

extern int Rr_IncrementAtomicRelaxed(Rr_AtomicInt *AtomicInt);

extern int Rr_DecrementAtomicRelaxed(Rr_AtomicInt *AtomicInt);

typedef Rr_AtomicInt Rr_Spinlock;

extern void Rr_LockSpinlock(Rr_Spinlock *Spinlock);

extern bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock);

extern void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock);

extern void Rr_SleepNS(uint64_t Nanoseconds);

extern Rr_Platform *gPlatform;
