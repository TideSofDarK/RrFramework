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

#include "Rr_App.h"
#include "Rr_Log.h"

#include "stdlib.h"

Rr_Platform *gPlatform = NULL;

Rr_IntVec2 Rr_GetDefaultWindowSize(void)
{
    Rr_IntVec2 DisplaySize = Rr_GetDisplaySize();

    float ScaleFactor = 0.75f;

    return (Rr_IntVec2){ .Width = (int32_t)(DisplaySize.Width * ScaleFactor),
                         .Height =
                             (int32_t)(DisplaySize.Height * ScaleFactor) };
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    return gPlatform->MousePositionDelta;
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
            _mm_pause();
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
