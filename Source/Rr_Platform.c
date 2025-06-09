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

void Rr_LockSpinlock(Rr_Spinlock *Spinlock)
{
    for (;;)
    {
        if (!atomic_exchange_explicit(Spinlock, true, memory_order_acquire))
        {
            return;
        }
        while (atomic_load_explicit(Spinlock, memory_order_relaxed))
            ;
    }
}

bool Rr_TryLockSpinlock(Rr_Spinlock *Spinlock)
{
    return !atomic_load_explicit(Spinlock, memory_order_relaxed) &&
           !atomic_exchange_explicit(Spinlock, true, memory_order_acquire);
}

void Rr_UnlockSpinlock(Rr_Spinlock *Spinlock)
{
    atomic_store_explicit(Spinlock, false, memory_order_release);
}
