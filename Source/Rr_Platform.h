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

#include <Rr/Rr_Platform.h>

#include <Rr/Rr_Memory.h>

struct Rr_AppConfig;

extern bool Rr_InitPlatform(void);

extern bool Rr_InitPlatformLibrary(struct Rr_AppConfig *Config);

extern bool Rr_CleanupPlatformLibrary(void);

extern void (*Rr_GetVkGetInstanceProcAddr(void))(void);

extern const char *const *Rr_GetVulkanExtensions(uint32_t *Count);

extern bool Rr_CreateVulkanSurface(
    Rr_Window Window,
    void *Instance,
    void **Surface);

typedef atomic_bool Rr_Spinlock;

extern void Rr_LockSpinlock(Rr_Spinlock *Spinlock);

extern bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock);

extern void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock);

extern void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes);

extern void Rr_AlignedFree(void *Ptr);

extern bool Rr_PollPlatformEvent(Rr_Event *Event);

extern Rr_Window Rr_CreateWindow(const char *Title, Rr_WindowFlags Flags);

extern void Rr_DestroyWindow(Rr_Window Window);

extern void Rr_ShowWindow(Rr_Window Window);

extern bool Rr_IsWindowMinimized(Rr_Window Window);

extern bool Rr_IsWindowFullscreen(Rr_Window Window);

extern void Rr_SetWindowFullscreen(Rr_Window Window, bool Fullscreen);

extern void Rr_SetWindowRelativeMouseMode(Rr_Window Window, bool Relative);

extern float Rr_GetDisplayRefreshRate(Rr_Window Window);
