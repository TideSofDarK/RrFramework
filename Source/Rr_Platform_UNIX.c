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

#define __USE_POSIX199309
#include <assert.h>
#include <time.h>

#define __USE_MISC
#include <sys/mman.h>
#include <unistd.h>

#include <stdlib.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform(void)
{
    PlatformInfo.PageSize = getpagesize();
    PlatformInfo.AllocationGranularity = PlatformInfo.PageSize;

    struct timespec Timespec;
    assert(clock_gettime(CLOCK_MONOTONIC_RAW, &Timespec) == 0);

    return true;
}

Rr_PlatformInfo *Rr_GetPlatformInfo(void)
{
    return &PlatformInfo;
}

uint64_t Rr_GetPerformanceCounter(void)
{
    struct timespec Now;

    clock_gettime(CLOCK_MONOTONIC_RAW, &Now);
    uint64_t Ticks = (uint64_t)Now.tv_sec;
    Ticks *= 1000000000;
    Ticks += (uint64_t)Now.tv_nsec;

    return Ticks;
}

uint64_t Rr_GetPerformanceFrequency(void)
{
    return 1000000000;
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

void *Rr_AlignedAlloc(size_t Size, size_t Alignment)
{
    return aligned_alloc(Alignment, Size);
}

void Rr_AlignedFree(void *Ptr)
{
    free(Ptr);
}

int Rr_LoadAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    return __atomic_load_n(&AtomicInt->Value, __ATOMIC_RELAXED);
}

int Rr_ExchangeAtomicAcquire(Rr_AtomicInt *AtomicInt, int Value)
{
    return __atomic_exchange_n(&AtomicInt->Value, Value, __ATOMIC_ACQUIRE);
}

void Rr_StoreAtomicRelease(Rr_AtomicInt *AtomicInt, int Value)
{
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELEASE);
}

void Rr_StoreAtomicRelaxed(Rr_AtomicInt *AtomicInt, int Value)
{
    __atomic_store_n(&AtomicInt->Value, Value, __ATOMIC_RELAXED);
}

int Rr_IncrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    return __atomic_fetch_add(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
}

int Rr_DecrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    return __atomic_fetch_sub(&AtomicInt->Value, 1, __ATOMIC_RELAXED);
}

void Rr_SleepNS(uint64_t Nanoseconds)
{
    struct timespec Requested, Remaining;
    Remaining.tv_sec = (time_t)(Nanoseconds / 1000000000);
    Remaining.tv_nsec = (long)(Nanoseconds % 1000000000);
    Requested.tv_sec = Remaining.tv_sec;
    Requested.tv_nsec = Remaining.tv_nsec;
    nanosleep(&Requested, &Remaining);
}
