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

Rr_PlatformInfo *Rr_GetPlatformInfo(void)
{
    static Rr_PlatformInfo PlatformInfo = { 0 };

    return &PlatformInfo;
}

Rr_Platform *Rr_GetPlatform(void)
{
    static Rr_Platform Platform = { 0 };

    return &Platform;
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    Rr_Platform *Platform = Rr_GetPlatform();

    return Platform->MousePositionDelta;
}

void Rr_ToggleWindowFullscreen(void)
{
    Rr_SetWindowFullscreen(!Rr_IsWindowFullscreen());
}

void Rr_LockSpinlock(Rr_Spinlock *Spinlock)
{
    for (;;)
    {
        if (!Rr_ExchangeAtomicAcquire(Spinlock, 1))
        {
            return;
        }
        while (Rr_LoadAtomicRelaxed(Spinlock))
        {
#ifdef __SSE2__
            _mm_pause();
#endif
        }
    }
}

bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock)
{
    bool Locked = !Rr_LoadAtomicRelaxed(Spinlock) &&
                  !Rr_ExchangeAtomicAcquire(Spinlock, 1);
    return Locked;
}

void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock)
{
    Rr_StoreAtomicRelease(Spinlock, 0);
}
