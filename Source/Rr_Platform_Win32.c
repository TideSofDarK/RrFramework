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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

static Rr_PlatformInfo PlatformInfo;

bool Rr_InitPlatform()
{
    SYSTEM_INFO SystemInfo;
    GetSystemInfo(&SystemInfo);
    PlatformInfo.PageSize = SystemInfo.dwPageSize;
    PlatformInfo.AllocationGranularity = SystemInfo.dwAllocationGranularity;

    LARGE_INTEGER Frequency;
    QueryPerformanceFrequency(&Frequency);
    PlatformInfo.PerformanceFrequency = (uint64_t)Frequency.QuadPart;

    return true;
}

Rr_PlatformInfo *Rr_GetPlatformInfo()
{
    return &PlatformInfo;
}

uint64_t Rr_GetPerformanceCounter(void)
{
    LARGE_INTEGER Counter;
    QueryPerformanceCounter(&Counter);
    return (uint64_t)Counter.QuadPart;
}

uint64_t Rr_GetPerformanceFrequency(void)
{
    return PlatformInfo.PerformanceFrequency;
}

void *Rr_ReserveMemory(size_t Size)
{
    return VirtualAlloc(0, Size, MEM_RESERVE, PAGE_READWRITE);
}

void Rr_ReleaseMemory(void *Data, size_t Size)
{
    VirtualFree(Data, 0, MEM_RELEASE);
}

bool Rr_CommitMemory(void *Data, size_t Size)
{
    return VirtualAlloc(Data, Size, MEM_COMMIT, PAGE_READWRITE) != NULL;
}

void Rr_DecommitMemory(void *Data, size_t Size)
{
    VirtualFree(Data, Size, MEM_DECOMMIT);
}

void *Rr_AlignedAlloc(size_t Size, size_t Alignment)
{
    return _aligned_malloc(Size, Alignment);
}

void Rr_AlignedFree(void *Ptr)
{
    _aligned_free(Ptr);
}

/* TODO: Figure out *NoFence functions. */

int Rr_LoadAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    /* return _InterlockedOr((long *)&AtomicInt->Value, 0); */
    return AtomicInt->Value;
}

int Rr_ExchangeAtomicAcquire(Rr_AtomicInt *AtomicInt, int Value)
{
    return _InterlockedExchange(&AtomicInt->Value, Value);
}

void Rr_StoreAtomicRelease(Rr_AtomicInt *AtomicInt, int Value)
{
    _InterlockedExchange(&AtomicInt->Value, Value);
}

void Rr_StoreAtomicRelaxed(Rr_AtomicInt *AtomicInt, int Value)
{
    AtomicInt->Value = Value;
}

int Rr_IncrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    return _InterlockedIncrement(&AtomicInt->Value);
}

int Rr_DecrementAtomicRelaxed(Rr_AtomicInt *AtomicInt)
{
    return _InterlockedDecrement(&AtomicInt->Value);
}

static HANDLE Rr_GetWaitableEvent(void)
{
    static RR_THREAD_LOCAL HANDLE Event = NULL;
    if (!Event)
    {
        Event = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    return Event;
}

#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2
#endif

static HANDLE Rr_GetWaitableTimer(void)
{
    static RR_THREAD_LOCAL HANDLE Timer = NULL;
    if (!Timer)
    {
        Timer = CreateWaitableTimerExW(
            NULL,
            NULL,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
            TIMER_ALL_ACCESS);
    }
    return Timer;
}

void Rr_SleepNS(uint64_t Nanoseconds)
{
    HANDLE Timer = Rr_GetWaitableTimer();
    if (Timer)
    {
        LARGE_INTEGER DueTime;
        DueTime.QuadPart = -((LONGLONG)Nanoseconds / 100);
        if ((SetWaitableTimerEx(Timer, &DueTime, 0, NULL, NULL, NULL, 0)) ||
            SetWaitableTimer(Timer, &DueTime, 0, NULL, NULL, 0))
        {
            WaitForSingleObject(Timer, INFINITE);
        }
        return;
    }

    const uint64_t MaxDelay = 0xffffffffLL * 1000000;
    if (Nanoseconds > MaxDelay)
    {
        Nanoseconds = MaxDelay;
    }
    const DWORD Delay = (DWORD)(Nanoseconds / 1000000);

    HANDLE Event = Rr_GetWaitableEvent();
    if (Event)
    {
        WaitForSingleObjectEx(Event, Delay, FALSE);
        return;
    }

    Sleep(Delay);
}
