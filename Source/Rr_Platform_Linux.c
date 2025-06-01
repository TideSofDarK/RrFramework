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

#include "Rr_Log.h"

#include <assert.h>

#define __USE_MISC
#include <sys/mman.h>
#include <unistd.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform(void)
{
    PlatformInfo.PageSize = getpagesize();
    PlatformInfo.AllocationGranularity = PlatformInfo.PageSize;

    return true;
}

Rr_PlatformInfo *Rr_GetPlatformInfo(void)
{
    return &PlatformInfo;
}

void *Rr_ReserveMemory(size_t Size)
{
    return mmap(0, Size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

void Rr_ReleaseMemory(void *Data, size_t Size)
{
    munmap(Data, Size);
}

bool Rr_CommitMemory(void *Data, size_t Size)
{
    if (mprotect(Data, Size, PROT_READ | PROT_WRITE))
    {
        return false;
    }
    mlock(Data, Size);
    munlock(Data, Size);
    return true;
}

void Rr_DecommitMemory(void *Data, size_t Size)
{
    madvise(Data, Size, MADV_DONTNEED);
    mprotect(Data, Size, PROT_NONE);
}

int Rr_GetAtomicInt(Rr_AtomicInt *AtomicInt)
{
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_SEQ_CST);
}

int Rr_SetAtomicInt(Rr_AtomicInt *AtomicInt, int Value)
{
    return __sync_lock_test_and_set(&AtomicInt->Value, Value);
}

int Rr_AddAtomicInt(Rr_AtomicInt *AtomicInt, int Value)
{
    return __sync_fetch_and_add(&AtomicInt->Value, Value);
}

bool Rr_TryLockSpinlock(Rr_Spinlock *SpinLock)
{
    return __sync_lock_test_and_set(SpinLock, 1) == 0;
}

void Rr_UnlockSpinlock(Rr_Spinlock *SpinLock)
{
    __sync_lock_release(SpinLock);
}
